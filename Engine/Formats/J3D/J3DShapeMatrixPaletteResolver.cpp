#include "Formats/J3D/J3DShapeMatrixPaletteResolver.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Okari
{
	namespace
	{
		constexpr std::uint16_t ReusePreviousMatrix =
			0xFFFF;

		J3DShapeMatrixPaletteResult Failure(
			const std::string& message
		)
		{
			J3DShapeMatrixPaletteResult result;
			result.Error = message;
			return result;
		}

		bool ValidateDrawMatrixIndex(
			std::uint16_t index,
			const J3DDrawMatrixPalette& drawPalette
		)
		{
			return index < drawPalette.Matrices.size();
		}
	}

	J3DShapeMatrixPaletteResult
		J3DShapeMatrixPaletteResolver::Resolve(
			const J3DSHP1Data& shp1,
			const J3DDrawMatrixPalette& drawPalette
		)
	{
		if (drawPalette.Matrices.empty())
		{
			return Failure(
				"Cannot resolve SHP1 matrix palettes "
				"without DRW1 draw matrices"
			);
		}

		J3DShapeMatrixPalette output;

		for (
			const J3DShapeRecord& shape :
			shp1.Shapes
			)
		{
			// GX matrix state carried between matrix groups
			// belonging to the same shape.
			std::vector<
				std::optional<std::uint16_t>
			> slotState;

			for (
				const J3DShapeMatrixGroup& group :
				shape.MatrixGroups
				)
			{
				J3DResolvedShapeMatrixGroup resolved;

				resolved.ShapeIndex =
					shape.LogicalIndex;

				resolved.GroupIndex =
					group.LocalIndex;

				switch (shape.MatrixType)
				{
				case J3DShapeMatrixType::SingleMatrix:
				case J3DShapeMatrixType::Billboard:
				case J3DShapeMatrixType::YBillboard:
				{
					if (
						group.UseMatrixIndex ==
						ReusePreviousMatrix
						)
					{
						return Failure(
							"SHP1 single-matrix shape " +
							std::to_string(
								shape.LogicalIndex
							) +
							" uses 0xFFFF as UseMatrixIndex"
						);
					}

					if (!ValidateDrawMatrixIndex(
						group.UseMatrixIndex,
						drawPalette
					))
					{
						return Failure(
							"SHP1 shape " +
							std::to_string(
								shape.LogicalIndex
							) +
							" references invalid draw matrix " +
							std::to_string(
								group.UseMatrixIndex
							)
						);
					}

					resolved.DrawMatrixIndices.push_back(
						group.UseMatrixIndex
					);

					resolved.LoadedSlotCount = 1;
					++output.LoadedSlotCount;

					break;
				}

				case J3DShapeMatrixType::MultiMatrix:
				{
					if (
						group.RawMatrixTable.size() !=
						group.UseMatrixCount
						)
					{
						return Failure(
							"SHP1 matrix-table size mismatch "
							"for shape " +
							std::to_string(
								shape.LogicalIndex
							) +
							", group " +
							std::to_string(
								group.LocalIndex
							)
						);
					}

					if (
						slotState.size() <
						group.UseMatrixCount
						)
					{
						slotState.resize(
							group.UseMatrixCount
						);
					}

					resolved.DrawMatrixIndices.reserve(
						group.UseMatrixCount
					);

					for (
						std::size_t slot = 0;
						slot <
						group.RawMatrixTable.size();
						++slot
						)
					{
						const std::uint16_t rawIndex =
							group.RawMatrixTable[slot];

						if (
							rawIndex ==
							ReusePreviousMatrix
							)
						{
							if (!slotState[slot].has_value())
							{
								return Failure(
									"SHP1 shape " +
									std::to_string(
										shape.LogicalIndex
									) +
									", group " +
									std::to_string(
										group.LocalIndex
									) +
									" reuses unresolved matrix slot " +
									std::to_string(slot)
								);
							}

							++resolved.ReusedSlotCount;
							++output.ReusedSlotCount;
						}
						else
						{
							if (!ValidateDrawMatrixIndex(
								rawIndex,
								drawPalette
							))
							{
								return Failure(
									"SHP1 shape " +
									std::to_string(
										shape.LogicalIndex
									) +
									", group " +
									std::to_string(
										group.LocalIndex
									) +
									" references invalid draw matrix " +
									std::to_string(rawIndex)
								);
							}

							slotState[slot] =
								rawIndex;

							++resolved.LoadedSlotCount;
							++output.LoadedSlotCount;
						}

						const std::uint16_t effectiveIndex =
							*slotState[slot];

						resolved.DrawMatrixIndices.push_back(
							effectiveIndex
						);

						if (
							!output.HasMatrices ||
							effectiveIndex >
							output.MaximumDrawMatrixIndex
							)
						{
							output.MaximumDrawMatrixIndex =
								effectiveIndex;

							output.HasMatrices = true;
						}
					}

					break;
				}
				}

				// Single-matrix paths also contribute to the max.
				for (
					const std::uint16_t matrixIndex :
				resolved.DrawMatrixIndices
					)
				{
					if (
						!output.HasMatrices ||
						matrixIndex >
						output.MaximumDrawMatrixIndex
						)
					{
						output.MaximumDrawMatrixIndex =
							matrixIndex;

						output.HasMatrices = true;
					}
				}

				output.Groups.push_back(
					std::move(resolved)
				);
			}
		}

		J3DShapeMatrixPaletteResult result;
		result.Palette = std::move(output);

		return result;
	}
}