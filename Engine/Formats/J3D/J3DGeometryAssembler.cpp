#include "Formats/J3D/J3DGeometryAssembler.h"

#include <string>
#include <utility>

namespace Okari
{
	namespace
	{
		J3DGeometryAssemblyResult Failure(
			const std::string& message
		)
		{
			J3DGeometryAssemblyResult result;
			result.Error = message;
			return result;
		}

		std::string VertexContext(
			std::uint16_t shapeIndex,
			std::uint16_t groupIndex,
			std::size_t vertexIndex
		)
		{
			return
				"shape " +
				std::to_string(shapeIndex) +
				", group " +
				std::to_string(groupIndex) +
				", vertex " +
				std::to_string(vertexIndex);
		}
	}

	J3DGeometryAssemblyResult
		J3DGeometryAssembler::Assemble(
			const J3DShapeVertexReferenceData& references,
			const J3DDecodedVertexData& vertexData,
			const J3DDrawMatrixPalette& drawPalette
		)
	{
		if (vertexData.Positions.empty())
		{
			return Failure(
				"Cannot assemble J3D geometry without positions"
			);
		}

		if (drawPalette.Matrices.empty())
		{
			return Failure(
				"Cannot assemble J3D geometry without draw matrices"
			);
		}

		J3DAssembledGeometry geometry;

		geometry.Groups.reserve(
			references.Groups.size()
		);

		for (
			const J3DDecodedShapeGroup& sourceGroup :
			references.Groups
			)
		{
			J3DAssembledShapeGroup assembledGroup;

			assembledGroup.ShapeIndex =
				sourceGroup.ShapeIndex;

			assembledGroup.GroupIndex =
				sourceGroup.GroupIndex;

			assembledGroup.Primitives.reserve(
				sourceGroup.Primitives.size()
			);

			for (
				const J3DDecodedShapePrimitive& sourcePrimitive :
				sourceGroup.Primitives
				)
			{
				J3DAssembledPrimitive assembledPrimitive;

				assembledPrimitive.Type =
					sourcePrimitive.Type;

				assembledPrimitive.VertexFormat =
					sourcePrimitive.VertexFormat;

				assembledPrimitive.SourceCommandOffset =
					sourcePrimitive.SourceCommandOffset;

				assembledPrimitive.Vertices.reserve(
					sourcePrimitive.Vertices.size()
				);

				for (
					std::size_t vertexIndex = 0;
					vertexIndex <
					sourcePrimitive.Vertices.size();
					++vertexIndex
					)
				{
					const J3DShapeVertexReference& reference =
						sourcePrimitive.Vertices[
							vertexIndex
						];

					const std::string context =
						VertexContext(
							sourceGroup.ShapeIndex,
							sourceGroup.GroupIndex,
							vertexIndex
						);

					if (!reference.PositionIndex.has_value())
					{
						return Failure(
							context +
							" has no position index"
						);
					}

					if (
						*reference.PositionIndex >=
						vertexData.Positions.size()
						)
					{
						return Failure(
							context +
							" references position " +
							std::to_string(
								*reference.PositionIndex
							) +
							" outside decoded VTX1 positions"
						);
					}

					if (
						reference.DrawMatrixIndex >=
						drawPalette.Matrices.size()
						)
					{
						return Failure(
							context +
							" references draw matrix " +
							std::to_string(
								reference.DrawMatrixIndex
							) +
							" outside the effective DRW1 palette"
						);
					}

					if (reference.UsesNBT)
					{
						return Failure(
							context +
							" uses NBT, which is not yet "
							"supported by the CPU geometry assembler"
						);
					}

					if (reference.NormalIndexCount > 1)
					{
						return Failure(
							context +
							" contains multiple normal indices "
							"without NBT support"
						);
					}

					J3DAssembledVertex vertex;

					vertex.Position =
						vertexData.Positions[
							*reference.PositionIndex
						];

					vertex.DrawMatrixIndex =
						reference.DrawMatrixIndex;

					vertex.PositionMatrixSlot =
						reference.PositionMatrixSlot;

					vertex.RawPositionMatrixIndex =
						reference.RawPositionMatrixIndex;

					vertex.RawTextureMatrixIndices =
						reference.RawTextureMatrixIndices;

					if (
						reference.NormalIndices[0].
						has_value()
						)
					{
						const std::uint32_t normalIndex =
							*reference.NormalIndices[0];

						if (
							normalIndex >=
							vertexData.Normals.size()
							)
						{
							return Failure(
								context +
								" references normal " +
								std::to_string(normalIndex) +
								" outside decoded VTX1 normals"
							);
						}

						vertex.Normal =
							vertexData.Normals[
								normalIndex
							];
					}

					for (
						std::size_t channel = 0;
						channel <
						reference.ColorIndices.size();
						++channel
						)
					{
						if (
							!reference.ColorIndices[channel].
							has_value()
							)
						{
							continue;
						}

						const std::uint32_t colorIndex =
							*reference.ColorIndices[channel];

						if (
							colorIndex >=
							vertexData.Colors[channel].size()
							)
						{
							return Failure(
								context +
								" references CLR" +
								std::to_string(channel) +
								" index " +
								std::to_string(colorIndex) +
								" outside decoded VTX1 colors"
							);
						}

						vertex.Colors[channel] =
							vertexData.Colors[channel][
								colorIndex
							];
					}

					for (
						std::size_t channel = 0;
						channel <
						reference.TexCoordIndices.size();
						++channel
						)
					{
						if (
							!reference.TexCoordIndices[channel].
							has_value()
							)
						{
							continue;
						}

						const std::uint32_t texCoordIndex =
							*reference.TexCoordIndices[channel];

						if (
							texCoordIndex >=
							vertexData.TexCoords[channel].
							size()
							)
						{
							return Failure(
								context +
								" references TEX" +
								std::to_string(channel) +
								" index " +
								std::to_string(texCoordIndex) +
								" outside decoded VTX1 texcoords"
							);
						}

						vertex.TexCoords[channel] =
							vertexData.TexCoords[channel][
								texCoordIndex
							];
					}

					assembledPrimitive.Vertices.push_back(
						std::move(vertex)
					);
				}

				assembledGroup.Primitives.push_back(
					std::move(assembledPrimitive)
				);
			}

			geometry.Groups.push_back(
				std::move(assembledGroup)
			);
		}

		J3DGeometryAssemblyResult result;
		result.Geometry = std::move(geometry);

		return result;
	}
}