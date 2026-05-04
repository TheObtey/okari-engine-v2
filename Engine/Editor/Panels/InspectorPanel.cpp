#include "Editor/Panels/InspectorPanel.h"
#include "../../../Game/Actors/ActorRegistry.h"

#include <imgui.h>
#include <filesystem>
#include <string>

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
			const auto& actorTypes = ActorRegistry::GetActorTypes();

			for (const std::string& type : actorTypes)
			{
				bool selected = obj->ActorType == type;

				if (ImGui::Selectable(type.c_str(), selected))
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

		ImGui::Text("Mesh Component");

		if (ImGui::Checkbox("Mesh Enabled", &obj->Mesh.Enabled))
			if (m_OnModified) m_OnModified();

		auto drawAssetField = [&](const char* label, std::string& path)
			{
				char buffer[512];
				std::snprintf(buffer, sizeof(buffer), "%s", path.c_str());

				ImGui::SetNextItemWidth(-80.0f);

				if (ImGui::InputText(label, buffer, sizeof(buffer)))
				{
					path = buffer;
					if (m_OnModified) m_OnModified();
				}

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ITEM"))
					{
						const char* droppedPath = static_cast<const char*>(payload->Data);

						std::filesystem::path assetRoot = std::filesystem::absolute(OKARI_ASSET_DIR);
						std::filesystem::path dropped = std::filesystem::absolute(droppedPath);

						if (std::filesystem::is_regular_file(dropped))
						{
							std::filesystem::path relative = std::filesystem::relative(dropped, assetRoot);
							path = "Assets/" + relative.generic_string();

							if (m_OnModified) m_OnModified();
						}
					}

					ImGui::EndDragDropTarget();
				}

				ImGui::SameLine();

				if (ImGui::Button((std::string("Clear##") + label).c_str()))
				{
					path.clear();
					if (m_OnModified) m_OnModified();
				}
			};

		drawAssetField("Mesh", obj->Mesh.MeshPath);
		drawAssetField("Texture", obj->Mesh.TexturePath);

		ImGui::Separator();

		ImGui::Text("Actor's Custom Data");

		const ActorDefinition* def = ActorRegistry::GetDefinition(obj->ActorType);

		if (!def || def->Properties.empty())
		{
			ImGui::TextDisabled("No custom data");
		}
		else
		{
			for (const auto& prop : def->Properties)
			{
				switch (prop.Type)
				{
				case ActorPropertyType::Bool:
				{
					bool value = obj->ActorData.value(prop.Name, false);

					if (ImGui::Checkbox(prop.Name.c_str(), &value))
					{
						obj->ActorData[prop.Name] = value;
						if (m_OnModified) m_OnModified();
					}
					break;
				}

				case ActorPropertyType::Float:
				{
					float value = obj->ActorData.value(prop.Name, 0.0f);

					if (ImGui::DragFloat(prop.Name.c_str(), &value))
					{
						obj->ActorData[prop.Name] = value;
						if (m_OnModified) m_OnModified();
					}
					break;
				}

				case ActorPropertyType::String:
				{
					std::string str = obj->ActorData.value(prop.Name, "");

					char buffer[256];
					std::snprintf(buffer, sizeof(buffer), "%s", str.c_str());

					if (ImGui::InputText(prop.Name.c_str(), buffer, sizeof(buffer)))
					{
						obj->ActorData[prop.Name] = std::string(buffer);
						if (m_OnModified) m_OnModified();
					}
					break;
				}

				default:
					ImGui::Text("%s (unsupported)", prop.Name.c_str());
					break;
				}
			}
		}

		ImGui::End();
	}
}