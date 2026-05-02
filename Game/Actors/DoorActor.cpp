#include "DoorActor.h"

#include <iostream>

namespace Okari
{
	void DoorActor::OnCreate()
	{
		std::cout << "[DoorActor] Created: " << m_Object->Name << std::endl;
	}

	void DoorActor::OnUpdate(float deltaTime)
	{ }

	void DoorActor::OnDestroy()
	{
		std::cout << "[DoorActor] Destroyed: " << m_Object->Name << std::endl;
	}
}