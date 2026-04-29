#include "Editor/EditorLayer.h"
#include "Editor/EditorUI.h"
#include "Platform/Window.h"
#include "Core/Application.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Okari
{
    EditorLayer::EditorLayer()
        : m_World(nullptr),
        m_HierarchyPanel(nullptr),
        m_InspectorPanel(nullptr),
        m_ViewportPanel(nullptr),
        m_EditorCamera(nullptr),
        m_SelectedObjectID(0)
    { }

    void EditorLayer::Init()
    {
        m_World = std::make_unique<World>();

        m_World->LoadFromFile("Assets/Levels/test_level.json");

        m_EditorCamera = std::make_unique<Camera>(16.0f / 9.0f);
        m_EditorCamera->SetPosition(glm::vec3(0.0f, 3.0f, 6.0f));
        m_EditorCamera->SetTarget(glm::vec3(0.0f, 0.0f, 0.0f));

        //auto& objects = m_World->GetObjects();

        //objects.push_back({ "Cube 1" });
        //objects.push_back({ "Je suis pas un putain de node!!" });
        //objects.push_back({ "Light" });

        m_HierarchyPanel = std::make_unique<HierarchyPanel>(m_World.get(), &m_SelectedObjectID);
        m_InspectorPanel = std::make_unique<InspectorPanel>(m_World.get(), &m_SelectedObjectID);
        m_ViewportPanel = std::make_unique<ViewportPanel>();

        auto window = Application::Get().GetWindow().GetNativeWindow();
        EditorUI::Init(window);
    }

    void EditorLayer::Update(float deltaTime)
    { }

    void EditorLayer::Render(Renderer& renderer)
    {
        m_ViewportPanel->GetFramebuffer().Bind();

        renderer.BeginFrame();

        if (m_World && m_EditorCamera)
            m_World->Render(renderer, *m_EditorCamera);

        m_ViewportPanel->GetFramebuffer().Unbind();

        EditorUI::BeginFrame();

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
                ImGui::MenuItem("New Scene");
                ImGui::MenuItem("Open Scene");
                ImGui::MenuItem("Save Scene");
                ImGui::Separator();
                ImGui::MenuItem("Exit");
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        m_HierarchyPanel->OnImGuiRender();

        m_ViewportPanel->OnImGuiRender();

        m_InspectorPanel->OnImGuiRender();

        ImGui::Begin("Assets");
        ImGui::Text("Asset Browser");
        ImGui::End();

        ImGui::End();

        EditorUI::EndFrame();
    }
}