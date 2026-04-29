#pragma once

#include "Scene/Transform.h"
#include "Rendering/Camera.h"

namespace Okari
{
	class PlayerActor
	{
	public:
		void Update(float deltaTime, const Camera& camera);

		Transform& GetTransform() { return m_Transform; }
		const Transform& GetTransform() const { return m_Transform; }

	private:
		Transform m_Transform;
		float m_MoveSpeed = 2.5f;
	};
}