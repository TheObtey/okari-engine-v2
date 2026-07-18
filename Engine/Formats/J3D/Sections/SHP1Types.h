#pragma once

#include "Formats/J3D/J3DGeometryTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	enum class J3DShapeMatrixType : std::uint8_t
	{
		SingleMatrix = 0,
		Billboard = 1,
		YBillboard = 2,
		MultiMatrix = 3
	};

	struct J3DShapeRecord
	{
		// Index used by INF1 and by the rest of the model
		std::uint16_t LogicalIndex = 0;

		// Physical record selected by remap table
		std::uint16_t DataIndex = 0;

		J3DShapeMatrixType MatrixType = J3DShapeMatrixType::SingleMatrix;

		std::uint8_t Padding0x01 = 0;

		std::uint16_t MatrixGroupCount = 0;

		std::uint16_t VertexDescriptorListOffset = 0;

		std::uint16_t MatrixInitDataIndex = 0;
		std::uint16_t DrawInitDataIndex = 0;
		
		std::uint16_t Padding0x0A = 0;

		float BoundingSphereRadius = 0.0f;
		J3DBoundingBox Bounds;
	};

	struct J3DSHP1Data
	{
		std::uint16_t ShapeCount = 0;
		std::uint16_t Padding = 0;

		std::uint32_t ShapeInitDataOffset = 0;
		std::uint32_t RemapTableOffset = 0;
		std::uint32_t NameTableOffset = 0;
		std::uint32_t VertexDescriptorTableOffset = 0;
		std::uint32_t MatrixTableOffset = 0;
		std::uint32_t DisplayListDataOffset = 0;
		std::uint32_t MatrixInitDataOffset = 0;
		std::uint32_t DrawInitDataOffset = 0;

		std::vector<std::uint16_t> RemapTable;
		std::vector<J3DShapeRecord> Shapes;
	};

	struct J3DSHP1ParseResult
	{
		J3DSHP1Data Data;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}