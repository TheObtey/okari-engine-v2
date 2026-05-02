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

	private:
		Transform m_Transform;
		float m_MoveSpeed = 2.5f;
	};
}