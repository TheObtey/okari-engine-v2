#include "Editor/EditorLayer.h"
#include "Core/Application.h"
#include "Platform/Window.h"
#include "Editor/EditorUI.h"
#include "Resources/OKCOLLoader.h"
#include "Input/InputContext.h"
#include "Input/InputManager.h"
#include "Actors/ActorRegistry.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
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

        if (m_OpenScenes[index] && m_OpenScenes[index]->World)
            m_OpenScenes[index]->World->ExitPlayMode();

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

#pragma region SAVE_DIRTY_SCENE
    bool EditorLayer::HasDirtyScenes() const
    {
        for (const auto& scene : m_OpenScenes)
        {
            if (scene && scene->Dirty)
                return true;
        }

        return false;
    }

    void EditorLayer::SaveDirtyScenesAndClose()
    {
        for (int i = 0; i < static_cast<int>(m_OpenScenes.size()); i++)
        {
            SceneDocument* scene = m_OpenScenes[i].get();

            if (!scene || !scene->Dirty)
                continue;

            SetActiveScene(i);

            if (!scene->HasBeenSaved || scene->Path.empty())
            {
                m_CloseEditorAfterSave = true;
                RequestSaveActiveScene();
                return;
            }

            scene->World->SaveToFile(scene->Path, scene->Name);
            scene->Dirty = false;
        }

        m_ForceCloseEditor = true;
        Application::Get().Close();
    }

    void EditorLayer::DrawCloseEditorPopup()
    {
        if (m_ShouldOpenCloseEditorPopup)
        {
            ImGui::OpenPopup("Unsaved Scenes");
            m_ShouldOpenCloseEditorPopup = false;
        }

        if (ImGui::BeginPopupModal("Unsaved Scenes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Some scenes have unsaved changes.");
            ImGui::Text("Do you want to save before closing?");

            ImGui::Separator();

            if (ImGui::Button("Save All"))
            {
                SaveDirtyScenesAndClose();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Don't Save"))
            {
                m_ForceCloseEditor = true;

                Application::Get().Close();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
#pragma endregion

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

        SceneDocument* activeScene = GetActiveScene();

        activeScene->World->GetCollisionWorld().SetMesh(
            OKCOLLoader::Load("Assets/Models/Stages/ToalFarm/room.okcol")
        );
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

                        if (m_CloseEditorAfterSave)
                        {
                            m_CloseEditorAfterSave = false;
                            SaveDirtyScenesAndClose();
                        }

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
        ActorRegistry::Init();

        auto explorationCtx = std::make_shared<InputContext>("Exploration");
        explorationCtx->BindKey(GLFW_KEY_W, "MoveForward");
        explorationCtx->BindKey(GLFW_KEY_S, "MoveBackward");
        explorationCtx->BindKey(GLFW_KEY_A, "MoveLeft");
        explorationCtx->BindKey(GLFW_KEY_D, "MoveRight");
        explorationCtx->BindKey(GLFW_KEY_E, "Interact");
        explorationCtx->BindKey(GLFW_KEY_ESCAPE, "Cancel");

        InputManager::RegisterContext(explorationCtx);

        auto editorCameraCtx = std::make_shared<InputContext>("EditorCamera");
        editorCameraCtx->BindKey(GLFW_KEY_W, "MoveForward");
        editorCameraCtx->BindKey(GLFW_KEY_S, "MoveBackward");
        editorCameraCtx->BindKey(GLFW_KEY_A, "MoveLeft");
        editorCameraCtx->BindKey(GLFW_KEY_D, "MoveRight");
        editorCameraCtx->BindKey(GLFW_KEY_E, "MoveUp");
        editorCameraCtx->BindKey(GLFW_KEY_Q, "MoveDown");
        editorCameraCtx->BindKey(GLFW_KEY_LEFT_SHIFT, "SpeedUp");

        editorCameraCtx->BindKey(GLFW_KEY_F1, "TevDebugMode0");
        editorCameraCtx->BindKey(GLFW_KEY_F2, "TevDebugMode1");
        editorCameraCtx->BindKey(GLFW_KEY_F3, "TevDebugMode2");
        editorCameraCtx->BindKey(GLFW_KEY_F4, "TevDebugMode3");
        
        editorCameraCtx->BindKey(GLFW_KEY_F5, "TevDebugMode7");
        editorCameraCtx->BindKey(GLFW_KEY_F6, "TevDebugMode12");
        
        editorCameraCtx->BindKey(GLFW_KEY_F8, "TevDebugMode20");
        editorCameraCtx->BindKey(GLFW_KEY_F9, "TevDebugMode21");
        editorCameraCtx->BindKey(GLFW_KEY_F10, "TevDebugMode22");
        editorCameraCtx->BindKey(GLFW_KEY_F11, "TevDebugMode23");

        InputManager::RegisterContext(editorCameraCtx);
        InputManager::PushContext("EditorCamera");

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

        m_Window = Application::Get().GetWindow().GetNativeWindow();

        EditorUI::Init(m_Window);
    }

    void EditorLayer::Update(float deltaTime)
    {
        SceneDocument* activeScene = GetActiveScene();

        if (activeScene && activeScene->World && m_EditorCamera)
        {
            if (!activeScene->World->IsPlaying() && m_Window)
                m_EditorCameraController.Update(deltaTime, *m_EditorCamera, m_Window, m_ViewportPanel->IsViewportHovered(), m_ViewportPanel->GetViewportCenter());

            activeScene->World->UpdateActors(deltaTime, *m_EditorCamera);
        }
    }

    void EditorLayer::Render(Renderer& renderer)
    {
        Framebuffer& viewportFramebuffer = m_ViewportPanel->GetFramebuffer();

        viewportFramebuffer.Bind();
        viewportFramebuffer.SetDrawAttachment(0);

        if (InputManager::IsActionPressed("TevDebugMode0"))
            renderer.SetTevDebugMode(0);

        if (InputManager::IsActionPressed("TevDebugMode1"))
            renderer.SetTevDebugMode(1);

        if (InputManager::IsActionPressed("TevDebugMode2"))
            renderer.SetTevDebugMode(2);

        if (InputManager::IsActionPressed("TevDebugMode3"))
            renderer.SetTevDebugMode(3);

        if (InputManager::IsActionPressed("TevDebugMode7"))
            renderer.SetTevDebugMode(7);

        if (InputManager::IsActionPressed("TevDebugMode12"))
            renderer.SetTevDebugMode(12);

        if (InputManager::IsActionPressed("TevDebugMode20"))
            renderer.SetTevDebugMode(20);

        if (InputManager::IsActionPressed("TevDebugMode21"))
            renderer.SetTevDebugMode(21);

        if (InputManager::IsActionPressed("TevDebugMode22"))
            renderer.SetTevDebugMode(22);

        if (InputManager::IsActionPressed("TevDebugMode23"))
            renderer.SetTevDebugMode(23);

        renderer.BeginFrame();

        SceneDocument* activeScene = GetActiveScene();

        if (activeScene && activeScene->World && m_EditorCamera)
            activeScene->World->Render(renderer, *m_EditorCamera, activeScene->SelectedObjectID);

        viewportFramebuffer.ClearPickingAttachment();
        glClear(GL_DEPTH_BUFFER_BIT);

        if (activeScene && activeScene->World && m_EditorCamera)
            activeScene->World->RenderPicking(renderer, *m_EditorCamera);

        viewportFramebuffer.SetDrawAttachment(0);
        viewportFramebuffer.Unbind();

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

            SceneDocument* activeScene = GetActiveScene();

            if (activeScene && activeScene->World)
            {
                if (!activeScene->World->IsPlaying())
                {
                    if (ImGui::Button("Play"))
                    {
                        activeScene->World->EnterPlayMode();
                        InputManager::PushContext("Exploration");
                    }
                }
                else
                {
                    if (ImGui::Button("Stop"))
                    {
                        activeScene->World->ExitPlayMode();
                        InputManager::PopContext("Exploration");
                    }
                }
            }

            ImGui::EndMenuBar();
        }

        uint32_t pickedID = 0;

        if (m_ViewportPanel->GetFramebuffer().PollPickResult(pickedID))
        {
            SceneDocument* activeScene = GetActiveScene();

            if (activeScene)
                activeScene->SelectedObjectID = pickedID;
        }

        m_HierarchyPanel->OnImGuiRender();

        m_ViewportPanel->OnImGuiRender();

        m_InspectorPanel->OnImGuiRender();

        m_AssetBrowserPanel->OnImGuiRender();

        DrawLoadScenePopup();
        DrawSaveScenePopup();
        DrawUnsavedScenePopup();
        DrawCloseEditorPopup();

        if (m_ViewportPanel->HasPendingPick())
        {
            SceneDocument* activeScene = GetActiveScene();

            if (activeScene && activeScene->World)
            {
                m_ViewportPanel->GetFramebuffer().RequestPickRead(
                    m_ViewportPanel->GetPickX(),
                    m_ViewportPanel->GetPickY()
                );
            }

            m_ViewportPanel->ClearPendingPick();
        }

        ImGui::End();

        EditorUI::EndFrame();
    }

    bool EditorLayer::OnWindowCloseRequested()
    {
        if (m_ForceCloseEditor)
            return true;

        if (!HasDirtyScenes())
            return true;

        m_ShouldOpenCloseEditorPopup = true;
        return false;
    }
}