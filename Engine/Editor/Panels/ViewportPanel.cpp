#include "Editor/Panels/ViewportPanel.h"

#include <imgui.h>

namespace Okari
{
	ViewportPanel::ViewportPanel()
	{
		m_Framebuffer = std::make_unique<Framebuffer>(1280, 720);
	}

	void ViewportPanel::OnImGuiRender()
	{
		ImGui::Begin("Viewport");

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