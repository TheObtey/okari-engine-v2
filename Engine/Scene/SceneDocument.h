#pragma once

#include "../../Game/World/World.h"
#include <memory>

namespace Okari
{
	struct SceneDocument
	{
		std::unique_ptr<World> World = nullptr;

		std::string Name;
		std::string Path;

		uint64_t SelectedObjectID = 0;

		bool Dirty = false;
		bool HasBeenSaved = false;
	};
}