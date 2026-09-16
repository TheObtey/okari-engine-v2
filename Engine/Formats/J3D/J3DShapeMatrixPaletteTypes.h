#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	struct J3DResolvedShapeMatrixGroup
	{
		std::uint16_t ShapeIndex = 0;
		std::uint16_t GroupIndex = 0;

		// slot GX -> index dans J3DDrawMatrixPalette::Matrices
		std::vector<std::uint16_t> DrawMatrixIndices;

		std::size_t LoadedSlotCount = 0;
		std::size_t ReusedSlotCount = 0;
	};

	struct J3DShapeMatrixPalette
	{
		std::vector<J3DResolvedShapeMatrixGroup> Groups;

		std::size_t LoadedSlotCount = 0;
		std::size_t ReusedSlotCount = 0;

		bool HasMatrices = false;
		std::uint16_t MaximumDrawMatrixIndex = 0;
	};

	struct J3DShapeMatrixPaletteResult
	{
		J3DShapeMatrixPalette Palette;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}