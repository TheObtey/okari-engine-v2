#pragma once

#include "Rendering/Shader.h"
#include "Rendering/Material/Material.h"

namespace Okari
{
	class GXMaterialBinder
	{
	public:
		static void BindMaterialUniforms(const Shader& shader, const Material& material);
	};
}