#include "Editor/EditorLayer.h"
#include "Editor/EditorUI.h"
#include "Platform/Window.h"
#include "Core/Application.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>

namespace Okari
{
    EditorLayer::EditorLayer()
        : m_HierarchyPanel(nullptr),
        m_InspectorPanel(nullptr),
        m_ViewportPanel(nullptr),
        m_AssetBrowserPanel(nullptr),
        m_EditorCamera(nullptr)
    { }

    EditorLayer::~EditorLayer()
    {
        if (m_HierarchyPanel)
            m_HierarchyPanel->SetContext(nullptr, nullptr);

        if (m_InspectorPanel)
            m_InspectorPanel->SetContext(nullptr, nullptr);

        if (m_ViewportPanel)
        {
            m_ViewportPanel->SetScenesContext(nullptr, nullptr);
            m_ViewportPanel->SetCallbacks(nullptr, nullptr);
        }

        if (m_AssetBrowserPanel)
            m_AssetBrowserPanel->SetSceneOpenCallback(nullptr);
    }

    SceneDocument* EditorLayer::GetActiveScene()
    {
        if (m_ActiveSceneIndex < 0 || m_ActiveSceneIndex >= static_cast<int>(m_OpenScenes.size()))
            return nullptr;

        return m_OpenScenes[m_ActiveSceneIndex].get();
    }

    void EditorLayer::SetActiveScene(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_OpenScenes.size()))
            return;

        m_ActiveSceneIndex = index;

        SceneDocument* activeScene = GetActiveScene();

        if (!activeScene)
            return;

        if (m_HierarchyPanel)
            m_HierarchyPanel->SetContext(activeScene->World.get(), &activeScene->SelectedObjectID);

        if (m_InspectorPanel)
            m_InspectorPanel->SetContext(activeScene->World.get(), &activeScene->SelectedObjectID);
    }

    void EditorLayer::CloseScene(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_OpenScenes.size()))
            return;

        m_OpenScenes.erase(m_OpenScenes.begin() + index);

        if (m_OpenScenes.empty())
        {
            m_ActiveSceneIndex = -1;

            if (m_HierarchyPanel)
                m_HierarchyPanel->SetContext(nullptr, nullptr);

            if (m_InspectorPanel)
                m_InspectorPanel->SetContext(nullptr, nullptr);

            return;
        }

        if (m_ActiveSceneIndex >= static_cast<int>(m_OpenScenes.size()))
            m_ActiveSceneIndex = static_cast<int>(m_OpenScenes.size()) - 1;

        SetActiveScene(m_ActiveSceneIndex);
    }

    void EditorLayer::NewScene()
    {
        auto scene = std::make_unique<SceneDocument>();

        scene->World = std::make_unique<World>();
        scene->Name = "untitled";
        scene->Path = "";
        scene->SelectedObjectID = 0;
        scene->Dirty = true;
        scene->HasBeenSaved = false;

        m_OpenScenes.push_back(std::move(scene));

        SetActiveScene(static_cast<int>(m_OpenScenes.size()) - 1);
    }

#pragma region LOAD_SCENE
    void EditorLayer::RequestLoadScene()
    {
        std::string defaultPath = std::string(OKARI_ASSET_DIR) + "/Levels/";

        std::snprintf(m_LoadScenePathBuffer, sizeof(m_LoadScenePathBuffer), "%s", defaultPath.c_str());

        m_ShouldOpenLoadScenePopup = true;
    }

    void EditorLayer::LoadSceneFromFile(const std::string& path)
    {
        auto scene = std::make_unique<SceneDocument>();

        scene->World = std::make_unique<World>();
        scene->Path = path;
        scene->SelectedObjectID= 0;
        scene->Dirty = false;
        scene->HasBeenSaved = true;

        std::string loadedSceneName = "untitled";

        if (!scene->World->LoadFromFile(path, &loadedSceneName))
            return;

        scene->Name = loadedSceneName;

        m_OpenScenes.push_back(std::move(scene));
        SetActiveScene(static_cast<int>(m_OpenScenes.size()) - 1);
    }

    void EditorLayer::DrawLoadScenePopup()
    {
        if (m_ShouldOpenLoadScenePopup)
        {
            ImGui::OpenPopup("Load Scene");
            m_ShouldOpenLoadScenePopup = false;
        }

        if (ImGui::BeginPopupModal("Load Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Path", m_LoadScenePathBuffer, sizeof(m_LoadScenePathBuffer));

            ImGui::Separator();

            if (ImGui::Button("Load"))
            {
                LoadSceneFromFile(m_LoadScenePathBuffer);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }
#pragma endregion

#pragma region SAVE_SCENE
    void EditorLayer::SaveActiveScene()
    {
        SceneDocument* activeScene = GetActiveScene();

        if (!activeScene || !activeScene->World)
            return;

        activeScene->World->SaveToFile(activeScene->Path, activeScene->Name);
        activeScene->Dirty = false;
    }

    void EditorLayer::RequestSaveActiveScene()
    {
        SceneDocument* activeScene = GetActiveScene();

        if (!activeScene)
            return;

        if (!activeScene->HasBeenSaved || activeScene->Path.empty())
        {
            OpenSaveScenePopup();
            return;
        }

        SaveActiveScene();
    }

    void EditorLayer::OpenSaveScenePopup()
    {
        SceneDocument* activeScene = GetActiveScene();

        if (!activeScene)
            return;

        std::snprintf(m_SaveSceneNameBuffer, sizeof(m_SaveSceneNameBuffer), "%s", activeScene->Name.c_str());

        std::string defaultDirectory = std::string(OKARI_ASSET_DIR) + "/Levels/";
        std::snprintf(m_SaveSceneDirectoryBuffer, sizeof(m_SaveSceneDirectoryBuffer), "%s", defaultDirectory.c_str());

        m_ShouldOpenSaveScenePopup = true;
    }

    void EditorLayer::DrawSaveScenePopup()
    {
        if (m_ShouldOpenSaveScenePopup)
        {
            ImGui::OpenPopup("Save Scene As");
            m_ShouldOpenSaveScenePopup = false;
        }

        if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Scene Name", m_SaveSceneNameBuffer, sizeof(m_SaveSceneNameBuffer));
            ImGui::InputText("Directory", m_SaveSceneDirectoryBuffer, sizeof(m_SaveSceneDirectoryBuffer));

            ImGui::Separator();

            if (ImGui::Button("Save"))
            {
                SceneDocument* activeScene = GetActiveScene();

                if (activeScene && activeScene->World)
                {
                    std::string sceneName = m_SaveSceneNameBuffer;
                    std::string directory = m_SaveSceneDirectoryBuffer;

                    if (!directory.empty() && directory.back() != '/' && directory.back() != '\\')
                        directory += "/";

                    std::string path = directory + sceneName + ".okscene";

                    activeScene->Name = sceneName;
                    activeScene->Path = path;

                    if (activeScene->World->SaveToFile(activeScene->Path, activeScene->Name))
                    {
                        activeScene->Dirty = false;
                        activeScene->HasBeenSaved = true;

                        if (m_ClosePendingSceneAfterSave)
                        {
                            m_ClosePendingSceneAfterSave = false;

                            int indexToClose = m_PendingCloseSceneIndex;
                            m_PendingCloseSceneIndex = -1;

                            CloseScene(indexToClose);
                        }
                    }
                }

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
            {
                m_ClosePendingSceneAfterSave = false;
                m_PendingCloseSceneIndex = -1;

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
#pragma endregion

    void EditorLayer::RequestCloseScene(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_OpenScenes.size()))
            return;

        SceneDocument* scene = m_OpenScenes[index].get();

        if (scene && scene->Dirty)
        {
            m_PendingCloseSceneIndex = index;
            m_ShouldOpenUnsavedScenePopup = true;
            return;
        }

        CloseScene(index);
    }

    void EditorLayer::DrawUnsavedScenePopup()
    {
        if (m_ShouldOpenUnsavedScenePopup)
        {
            ImGui::OpenPopup("Unsaved Scene");
            m_ShouldOpenUnsavedScenePopup = false;
        }

        if (ImGui::BeginPopupModal("Unsaved Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("This scene has unsaved changes.");
            ImGui::Text("Do you want to save before closing?");

            ImGui::Separator();

            if (ImGui::Button("Save"))
            {
                int index = m_PendingCloseSceneIndex;

                SetActiveScene(index);

                m_ClosePendingSceneAfterSave = true;
                RequestSaveActiveScene();

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Don't Save"))
            {
                CloseScene(m_PendingCloseSceneIndex);

                m_PendingCloseSceneIndex = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
            {
                m_PendingCloseSceneIndex = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void EditorLayer::Init()
    {
        m_EditorCamera = std::make_unique<Camera>(16.0f / 9.0f);
        m_EditorCamera->SetPosition(glm::vec3(0.0f, 3.0f, 6.0f));
        m_EditorCamera->SetTarget(glm::vec3(0.0f, 0.0f, 0.0f));

        auto markDirty = [this]()
            {
                SceneDocument* scene = GetActiveScene();
                if (scene)
                    scene->Dirty = true;
            };

        m_HierarchyPanel = std::make_unique<HierarchyPanel>(nullptr, nullptr);
        m_HierarchyPanel->SetOnModifedCallback(markDirty);

        m_InspectorPanel = std::make_unique<InspectorPanel>(nullptr, nullptr);
        m_InspectorPanel->SetOnModifedCallback(markDirty);

        m_ViewportPanel = std::make_unique<ViewportPanel>();
        m_ViewportPanel->SetScenesContext(&m_OpenScenes, &m_ActiveSceneIndex);
        m_ViewportPanel->SetCallbacks(
            [this](int index)
            {
                SetActiveScene(index);
            },
            [this](int index)
            {
                RequestCloseScene(index);
            }
        );

        m_AssetBrowserPanel = std::make_unique<AssetBrowserPanel>();
        m_AssetBrowserPanel->SetSceneOpenCallback(
            [this](const std::string& path)
            {
                LoadSceneFromFile(path);
            }
        );

        NewScene();

        auto window = Application::Get().GetWindow().GetNativeWindow();
        EditorUI::Init(window);
    }

    void EditorLayer::Update(float deltaTime)
    { }

    void EditorLayer::Render(Renderer& renderer)
    {
        m_ViewportPanel->GetFramebuffer().Bind();

        renderer.BeginFrame();

        SceneDocument* activeScene = GetActiveScene();

        if (activeScene && activeScene->World && m_EditorCamera)
            activeScene->World->Render(renderer, *m_EditorCamera);

        m_ViewportPanel->GetFramebuffer().Unbind();

        EditorUI::BeginFrame();

        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_O))
            RequestLoadScene();

        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_N))
            NewScene();

        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_S))
            RequestSaveActiveScene();

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("OkariDockspace", nullptr, windowFlags);

        ImGui::PopStyleVar(2);

        ImGuiID dockspaceId = ImGui::GetID("OkariMainDockspace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        static bool firstTime = true;

        if (firstTime)
        {
            firstTime = false;

            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

            ImGuiID dockMainId = dockspaceId;

            ImGuiID dockLeftId = ImGui::DockBuilderSplitNode(
                dockMainId,
                ImGuiDir_Left,
                0.20f,
                nullptr,
                &dockMainId
            );

            ImGuiID dockRightId = ImGui::DockBuilderSplitNode(
                dockMainId,
                ImGuiDir_Right,
                0.25f,
                nullptr,
                &dockMainId
            );

            ImGuiID dockBottomId = ImGui::DockBuilderSplitNode(
                dockMainId,
                ImGuiDir_Down,
                0.25f,
                nullptr,
                &dockMainId
            );

            ImGui::DockBuilderDockWindow("Hierarchy", dockLeftId);
            ImGui::DockBuilderDockWindow("Inspector", dockRightId);
            ImGui::DockBuilderDockWindow("Assets", dockBottomId);
            ImGui::DockBuilderDockWindow("Viewport", dockMainId);

            ImGui::DockBuilderFinish(dockspaceId);
        }

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Scene", "Ctrl+N"))
                    NewScene();

                if (ImGui::MenuItem("Load Scene", "Ctrl+O"))
                    RequestLoadScene();
                
                if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                    RequestSaveActiveScene();

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        m_HierarchyPanel->OnImGuiRender();

        m_ViewportPanel->OnImGuiRender();

        m_InspectorPanel->OnImGuiRender();

        m_AssetBrowserPanel->OnImGuiRender();

        DrawLoadScenePopup();
        DrawSaveScenePopup();
        DrawUnsavedScenePopup();

        ImGui::End();

        EditorUI::EndFrame();
    }
}