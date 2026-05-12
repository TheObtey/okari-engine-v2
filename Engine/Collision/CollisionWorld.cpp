#include "CollisionWorld.h"
#include "Rendering/Renderer.h"

namespace Okari
{
	void CollisionWorld::AddMesh(const CollisionMesh& mesh, const glm::mat4& worldMatrix)
	{
		CollisionEntry entry;
		entry.Mesh = mesh;
		entry.WorldMatrix = worldMatrix;
		m_Entries.push_back(std::move(entry));
	}

	void CollisionWorld::Clear()
	{
		m_Entries.clear();
	}

	void CollisionWorld::DebugRender(Renderer& renderer, const Camera& camera)
	{
		for (const CollisionEntry& entry : m_Entries)
			renderer.DrawCollisionMesh(entry.Mesh, camera, entry.WorldMatrix);
	}
}