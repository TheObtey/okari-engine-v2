#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <unordered_map>

namespace Okari
{
	class Input
	{
	public:
		static void Init(GLFWwindow* window);
		static void Update();

		static bool IsKeyPressed(int key);
		static bool IsKeyHeld(int key);
		static bool IsKeyReleased(int key);

	private:
		static GLFWwindow* s_Window;

		static std::unordered_map<int, bool> s_CurrentKeys;
		static std::unordered_map<int, bool> s_PreviousKeys;
	};
}