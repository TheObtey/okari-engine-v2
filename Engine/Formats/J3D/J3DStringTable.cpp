#include "Formats/J3D/J3DStringTable.h"

#include "IO/BigEndianReader.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace Okari
{
	namespace
	{
		constexpr std::size_t StringTableHeaderSize = 0x04;
		constexpr std::size_t StringTableEntrySize = 0x04;

		struct StringDescriptor
		{
			std::uint16_t Hash = 0;
			std::uint16_t Offset = 0;
		};

		J3DStringTableReadResult Failure(const std::string& message)
		{
			J3DStringTableReadResult result;
			result.Error = message;
			return result;
		}
	}

	J3DStringTableReadResult J3DStringTableReader::Read(
		const J3DDocument& document,
		const J3DSectionInfo& section,
		std::uint32_t tableOffset
	)
	{
		const std::uint64_t sectionEnd =
			static_cast<std::uint64_t>(section.Offset) +
			section.Size;

		const std::uint64_t tableStart =
			static_cast<std::uint64_t>(section.Offset) +
			tableOffset;

		if (tableStart + StringTableHeaderSize > sectionEnd)
			return Failure("J3D string table header is outside the section");

		try
		{
			BigEndianReader reader(document.Data);

			reader.Seek(
				static_cast<std::size_t>(tableStart)
			);

			const std::uint16_t stringCount = reader.ReadU16();

			J3DStringTable table;
			table.Padding = reader.ReadU16();

			const std::uint64_t descriptorsSize =
				static_cast<std::uint64_t>(stringCount) *
				StringTableEntrySize;

			if (tableStart + StringTableHeaderSize + descriptorsSize > sectionEnd)
				return Failure("J3D string table entries exceed the section");

			std::vector<StringDescriptor> descriptors;
			descriptors.reserve(stringCount);

			for (std::uint16_t index = 0; index < stringCount; ++index)
			{
				StringDescriptor descriptor;

				descriptor.Hash = reader.ReadU16();
				descriptor.Offset = reader.ReadU16();

				descriptors.push_back(descriptor);
			}

			table.Entries.reserve(stringCount);

			for (const StringDescriptor& descriptor : descriptors)
			{
				const std::uint64_t stringStart = tableStart + descriptor.Offset;

				if (stringStart >= sectionEnd)
					return Failure("J3D string offset is outside the section");

				const std::size_t maximumLength = static_cast<std::size_t>(sectionEnd - stringStart);

				reader.Seek(
					static_cast<std::size_t>(stringStart)
				);

				J3DStringTableEntry entry;

				entry.Hash = descriptor.Hash;
				entry.StringOffset = descriptor.Offset;
				entry.Value = reader.ReadCString(maximumLength);

				table.Entries.push_back(std::move(entry));
			}

			J3DStringTableReadResult result;
			result.Table = std::move(table);

			return result;
		}
		catch (const std::exception& exception)
		{
			return Failure(
				std::string("Malformed J3D string table: ") +
				exception.what()
			);
		}
	}
}