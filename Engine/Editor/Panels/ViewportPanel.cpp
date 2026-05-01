#include "Editor/Panels/ViewportPanel.h"

#include <imgui.h>

namespace Okari
{
	ViewportPanel::ViewportPanel()
	{
		m_Framebuffer = std::make_unique<Framebuffer>(1280, 720);
	}

	void ViewportPanel::SetScenesContext(const std::vector<std::unique_ptr<SceneDocument>>* scenes, int* activeIndex)
	{
		m_Scenes = scenes;
		m_ActiveSceneIndex = activeIndex;
	}

	void ViewportPanel::SetCallbacks(const std::function<void(int)>& onSelect, const std::function<void(int)>& onClose)
	{
		m_OnSceneSelected = onSelect;
		m_OnSceneClosed = onClose;
	}

	void ViewportPanel::OnImGuiRender()
	{
		ImGui::Begin("Viewport");

		ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 4.0f);

		if (m_Scenes && ImGui::BeginTabBar("##ViewportTabs"))
		{
			for (int i = 0; i < (int)m_Scenes->size(); i++)
			{
				auto& scene = (*m_Scenes)[i];

				bool isOpen = true;

				std::string name = scene->Name;
				if (scene->Dirty)
					name += "*";

				if (ImGui::BeginTabItem(name.c_str(), &isOpen))
				{
					if (m_ActiveSceneIndex && *m_ActiveSceneIndex != i)
					{
						if (m_OnSceneSelected)
							m_OnSceneSelected(i);
					}

					ImGui::EndTabItem();
				}

				if (!isOpen)
				{
					if (m_OnSceneClosed)
						m_OnSceneClosed(i);

					break;
				}
			}

			ImGui::EndTabBar();
		}

		ImGui::PopStyleVar();

		ImVec2 viewportSize = ImGui::GetContentRegionAvail();

		if (viewportSize.x > 0 && viewportSize.y > 0)
		{
			m_ViewportWidth = viewportSize.x;
			m_ViewportHeight = viewportSize.y;

			m_Framebuffer->Resize(
				static_cast<uint32_t>(m_ViewportWidth),
				static_cast<uint32_t>(m_ViewportHeight)
			);
		}

		ImTextureID textureID = static_cast<ImTextureID>(m_Framebuffer->GetColorAttachment());

		ImGui::Image(
			textureID,
			ImVec2(m_ViewportWidth, m_ViewportHeight),
			ImVec2(0, 1),
			ImVec2(1, 0)
		);

		ImGui::End();
	}
}