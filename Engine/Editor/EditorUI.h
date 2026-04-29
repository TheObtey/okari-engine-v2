#pragma once

struct GLFWwindow;

namespace Okari
{
	class EditorUI
	{
	public:
		static void Init(GLFWwindow* window);
		static void Shutdown();

		static void BeginFrame();
		static void EndFrame();
	};
}