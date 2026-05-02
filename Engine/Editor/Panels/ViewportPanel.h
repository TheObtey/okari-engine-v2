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

		bool HasPendingPick() const { return m_HasPendingPick; }
		uint32_t GetPickX() const { return m_PickX; }
		uint32_t GetPickY() const { return m_PickY; }
		void ClearPendingPick() { m_HasPendingPick = false; }

	private:
		const std::vector<std::unique_ptr<SceneDocument>>* m_Scenes = nullptr;
		int* m_ActiveSceneIndex = nullptr;

		std::function<void(int)> m_OnSceneSelected;
		std::function<void(int)> m_OnSceneClosed;

		std::unique_ptr<Framebuffer> m_Framebuffer;

		float m_ViewportWidth = 1280.0f;
		float m_ViewportHeight = 720.0f;

		bool m_HasPendingPick = false;
		uint32_t m_PickX = 0;
		uint32_t m_PickY = 0;
	};
}