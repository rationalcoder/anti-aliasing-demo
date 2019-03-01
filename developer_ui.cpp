#include "tanks.h"

static void
show_developer_ui(Developer_UI& ui)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(v2(io.DisplaySize.x - 300, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(v2(300, io.DisplaySize.y), ImGuiCond_Always);
    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoDecoration);

    for_each_header(Render_Command_Header, header, gGame->frameBeginCommands) {
    }

    for_each_header(Render_Command_Header, header, gGame->residentCommands) {
    }

    ImGui::End();
}
