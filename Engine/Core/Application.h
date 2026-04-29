#pragma once

#include "Platform/Window.h"
#include "Rendering/Renderer.h"
#include "Core/Layer.h"

#include <memory>

namespace Okari
{
	class Application
	{
	public:
		Application(const std::string& title = "Okari Engine");
		~Application();

		static Application& Get();
		Window& GetWindow();

		void SetLayer(std::unique_ptr<Layer> layer);
		void Run();

	private:
		std::unique_ptr<Window> m_Window;
		std::unique_ptr<Renderer> m_Renderer;
		std::unique_ptr<Layer> m_Layer;
	};
}