#pragma once

#include "World/World.h"

namespace Okari
{
	class InspectorPanel
	{
	public:
		InspectorPanel(World* world, int* selectedIndex);

		void OnImGuiRender();

	private:
		World* m_World;
		int* m_SelectedIndex;
	};
}