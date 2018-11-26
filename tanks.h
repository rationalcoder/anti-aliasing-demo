#pragma once

#define USING_IMGUI 1

#include "common.h"
#include "memory.h"
#include "platform.h"
#include "primitives.h"
#include "input.h"
#include "camera.h"
#include "renderer.h"

constexpr u32 us_per_update() { return 5000; }
constexpr b32 should_step()   { return false; }

constexpr u64 target_frame_time_us() { return 16667; }

constexpr int target_opengl_version_major() { return 4; }
constexpr int target_opengl_version_minor() { return 3; }

struct Game_Frame_Stats
{
    u32 frameTimes[5]; // us
    u32_window frameTimeWindow;

    Game_Frame_Stats() { frameTimeWindow.reset(frameTimes, ArraySize(frameTimes), 16667); }

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

struct AA_Demo
{
    bool showSceneConfig = true;

    AA_Technique techniqueCatalog[AA_COUNT_] =
    {
        AA_NONE, AA_FXAA, AA_MSAA_2X, AA_MSAA_2X_FXAA, AA_MSAA_4X,
        AA_MSAA_4X_FXAA, AA_MSAA_8X, AA_MSAA_8X_FXAA, AA_MSAA_16X,
    };

    AA_Technique curTechniques[AA_COUNT_] = {};
    u32          chosenCount              = 0;

    AA_Technique selectedTechnique = AA_NONE;

    Game_Resolution res;

    b32 on = false;

    // Saved information
    AA_Technique chosenTechniques[AA_COUNT_] = {};
    u32          catalogOffset               = 0;
    char         euid[17]                    = {};
};

inline const char*
cstr(AA_Technique t)
{
    switch (t) {
    case AA_NONE:         return "None";
    case AA_FXAA:         return "FXAA";
    case AA_MSAA_2X:      return "MSAA 2X";
    case AA_MSAA_4X:      return "MSAA 4X";
    case AA_MSAA_8X:      return "MSAA 8X";
    case AA_MSAA_16X:     return "MSAA 16X";
    case AA_MSAA_2X_FXAA: return "MSAA 2X FXAA";
    case AA_MSAA_4X_FXAA: return "MSAA 4X FXAA";
    case AA_MSAA_8X_FXAA: return "MSAA 8X FXAA";
    }

    return "Unknown";
}

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

    b32 shouldQuit = false;

    AA_Demo demo; // @Temporary
    b32 demoStarted = false;
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

#define allocate(size, alignment) game_allocate_(size, alignment)
#define allocate_bytes(size) (char*)game_allocate_(size, 1)
#define allocate_array(n, type) (type*)game_allocate_((n)*sizeof(type), alignof(type))
#define allocate_array_zero(n, type) (type*)game_allocate_zero_((n)*sizeof(type), alignof(type))
#define allocate_array_copy(n, type, data) (type*)game_allocate_copy_((n)*sizeof(type), alignof(type), data)
#define allocate_type(type) ((type*)game_allocate_(sizeof(type), alignof(type)))
#define allocate_new(type, ...) (new (game_allocate_(sizeof(type), alignof(type))) (type)(__VA_ARGS__))

#define temp_allocate(size, alignment) push(*gGame->temp, size, alignment)
#define temp_bytes(size) (char*)push_bytes(*gGame->temp, size)
#define temp_array(n, type) (type*)push(*gGame->temp, (n)*sizeof(type), alignof(type))
#define temp_array_zero(n, type) (type*)push_zero(*gGame->temp, (n)*sizeof(type), alignof(type))
#define temp_array_copy(n, type, data) (type*)push_copy(*gGame->temp, (n)*sizeof(type), alignof(type), data)
#define temp_type(type) ((type*)push(*gGame->temp, sizeof(type), alignof(type)))
#define temp_new(type, ...) (new (push(*gGame->temp, sizeof(type), alignof(type))) (type)(__VA_ARGS__))

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
    modelLoading.max  = Megabytes(16);

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
game_update(f32 dt);

extern void
game_step();

extern void
game_resize(Game_Resolution clientRes);

extern void
game_play_sound(u64 microElapsed);

extern void
game_render(f32 frameRatio, struct ImDrawData* drawData);

extern void
game_quit();
