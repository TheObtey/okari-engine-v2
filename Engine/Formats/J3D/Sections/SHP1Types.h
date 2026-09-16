#pragma once

#include "Formats/J3D/J3DGeometryTypes.h"
#include "Formats/J3D/J3DGXVertexTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	struct J3DShapeVertexDescriptor
	{
		J3DVertexAttribute Attribute = J3DVertexAttribute::Null;

		J3DVertexInputType InputType = J3DVertexInputType::None;
	};

	enum class J3DShapeMatrixType : std::uint8_t
	{
		SingleMatrix = 0,
		Billboard = 1,
		YBillboard = 2,
		MultiMatrix = 3
	};

	struct J3DShapeMatrixGroup
	{
		std::uint16_t LocalIndex = 0;

		std::uint32_t MatrixInitDataIndex = 0;
		std::uint32_t DrawInitDataIndex = 0;

		std::uint16_t UseMatrixIndex = 0;
		std::uint16_t UseMatrixCount = 0;
		std::uint32_t FirstUseMatrixIndex = 0;

		std::vector<std::uint16_t> RawMatrixTable;

		std::uint32_t DisplayListSize = 0;
		std::uint32_t DisplayListOffset = 0;
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

		std::vector<J3DShapeVertexDescriptor> VertexDescriptors;

		std::uint32_t VertexDescriptorTerminatorType = 0;

		std::vector<J3DShapeMatrixGroup> MatrixGroups;

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