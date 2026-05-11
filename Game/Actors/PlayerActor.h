#pragma once

#include "Actor/Actor.h"
#include "Scene/Transform.h"
#include "Rendering/Camera.h"

namespace Okari
{
	class PlayerActor : public Actor
	{
	public:
		PlayerActor(WorldObject* object = nullptr);

		void OnCreate() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

		void UpdateMovement(float deltaTime, const Camera& camera);

		Transform& GetTransform() { return m_Transform; }
		const Transform& GetTransform() const { return m_Transform; }

		Mesh* GetMesh() const { return m_Mesh; }
		const std::string& GetTexturePath() const { return m_TexturePath; }

	private:
		Transform m_Transform;
		float m_MoveSpeed = 2.5f;

		Mesh* m_Mesh = nullptr;
		std::string m_MeshPath = "Assets/Models/Link/Demo01_00_002.fbx";
		std::string m_TexturePath = "";
	};
}