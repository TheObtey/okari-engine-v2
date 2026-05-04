#pragma once

#include <string>

namespace Okari
{
	struct MeshComponent
	{
		bool Enabled = true;

		std::string MeshPath;
		std::string TexturePath;
	};
}