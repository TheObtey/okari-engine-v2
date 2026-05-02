#include "Editor/Panels/HierarchyPanel.h"

#include <imgui.h>
#include <cstring>
#include <string>

namespace Okari
{
    HierarchyPanel::HierarchyPanel(World* world, uint64_t* selectedID)
        : m_World(world), m_SelectedID(selectedID)
    { }

    void HierarchyPanel::SetContext(World* world, uint64_t* selectedID)
    {
        m_World = world;
        m_SelectedID = selectedID;
    }

    void HierarchyPanel::SetOnModifedCallback(const std::function<void()>& callback)
    {
        m_OnModified = callback;
    }

    void HierarchyPanel::DrawObjectNode(WorldObject& obj, uint64_t parentID)
    {
        ImGui::PushID(static_cast<int>(obj.ID));

        auto children = m_World->GetChildren(obj.ID);
        bool hasChildren = !children.empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (*m_SelectedID == obj.ID)
            flags |= ImGuiTreeNodeFlags_Selected;

        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;

        bool isRenaming = (m_RenamingID == obj.ID);
        bool opened = false;

        if (isRenaming)
        {
            ImGui::Indent();

            ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1.0f);

            bool validateRename = ImGui::InputText(
                "##RenameObject",
                m_RenameBuffer,
                sizeof(m_RenameBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
            );

            if (validateRename)
            {
                obj.Name = m_RenameBuffer;
                m_RenamingID = 0;

                if (m_OnModified)
                    m_OnModified();
            }

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                obj.Name = m_RenameBuffer;
                m_RenamingID = 0;

                if (m_OnModified)
                    m_OnModified();

            }

            ImGui::Unindent();

            opened = false;
        }
        else
        {
            opened = ImGui::TreeNodeEx(obj.Name.c_str(), flags);

            if (ImGui::IsItemClicked())
                *m_SelectedID = obj.ID;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                *m_SelectedID = obj.ID;
                m_RenamingID = obj.ID;

                std::strncpy(m_RenameBuffer, obj.Name.c_str(), sizeof(m_RenameBuffer));
                m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Delete"))
                    m_PendingDeleteID = obj.ID;

                ImGui::EndPopup();
            }
        }

        if (!isRenaming)
        {
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload("HIERARCHY_OBJECT", &obj.ID, sizeof(uint64_t));
                ImGui::Text("%s", obj.Name.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget())
            {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_OBJECT");

                if (payload)
                {
                    uint64_t draggedID = *(const uint64_t*)payload->Data;
                    m_World->SetParent(draggedID, obj.ID);

                    if (m_OnModified)
                        m_OnModified();
                }

                ImGui::EndDragDropTarget();
            }
        }

        if (opened && hasChildren)
        {
            for (auto* child : children)
            {
                DrawDropLine(obj.ID, child->ID);
                DrawObjectNode(*child, obj.ID);
            }

            DrawDropLine(obj.ID, 0);
        }

        if (opened)
            ImGui::TreePop();

        ImGui::PopID();
    }

    void HierarchyPanel::DrawDropLine(uint64_t parentID, uint64_t beforeID)
    {
        ImGui::PushID((void*)(uintptr_t)parentID);
        ImGui::PushID((void*)(uintptr_t)beforeID);

        ImVec2 lineStart = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("DropLine", ImVec2(-1.0f, 4.0f));

        bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        if (hovered)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            float width = ImGui::GetContentRegionAvail().x;
            float y = lineStart.y + 3.0f;

            drawList->AddLine(
                ImVec2(lineStart.x, y),
                ImVec2(lineStart.x + width, y),
                IM_COL32(120, 170, 255, 255),
                2.0f
            );
        }

        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_OBJECT");

            if (payload)
            {
                uint64_t draggedID = *(const uint64_t*)payload->Data;

                if (beforeID != 0)
                {
                    m_World->SetParent(draggedID, parentID);
                    m_World->MoveObjectBefore(draggedID, beforeID);

                    if (m_OnModified)
                        m_OnModified();
                }
                else
                    m_World->MoveObjectToEndOfParent(draggedID, parentID);
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
        ImGui::PopID();
    }

    void HierarchyPanel::OnImGuiRender()
    {
        ImGui::Begin("Hierarchy");

        if (!m_World || !m_SelectedID)
        {
            ImGui::TextDisabled("No scene loaded");
            ImGui::End();
            return;
        }

        auto roots = m_World->GetChildren(0);

        for (auto* obj : roots)
        {
            DrawDropLine(0, obj->ID);
            DrawObjectNode(*obj, 0);
        }

        DrawDropLine(0, 0);

        if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Empty"))
            {
                WorldObject& obj = m_World->CreateObject("Empty");
                *m_SelectedID = obj.ID;

                if (m_OnModified)
                    m_OnModified();
            }

            ImGui::EndPopup();
        }

        if (m_PendingDeleteID != 0)
        {
            if (*m_SelectedID == m_PendingDeleteID)
                *m_SelectedID = 0;

            if (m_World->RemoveObject(m_PendingDeleteID))
            {
                if (m_OnModified)
                    m_OnModified();
            }

            m_PendingDeleteID = 0;
        }

        ImGui::End();
    }
}