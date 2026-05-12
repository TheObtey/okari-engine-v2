#pragma once

#include "CollisionTypes.h"
#include "Rendering/Camera.h"

#include <glm.hpp>
#include <vector>

namespace Okari
{
	class Renderer;

	struct CollisionEntry
	{
		CollisionMesh Mesh;
		glm::mat4 WorldMatrix = glm::mat4(1.0f);
	};

	class CollisionWorld
	{
	public:
		void AddMesh(const CollisionMesh& mesh, const glm::mat4& worldMatrix);
		void Clear();

		const std::vector<CollisionEntry>& GetEntries() const { return m_Entries; }
		bool HasEntries() const { return !m_Entries.empty(); }

		void DebugRender(Renderer& renderer, const Camera& camera);

	private:
		std::vector<CollisionEntry> m_Entries;
	};
}