#include "tanks.h"

#include "stb.h"
#include "imgui.h"

#include "game_rendering.h"
#include "obj_file.h"

#include "platform.cpp"
#include "opengl_renderer.cpp"
#include "obj_file.cpp"

// @CRT @Dependency
#include <math.h>
#include <time.h>

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

// For stuff like setting a default specular coefficient.
static inline void
sanitize_materials(MTL_File& file)
{
    for (u32 i = 0; i < file.materialCount; i++) {
        MTL_Material& mat = file.materials[i];
        if (mat.specularExponent == 0)
            mat.specularExponent = 80;
    }
}

static inline MTL_File
read_mtl_file(buffer32 path)
{
    buffer32 buffer = read_file_buffer(path);

    MTL_File result = parse_mtl_file(buffer);
    sanitize_materials(result);

    return result;
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
    OBJ_File box  = read_obj_file("demo/assets/box.obj");
    OBJ_File jeep = read_obj_file("demo/assets/jeep.obj");

    //MTL_File bobMtl  = read_mtl_file(cat("demo/assets/", bob.mtllib));
    MTL_File heliMtl = read_mtl_file(cat("demo/assets/", heli.mtllib));
    MTL_File boxMtl  = read_mtl_file(cat("demo/assets/", box.mtllib));
    MTL_File jeepMtl = read_mtl_file(cat("demo/assets/", jeep.mtllib));

    //Static_Mesh bobMesh  = load_static_mesh(bob, bobMtl, "demo/assets/");
    Static_Mesh heliMesh = load_static_mesh(heli, heliMtl, "demo/assets/");
    Static_Mesh boxMesh  = load_static_mesh(box,  boxMtl,  "demo/assets/");
    Static_Mesh jeepMesh = load_static_mesh(jeep, jeepMtl, "demo/assets/");

    gGame->allocator.data = &gMem->perm;

    // TODO(blake): make these macros that clear the render target after queing commands.
    set_render_target(gGame->frameBeginCommands); {
        cmd_set_clear_color(0.015f, 0.015f, 0.015f, 1.0f);
        cmd_set_projection_matrix(glm::perspective(glm::radians(gGame->camera.fov), 16.0f/9.0f, .1f, 100.0f));
        cmd_set_view_matrix(gGame->camera.view_matrix());
        cmd_set_viewport(gGame->clientRes);
    }

    set_render_target(gGame->residentCommands); {
        v3 lightPos(-3, 1, 3);
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

        mat4 xform;
        xform = glm::rotate(xform, glm::pi<f32>()/2, v3(1, 0, 0));
        xform = glm::scale(xform, v3(.02f));

        cmd_render_static_mesh(heliMesh, xform);

        xform = mat4();
        xform = glm::translate(xform, v3(-3.5, -0.2, -.2f));
        //xform = glm::rotate(xform, glm::pi<f32>()/11, v3(0, 0, 1));
        xform = glm::scale(xform, v3(3, 3, .2f));
        cmd_render_static_mesh(boxMesh, xform);

        xform = mat4();
        xform = glm::translate(xform, v3(-2, 3, 0));
        xform = glm::rotate(xform, glm::pi<f32>(), v3(0, 0, 1));
        xform = glm::rotate(xform, glm::pi<f32>()/2, v3(1, 0, 0));
        xform = glm::scale(xform, v3(.02f));

        xform = glm::scale(xform, v3(.2f));
        cmd_render_static_mesh(jeepMesh, xform);

        // TODO(blake): make debug geometry more systemic.
        //Debug_Normals boxNormals = make_debug_normals(boxMesh);
        //cmd_render_debug_lines(boxNormals.t, boxNormals.vertexCount, v3(1, 0, 0));
        //cmd_render_debug_lines(boxNormals.b, boxNormals.vertexCount, v3(0, 1, 0));
        //cmd_render_debug_lines(boxNormals.n, boxNormals.vertexCount, v3(0, 0, 1));
    }
}

static inline void
init_aa_demo(AA_Demo& demo)
{
    demo.res = gGame->closestRes;
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

    gGame->frameBeginCommands = sub_allocate(gMem->perm, Kilobytes(4), Kilobytes(8), "Frame Game Render Commands");
    gGame->residentCommands   = sub_allocate(gMem->perm, Kilobytes(4), Kilobytes(8), "Resident Game Render Commands");

    gGame->targetRenderCommandBuffer = &gGame->frameBeginCommands;

    setup_test_scene();
    init_aa_demo(gGame->demo);
    return true;
}

extern void
game_patch_after_hotload(Game_Memory* memory, Platform* platform)
{
    gMem      = memory;
    gGame     = (Game*)memory->perm.start;
    gPlatform = platform;
}

static inline void
pick_technique_set(AA_Demo& demo)
{
    // We need the options to be different per person.
    srand((u32)time(NULL));

    demo.catalogOffset = rand() % VALID_COUNT_;

    u32 offset = demo.catalogOffset;
    for (u32 i = 0; i < VALID_COUNT_; i++, right_rotate_value(offset, 0, VALID_COUNT_-1)) {
        demo.curTechniques[i] = demo.techniqueCatalog[offset];
        log_debug("Tech: %s\n", cstr(demo.curTechniques[i]));
    }
}

static inline void
save_demo_results(AA_Demo& demo)
{
    buffer32 content = str("Quality in descending order:\n");

    for (int i = 0; i < ArraySize(demo.chosenTechniques); i++)
        content = cat(cat(content, "\n"), cstr(demo.chosenTechniques[i]));

    const char* path = fmt_cstr("demo/results/%s_%ux%u.txt", demo.euid,
                                demo.res.w, demo.res.h);

    platform_write_file(path, content.data, content.size);
}

static inline void
start_new_demo(AA_Demo& demo)
{
    pick_technique_set(demo);

    demo.chosenCount       = 0;
    demo.selectedTechnique = AA_INVALID;

    memset(demo.euid,             0, sizeof(demo.euid));
    memset(demo.chosenTechniques, 0, sizeof(demo.chosenTechniques));
}


static void
update_aa_demo(AA_Demo& demo)
{
    ImGui::SetNextWindowSize(v2(150, 370), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(v2(5, 5), ImGuiCond_FirstUseEver);

    ImGui::Begin("AA Demo", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_NoResize);

    if (!demo.on) {
        if (ImGui::Button("Toggle Fullscreen", v2(-1, 0)))
            platform_toggle_fullscreen();

        if (ImGui::Button("Start Demo", v2(-1, 0))) {
            pick_technique_set(demo);
            demo.on = true;
        }

        ImGui::Checkbox("Show Techniques", &demo.showTechniques);
    }
    else {
        if (ImGui::Button("Back", v2(-1, 0))) {
            renderer_stop_aa_demo(&gGame->rendererWorkspace);
            demo.on = false;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (int i = 0; i < ArraySize(demo.curTechniques); i++) {
            if (demo.curTechniques[i] == AA_INVALID) continue;
            ImGui::PushID(i);

            if (demo.showTechniques) {
                if (ImGui::Button(fmt_cstr("%s", cstr(demo.curTechniques[i])), v2(-40, 0)))
                    demo.selectedTechnique = demo.curTechniques[i];
            }
            else {
                if (ImGui::Button(fmt_cstr("AA %d", i), v2(-40, 0)))
                    demo.selectedTechnique = demo.curTechniques[i];
            }

            ImGui::SameLine(0, 5);

            if (ImGui::Button("Best", v2(-1, 0))) {
                demo.chosenTechniques[demo.chosenCount] = demo.curTechniques[i];
                demo.curTechniques[i] = AA_INVALID;
                demo.chosenCount++;
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("EUID:");
        ImGui::SameLine();

        ImGui::PushItemWidth(-1);

        b32 submitted    = false;
        umm submittedLen = 0;
        if (ImGui::InputText("##EUID", demo.euid, ArraySize(demo.euid),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            submittedLen = strlen(demo.euid);

            if (submittedLen)
                submitted = true;
        }

        ImGui::PopItemWidth();

        ImGui::Spacing();

        if (ImGui::Button("Sumbit", v2(-1, 0))) {
            if (!submitted) {
                submittedLen = strlen(demo.euid);

                if (submittedLen)
                    submitted = true;
            }
        }

        if (submitted) {
            save_demo_results(demo);
            start_new_demo(demo);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Reset", v2(-1, 0)))
            start_new_demo(demo);
    }

    ImGui::End();
    // ImGui::ShowDemoWindow();

    // TODO: options to override this.
    demo.res = gGame->clientRes;
}

extern void
game_update(f32 dt)
{
    Game_Keyboard& kb = gGame->input.keyboard;
    if (kb.released(GK_ESCAPE)) {
        gGame->shouldQuit = true;
    }

    update_aa_demo(gGame->demo);
    if (gGame->demo.on) return;


    f32 camSpeed = 3 * dt; // XXX
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

    gGame->closestRes = closestResolution;
    gGame->clientRes  = clientRes;

    set_render_target(gGame->frameBeginCommands);
    cmd_set_viewport(0, 0, clientRes.w, clientRes.h);
    log_debug("Set Viewport: %u %u\n", clientRes.w, clientRes.h);
}

extern void
game_play_sound(u64)
{
}

extern void
game_render(f32 /*frameRatio*/, struct ImDrawData* imguiData)
{
    AA_Demo& demo = gGame->demo;

    if (!gGame->demo.on) {
        // Render frame local changes like resizes, viewport, etc.
        set_render_target(gGame->frameBeginCommands);
        cmd_set_view_matrix(gGame->camera.view_matrix());

        renderer_begin_frame(&gGame->rendererWorkspace,
                             gGame->frameBeginCommands.arena.start,
                             gGame->frameBeginCommands.count);

        render_commands(gGame->residentCommands);

        renderer_end_frame(&gGame->rendererWorkspace, imguiData);
    }
    else {
        // Render frame local changes like resizes, viewport, etc.
        set_render_target(gGame->frameBeginCommands);
        cmd_set_view_matrix(gGame->camera.view_matrix());

        renderer_demo_aa(&gGame->rendererWorkspace, demo.res,   demo.selectedTechnique,
                         gGame->frameBeginCommands.arena.start, gGame->frameBeginCommands.count,
                         gGame->residentCommands.arena.start,   gGame->residentCommands.count,
                         imguiData);
    }
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

