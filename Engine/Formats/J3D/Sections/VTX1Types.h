#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	enum class J3DVertexAttribute : std::uint32_t
	{
		Position = 9,
		Normal = 10,
		Color0 = 11,
		Color1 = 12,
		TexCoord0 = 13,
		TexCoord1 = 14,
		TexCoord2 = 15,
		TexCoord3 = 16,
		TexCoord4 = 17,
		TexCoord5 = 18,
		TexCoord6 = 19,
		TexCoord7 = 20,

		// Particular GX attribute that regroup normal, binormal and tengent
		NBT = 25,

		Null = 0xFF
	};

	struct J3DVertexFormatDescriptor
	{
		J3DVertexAttribute Attribute = J3DVertexAttribute::Null;

		std::uint32_t ComponentCount = 0;
		std::uint32_t ComponentType = 0;

		std::uint8_t FractionalBits = 0;

		std::array<std::uint8_t, 3> Padding{};

		// Size of a complete element in associated table
		std::uint32_t ElementStride = 0;
	};

	struct J3DVertexArrayData
	{
		J3DVertexAttribute Attribute = J3DVertexAttribute::Null;

		std::uint32_t Offset = 0;
		std::uint32_t ByteSize = 0;

		std::uint32_t ElementStride = 0;

		std::uint32_t ElementCapacity = 0;

		std::uint32_t RemainderByteCount = 0;

		std::vector<std::uint8_t> RawData;
	};

	struct J3DVTX1Data
	{
		static constexpr std::size_t ArraySlotCount = 13;

		std::uint32_t FormatTableOffset = 0;

		std::array<std::uint32_t, ArraySlotCount> ArrayOffsets{};

		std::vector<J3DVertexFormatDescriptor> Formats;
		std::vector<J3DVertexArrayData> Arrays;

		const J3DVertexFormatDescriptor* FindFormat(J3DVertexAttribute attribute) const
		{
			for (const J3DVertexFormatDescriptor& format : Formats)
			{
				if (format.Attribute == attribute)
					return &format;
			}

			return nullptr;
		}

		const J3DVertexArrayData* FindArray(J3DVertexAttribute attribute) const
		{
			for (const J3DVertexArrayData& array : Arrays)
			{
				if (array.Attribute == attribute)
					return &array;
			}

			return nullptr;
		}
	};

	struct J3DVTX1ParseResult
	{
		J3DVTX1Data Data;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};

	const char* ToString(J3DVertexAttribute attribute);

	std::string DescribeComponentCount(const J3DVertexFormatDescriptor& format);

	std::string DescribeComponentType(const J3DVertexFormatDescriptor& format);
}