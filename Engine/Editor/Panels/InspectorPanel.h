#pragma once

#include "World/World.h"

namespace Okari
{
	class InspectorPanel
	{
	public:
		InspectorPanel(World* world, uint64_t* selectedID);

		void OnImGuiRender();

	private:
		World* m_World;
		uint64_t* m_SelectedID;
	};
}