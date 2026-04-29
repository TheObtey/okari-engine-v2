#include "Editor/Panels/HierarchyPanel.h"

#include <imgui.h>
#include <string>

namespace Okari
{
    HierarchyPanel::HierarchyPanel(World* world, uint64_t* selectedID)
        : m_World(world), m_SelectedID(selectedID)
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
            auto& obj = objects[i];

            bool selected = (obj.ID == *m_SelectedID);
            
            std::string label = obj.Name + "##" + std::to_string(obj.ID);

            if (ImGui::Selectable(label.c_str(), selected))
            {
                *m_SelectedID = obj.ID;
            }
        }

        ImGui::End();
    }
}