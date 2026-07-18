#pragma once

#include "Formats/J3D/J3DTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	struct J3DStringTableEntry
	{
		std::uint16_t Hash = 0;

		std::uint16_t StringOffset = 0;

		std::string Value;
	};

	struct J3DStringTable
	{
		std::uint16_t Padding = 0;
		std::vector<J3DStringTableEntry> Entries;
	};

	struct J3DStringTableReadResult
	{
		J3DStringTable Table;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};

	class J3DStringTableReader
	{
	public:
		static J3DStringTableReadResult Read(
			const J3DDocument& document,
			const J3DSectionInfo& section,
			std::uint32_t tableOffset
		);
	};
}