#include "tanks.h"
#include "game_rendering.h"

#include "platform.cpp"
#include "mesh_loading.cpp"
#include "scene_printer.cpp"
#include "stb.cpp"
#include "opengl_renderer.cpp"

// All globals:

Platform* gPlatform = nullptr;
Game* gGame = nullptr;
Game_Memory* gMem = nullptr;

// TODO: query supported resolutions, start in fullscreen mode, etc.
static Game_Resolution supportedResolutions[] = {
    Game_Resolution { 1024, 576  },
    Game_Resolution { 1152, 648  },
    Game_Resolution { 1280, 720  },
    Game_Resolution { 1366, 768  },
    Game_Resolution { 1600, 900  },
    Game_Resolution { 1920, 1080 },
};

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
    if (!(gGame = arena_push_new(memory->perm, Game)))
        return false;

    game_patch_after_hotload(memory, platform);

    Game_Resolution closestResolution = tightest_supported_resolution(clientRes);
    //log_debug("Closest Resolution: %ux%u\n", closestResolution.w, closestResolution.h);

    if (!renderer_init(&memory->perm, &gGame->rendererWorkspace)) return false;
    gGame->renderCommandBuffer = arena_sub_allocate_push_buffer(memory->perm, Kilobytes(4), 16, "Game Render Commands");

    gGame->camera.look_at(glm::vec3(0, 0, 5), glm::vec3(0, 1, 0));

    gGame->closestRes = closestResolution;
    gGame->clientRes  = clientRes;
    return true;
}

extern void
game_patch_after_hotload(Game_Memory* memory, Platform* platform)
{
    gMem      = memory;
    gGame     = (Game*)memory->perm.start;
    gPlatform = platform;
}

static inline f32
map_bilateral(s32 val, u32 range)
{
    return val/(range/2.0f);
}

extern void
game_update(f32 dt)
{
    Game_Keyboard& kb = gGame->input.keyboard;
    if (kb.released(GK_ESCAPE)) {
        gGame->shouldQuit = true;
    }

    f32 camSpeed = 2*dt;
    Camera& camera = gGame->camera;

    if (kb.down(GK_W)) camera.forward_by(camSpeed);
    if (kb.down(GK_S)) camera.forward_by(-camSpeed);
    if (kb.down(GK_A)) camera.right_by(-camSpeed);
    if (kb.down(GK_D)) camera.right_by(camSpeed);
    if (kb.down(GK_E)) camera.up_by(camSpeed);

    Game_Mouse_State& mouse = gGame->input.mouse.cur;
    if (mouse.dragging) {
        if (mouse.xDrag) {
            f32 xDrag = map_bilateral(mouse.xDrag, gGame->clientRes.w);
            camera.yaw_by(2*-xDrag);
        }

        if (mouse.yDrag) {
            f32 yDrag = map_bilateral(mouse.yDrag, gGame->clientRes.h);
            camera.pitch_by(2*yDrag);
        }
    }

    // FPS-style clamping.
    camera.up = glm::vec3(0.0f, 0.0f, 1.0f);

    //log_debug("FPS: %f %f\n", gGame->frameStats.frameTimeAverage, dt);
}

extern void
game_resize(Game_Resolution clientRes)
{
    Game_Resolution closestResolution = tightest_supported_resolution(clientRes);
    //printf("Resizing to: %ux%u\n", closestResolution.w, closestResolution.h);

    gGame->closestRes = closestResolution;
    gGame->clientRes  = clientRes;
}

extern void
game_play_sound()
{
}

extern void
game_render(f32 /*frameRatio*/)
{
    push_buffer_scope(gGame->renderCommandBuffer);

    set_clear_color(.4f, 0.0f, .6f, 1.0f);
    set_projection_matrix(glm::perspective(glm::radians(gGame->camera.fov), 16.0f/9.0f, .1f, 100.0f));
    set_view_matrix(gGame->camera.view_matrix());

    begin_render_pass();

    Grid grid = make_xy_grid(20, 20, &gMem->temp);
    render_debug_lines(grid.vertices, grid.vertexCount, glm::vec4(.1, 0, .5, 1));

    renderer_exec(&gGame->rendererWorkspace, gGame->renderCommandBuffer.arena.start,
                  gGame->renderCommandBuffer.count);
}

extern void
game_end_frame()
{
    arena_reset(gMem->temp);
}

extern void
game_quit()
{
}

// NOTE: not currently needed, so it's moved out of the way. It should be after update()
extern void
game_step() {}

