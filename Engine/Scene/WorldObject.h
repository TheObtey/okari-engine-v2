#pragma once

#include "Scene/Transform.h"
#include <string>
#include <cstdint>

namespace Okari
{
	struct WorldObject
	{
		uint64_t ID = 0;
		uint64_t ParentID = 0;
		std::string Name;
		Transform Transform;
		std::string TexturePath;
	};
}