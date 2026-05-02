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

	void InspectorPanel::SetOnModifedCallback(const std::function<void()>& callback)
	{
		m_OnModified = callback;
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

		if (*m_SelectedID != m_LastSelectedID)
		{
			m_LastSelectedID = *m_SelectedID;
			std::snprintf(m_NameBuffer, sizeof(m_NameBuffer), "%s", obj->Name.c_str());
		}

		if (!obj)
		{
			ImGui::Text("No object selected");
			ImGui::End();
			return;
		}

		if (ImGui::Checkbox("##Enabled", &obj->Enabled))
			if (m_OnModified) m_OnModified();

		ImGui::SameLine();

		ImGui::SetNextItemWidth(150);
		if (ImGui::InputText("##Name", m_NameBuffer, sizeof(m_NameBuffer)))
		{
			obj->Name = m_NameBuffer;
			if (m_OnModified) m_OnModified();
		}

		ImGui::SameLine();

		ImGui::Text("ID: %llu", obj->ID);

		ImGui::Separator();

		ImGui::Text("Actor Type / Class");

		if (ImGui::BeginCombo("##ActorType", obj->ActorType.c_str()))
		{
			const char* types[] = { "None", "Door", "NPC", "Enemy" };

			for (const char* type : types)
			{
				bool selected = obj->ActorType == type;

				if (ImGui::Selectable(type, selected))
				{
					obj->ActorType = type;
					if (m_OnModified) m_OnModified();
				}

				if (selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		ImGui::Separator();

		ImGui::Text("Transform");

		if (ImGui::DragFloat3("Position", &obj->Transform.Position.x, 0.1f))
			if (m_OnModified) m_OnModified();

		if (ImGui::DragFloat3("Rotation", &obj->Transform.Rotation.x, 0.5f))
			if (m_OnModified) m_OnModified();

		if (ImGui::DragFloat3("Scale", &obj->Transform.Scale.x, 0.1f, 0.0f, 100.0f))
			if (m_OnModified) m_OnModified();

		ImGui::Separator();

		if (obj->ActorType == "Door")
		{
			ImGui::Text("Door settings...");
		}
		else if (obj->ActorType == "NPC")
		{
			ImGui::Text("NPC settings...");
		}
		else if (obj->ActorType == "Enemy")
		{
			ImGui::Text("Enemy settings...");
		}
		else
		{
			ImGui::Text("No custom data...");
		}

		ImGui::End();
	}
}