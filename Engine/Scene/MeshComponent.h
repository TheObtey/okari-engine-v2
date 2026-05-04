#pragma once

#include "Rendering/Mesh.h"

#include <string>

namespace Okari
{
	struct MeshComponent
	{
		bool Enabled = true;

		std::string MeshPath;
		std::string TexturePath;

		Mesh* RuntimeMesh = nullptr;
	};
}