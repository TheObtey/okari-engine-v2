#pragma once

#include "World/World.h"

#include <functional>

namespace Okari
{
	class InspectorPanel
	{
	public:
		InspectorPanel(World* world, uint64_t* selectedID);

		void SetContext(World* world, uint64_t* selectedID);
		void SetOnModifedCallback(const std::function<void()>& callback);

		void OnImGuiRender();

	private:
		World* m_World;
		uint64_t* m_SelectedID;

		std::function<void()> m_OnModified;
	};
}