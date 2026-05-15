#include "PlayerActor.h"
#include "Input/InputManager.h"
#include "Resources/MeshManager.h"
#include "Physics/PhysicsSystem.h"

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
		else if (m_HasSpawnPosition)
			m_Transform.Position = m_SpawnPosition;
		else
			m_Transform.Position = glm::vec3(0.0f, 0.0f, 0.0f);

		m_Transform.Scale = glm::vec3(0.01f);

		m_Mesh = MeshManager::Get().LoadMesh(m_MeshPath);

		std::cout << "[PlayerActor] Created at ("
			<< m_Transform.Position.x << ", "
			<< m_Transform.Position.y << ", "
			<< m_Transform.Position.z << ")"
			<< (m_HasSpawnPosition ? " [pl_spawn]" : " [default]")
			<< std::endl;
	}

	void PlayerActor::OnUpdate(float deltaTime)
	{ }

	void PlayerActor::OnDestroy()
	{
		std::cout << "[PlayerActor] Destroyed" << std::endl;
	}

	void PlayerActor::UpdateMovement(float deltaTime, const Camera& camera, const CollisionWorld& collisionWorld)
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

		// Apply gravity and ground snapping.
		PhysicsSystem::Update(m_Physics, m_Transform.Position, collisionWorld, deltaTime);
	}
}