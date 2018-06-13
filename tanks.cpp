#include "tanks.h"
#include "editor.h"
#include "main_menu.h"
#include "demo.h"
#include <cstdio>

#include <new>

// All globals:

Platform* gPlatform = nullptr;
Game* gGame = nullptr;
Game_Memory* gGameMemory = nullptr;
Game_State gMainMenuState;
Game_State gEditorState;

// TODO: query supported resolutions, start in fullscreen mode, etc.
static Game_Resolution supportedResolutions[] = {
    Game_Resolution { 1024, 576  },
    Game_Resolution { 1152, 648  },
    Game_Resolution { 1280, 720  },
    Game_Resolution { 1366, 768  },
    Game_Resolution { 1600, 900  },
    Game_Resolution { 1920, 1080 },
};

template <typename T_, typename... Args_> static inline T_*
arena_push(Memory_Arena* arena, Args_&&... args)
{
    if (arena->filled + sizeof(T_) > arena->size)
        return nullptr;

    arena->filled += sizeof(T_);
    return new (arena->start) T_(args...);
}

static inline Game_Resolution
tightest_supported_resolution(Game_Resolution candidate)
{
    Game_Resolution* highest = supportedResolutions + (ArraySize(supportedResolutions)-1);
    Game_Resolution* cur     = supportedResolutions;

    for (; cur != highest; cur++) {
        if (candidate.w <= cur->w && candidate.h <= cur->h)
            return *cur;
    }

    return *highest;
}

extern b32
game_init(Game_Memory* memory, Platform* platform, Game_Resolution clientRes)
{
    if (!arena_push<Game>(&memory->perm))
        return false;

    game_patch_after_hotload(memory, platform);
    gGame->state = gMainMenuState;

    Game_Resolution closestResolution = tightest_supported_resolution(clientRes);
    //printf("Closest Resolution: %ux%u\n", closestResolution.w, closestResolution.h);

    if (!demo_load(&gGame->demoScene, clientRes, closestResolution))
        return false;

    gGame->closestRes = closestResolution;
    gGame->clientRes  = clientRes;
    return true;
}

extern void
game_patch_after_hotload(Game_Memory* memory, Platform* platform)
{
    gGameMemory = memory;
    gGame       = (Game*)memory->perm.start;
    gPlatform   = platform;

    grab_main_menu_state(&gGame->mainMenu);
    grab_editor_state(&gGame->editor);
}

// TODO: move input handling out of update.

extern void
game_update(f32 dt)
{
    Game_State* curState  = &gGame->state;
    Game_State* nextState = curState->update(curState);

    if (nextState) {
        curState->leave(curState->data);
        nextState->enter(nextState->data);
        *curState = *nextState;
    }

    demo_update(&gGame->demoScene, &gGame->input, dt);
}

extern void
game_resize(Game_Resolution clientRes)
{
    // TODO: add resizing to states.
    Game_Resolution closestResolution = tightest_supported_resolution(clientRes);
    //printf("Resizing to: %ux%u\n", closestResolution.w, closestResolution.h);

    demo_resize(&gGame->demoScene, clientRes, closestResolution);
    gGame->closestRes = closestResolution;
    gGame->clientRes  = clientRes;
}

extern void
game_play_sound()
{
    Game_State* state = &gGame->state;
    state->play_sound(state->data);
}

extern void
game_render(f32 frameRatio)
{
    Game_State* state = &gGame->state;
    state->render(state->data, frameRatio);
    demo_render(&gGame->demoScene);
}

extern void
game_quit()
{
    demo_free(&gGame->demoScene);
}


// NOTE: not currently needed, so it's moved out of the way. It should be after update()
extern void
game_step() {}

