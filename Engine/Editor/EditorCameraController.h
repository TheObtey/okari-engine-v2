#pragma once

#include "Rendering/Camera.h"

#include <glm.hpp>
#include <GLFW/glfw3.h>

namespace Okari
{
	class EditorCameraController
	{
	public:
		void Update(float deltaTime, Camera& camera, GLFWwindow* window, bool viewportHovered, const glm::vec2& viewportCenter);
		void FocusOn(Camera& camera, const glm::vec3& targetPos, float distance = 5.0f);

	private:
		bool m_IsControlling = false;

		float m_Yaw = -90.0f;
		float m_Pitch = -25.0f;

		float m_MoveSpeed = 8.0f;
		float m_MouseSensitivity = 0.1f;
	};
}