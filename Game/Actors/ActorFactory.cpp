#include "ActorFactory.h"

#include "PlayerActor.h"
#include "DoorActor.h"

namespace Okari
{
	std::unique_ptr<Actor> ActorFactory::CreateActor(const std::string& type, WorldObject* object)
	{
		if (type == "Player")
			return std::make_unique<PlayerActor>(object);

		if (!object)
			return nullptr;

		if (type == "Door")
			return std::make_unique<DoorActor>(object);

		return nullptr;
	}
}