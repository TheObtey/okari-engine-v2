#pragma once

#include "World/World.h"

namespace Okari
{
    class HierarchyPanel
    {
    public:
        HierarchyPanel(World* world, int* selectedIndex);

        void OnImGuiRender();

    private:
        World* m_World;
        int* m_SelectedIndex;
    };
}