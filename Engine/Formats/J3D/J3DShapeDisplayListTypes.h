#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	enum class J3DGXPrimitiveType : std::uint8_t
	{
		Quads = 0x80,
		Quads2 = 0x88,
		Triangles = 0x90,
		TriangleStrip = 0x98,
		TriangleFan = 0xA0,
		Lines = 0xA8,
		LineStrip = 0xB0,
		Points = 0xB8
	};

	struct J3DShapePrimitiveRecord
	{
		J3DGXPrimitiveType Type = J3DGXPrimitiveType::Triangles;

		std::uint8_t VertexFormat = 0;
		std::uint16_t VertexCount = 0;

		// Relatifs au début de la display list du matrix group.
		std::uint32_t CommandOffset = 0;
		std::uint32_t VertexDataOffset = 0;
		std::uint32_t VertexDataByteSize = 0;
	};

	struct J3DShapeDisplayListGroup
	{
		std::uint16_t ShapeIndex = 0;
		std::uint16_t GroupIndex = 0;

		std::uint32_t DisplayListSize = 0;
		std::uint32_t EncodedVertexSize = 0;

		std::uint32_t TrailingNoopByteCount = 0;
		std::uint32_t EmbeddedNoopByteCount = 0;

		std::vector<J3DShapePrimitiveRecord> Primitives;
	};

	struct J3DShapeDisplayListData
	{
		std::vector<J3DShapeDisplayListGroup> Groups;
	};

	struct J3DShapeDisplayListParseResult
	{
		J3DShapeDisplayListData Data;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};

	const char* ToString(J3DGXPrimitiveType type);
}