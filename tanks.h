#define USING_IMGUI 1

#ifdef _WIN64
    #include "win32_tanks.h"
#else
    #error "Unsupported platform"
#endif

// @CRT @Dependency
#include <new>

#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <assert.h>
#include <math.h>
#include <time.h>

// @Dependency
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "globals.h"
#include "platform.h"
#include "common.h"
#include "primitives.h"
#include "memory.h"
#include "buffer.h"
#include "input.h"
#include "camera.h"
#include "mesh.h"
#include "renderer.h"
#include "containers.h"
#include "obj_file.h"
#include "geometry.h"
#include "game_rendering.h"
#include "developer_ui.h"

#include "stb.h"
#include "imgui.h"


constexpr u32 us_per_step() { return 8333; }
constexpr b32 should_step() { return false; }

constexpr u32 target_frame_time_us() { return 16667; }

constexpr int target_opengl_version_major() { return 4; }
constexpr int target_opengl_version_minor() { return 3; }


struct Game_Frame_Stats
{
    u64 frameTimes[5]; // us
    u64_window frameTimeWindow;

    Game_Frame_Stats() { frameTimeWindow.reset(frameTimes, array_size(frameTimes), 16667); }

    f32 fps() const { return (f32)(1000000/frameTimeWindow.average); }
};

struct Game_State
{
    void* data                   = nullptr;
    void (*enter)(void*)         = nullptr;
    void (*leave)(void*)         = nullptr;
    void (*play_sound)(void*)    = nullptr;
    void (*render)(void*, f32)   = nullptr;
    Game_State* (*update)(void*) = nullptr;
};

struct Tank
{
    Static_Mesh mesh;
    AABB boundingBox;
    v2 forward;
};

struct Game
{
    // Filled in during game_init()
    Allocator allocator = {};
    Memory_Arena* temp  = nullptr;

    // Touched by the platform layer.
    Game_Frame_Stats frameStats;
    Game_Input input;
    Game_Resolution closestRes;
    Game_Resolution clientRes;

    Input_Smoother inputSmoother;

    Camera camera;

    Memory_Arena rendererWorkspace; // for the actual renderer

    Push_Buffer* targetRenderCommandBuffer = nullptr; // where to push game render commands
    Push_Buffer  frameBeginCommands;
    Push_Buffer  residentCommands;

    Developer_UI devUi;

    Bucket_List<Tank> tanks;

    bool showDeveloperUi = false;
    b32 shouldQuit = false;
};


inline void*
game_allocate_(umm size, u32 alignment)
{ return gGame->allocator.func(gGame->allocator.data, size, alignment); }

inline void*
game_allocate_zero_(umm size, u32 alignment)
{
    void* space = gGame->allocator.func(gGame->allocator.data, size, alignment);
    memset(space, 0, size);

    return space;
}

inline void*
game_allocate_copy_(umm size, u32 alignment, void* data)
{
    void* space = gGame->allocator.func(gGame->allocator.data, size, alignment);
    memcpy(space, data, size);

    return space;
}

template <typename T_> inline T_
deduce_type_from_new_expression_(T_*);

#define allocate(size, alignment) game_allocate_(size, alignment)
#define allocate_bytes(size) (char*)game_allocate_(size, 1)
#define allocate_array(n, type) (type*)game_allocate_((n)*sizeof(type), alignof(type))
#define allocate_array_zero(n, type) (type*)game_allocate_zero_((n)*sizeof(type), alignof(type))
#define allocate_array_copy(n, type, data) (type*)game_allocate_copy_((n)*sizeof(type), alignof(type), data)
#define allocate_type(type) ((type*)game_allocate_(sizeof(type), alignof(type)))
#define allocate_new(newExpr)\
    ([&] () {\
        using Type = decltype(deduce_type_from_new_expression_(new newExpr));\
        return (new (game_allocate_(sizeof(Type), alignof(Type))) newExpr);\
    } ())

#define allocate_new_array(n, newExpr)\
    [&]() {\
        using Type = decltype(deduce_type_from_new_expression_(new newExpr));\
        Type* data = (Type*)game_allocate_((n)*sizeof(Type), alignof(Type));\
        for (int i = 0; i < (int)n; i++)\
            new (data + i) newExpr;\
        return data;\
    } ()

#define temp_allocate(size, alignment) push(*gGame->temp, size, alignment)
#define temp_bytes(size) (char*)push_bytes(*gGame->temp, size)
#define temp_array(n, type) (type*)push(*gGame->temp, (n)*sizeof(type), alignof(type))
#define temp_array_zero(n, type) (type*)push_zero(*gGame->temp, (n)*sizeof(type), alignof(type))
#define temp_array_copy(n, type, data) (type*)push_copy(*gGame->temp, (n)*sizeof(type), alignof(type), data)
#define temp_type(type) ((type*)push(*gGame->temp, sizeof(type), alignof(type)))
#define temp_new(newExpr)\
    [&]() {\
        using Type = decltype(deduce_type_from_new_expression_(new newExpr));\
        return new (push(*gGame->temp, sizeof(Type), alignof(Type))) newExpr;\
    } ()

#define temp_new_array(n, newExpr)\
    [&]() {\
        using Type = decltype(deduce_type_from_new_expression_(new newExpr));\
        Type* data = (Type*)push(*gGame->temp, (n)*sizeof(Type), alignof(Type));\
        for (int i = 0; i < (int)n; i++)\
            new (data + i) newExpr;\
        return data;\
    } ()


struct Allocator_Scope
{
    Allocator saved;
    Allocator temp;
    b32 moved = false; // support null allocator just in case.

    Allocator_Scope(Allocator temp) : saved(gGame->allocator), temp(temp) { gGame->allocator = temp; }
    Allocator_Scope(Allocator_Scope&& scope) : saved(scope.saved), temp(scope.temp) { scope.moved = true; }

    ~Allocator_Scope() { if (!moved) gGame->allocator = saved; }
};

inline Allocator_Scope
make_allocator_scope(Allocate_Func* func, void* data) { return Allocator_Scope(Allocator(func, data)); }

inline Allocator_Scope
make_allocator_scope(Memory_Arena& arena) { return Allocator_Scope(Allocator(&arena_allocate, &arena)); }

inline Allocator_Scope
make_allocator_scope(Memory_Arena* arena) { return Allocator_Scope(Allocator(&arena_allocate, arena)); }

#define allocator_scope_impl(counter, ...) Allocator_Scope allocatorScope##counter = make_allocator_scope(__VA_ARGS__)
#define allocator_scope(...) allocator_scope_impl(__COUNTER__, __VA_ARGS__)

#define temp_scope() arena_scope(gMem->temp)

struct Game_Memory
{
    FIELD_ARRAY(Memory_Arena, arenas, {
        Memory_Arena perm;
        Memory_Arena temp;
        Memory_Arena file;
        Memory_Arena modelLoading;
    });
};

// NOTE: the platform layer will fill the actual memory fields out
// and pass it back to us in game_init().
inline Game_Memory
game_get_memory_request(Platform* platform)
{
    Game_Memory request = {};

    Memory_Arena& perm = request.perm;
    perm.tag  = "Permanent Storage";
    perm.size = Kilobytes(16);
    perm.max  = Megabytes(2);

    Memory_Arena& temp = request.temp;
    temp.tag  = "Temporary Storage";
    temp.size = Kilobytes(16);
    temp.max  = Megabytes(2);

    Memory_Arena& file = request.file;
    file.tag  = "File Storage";
    file.size = Megabytes(1);
    file.max  = Megabytes(16);

    Memory_Arena& modelLoading = request.modelLoading;
    modelLoading.tag  = "Model Loading Storage";
    modelLoading.size = Megabytes(1);
    modelLoading.max  = Megabytes(64);

    // NOTE(blake): where/how this CB is set highly subject to change.
    assert(platform->initialized);
    for (Memory_Arena& arena : request.arenas)
        arena.expand = platform->expand_arena;

    return request;
}

extern b32
game_init(Game_Memory* memory, Platform* platform, Game_Resolution clientRes);

extern void
game_patch_after_hotload(Game_Memory* memory, Platform* platform);

extern void
game_tick(f32 dt);

extern void
game_step(f32 dt);

extern void
game_resize(Game_Resolution clientRes);

extern void
game_play_sound(u64 microElapsed);

extern void
game_render(f32 stepDt, f32 frameRatio, struct ImDrawData* drawData);

extern void
game_quit();
