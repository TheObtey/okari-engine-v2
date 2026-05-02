#pragma once

#include "Actor/Actor.h"

namespace Okari
{
	class DoorActor : public Actor
	{
	public:
		using Actor::Actor;

		void OnCreate() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;
	};
}