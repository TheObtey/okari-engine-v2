#pragma once

#include "Rendering/Material/Material.h"

#include <glad/glad.h>
#include <glm.hpp>

namespace Okari
{
	struct AABB
	{
		glm::vec3 Min;
		glm::vec3 Max;
	};

	struct Vertex
	{
		float Position[3];
		float UV[2];
		float Normal[3];
	};

	struct SubMesh
	{
		uint32_t VertexOffset = 0;
		uint32_t VertexCount = 0;
		uint32_t MaterialIndex = 0;
	};

	class Mesh
	{
	public:
		Mesh(
			const std::vector<Vertex>& vertices,
			const std::vector<SubMesh>& subMeshes = {},
			const std::vector<Material>& materials = {}
		);

		~Mesh();

		void Bind() const;
		
		uint32_t GetVertexCount() const;
		const AABB& GetBounds() const { return m_Bounds; };

		const std::vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }
		const std::vector<Material>& GetMaterials() const { return m_Materials; }
		std::vector<Material>& GetMaterials() { return m_Materials; }

	private:
		uint32_t m_VAO = 0;
		uint32_t m_VBO = 0;
		uint32_t m_VertexCount = 0;

		AABB m_Bounds;

		std::vector<SubMesh> m_SubMeshes;
		std::vector<Material> m_Materials;
	};
}