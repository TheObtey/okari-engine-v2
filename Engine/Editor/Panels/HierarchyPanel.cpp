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

        for (auto& obj : objects)
        {
            bool selected = (obj.ID == *m_SelectedID);

            std::string label = obj.Name + "##" + std::to_string(obj.ID);

            if (ImGui::Selectable(label.c_str(), selected))
            {
                *m_SelectedID = obj.ID;
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Duplicate"))
                {
                    WorldObject* newObj = m_World->DuplicateObject(obj.ID);
                
                    if (newObj)
                        *m_SelectedID = newObj->ID;
                }

                if (ImGui::MenuItem("Delete"))
                {
                    uint64_t deletedID = obj.ID;

                    m_World->RemoveObject(deletedID);

                    if (*m_SelectedID == deletedID)
                        *m_SelectedID = 0;

                    ImGui::EndPopup();
                    break;
                }

                ImGui::EndPopup();
            }
        }

        if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Empty"))
            {
                WorldObject& obj = m_World->CreateObject("Empty");
                *m_SelectedID = obj.ID;
            }

            ImGui::EndPopup();
        }

        ImGui::End();
    }
}