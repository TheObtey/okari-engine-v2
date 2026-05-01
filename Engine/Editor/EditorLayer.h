#pragma once

#include "Core/Layer.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Editor/Panels/AssetBrowserPanel.h"
#include "Rendering/Camera.h"
#include "Scene/SceneDocument.h"

#include <memory>

namespace Okari
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		~EditorLayer() override;

		void Init() override;
		void Update(float deltaTime) override;
		void Render(Renderer& renderer) override;

	private:
		SceneDocument* GetActiveScene();
		void SetActiveScene(int index);
		void CloseScene(int index);

		void NewScene();
		void SaveActiveScene();

		void RequestLoadScene();
		void LoadSceneFromFile(const std::string& path);
		void DrawLoadScenePopup();

		void RequestSaveActiveScene();
		void OpenSaveScenePopup();
		void DrawSaveScenePopup();

		void RequestCloseScene(int index);
		void DrawUnsavedScenePopup();

	private:
		std::vector<std::unique_ptr<SceneDocument>> m_OpenScenes;
		int m_ActiveSceneIndex = -1;

		char m_LoadScenePathBuffer[512] = "";
		bool m_ShouldOpenLoadScenePopup = false;

		char m_SaveSceneNameBuffer[128] = "untitled";
		char m_SaveSceneDirectoryBuffer[512] = "";
		bool m_ShouldOpenSaveScenePopup = false;

		int m_PendingCloseSceneIndex = -1;
		bool m_ShouldOpenUnsavedScenePopup = false;

		bool m_ClosePendingSceneAfterSave = false;

		std::unique_ptr<HierarchyPanel> m_HierarchyPanel;
		std::unique_ptr<InspectorPanel> m_InspectorPanel;
		std::unique_ptr<ViewportPanel> m_ViewportPanel;
		std::unique_ptr<AssetBrowserPanel> m_AssetBrowserPanel;
		std::unique_ptr<Camera> m_EditorCamera;	
	};
}