#include "ActorRegistry.h"

namespace Okari
{
	std::unordered_map<std::string, ActorDefinition> ActorRegistry::s_Definitions;
	std::vector<std::string> ActorRegistry::s_TypeNames;

	void ActorRegistry::Init()
	{
		s_Definitions.clear();
		s_TypeNames.clear();

		{ // NONE
			ActorDefinition def;
			def.TypeName = "None";

			s_Definitions[def.TypeName] = def;
			s_TypeNames.push_back(def.TypeName);
		}

		{ // DOOR
			ActorDefinition def;
			def.TypeName = "Door";

			s_Definitions[def.TypeName] = def;
			s_TypeNames.push_back(def.TypeName);
		}

		{ // NPC
			ActorDefinition def;
			def.TypeName = "NPC";

			s_Definitions[def.TypeName] = def;
			s_TypeNames.push_back(def.TypeName);
		}

		{ // ENEMY
			ActorDefinition def;
			def.TypeName = "Enemy";

			s_Definitions[def.TypeName] = def;
			s_TypeNames.push_back(def.TypeName);
		}

		{ // DIRECTIONAL LIGHT
			ActorDefinition def;
			def.TypeName = "DirectionalLight";

			def.Properties.push_back({ "direction", ActorPropertyType::Vec3 });
			def.Properties.push_back({ "color", ActorPropertyType::Vec3 });
			def.Properties.push_back({ "intensity", ActorPropertyType::Float });
			def.Properties.push_back({ "ambiant", ActorPropertyType::Vec3 });

			s_Definitions[def.TypeName] = def;
			s_TypeNames.push_back(def.TypeName);
		}
	}

	const std::vector<std::string>& ActorRegistry::GetActorTypes()
	{
		return s_TypeNames;
	}

	const ActorDefinition* ActorRegistry::GetDefinition(const std::string& type)
	{
		auto it = s_Definitions.find(type);

		if (it == s_Definitions.end())
			return nullptr;

		return &it->second;
	}
}