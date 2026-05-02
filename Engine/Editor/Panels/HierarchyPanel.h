#pragma once

#include "World/World.h"

#include <cstdint>
#include <functional>

namespace Okari
{
    class HierarchyPanel
    {
    public:
        HierarchyPanel(World* world, uint64_t* selectedID);

        void SetContext(World* world, uint64_t* selectedID);
        void SetOnModifedCallback(const std::function<void()>& callback);

        void DrawObjectNode(WorldObject& obj, uint64_t parentID);
        void DrawDropLine(uint64_t parentID, uint64_t beforeID);

        void DeleteObject(int id);

        void OnImGuiRender();

    private:
        World* m_World;
        uint64_t* m_SelectedID;

        uint64_t m_RenamingID = 0;
        char m_RenameBuffer[256] = {};
        bool m_RenameJustStarted = false;

        uint64_t m_PendingDeleteID = 0;

        std::function<void()> m_OnModified;
    };
}