#pragma once

#include "Rendering/Framebuffer.h"
#include "Scene/SceneDocument.h"

#include <memory>
#include <vector>
#include <functional>
#include <string>

namespace Okari
{
	class ViewportPanel
	{
	public:
		ViewportPanel();

		void SetScenesContext(const std::vector<std::unique_ptr<SceneDocument>>* scenes, int* activeIndex);

		void SetCallbacks(const std::function<void(int)>& onSelect, const std::function<void(int)>& onClose);

		void OnImGuiRender();

		Framebuffer& GetFramebuffer() { return *m_Framebuffer; }

	private:
		const std::vector<std::unique_ptr<SceneDocument>>* m_Scenes = nullptr;
		int* m_ActiveSceneIndex = nullptr;

		std::function<void(int)> m_OnSceneSelected;
		std::function<void(int)> m_OnSceneClosed;

		std::unique_ptr<Framebuffer> m_Framebuffer;

		float m_ViewportWidth = 1280.0f;
		float m_ViewportHeight = 720.0f;
	};
}