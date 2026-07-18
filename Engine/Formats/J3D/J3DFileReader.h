#pragma once

#include "Formats/J3D/J3DTypes.h";

#include <filesystem>;

namespace Okari
{
	class J3DFileReader
	{
	public:
		static J3DReadResult Read(const std::filesystem::path& path);
	};
}