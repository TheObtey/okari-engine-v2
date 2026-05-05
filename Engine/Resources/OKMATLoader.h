#pragma once

#include "Rendering/Mesh.h"

#include <string>

namespace Okari
{
	class OKMATLoader
	{
	public:
		static bool ApplyToMesh(Mesh* mesh, const std::string& okmatPath);
	};
}