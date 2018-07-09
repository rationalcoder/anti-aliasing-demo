#pragma once

#include "common.h"
#include "memory.h"

#include "platform.h"
#include "primitives.h"
#include "input.h"
#include "camera.h"

//#include "demo.h"

constexpr u32 us_per_update() { return 5000; }
constexpr b32 should_step()   { return false; }

constexpr int target_opengl_version_major() { return 4; }
constexpr int target_opengl_version_minor() { return 3; }


struct Game_Frame_Stats
{
    u64 frameTimeAccumulator = 0;
    f64 frameTimeAverage     = 0;
    u32 framesAveraged       = 1; // 1 on purpose.

    f32 fps() const { return (f32)(1000000/frameTimeAverage); }
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

struct Game
{
    Game_Frame_Stats frameStats;
    Game_Input input;
    Game_Resolution closestRes;
    Game_Resolution clientRes;

    Camera camera;

    // Demo_Scene demoScene;

    Memory_Arena rendererWorkspace; // for the actual renderer
    Push_Buffer renderCommandBuffer;

    b32 shouldQuit = false;
};

struct Game_Memory
{
    FIELD_ARRAY(Memory_Arena, arenas, {
        Memory_Arena perm;
        Memory_Arena temp;
        Memory_Arena file;
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
    perm.max  = Megabytes(1);

    Memory_Arena& temp = request.temp;
    temp.tag  = "Temporary Storage";
    temp.size = Kilobytes(16);
    temp.max  = Megabytes(4);

    Memory_Arena& file = request.file;
    file.tag  = "File Storage";
    file.size = Megabytes(1);
    file.max  = Megabytes(8);

    // XXX NOTE(blake): where/how this CB is set highly subject to change.
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
game_play_sound();

extern void
game_render(f32 frameRatio);

extern void
game_quit();
