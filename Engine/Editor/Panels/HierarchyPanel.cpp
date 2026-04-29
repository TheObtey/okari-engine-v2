#include "Editor/Panels/HierarchyPanel.h"

#include <imgui.h>
#include <string>

namespace Okari
{
    HierarchyPanel::HierarchyPanel(World* world, int* selectedIndex)
        : m_World(world), m_SelectedIndex(selectedIndex)
    { }

    void HierarchyPanel::OnImGuiRender()
    {
        ImGui::Begin("Hierarchy");

        if (!m_World)
        {
            ImGui::Text("No World loaded");
            ImGui::End();
            return;
        }

        auto& objects = m_World->GetObjects();

        for (int i = 0; i < objects.size(); i++)
        {
            bool selected = (*m_SelectedIndex == i);
            std::string label = objects[i].Name + "##" + std::to_string(i);

            if (ImGui::Selectable(label.c_str(), selected))
            {
                *m_SelectedIndex = i;
            }
        }

        ImGui::End();
    }
}