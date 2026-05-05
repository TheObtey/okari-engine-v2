#pragma once

#include <glm.hpp>

namespace Okari
{
	struct DirectionalLight
	{
		glm::vec3 Direction = glm::vec3(-0.4f, -1.0f, -0.3f);
		glm::vec3 Color = glm::vec3(1.0f, 0.95f, 0.85f);
		glm::vec3 Ambiant = glm::vec3(0.25f, 0.25f, 0.30f);
	};
}