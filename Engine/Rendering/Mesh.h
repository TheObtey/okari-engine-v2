#pragma once

#include <vector>
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
	};

	class Mesh
	{
	public:
		Mesh(const std::vector<Vertex>& vertices);
		~Mesh();

		void Bind() const;
		uint32_t GetVertexCount() const;

		const AABB& GetBounds() const { return m_Bounds; };

	private:
		uint32_t m_VAO = 0;
		uint32_t m_VBO = 0;
		uint32_t m_VertexCount = 0;
		AABB m_Bounds;
	};
}