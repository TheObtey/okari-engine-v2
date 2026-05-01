#pragma once

#include "Core/Layer.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Rendering/Camera.h"
#include "Scene/SceneDocument.h"

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
		SceneDocument* GetActiveScene();
		void SetActiveScene(int index);

		void NewScene();
		void SaveActiveScene();

		void RequestSaveActiveScene();
		void OpenSaveScenePopup();
		void DrawSaveScenePopup();

	private:
		std::vector<std::unique_ptr<SceneDocument>> m_OpenScenes;
		int m_ActiveSceneIndex = -1;

		char m_SaveSceneNameBuffer[128] = "untitled";
		char m_SaveSceneDirectoryBuffer[512] = "";
		bool m_ShouldOpenSaveScenePopup = false;

		std::unique_ptr<HierarchyPanel> m_HierarchyPanel;
		std::unique_ptr<InspectorPanel> m_InspectorPanel;
		std::unique_ptr<ViewportPanel> m_ViewportPanel;
		std::unique_ptr<Camera> m_EditorCamera;	
	};
}