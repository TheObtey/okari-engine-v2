#include "Formats/J3D/J3DShapeVertexReferenceDecoder.h"

#include "IO/BigEndianReader.h"

#include <string>
#include <utility>

namespace Okari
{
	namespace
	{
		J3DShapeVertexReferenceDecodeResult Failure(
			const std::string& message
		)
		{
			J3DShapeVertexReferenceDecodeResult result;
			result.Error = message;
			return result;
		}

		const J3DShapeRecord* FindShape(
			const J3DSHP1Data& shp1,
			std::uint16_t shapeIndex
		)
		{
			for (const J3DShapeRecord& shape : shp1.Shapes)
			{
				if (shape.LogicalIndex == shapeIndex)
					return &shape;
			}

			return nullptr;
		}

		const J3DShapeMatrixGroup* FindMatrixGroup(
			const J3DShapeRecord& shape,
			std::uint16_t groupIndex
		)
		{
			for (
				const J3DShapeMatrixGroup& group :
				shape.MatrixGroups
				)
			{
				if (group.LocalIndex == groupIndex)
					return &group;
			}

			return nullptr;
		}

		const J3DResolvedShapeMatrixGroup*
			FindResolvedMatrixGroup(
				const J3DShapeMatrixPalette& palette,
				std::uint16_t shapeIndex,
				std::uint16_t groupIndex
			)
		{
			for (
				const J3DResolvedShapeMatrixGroup& group :
				palette.Groups
				)
			{
				if (
					group.ShapeIndex == shapeIndex &&
					group.GroupIndex == groupIndex
					)
				{
					return &group;
				}
			}

			return nullptr;
		}

		bool IsTextureMatrixIndexAttribute(
			J3DVertexAttribute attribute
		)
		{
			return
				attribute >=
				J3DVertexAttribute::TexCoord0MatrixIndex &&
				attribute <=
				J3DVertexAttribute::TexCoord7MatrixIndex;
		}

		std::size_t GetTextureMatrixChannel(
			J3DVertexAttribute attribute
		)
		{
			return static_cast<std::size_t>(
				static_cast<std::uint32_t>(attribute) -
				static_cast<std::uint32_t>(
					J3DVertexAttribute::TexCoord0MatrixIndex
					)
				);
		}

		std::size_t GetTexCoordChannel(
			J3DVertexAttribute attribute
		)
		{
			return static_cast<std::size_t>(
				static_cast<std::uint32_t>(attribute) -
				static_cast<std::uint32_t>(
					J3DVertexAttribute::TexCoord0
					)
				);
		}

		bool ReadIndex(
			BigEndianReader& reader,
			J3DVertexInputType inputType,
			std::uint32_t& index
		)
		{
			switch (inputType)
			{
			case J3DVertexInputType::Index8:
				index = reader.ReadU8();
				return true;

			case J3DVertexInputType::Index16:
				index = reader.ReadU16();
				return true;

			default:
				return false;
			}
		}

		std::uint32_t GetNormalIndexCount(
			const J3DVertexFormatDescriptor& format
		)
		{
			// NBT3 contient trois indices indépendants.
			return format.ComponentCount == 2
				? 3
				: 1;
		}
	}

	J3DShapeVertexReferenceDecodeResult
		J3DShapeVertexReferenceDecoder::Decode(
			const J3DDocument& document,
			const J3DSectionInfo& shp1Section,
			const J3DSHP1Data& shp1,
			const J3DVTX1Data& vtx1,
			const J3DShapeDisplayListData& displayLists,
			const J3DShapeMatrixPalette& matrixPalette
		)
	{
		J3DShapeVertexReferenceData output;
		BigEndianReader reader(document.Data);

		for (
			const J3DShapeDisplayListGroup& displayGroup :
			displayLists.Groups
			)
		{
			const J3DShapeRecord* shape =
				FindShape(
					shp1,
					displayGroup.ShapeIndex
				);

			if (shape == nullptr)
			{
				return Failure(
					"Vertex decoder references unknown SHP1 shape"
				);
			}

			const J3DShapeMatrixGroup* matrixGroup =
				FindMatrixGroup(
					*shape,
					displayGroup.GroupIndex
				);

			if (matrixGroup == nullptr)
			{
				return Failure(
					"Vertex decoder references unknown matrix group"
				);
			}

			const J3DResolvedShapeMatrixGroup*
				resolvedMatrixGroup =
				FindResolvedMatrixGroup(
					matrixPalette,
					displayGroup.ShapeIndex,
					displayGroup.GroupIndex
				);

			if (resolvedMatrixGroup == nullptr)
			{
				return Failure(
					"Vertex decoder has no resolved matrix palette"
				);
			}

			if (
				resolvedMatrixGroup->
				DrawMatrixIndices.empty()
				)
			{
				return Failure(
					"Resolved matrix group contains no matrices"
				);
			}

			const std::uint64_t groupStart =
				static_cast<std::uint64_t>(
					shp1Section.Offset
					) +
				shp1.DisplayListDataOffset +
				matrixGroup->DisplayListOffset;

			J3DDecodedShapeGroup decodedGroup;

			decodedGroup.ShapeIndex =
				displayGroup.ShapeIndex;

			decodedGroup.GroupIndex =
				displayGroup.GroupIndex;

			for (
				const J3DShapePrimitiveRecord& primitive :
				displayGroup.Primitives
				)
			{
				const std::uint64_t primitiveStart =
					groupStart +
					primitive.VertexDataOffset;

				const std::uint64_t primitiveEnd =
					primitiveStart +
					primitive.VertexDataByteSize;

				reader.Seek(
					static_cast<std::size_t>(
						primitiveStart
						)
				);

				J3DDecodedShapePrimitive decodedPrimitive;

				decodedPrimitive.Type =
					primitive.Type;

				decodedPrimitive.VertexFormat =
					primitive.VertexFormat;

				decodedPrimitive.SourceCommandOffset =
					primitive.CommandOffset;

				decodedPrimitive.Vertices.reserve(
					primitive.VertexCount
				);

				for (
					std::uint16_t vertexNumber = 0;
					vertexNumber < primitive.VertexCount;
					++vertexNumber
					)
				{
					J3DShapeVertexReference vertex;

					// Single-matrix shapes n'ont pas besoin
					// de PNMTXIDX : le slot 0 est implicite.
					if (
						shape->MatrixType !=
						J3DShapeMatrixType::MultiMatrix
						)
					{
						vertex.PositionMatrixSlot = 0;

						vertex.DrawMatrixIndex =
							resolvedMatrixGroup->
							DrawMatrixIndices[0];
					}

					for (
						const J3DShapeVertexDescriptor& descriptor :
						shape->VertexDescriptors
						)
					{
						if (
							descriptor.InputType ==
							J3DVertexInputType::None
							)
						{
							continue;
						}

						if (
							descriptor.Attribute ==
							J3DVertexAttribute::
							PositionMatrixIndex
							)
						{
							if (
								descriptor.InputType !=
								J3DVertexInputType::Direct
								)
							{
								return Failure(
									"PNMTXIDX is not DIRECT"
								);
							}

							const std::uint8_t rawIndex =
								reader.ReadU8();

							if ((rawIndex % 3) != 0)
							{
								return Failure(
									"PNMTXIDX is not aligned "
									"to a GX matrix slot"
								);
							}

							const std::uint8_t slot =
								rawIndex / 3;

							if (
								slot >=
								resolvedMatrixGroup->
								DrawMatrixIndices.size()
								)
							{
								return Failure(
									"PNMTXIDX references a "
									"matrix slot outside the "
									"resolved SHP1 palette"
								);
							}

							vertex.RawPositionMatrixIndex =
								rawIndex;

							vertex.PositionMatrixSlot =
								slot;

							vertex.DrawMatrixIndex =
								resolvedMatrixGroup->
								DrawMatrixIndices[slot];

							continue;
						}

						if (
							IsTextureMatrixIndexAttribute(
								descriptor.Attribute
							)
							)
						{
							if (
								descriptor.InputType !=
								J3DVertexInputType::Direct
								)
							{
								return Failure(
									"Texture matrix index "
									"is not DIRECT"
								);
							}

							const std::uint8_t rawIndex =
								reader.ReadU8();

							vertex.RawTextureMatrixIndices[
								GetTextureMatrixChannel(
									descriptor.Attribute
								)
							] = rawIndex;

							continue;
						}

						if (
							descriptor.InputType ==
							J3DVertexInputType::Direct
							)
						{
							return Failure(
								"Direct non-matrix GX vertex "
								"attributes are not supported yet"
							);
						}

						const J3DVertexFormatDescriptor* format =
							vtx1.FindFormat(
								descriptor.Attribute
							);

						if (format == nullptr)
						{
							return Failure(
								"Vertex descriptor references "
								"a missing VTX1 format"
							);
						}

						switch (descriptor.Attribute)
						{
						case J3DVertexAttribute::Position:
						{
							std::uint32_t index;

							if (!ReadIndex(
								reader,
								descriptor.InputType,
								index
							))
							{
								return Failure(
									"Unable to read POS index"
								);
							}

							vertex.PositionIndex = index;
							break;
						}

						case J3DVertexAttribute::Normal:
						case J3DVertexAttribute::NBT:
						{
							const std::uint32_t indexCount =
								GetNormalIndexCount(
									*format
								);

							vertex.NormalIndexCount =
								static_cast<std::uint8_t>(
									indexCount
									);

							vertex.UsesNBT =
								descriptor.Attribute ==
								J3DVertexAttribute::NBT ||
								format->ComponentCount != 0;

							for (
								std::uint32_t indexNumber = 0;
								indexNumber < indexCount;
								++indexNumber
								)
							{
								std::uint32_t index;

								if (!ReadIndex(
									reader,
									descriptor.InputType,
									index
								))
								{
									return Failure(
										"Unable to read "
										"NRM/NBT index"
									);
								}

								vertex.NormalIndices[
									indexNumber
								] = index;
							}

							break;
						}

						case J3DVertexAttribute::Color0:
						case J3DVertexAttribute::Color1:
						{
							const std::size_t channel =
								descriptor.Attribute ==
								J3DVertexAttribute::Color0
								? 0
								: 1;

							std::uint32_t index;

							if (!ReadIndex(
								reader,
								descriptor.InputType,
								index
							))
							{
								return Failure(
									"Unable to read color index"
								);
							}

							vertex.ColorIndices[channel] =
								index;

							break;
						}

						default:
						{
							if (
								descriptor.Attribute >=
								J3DVertexAttribute::TexCoord0 &&
								descriptor.Attribute <=
								J3DVertexAttribute::TexCoord7
								)
							{
								std::uint32_t index;

								if (!ReadIndex(
									reader,
									descriptor.InputType,
									index
								))
								{
									return Failure(
										"Unable to read TEX index"
									);
								}

								vertex.TexCoordIndices[
									GetTexCoordChannel(
										descriptor.Attribute
									)
								] = index;

								break;
							}

							return Failure(
								"Unsupported indexed GX attribute"
							);
						}
						}
					}

					if (!vertex.PositionIndex.has_value())
					{
						return Failure(
							"Decoded GX vertex has no POS index"
						);
					}

					decodedPrimitive.Vertices.push_back(
						std::move(vertex)
					);
				}

				if (reader.Tell() != primitiveEnd)
				{
					return Failure(
						"Decoded GX primitive did not consume "
						"its complete payload"
					);
				}

				decodedGroup.Primitives.push_back(
					std::move(decodedPrimitive)
				);
			}

			output.Groups.push_back(
				std::move(decodedGroup)
			);
		}

		J3DShapeVertexReferenceDecodeResult result;
		result.Data = std::move(output);

		return result;
	}
}