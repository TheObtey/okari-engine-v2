#pragma once

#include "Rendering/Framebuffer.h"

#include <memory>

namespace Okari
{
	class ViewportPanel
	{
	public:
		ViewportPanel();

		void OnImGuiRender();

		Framebuffer& GetFramebuffer() { return *m_Framebuffer; }

	private:
		std::unique_ptr<Framebuffer> m_Framebuffer;

		float m_ViewportWidth = 1280.0f;
		float m_ViewportHeight = 720.0f;
	};
}