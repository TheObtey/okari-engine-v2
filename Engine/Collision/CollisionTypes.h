#pragma once

#include <glm.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace Okari
{
	struct CollisionTriangle
	{
		uint32_t Index = 0;

		glm::vec3 V0 = glm::vec3(0.0f);
		glm::vec3 V1 = glm::vec3(0.0f);
		glm::vec3 V2 = glm::vec3(0.0f);

		glm::vec3 Normal = glm::vec3(0.0f, 1.0f, 0.0f);

		std::string Material = "default";
		uint32_t MaterialType = 0;
		std::string AttributeRaw = "0x0000";

		std::vector<std::string> Flags;
		bool Passthrough = false;
	};

	struct CollisionMesh
	{
		std::string SourceFile;
		std::vector<CollisionTriangle> Triangles;

		bool IsValid() const
		{
			return !Triangles.empty();
		}
	};
}