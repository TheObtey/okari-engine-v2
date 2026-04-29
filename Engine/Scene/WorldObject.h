#pragma once

#include "Scene/Transform.h"
#include <string>

namespace Okari
{
	struct WorldObject
	{
		std::string Name;
		Transform Transform;
		std::string TexturePath;
	};
}