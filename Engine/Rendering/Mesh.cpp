#include "Mesh.h"

namespace Okari
{
	Mesh::Mesh(const std::vector<Vertex>& vertices)
	{
		m_VertexCount = (uint32_t)vertices.size();

		glGenVertexArrays(1, &m_VAO);
		glGenBuffers(1, &m_VBO);

		glBindVertexArray(m_VAO);

		glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));

		glBindVertexArray(0);

		glm::vec3 min(FLT_MAX);
		glm::vec3 max(-FLT_MAX);

		for (const auto& v : vertices)
		{
			glm::vec3 pos(v.Position[0], v.Position[1], v.Position[2]);

			min = glm::min(min, pos);
			max = glm::max(max, pos);
		}

		m_Bounds.Min = min;
		m_Bounds.Max = max;
	}

	Mesh::~Mesh()
	{
		glDeleteVertexArrays(1, &m_VAO);
		glDeleteBuffers(1, &m_VBO);
	}

	void Mesh::Bind() const
	{
		glBindVertexArray(m_VAO);
	}

	uint32_t Mesh::GetVertexCount() const
	{
		return m_VertexCount;
	}
}