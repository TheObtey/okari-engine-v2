#include "Editor/Panels/InspectorPanel.h"

#include <imgui.h>

namespace Okari
{
	InspectorPanel::InspectorPanel(World* world, uint64_t* selectedID)
		: m_World(world), m_SelectedID(selectedID)
	{ }

	void InspectorPanel::SetContext(World* world, uint64_t* selectedID)
	{
		m_World = world;
		m_SelectedID = selectedID;
	}

	void InspectorPanel::OnImGuiRender()
	{
		ImGui::Begin("Inspector");

		if (!m_World || !m_SelectedID || *m_SelectedID == 0)
		{
			ImGui::Text("No object selected");
			ImGui::End();
			return;
		}

		WorldObject* obj = m_World->GetObjectByID(*m_SelectedID);

		if (!obj)
		{
			ImGui::Text("No object selected");
			ImGui::End();
			return;
		}

		ImGui::Text("Name: %s", obj->Name.c_str());

		ImGui::Separator();

		ImGui::DragFloat3("Position", &obj->Transform.Position.x, 0.1f);
		ImGui::DragFloat3("Rotation", &obj->Transform.Rotation.x, 0.5f);
		ImGui::DragFloat3("Scale", &obj->Transform.Scale.x, 0.1f, 0.0f, 100.0f);

		ImGui::End();
	}
}