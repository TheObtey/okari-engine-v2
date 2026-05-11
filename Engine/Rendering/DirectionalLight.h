#pragma once

#include <glm.hpp>

namespace Okari
{
	struct DirectionalLight
	{
		glm::vec3 Direction = glm::vec3(-0.4f, -1.0f, -0.3f);
		// GX hardware context: lights in Twilight Princess are configured with very low
		// attenuation so that the rasterized color (rasc) stays in [0..~0.25].
		// The TEV scale_4 (*4) then brings it back to [0..1].
		// To approximate this with a standard Lambert model, keep Intensity <= 0.25
		// and Ambiant very dark. The sum (Ambiant + Color*Intensity) should be ~[0..0.25].
		glm::vec3 Color = glm::vec3(1.0f, 0.95f, 0.85f);
		float Intensity = 0.22f;
		glm::vec3 Ambiant = glm::vec3(0.03f, 0.03f, 0.03f);
	};
}