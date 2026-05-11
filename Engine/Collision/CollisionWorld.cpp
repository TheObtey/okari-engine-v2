#include "CollisionWorld.h"
#include "Rendering/Renderer.h"

namespace Okari
{
	void CollisionWorld::SetMesh(const CollisionMesh& mesh)
	{
		m_Mesh = mesh;
	}

	void CollisionWorld::Clear()
	{
		m_Mesh.Triangles.clear();
		m_Mesh.SourceFile.clear();
	}

	void CollisionWorld::DebugRender(Renderer& renderer, const Camera& camera)
	{
		if (!HasMesh())
			return;

		renderer.DrawCollisionMesh(m_Mesh, camera);
	}
}