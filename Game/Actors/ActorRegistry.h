#pragma once

#include "ActorDefinition.h"

#include <unordered_map>

namespace Okari
{
	class ActorRegistry
	{
	public:
		static void Init();

		static const std::vector<std::string>& GetActorTypes();
		static const ActorDefinition* GetDefinition(const std::string& type);

	private:
		static std::unordered_map<std::string, ActorDefinition> s_Definitions;
		static std::vector<std::string> s_TypeNames;
	};
}