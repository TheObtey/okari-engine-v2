#include "Editor/Panels/HierarchyPanel.h"

#include <imgui.h>
#include <cstring>
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

            if (m_RenamingID == obj.ID)
            {
                ImGui::SetKeyboardFocusHere();

                std::string inputID = "##rename_" + std::to_string(obj.ID);

                bool validate = ImGui::InputText(
                    inputID.c_str(),
                    m_RenameBuffer,
                    sizeof(m_RenameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue
                );

                if (validate)
                {
                    obj.Name = m_RenameBuffer;
                    m_RenamingID = 0;
                }

                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    m_RenamingID = 0;
            }
            else
            {
                if (ImGui::Selectable(label.c_str(), selected))
                    *m_SelectedID = obj.ID;

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    *m_SelectedID = obj.ID;
                    m_RenamingID = obj.ID;

                    std::strncpy(m_RenameBuffer, obj.Name.c_str(), sizeof(m_RenameBuffer));
                    m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
                }
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