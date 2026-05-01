#include "GameLayer.h"
#include "Input/InputContext.h"
#include "Input/InputManager.h"
#include <GLFW/glfw3.h>
#include <memory>

namespace Okari
{
    GameLayer::GameLayer() = default;
    GameLayer::~GameLayer() = default;

    void GameLayer::Init()
    {
        auto explorationCtx = std::make_shared<InputContext>("Exploration");

        explorationCtx->BindKey(GLFW_KEY_W, "MoveForward");
        explorationCtx->BindKey(GLFW_KEY_S, "MoveBackward");
        explorationCtx->BindKey(GLFW_KEY_A, "MoveLeft");
        explorationCtx->BindKey(GLFW_KEY_D, "MoveRight");
        explorationCtx->BindKey(GLFW_KEY_E, "Interact");
        explorationCtx->BindKey(GLFW_KEY_ESCAPE, "Cancel");
        explorationCtx->BindKey(GLFW_KEY_F1, "Save");

        InputManager::RegisterContext(explorationCtx);
        InputManager::PushContext("Exploration");

        m_Camera = std::make_unique<Camera>(1280.0f / 720.0f);
        m_World = std::make_unique<World>();
        m_World->LoadFromFile("Assets/Levels/test_level.okscene");

        WorldObject cube;
        cube.Transform.Position = glm::vec3(3.0f, 0.0f, 0.0f);
        cube.Transform.Scale = glm::vec3(0.5f);

        m_World->AddObject(cube);
    }

    void GameLayer::Update(float deltaTime)
    {
        m_World->Update(deltaTime);
        m_Player.Update(deltaTime, *m_Camera);

        glm::vec3 playerPos = m_Player.GetTransform().Position;
        glm::vec3 cameraOffset = glm::vec3(0.0f, 2.0f, 5.0f);

        m_Camera->SetPosition(playerPos + cameraOffset);
        m_Camera->SetTarget(playerPos);
    }

    void GameLayer::Render(Renderer& renderer)
    {
        m_World->Render(renderer, *m_Camera);
        //renderer.DrawCube(m_Player.GetTransform(), *m_Camera);
    }
}