#pragma once

#include <memory>
#include <string>

#include "../../Engine/Actor/Actor.h"

namespace Okari
{
	class ActorFactory
	{
	public:
		static std::unique_ptr<Actor> CreateActor(const std::string& type, WorldObject* object);
	};
}