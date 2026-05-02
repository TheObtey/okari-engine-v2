#pragma once

#include <string>
#include <vector>

namespace Okari
{
	enum class ActorPropertyType
	{
		Bool,
		Int,
		Float,
		String,
		Vec3
	};

	struct ActorPropertyDefinition
	{
		std::string Name;
		ActorPropertyType Type;
	};

	struct ActorDefinition
	{
		std::string TypeName;
		std::vector<ActorPropertyDefinition> Properties;
	};
}