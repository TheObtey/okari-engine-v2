#include "Input/Input.h"

namespace Okari
{
	GLFWwindow* Input::s_Window = nullptr;

	std::unordered_map<int, bool> Input::s_CurrentKeys;
	std::unordered_map<int, bool> Input::s_PreviousKeys;

	void Input::Init(GLFWwindow* window)
	{
		s_Window = window;
	}

	void Input::Update()
	{
		s_PreviousKeys = s_CurrentKeys;

		for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; key++)
		{
			int state = glfwGetKey(s_Window, key);
			s_CurrentKeys[key] = state == GLFW_PRESS || state == GLFW_REPEAT;
		}
	}

	bool Input::IsKeyPressed(int key)
	{
		return s_CurrentKeys[key] && !s_PreviousKeys[key];
	}

	bool Input::IsKeyHeld(int key)
	{
		return s_CurrentKeys[key];
	}

	bool Input::IsKeyReleased(int key)
	{
		return !s_CurrentKeys[key] && s_PreviousKeys[key];
	}
}