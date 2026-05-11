#pragma once

#include "CollisionTypes.h"
#include "Rendering/Camera.h"

namespace Okari
{
	class Renderer;

	class CollisionWorld
	{
	public:
		void SetMesh(const CollisionMesh& mesh);
		void Clear();

		const CollisionMesh& GetMesh() const { return m_Mesh; }
		bool HasMesh() const { return m_Mesh.IsValid(); }

		void DebugRender(Renderer& renderer, const Camera& camera);

	private:
		CollisionMesh m_Mesh;
	};
}