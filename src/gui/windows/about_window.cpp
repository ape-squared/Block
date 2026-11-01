#include "about_window.hpp"

#include <imgui.h>

#include "version.hpp" // cppcheck-suppress missingInclude

void vkShade::AboutWindow::render()
{
    // Set a minimum size constraint
    ImGui::SetNextWindowSizeConstraints(ImVec2(300, 160), ImVec2(FLT_MAX, FLT_MAX));

    if (ImGui::Begin("About", &m_visible, ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize))
    {
        // Center the title
        float windowWidth = ImGui::GetContentRegionAvail().x;
        float textWidth = ImGui::CalcTextSize("BLOCK").x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (windowWidth - textWidth) * 0.5f);
        ImGui::Text("BLOCK");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("Post-processing for Vulkan and OpenGL.");

        ImGui::Spacing();

        ImGui::TextDisabled(VKSHADE_VERSION);
    }
    ImGui::End();
}
