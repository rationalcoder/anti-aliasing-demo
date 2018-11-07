#include "tanks.h"

#include "stb.h"
#include "imgui.h"

#include "game_rendering.h"
#include "obj_file.h"

#include "platform.cpp"
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
read_obj_file(const char* path)
{
    buffer32 buffer = read_file_buffer(path);

    return parse_obj_file(buffer, PostProcess_GenNormals | PostProcess_GenTangents | PostProcess_FlipUVs);
    //return parse_obj_file(buffer, PostProcess_GenNormals);
}

static inline MTL_File
read_mtl_file(buffer32 path)
{
    buffer32 buffer = read_file_buffer(path);

    return parse_mtl_file(buffer);
}

static inline void
setup_test_scene()
{
    gGame->camera.look_at(v3(-5, 0, 3), v3(0, 1, 0));
    gGame->camera.fov = 75;

    gGame->allocator.data = &gMem->modelLoading;

    //stbi_set_flip_vertically_on_load(true);

    //OBJ_File bob  = read_obj_file("demo/assets/boblampclean.obj");
    OBJ_File heli = read_obj_file("demo/assets/hheli.obj");
    OBJ_File box = read_obj_file("demo/assets/box.obj");

    //MTL_File bobMtl  = read_mtl_file(cat("demo/assets/", bob.mtllib));
    MTL_File heliMtl = read_mtl_file(cat("demo/assets/", heli.mtllib));
    MTL_File boxMtl = read_mtl_file(cat("demo/assets/", box.mtllib));

    //Static_Mesh bobMesh  = load_static_mesh(bob, bobMtl, "demo/assets/");
    Static_Mesh heliMesh = load_static_mesh(heli, heliMtl, "demo/assets/");
    Static_Mesh boxMesh = load_static_mesh(box, boxMtl, "demo/assets/");

    gGame->allocator.data = &gMem->perm;

    // TODO(blake): make these macros that clear the render target after queing commands.
    set_render_target(gGame->frameBeginCommands); {
        cmd_set_clear_color(0.015f, 0.015f, 0.015f, 1.0f);
        cmd_set_projection_matrix(glm::perspective(glm::radians(gGame->camera.fov), 16.0f/9.0f, .1f, 100.0f));
        cmd_set_view_matrix(gGame->camera.view_matrix());
        cmd_set_viewport(gGame->clientRes);
    }

    set_render_target(gGame->residentCommands); {
        v3 lightPos(-3, 0, 1);
        v3 lightColor(1, 1, 1);

        cmd_render_point_light(lightPos, lightColor);
        cmd_render_debug_cube(lightPos, .15f, lightColor);


        v3 color { .07, .07, .07 };

        Grid grid = make_xy_grid(20, 20);
        cmd_render_debug_lines(grid.vertices, grid.vertexCount, color);

        // render_debug_cube(v3(-10.5f, 10.5f, 0.0f), .5f, color);

        v3 centers[20] = {};
        points_by_step(20, v3(-10.5f, 10.5f, 0.0f), v3(0, -.5, 0), centers);
        cmd_render_debug_cubes(view_of(centers), .5f, color);
        //cmd_render_static_mesh(bobMesh, mat4(.5f));

        mat4 xform;
        xform = glm::rotate(xform, glm::pi<f32>()/2, v3(1, 0, 0));
        xform = glm::scale(xform, v3(.02f));

        //cmd_render_static_mesh(bobMesh, xform);
        cmd_render_static_mesh(heliMesh, xform);

        //cmd_render_static_mesh(boxMesh, mat4(mat3(.5f)));
        cmd_render_static_mesh(boxMesh, mat4(1));

        Debug_Normals boxNormals = make_debug_normals(boxMesh);
        cmd_render_debug_lines(boxNormals.t, boxNormals.vertexCount, v3(1, 0, 0));
        cmd_render_debug_lines(boxNormals.b, boxNormals.vertexCount, v3(0, 1, 0));
        cmd_render_debug_lines(boxNormals.n, boxNormals.vertexCount, v3(0, 0, 1));
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

    gGame->frameBeginCommands    = sub_allocate(gMem->perm, Kilobytes(4), Kilobytes(8), "Frame Game Render Commands");
    gGame->residentCommands = sub_allocate(gMem->perm, Kilobytes(4), Kilobytes(8), "Resident Game Render Commands");

    gGame->targetRenderCommandBuffer = &gGame->frameBeginCommands;

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

    f32 up      = 0;
    f32 right   = 0;
    f32 forward = 0;

    if (kb.down(GK_W)) forward += camSpeed;
    if (kb.down(GK_S)) forward -= camSpeed;
    if (kb.down(GK_A)) right   -= camSpeed;
    if (kb.down(GK_D)) right   += camSpeed;
    if (kb.down(GK_E)) up      += camSpeed;

    v3 movement(up, right, forward);

    if (movement.x || movement.y || movement.z) {
        movement = glm::normalize(v3(up, right, forward)) * camSpeed;

        camera.up_by(movement.x);
        camera.right_by(movement.y);
        camera.forward_by(movement.z);
    }


    Game_Mouse& mouse = gGame->input.mouse;
    Game_Mouse_State& curMouse = gGame->input.mouse.cur;

    Input_Smoother& smoother = gGame->inputSmoother;

    if (mouse.drag_started())
        smoother.reset_mouse_drag();


    if (curMouse.dragging) {
        f32 xDrag = map_bilateral(curMouse.xDrag, gGame->clientRes.w);
        f32 yDrag = map_bilateral(curMouse.yDrag, gGame->clientRes.h);

        v2 smoothedDrag = smoother.smooth_mouse_drag(xDrag, yDrag);
        //v2 smoothedDrag(xDrag, yDrag);

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

    set_render_target(gGame->frameBeginCommands);
    cmd_set_viewport(0, 0, clientRes.w, clientRes.h);
}

extern void
game_play_sound(u64)
{
}

extern void
game_render(f32 /*frameRatio*/, struct ImDrawData* imguiData)
{
    // Render frame local changes like resizes, viewport, etc.
    set_render_target(gGame->frameBeginCommands);
    cmd_set_view_matrix(gGame->camera.view_matrix());

    renderer_begin_frame(&gGame->rendererWorkspace,
                         gGame->frameBeginCommands.arena.start,
                         gGame->frameBeginCommands.count);

    render_commands(gGame->residentCommands);

    renderer_end_frame(&gGame->rendererWorkspace, imguiData);
}

extern void
game_end_frame()
{
    reset(gMem->temp);
    reset(gGame->frameBeginCommands);
}

extern void
game_quit()
{
}

// NOTE: not currently needed, so it's moved out of the way. It should be after update()
extern void
game_step() {}

