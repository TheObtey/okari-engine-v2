#pragma once

#include "Actor/Actor.h"
#include "Scene/Transform.h"
#include "Rendering/Camera.h"
#include "Physics/PhysicsComponent.h"
#include "Collision/CollisionWorld.h"

namespace Okari
{
	class PlayerActor : public Actor
	{
	public:
		PlayerActor(WorldObject* object = nullptr);

		void OnCreate() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

		void UpdateMovement(float deltaTime, const Camera& camera, const CollisionWorld& collisionWorld);

		void SetSpawnPosition(const glm::vec3& position) { m_SpawnPosition = position; m_HasSpawnPosition = true; }

		Transform& GetTransform() { return m_Transform; }
		const Transform& GetTransform() const { return m_Transform; }

		Mesh* GetMesh() const { return m_Mesh; }
		const std::string& GetTexturePath() const { return m_TexturePath; }

		PhysicsComponent& GetPhysics() { return m_Physics; }
		const PhysicsComponent& GetPhysics() const { return m_Physics; }

	private:
		Transform m_Transform;
		PhysicsComponent m_Physics;

		float m_MoveSpeed = 2.5f;

		glm::vec3 m_SpawnPosition = glm::vec3(0.0f);
		bool m_HasSpawnPosition = false;

		Mesh* m_Mesh = nullptr;
		std::string m_MeshPath = "Assets/Models/Link/Demo01_00_002.fbx";
		std::string m_TexturePath = "";
	};
}