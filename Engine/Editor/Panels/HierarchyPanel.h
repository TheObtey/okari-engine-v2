#pragma once

#include "World/World.h"

#include <string>
#include <cstdint>

namespace Okari
{
    class HierarchyPanel
    {
    public:
        HierarchyPanel(World* world, uint64_t* selectedID);

        void OnImGuiRender();

    private:
        World* m_World;
        uint64_t* m_SelectedID;

        uint64_t m_RenamingID = 0;
        char m_RenameBuffer[256] = {};
    };
}