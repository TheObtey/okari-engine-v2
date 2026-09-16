#pragma once

#include "Formats/J3D/J3DShapeDisplayListTypes.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Okari
{
	struct J3DShapeVertexReference
	{
		std::optional<std::uint32_t> PositionIndex;

		// NRM simple = NormalIndexCount 1.
		// NBT3 peut utiliser jusqu'à trois indices.
		std::array<std::optional<std::uint32_t>, 3>
			NormalIndices;

		std::uint8_t NormalIndexCount = 0;
		bool UsesNBT = false;

		std::array<std::optional<std::uint32_t>, 2>
			ColorIndices;

		std::array<std::optional<std::uint32_t>, 8>
			TexCoordIndices;

		// Présent uniquement si PNMTXIDX existe dans la VCD.
		std::optional<std::uint8_t>
			RawPositionMatrixIndex;

		// Slot GX logique, donc PNMTXIDX / 3.
		// Pour une SingleMatrix, il vaut implicitement 0.
		std::uint8_t PositionMatrixSlot = 0;

		// Index final dans la palette produite par
		// J3DDrawMatrixEvaluator.
		std::uint16_t DrawMatrixIndex = 0;

		std::array<std::optional<std::uint8_t>, 8>
			RawTextureMatrixIndices;
	};

	struct J3DDecodedShapePrimitive
	{
		J3DGXPrimitiveType Type =
			J3DGXPrimitiveType::Triangles;

		std::uint8_t VertexFormat = 0;
		std::uint32_t SourceCommandOffset = 0;

		std::vector<J3DShapeVertexReference> Vertices;
	};

	struct J3DDecodedShapeGroup
	{
		std::uint16_t ShapeIndex = 0;
		std::uint16_t GroupIndex = 0;

		std::vector<J3DDecodedShapePrimitive> Primitives;
	};

	struct J3DShapeVertexReferenceData
	{
		std::vector<J3DDecodedShapeGroup> Groups;
	};

	struct J3DShapeVertexReferenceDecodeResult
	{
		J3DShapeVertexReferenceData Data;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}