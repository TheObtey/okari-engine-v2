#include "EditorCameraController.h"
#include "Input/InputManager.h"

#include <imgui.h>
#include <glm.hpp>

namespace Okari
{
	void EditorCameraController::Update(float deltaTime, Camera& camera, GLFWwindow* window, bool viewportHovered, const glm::vec2& viewportCenter)
	{
		if (!window)
			return;

		if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			if (m_IsControlling)
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

			m_IsControlling = false;
			return;
		}

		if (!m_IsControlling && !viewportHovered)
			return;

		if (!m_IsControlling)
		{
			m_IsControlling = true;
			
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			glfwSetCursorPos(window, viewportCenter.x, viewportCenter.y);

			return;
		}

		double mouseX = 0.0f;
		double mouseY = 0.0f;

		glfwGetCursorPos(window, &mouseX, &mouseY);

		float deltaX = static_cast<float>(mouseX - viewportCenter.x);
		float deltaY = static_cast<float>(mouseY - viewportCenter.y);

		glfwSetCursorPos(window, viewportCenter.x, viewportCenter.y);

		m_Yaw += deltaX * m_MouseSensitivity;
		m_Pitch -= deltaY * m_MouseSensitivity;

		if (m_Pitch > 89.0f)
			m_Pitch = 89.0f;

		if (m_Pitch < -89.0f)
			m_Pitch = -89.0f;

		glm::vec3 forward;
		forward.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
		forward.y = sin(glm::radians(m_Pitch));
		forward.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
		forward = glm::normalize(forward);

		glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

		glm::vec3 movement(0.0f);

		if (InputManager::IsActionHeld("MoveForward"))
			movement += forward;

		if (InputManager::IsActionHeld("MoveBackward"))
			movement -= forward;

		if (InputManager::IsActionHeld("MoveRight"))
			movement += right;
		
		if (InputManager::IsActionHeld("MoveLeft"))
			movement -= right;
		
		if (InputManager::IsActionHeld("MoveUp"))
			movement += up;

		if (InputManager::IsActionHeld("MoveDown"))
			movement -= up;

		if (glm::length(movement) > 0.0f)
			movement = glm::normalize(movement);

		glm::vec3 position = camera.GetPosition();
		
		float speed = m_MoveSpeed;

		if (InputManager::IsActionHeld("SpeedUp"))
			speed *= 2.0f;

		position += movement * speed * deltaTime;

		camera.SetPosition(position);
		camera.SetTarget(position + forward);
	}
}