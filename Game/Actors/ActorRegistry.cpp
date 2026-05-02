#include "ActorRegistry.h"

#include <algorithm>

namespace Okari
{
	std::vector<std::string> ActorRegistry::s_ActorTypes;

	void ActorRegistry::Init()
	{
		s_ActorTypes.clear();

		s_ActorTypes.push_back("None");
		s_ActorTypes.push_back("Door");
		s_ActorTypes.push_back("NPC");
		s_ActorTypes.push_back("Enemy");
	}

	const std::vector<std::string>& ActorRegistry::GetActorTypes()
	{
		return s_ActorTypes;
	}

	bool ActorRegistry::IsValidActorType(const std::string& type)
	{
		return std::find(s_ActorTypes.begin(), s_ActorTypes.end(), type) != s_ActorTypes.end();
	}
}