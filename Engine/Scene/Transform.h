#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

namespace Okari
{
	struct Transform
	{
		glm::vec3 Position = glm::vec3(0.0f);
		glm::vec3 Rotation = glm::vec3(0.0f);
		glm::vec3 Scale = glm::vec3(1.0f);

		glm::mat4 GetModelMatrix() const
		{
			glm::mat4 model = glm::mat4(1.0f);

			model = glm::translate(model, Position);

			model = glm::rotate(model, Rotation.x, glm::vec3(1, 0, 0));
			model = glm::rotate(model, Rotation.y, glm::vec3(0, 1, 0));
			model = glm::rotate(model, Rotation.z, glm::vec3(0, 0, 1));

			model = glm::scale(model, Scale);

			return model;
		}
	};
}