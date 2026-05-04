#pragma once

#include "Rendering/Mesh.h"

#include <string>

namespace Okari
{
	class FBXLoader
	{
	public:
		static Mesh* LoadMesh(const std::string& path);
	};
}