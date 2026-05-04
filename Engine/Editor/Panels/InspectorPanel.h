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
		uint64_t m_LastSelectedID = -1;

		char m_NameBuffer[256] = {};
		char m_MeshPathBuffer[512] = {};
		char m_TexturePathBuffer[512] = {};

		std::function<void()> m_OnModified;
	};
}