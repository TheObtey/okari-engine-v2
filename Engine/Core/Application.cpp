#include "Core/Application.h"
#include "Core/Time.h"
#include "Input/Input.h"
#include "Input/InputManager.h"

//#include "Input/InputContext.h"

#include <glad/glad.h>
#include <iostream>

namespace Okari
{
	static Application* s_Instance = nullptr;

	Application::Application(const std::string& title)
	{
		s_Instance = this;
		m_Window = std::make_unique<Window>(1920, 1080, title);

		Input::Init(m_Window->GetNativeWindow());

		//auto explorationCtx = std::make_shared<InputContext>("Exploration");
		//explorationCtx->BindKey(GLFW_KEY_E, "Interact");
		//explorationCtx->BindKey(GLFW_KEY_ESCAPE, "Cancel");

		//explorationCtx->BindKey(GLFW_KEY_W, "MoveForward");
		//explorationCtx->BindKey(GLFW_KEY_S, "MoveBackward");
		//explorationCtx->BindKey(GLFW_KEY_A, "MoveLeft");
		//explorationCtx->BindKey(GLFW_KEY_D, "MoveRight");

		//InputManager::RegisterContext(explorationCtx);
		//InputManager::PushContext("Exploration");

		//auto menuCtx = std::make_shared<InputContext>("Menu");
		//menuCtx->BindKey(GLFW_KEY_E, "Confirm");
		//menuCtx->SetBlocking(true);

		//InputManager::RegisterContext(menuCtx);

		//m_GameLayer = std::make_unique<GameLayer>();
		//m_GameLayer->Init();

		m_Renderer = std::make_unique<Renderer>();
		m_Renderer->Init();
	}

	Application::~Application() = default;

	Application& Application::Get()
	{
		return *s_Instance;
	}

	Window& Application::GetWindow()
	{
		return *m_Window;
	}

	void Application::SetLayer(std::unique_ptr<Layer> layer)
	{
		m_Layer = std::move(layer);

		if (m_Layer)
			m_Layer->Init();
	}

	void Application::Run()
	{
		while (!m_Window->ShouldClose())
		{
			Time::Update();
			Input::Update();

			if (m_Layer)
				m_Layer->Update(Time::GetDeltaTime());

			//m_GameLayer->Update(Time::GetDeltaTime());

			//if (InputManager::IsActionPressed("Confirm"))
			//	std::cout << "Confirm in menu" << std::endl;
			//
			//if (InputManager::IsActionPressed("Interact"))
			//	std::cout << "[Exploration] Interact pressed" << std::endl;

			//if (InputManager::IsActionPressed("Cancel"))
			//	InputManager::PushContext("Menu");

			glClearColor(0.08f, 0.08f, 0.10f, 1.0f);

			m_Renderer->BeginFrame();

			if (m_Layer)
				m_Layer->Render(*m_Renderer);

			//m_GameLayer->Render(*m_Renderer);

			m_Renderer->EndFrame();

			m_Window->SwapBuffers();
			m_Window->PollEvents();
		}
	}
}