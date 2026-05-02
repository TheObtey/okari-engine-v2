#pragma once

#include "Scene/Transform.h"
#include <string>
#include <cstdint>

namespace Okari
{
	struct WorldObject
	{
		bool Enabled = true;

		uint64_t ID = 0;
		uint64_t ParentID = 0;
		
		std::string Name;
		std::string ActorType = "None";
		
		Transform Transform;
		
		std::string TexturePath;
	};
}