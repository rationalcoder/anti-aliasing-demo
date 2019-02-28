
static void
show_profiler(Developer_UI& ui, v2 topLeft, v2 size)
{
    ImGui::SetNextWindowPos(topLeft, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    //ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_NoDecoration & ~ImGuiWindowFlags_NoScrollbar);
    ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_HorizontalScrollbar);

    v2 p = (v2)ImGui::GetWindowPos() + v2(20, 0);

    ImGui::GetWindowDrawList()->AddLine(p, p + v2(0, 20), ImGui::GetColorU32(v4(.5f, .5f, 0, 1)));
    for (int i = 0; i < 200; i++) {
        ImGui::SameLine();
        ImGui::Button(fmt_cstr("Text##%d", i));
    }

    if (ImGui::IsMouseDragging()) {
    }

    ImGui::End();
}

static void
show_developer_ui(Developer_UI& ui)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(v2(io.DisplaySize.x - 300, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(v2(300, io.DisplaySize.y), ImGuiCond_Always);
    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoDecoration);

    if (ImGui::CollapsingHeader("Scene")) {
        if (ImGui::ColorEdit3("Clear color", (f32*)&ui.clearColor)) {
            set_render_target(gGame->frameBeginCommands); {
                cmd_set_clear_color(ui.clearColor[0], ui.clearColor[1], ui.clearColor[2]);
            }
        }
    }

    if (ImGui::Button("Profiler"))
        ui.showProfiler = !ui.showProfiler;

    if (ui.showProfiler) {
        show_profiler(ui, v2(0, 0), v2(io.DisplaySize.x - ImGui::GetWindowWidth(),
                                       io.DisplaySize.y));
    }

    for_each_header(Render_Command_Header, header, gGame->residentCommands) {
    }

    ImGui::End();
}

