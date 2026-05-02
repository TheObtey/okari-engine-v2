#pragma once

#include <string>
#include <vector>

namespace Okari
{
	class ActorRegistry
	{
	public:
		static void Init();

		static const std::vector<std::string>& GetActorTypes();
		static bool IsValidActorType(const std::string& type);

	private:
		static std::vector<std::string> s_ActorTypes;
	};
}