#include "PlayerActor.h"
#include "Input/InputManager.h"

namespace Okari
{
	void PlayerActor::Update(float deltaTime, const Camera& camera)
	{
		glm::vec3 forward = camera.GetForward();
		forward.y = 0.0f;
		forward = glm::normalize(forward);

		glm::vec3 right = camera.GetRight();
		right.y = 0.0f;
		right = glm::normalize(right);

		glm::vec3 direction(0.0f);

		if (InputManager::IsActionHeld("MoveForward"))
			direction += forward;

		if (InputManager::IsActionHeld("MoveBackward"))
			direction -= forward;

		if (InputManager::IsActionHeld("MoveLeft"))
			direction -= right;

		if (InputManager::IsActionHeld("MoveRight"))
			direction += right;

		if (glm::length(direction) > 0.0f)
		{
			direction = glm::normalize(direction);
			m_Transform.Position += direction * m_MoveSpeed * deltaTime;
		}
	}
}