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

    return obj_load(buffer, PostProcess_GenNormals | PostProcess_GenTangents | PostProcess_FlipUVs);
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
read_mtl_file(string32 path)
{
    buffer32 buffer = read_file_buffer(path);

    MTL_File result = mtl_load(buffer);
    sanitize_materials(result);

    return result;
}

static void
setup_test_map()
{
    gGame->camera.look_at(v3(-5, 0, 3), v3(0, 1, 0));
    gGame->camera.fov = 75;

    gGame->allocator.data = &gMem->modelLoading;

    //OBJ_File bob  = read_obj_file("assets/boblampclean.obj");
    OBJ_File heli   = read_obj_file("assets/hheli.obj");
    OBJ_File jeep   = read_obj_file("assets/jeep.obj");
    OBJ_File cyborg = read_obj_file("assets/cyborg/cyborg.obj");

    //MTL_File bobMtl  = read_mtl_file(cat("assets/", bob.mtllib));
    MTL_File heliMtl   = read_mtl_file(cat("assets/", heli.mtllib));
    MTL_File jeepMtl   = read_mtl_file(cat("assets/", jeep.mtllib));
    MTL_File cyborgMtl = read_mtl_file(cat("assets/cyborg/", cyborg.mtllib));

    //Static_Mesh bobMesh  = load_static_mesh(bob, bobMtl, "demo/assets/");
    Static_Mesh heliMesh   = load_static_mesh(heli, heliMtl, "assets/", "heli");
    Static_Mesh jeepMesh   = load_static_mesh(jeep, jeepMtl, "assets/", "jeep");
    Static_Mesh cyborgMesh = load_static_mesh(cyborg, cyborgMtl, "assets/cyborg/", "cyborg");

    Static_Mesh floorMesh = make_box(v3(0, 0, -.55f), v3(40, 40, 1), v3(.005, .005, .009));
    floorMesh.tag = dup("floor");

    gGame->allocator.data = &gMem->perm;

    // TODO(blake): make these macros that clear the render target after queing commands.
    set_render_target(gGame->frameBeginCommands); {
        //cmd_set_clear_color(0.015f, 0.015f, 0.015f, 1.0f); // gray (sRGB)
        cmd_set_clear_color(.05f, .05f, .05f, 1.0f);
        //cmd_set_clear_color(0.55f, 0.15f, 0.015f, 1.0f); // orange
        cmd_set_projection_matrix(glm::perspective(glm::radians(gGame->camera.fov), 16.0f/9.0f, .1f, 100.0f));
        cmd_set_view_matrix(gGame->camera.view_matrix());
        cmd_set_viewport(gGame->clientRes);
    }

    set_render_target(gGame->residentCommands); {

        mat4 xform;

        cmd_render_static_mesh(floorMesh, xform);

        v3 lightPos   { -3, 1, 3 };
        v3 lightColor {  1, 1, 1 };

        cmd_render_point_light(lightPos, lightColor);
        cmd_render_debug_cube(lightPos, .15f, lightColor);

        v3 color { .1f, .1f, .1f };

        Grid grid = make_xy_grid(20, 20);
        cmd_render_debug_lines(grid.vertices, grid.vertexCount, color);

        v3 centers[20] = {};
        points_by_step(20, v3(-10.5f, 10.5f, 0.0f), v3(0, -.5, 0), centers);
        cmd_render_debug_cubes(view_of(centers), .5f, color);

        xform = glm::rotate(xform, glm::pi<f32>()/2, v3(1, 0, 0));
        xform = glm::scale(xform, v3(.02f));

        cmd_render_static_mesh(heliMesh, xform);

        xform = mat4();
        xform = glm::translate(xform, v3(-2, 3, 0));
        xform = glm::rotate(xform, glm::pi<f32>(), v3(0, 0, 1));
        xform = glm::rotate(xform, glm::pi<f32>()/2, v3(1, 0, 0));
        xform = glm::scale(xform, v3(.02f));

        xform = glm::scale(xform, v3(.2f));
        cmd_render_static_mesh(jeepMesh, xform);

        xform = mat4();
        xform = glm::translate(xform, v3(-2, -2, 0));
        xform = glm::rotate(xform, glm::pi<f32>()/2, v3(1, 0, 0));
        xform = glm::rotate(xform, glm::pi<f32>()/1.5f, v3(0, -1, 0));
        xform = glm::scale(xform, v3(.6f));
        cmd_render_static_mesh(cyborgMesh, xform);

        // TODO(blake): make debug geometry more systemic.
        //Debug_Normals boxNormals = make_debug_normals(boxMesh);
        //cmd_render_debug_lines(boxNormals.t, boxNormals.vertexCount, v3(1, 0, 0));
        //cmd_render_debug_lines(boxNormals.b, boxNormals.vertexCount, v3(0, 1, 0));
        //cmd_render_debug_lines(boxNormals.n, boxNormals.vertexCount, v3(0, 0, 1));
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

    gGame->frameBeginCommands = sub_allocate(gMem->perm, Kilobytes(4), Kilobytes(8), "Frame Game Render Commands");
    gGame->residentCommands   = sub_allocate(gMem->perm, Kilobytes(4), Kilobytes(8), "Resident Game Render Commands");

    gGame->targetRenderCommandBuffer = &gGame->frameBeginCommands;

    setup_test_map();

    set_render_target(gGame->frameBeginCommands); {
        cmd_set_aa_technique(AA_MSAA_4X);
    }

    return true;
}

// Called after the game DLL is hot-loaded. This doesn't actaully happen, yet.
extern void
game_patch_after_hotload(Game_Memory* memory, Platform* platform)
{
    gMem      = memory;
    gGame     = (Game*)memory->perm.start;
    gPlatform = platform;
}

// TODO(blake): query supported resolutions from the OS.
extern void
game_resize(Game_Resolution clientRes)
{
    Game_Resolution closestResolution = tightest_supported_resolution(clientRes);

    gGame->closestRes = closestResolution;
    gGame->clientRes  = clientRes;

    // NOTE(blake): this resize code assumes the window can only be resized
    // to supported resolutions!!

    set_render_target(gGame->frameBeginCommands);
    cmd_set_viewport(0, 0, clientRes.w, clientRes.h);
    cmd_resize_buffers(clientRes.w, clientRes.h);
}


#if 0
static void
update_aa_demo(AA_Demo& demo)
{
    if (!demo.on) {
        int corner = 1;
        ImVec2 overlayPos = ImVec2((corner & 1) ? ImGui::GetIO().DisplaySize.x - 10 : 10,
                                   (corner & 2) ? ImGui::GetIO().DisplaySize.y - 10 : 10);
        ImVec2 overlayPosPivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
        ImGui::SetNextWindowPos(overlayPos, ImGuiCond_Always, overlayPosPivot);
        ImGui::SetNextWindowBgAlpha(0.3f);
        if (ImGui::Begin("", 0, (corner != -1 ? ImGuiWindowFlags_NoMove : 0) |
                                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
            const char* frameTime = fmt_cstr("Frame Time: %f ms (%f FPS)",
                                             gGame->frameStats.frameTimeWindow.average / 1000.0f,
                                             gGame->frameStats.fps());
            ImGui::Text(frameTime);
            ImGui::End();
        }
    }

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

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Technique Preview:");
        ImGui::Spacing();

        set_render_target(gGame->frameBeginCommands);

        for (u32 i = 1; i < AA_COUNT_; i++) {
            const char* name = to_cstr((AA_Technique)i);
            if (ImGui::Button(fmt_cstr("%s##%s", name, name), v2(-1, 0)))
                cmd_set_aa_technique((AA_Technique)i);
        }

        ImGui::Spacing();

        if (ImGui::Checkbox("Vsync Enabled", &demo.vsync))
            platform_enable_vsync(demo.vsync);
    }
    else {
        if (ImGui::Button("Back", v2(-1, 0))) {
            renderer_stop_aa_demo(&gGame->rendererWorkspace);
            demo.on = false;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        Game_Keyboard& kb = gGame->input.keyboard;

        for (int i = 0; i < ArraySize(demo.curTechniques); i++) {
            if (demo.curTechniques[i] == AA_INVALID) continue;
            ImGui::PushID(i);

            b32 keyReleased = kb.released((Game_Key)(GK_0 + i));

            if (demo.showTechniques) {
                if (ImGui::Button(fmt_cstr("%s", to_cstr(demo.curTechniques[i])), v2(-40, 0)) || keyReleased)
                    demo.selectedTechnique = demo.curTechniques[i];
            }
            else {
                if (ImGui::Button(fmt_cstr("AA %d", i), v2(-40, 0)) || keyReleased)
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
    //ImGui::ShowDemoWindow();

    // TODO: options to override this.
    demo.res = gGame->clientRes;
}
#endif

extern void
game_tick(f32 dt)
{
    Game_Keyboard& kb = gGame->input.keyboard;
    if (kb.released(GK_ESCAPE) || kb.released(GK_F4, GKM_ALT)) {
        gGame->shouldQuit = true;
        return;
    }

    if (kb.released(GK_RETURN, GKM_ALT))
        platform_toggle_fullscreen();


    constexpr f32 cameraV = 2; // m/s

    auto& camera = gGame->camera;

    f32 up      = 0;
    f32 right   = 0;
    f32 forward = 0;

    if (kb.down(GK_W)) forward += cameraV;
    if (kb.down(GK_S)) forward -= cameraV;
    if (kb.down(GK_A)) right   -= cameraV;
    if (kb.down(GK_D)) right   += cameraV;
    if (kb.down(GK_E)) up      += cameraV;

    v3 movement(up, right, forward);

    if (movement.x || movement.y || movement.z) {
        movement = glm::normalize(v3(up, right, forward)) * cameraV;

        camera.up_by(dt * movement.x);
        camera.right_by(dt * movement.y);
        camera.forward_by(dt * movement.z);
    }

    // NOTE(blake): not sure if stepping makes sense for mouse drag movement,
    // so I'm leaving it as is for now.

    auto& mouse    = gGame->input.mouse;
    auto& curMouse = gGame->input.mouse.cur;

    auto& smoother = gGame->inputSmoother;

    if (mouse.drag_started())
        smoother.reset_mouse_drag();

    if (curMouse.dragging) {
        f32 xDrag = map_bilateral(curMouse.xDrag, gGame->clientRes.w);
        f32 yDrag = map_bilateral(curMouse.yDrag, gGame->clientRes.h);

        //v2 smoothedDrag = smoother.smooth_mouse_drag(xDrag, yDrag);
        v2 smoothedDrag(xDrag, yDrag);

        camera.yaw_by(2 * -smoothedDrag.x);
        camera.pitch_by(2 * smoothedDrag.y);
    }

    // FPS-style clamping.
    camera.up = v3(0.0f, 0.0f, 1.0f);

    // log_debug("FPS: %f %f\n", gGame->frameStats.fps(), dt);
}

extern void
game_step(f32 dt)
{
}

extern void
game_play_sound(u64)
{
}

extern void
game_render(f32 stepDt, f32 frameRatio, struct ImDrawData* imguiData)
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
