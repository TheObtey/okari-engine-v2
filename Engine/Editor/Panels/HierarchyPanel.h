#pragma once

#include "World/World.h"

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
    };
}