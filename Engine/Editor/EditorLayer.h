#pragma once

#include "Core/Layer.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Rendering/Camera.h"
#include "World/World.h"

#include <memory>

namespace Okari
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		~EditorLayer() override = default;

		void Init() override;
		void Update(float deltaTime) override;
		void Render(Renderer& renderer) override;

	private:
		std::unique_ptr<World> m_World;
		std::unique_ptr<HierarchyPanel> m_HierarchyPanel;
		std::unique_ptr<InspectorPanel> m_InspectorPanel;
		std::unique_ptr<ViewportPanel> m_ViewportPanel;
		std::unique_ptr<Camera> m_EditorCamera;
		
		uint64_t m_SelectedObjectID = 0;
	};
}