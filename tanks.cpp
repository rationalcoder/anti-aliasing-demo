#include "tanks.h"

#include "game_rendering.h"
#include "obj_file.h"

#include "platform.cpp"
#include "stb.cpp"
#include "opengl_renderer.cpp"
#include "obj_file.cpp"

// All globals:

Platform*    gPlatform = nullptr;
Game*        gGame     = nullptr;
Game_Memory* gMem      = nullptr;

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

static inline OBJ_File
parse_test_obj_file(const char* path)
{
    arena_scope(gMem->file);
    buffer32 buffer = read_file_buffer(path);
    return parse_obj_file(buffer);
}

static inline void
setup_test_scene()
{
    gGame->frameCommands    = sub_allocate(gMem->perm, Kilobytes(4), Kilobytes(8), "Frame Game Render Commands");
    gGame->residentCommands = sub_allocate(gMem->perm, Kilobytes(4), Kilobytes(8), "Resident Game Render Commands");

    gGame->camera.look_at(v3(0, 0, 5), v3(0, 1, 0));
    gGame->camera.fov = 75;

    {
        // TODO: add convenient allocation scopes, e.g. allocator_scope(arena) macro;
        gGame->allocator.data = &gMem->modelLoading;

        OBJ_File bob  = parse_test_obj_file("demo/assets/boblampclean.obj");
        OBJ_File heli = parse_test_obj_file("demo/assets/hheli.obj");

        gGame->allocator.data = &gMem->perm;
    }

    // TODO(blake): make these macros that clear the render target after queing commands.
    render_target(gGame->frameCommands); {
        set_clear_color(0.0f, 0.0f, .2f, 1.0f);
        set_projection_matrix(glm::perspective(glm::radians(gGame->camera.fov), 16.0f/9.0f, .1f, 100.0f));
        set_view_matrix(gGame->camera.view_matrix());
        set_viewport(gGame->clientRes);
    }

    render_target(gGame->residentCommands); {
        begin_render_pass();

        v3 color { .1, .1, .1 };

        Grid grid = make_xy_grid(20, 20);
        render_debug_lines(grid.vertices, grid.vertexCount, color);
        // render_debug_cube(v3(-10.5f, 10.5f, 0.0f), .5f, color);

        v3 centers[20] = {};
        points_by_step(20, v3(-10.5f, 10.5f, 0.0f), v3(0, -1, 0), centers);
        render_debug_cubes(view_of(centers), .5f, color);
    }
}

extern b32
game_init(Game_Memory* memory, Platform* platform, Game_Resolution clientRes)
{
    if (!(gGame = push_new(memory->perm, Game)))
        return false;

    gGame->allocator.func = &arena_allocate;
    gGame->allocator.data = &memory->perm;
    gGame->temp           = &memory->temp;

    game_patch_after_hotload(memory, platform);

    Game_Resolution closestResolution = tightest_supported_resolution(clientRes);
    gGame->closestRes = closestResolution;
    gGame->clientRes  = clientRes;

    if (!renderer_init(&memory->perm, &gGame->rendererWorkspace))
        return false;

    gGame->targetRenderCommandBuffer = &gGame->frameCommands;

    setup_test_scene();
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

    f32 camSpeed = 3*dt; // XXX
    Camera& camera = gGame->camera;

    if (kb.down(GK_W)) camera.forward_by(camSpeed);
    if (kb.down(GK_S)) camera.forward_by(-camSpeed);
    if (kb.down(GK_A)) camera.right_by(-camSpeed);
    if (kb.down(GK_D)) camera.right_by(camSpeed);
    if (kb.down(GK_E)) camera.up_by(camSpeed);

    Game_Mouse& mouse = gGame->input.mouse;
    Game_Mouse_State& curMouse = gGame->input.mouse.cur;

    Input_Smoother& smoother = gGame->inputSmoother;

    if (mouse.drag_started())
        smoother.reset_mouse_drag();


    if (curMouse.dragging) {
        f32 xDrag = map_bilateral(curMouse.xDrag, gGame->clientRes.w);
        f32 yDrag = map_bilateral(curMouse.yDrag, gGame->clientRes.h);

        v2 smoothedDrag = smoother.smooth_mouse_drag(xDrag, yDrag);

        camera.yaw_by(2 * -smoothedDrag.x);
        camera.pitch_by(2 * smoothedDrag.y);
    }

    // FPS-style clamping.
    camera.up = v3(0.0f, 0.0f, 1.0f);

    // log_debug("FPS: %f %f\n", gGame->frameStats.fps(), dt);
}

extern void
game_resize(Game_Resolution clientRes)
{
    Game_Resolution closestResolution = tightest_supported_resolution(clientRes);
    //printf("Resizing to: %ux%u\n", closestResolution.w, closestResolution.h);
    // set_viewport(clientRes);

    gGame->closestRes = closestResolution;
    gGame->clientRes  = clientRes;

    render_target(gGame->frameCommands);
    set_viewport(0, 0, clientRes.w, clientRes.h);
}

extern void
game_play_sound(u64)
{
}

extern void
game_render(f32 /*frameRatio*/)
{
    // Render frame local changes like resizes, viewport, etc.
    render_target(gGame->frameCommands);

    set_view_matrix(gGame->camera.view_matrix());

    render_commands(gGame->frameCommands);
    render_commands(gGame->residentCommands);
}

extern void
game_end_frame()
{
    reset(gMem->temp);
    reset(gGame->frameCommands);
}

extern void
game_quit()
{
}

// NOTE: not currently needed, so it's moved out of the way. It should be after update()
extern void
game_step() {}

