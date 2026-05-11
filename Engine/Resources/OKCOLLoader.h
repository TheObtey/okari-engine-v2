#pragma once

#include "Collision/CollisionTypes.h"

#include<json.hpp>
#include <string>

namespace Okari
{
	class OKCOLLoader
	{
	public:
		static CollisionMesh Load(const std::string& path);
	};
}