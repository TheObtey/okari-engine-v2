#include "PlayerActor.h"
#include "Input/InputManager.h"

#include <iostream>

namespace Okari
{
	PlayerActor::PlayerActor(WorldObject* object)
		: Actor(object)
	{ }

	void PlayerActor::OnCreate()
	{
		if (m_Object)
			m_Transform = m_Object->Transform;
		else
			m_Transform.Position = glm::vec3(0.0f, 1.0f, 0.0f);

		std::cout << "[PlayerActor] Created" << std::endl;
	}

	void PlayerActor::OnUpdate(float deltaTime)
	{ }

	void PlayerActor::OnDestroy()
	{
		std::cout << "[PlayerActor] Destroyed" << std::endl;
	}

	void PlayerActor::UpdateMovement(float deltaTime, const Camera& camera)
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