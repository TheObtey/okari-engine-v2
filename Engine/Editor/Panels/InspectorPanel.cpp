#include "Editor/Panels/InspectorPanel.h"

#include <imgui.h>

namespace Okari
{
	InspectorPanel::InspectorPanel(World* world, int* selectedIndex)
		: m_World(world), m_SelectedIndex(selectedIndex)
	{ }

	void InspectorPanel::OnImGuiRender()
	{
		ImGui::Begin("Inspector");

		if (!m_World || !m_SelectedIndex)
		{
			ImGui::Text("No object selected");
			ImGui::End();
			return;
		}

		auto& objects = m_World->GetObjects();

		if (*m_SelectedIndex < 0 || *m_SelectedIndex >= objects.size())
		{
			ImGui::Text("No object selected");
			ImGui::End();
			return;
		}

		auto& obj = objects[*m_SelectedIndex];

		ImGui::Text("Name: %s", obj.Name.c_str());

		ImGui::Separator();

		ImGui::DragFloat3("Position", &obj.Transform.Position.x, 0.1f);
		ImGui::DragFloat3("Rotation", &obj.Transform.Rotation.x, 0.5f);
		ImGui::DragFloat3("Scale", &obj.Transform.Scale.x, 0.1f, 0.0f, 100.0f);

		ImGui::End();
	}
}