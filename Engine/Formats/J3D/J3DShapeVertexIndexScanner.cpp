#include "Formats/J3D/J3DShapeVertexIndexScanner.h"

#include "IO/BigEndianReader.h"

#include <string>

namespace Okari
{
	namespace
	{
		J3DShapeVertexIndexScanResult Failure(
			const std::string& message
		)
		{
			J3DShapeVertexIndexScanResult result;
			result.Error = message;
			return result;
		}

		const J3DShapeRecord* FindShape(
			const J3DSHP1Data& shp1,
			std::uint16_t logicalIndex
		)
		{
			for (
				const J3DShapeRecord& shape :
				shp1.Shapes
				)
			{
				if (shape.LogicalIndex == logicalIndex)
					return &shape;
			}

			return nullptr;
		}

		const J3DShapeMatrixGroup* FindMatrixGroup(
			const J3DShapeRecord& shape,
			std::uint16_t localIndex
		)
		{
			for (
				const J3DShapeMatrixGroup& group :
				shape.MatrixGroups
				)
			{
				if (group.LocalIndex == localIndex)
					return &group;
			}

			return nullptr;
		}

		bool IsTexCoordAttribute(
			J3DVertexAttribute attribute
		)
		{
			return
				attribute >=
				J3DVertexAttribute::TexCoord0 &&
				attribute <=
				J3DVertexAttribute::TexCoord7;
		}

		std::size_t GetTexCoordChannel(
			J3DVertexAttribute attribute
		)
		{
			return
				static_cast<std::size_t>(
					static_cast<std::uint32_t>(attribute) -
					static_cast<std::uint32_t>(
						J3DVertexAttribute::TexCoord0
						)
					);
		}

		J3DDirectMatrixIndexUsage* ResolveMatrixIndexUsage(
			J3DShapeVertexIndexUsage& usage,
			J3DVertexAttribute attribute
		)
		{
			if (
				attribute ==
				J3DVertexAttribute::PositionMatrixIndex
				)
			{
				return &usage.MatrixIndices.Position;
			}

			if (
				attribute >=
				J3DVertexAttribute::TexCoord0MatrixIndex &&
				attribute <=
				J3DVertexAttribute::TexCoord7MatrixIndex
				)
			{
				const std::size_t channel =
					static_cast<std::size_t>(
						static_cast<std::uint32_t>(attribute) -
						static_cast<std::uint32_t>(
							J3DVertexAttribute::TexCoord0MatrixIndex
							)
						);

				return &usage.MatrixIndices.TexCoords[channel];
			}

			return nullptr;
		}

		J3DVertexIndexRange* ResolveIndexRange(
			J3DShapeVertexIndexUsage& usage,
			J3DVertexAttribute attribute,
			const J3DVertexFormatDescriptor* format
		)
		{
			switch (attribute)
			{
			case J3DVertexAttribute::Position:
				return &usage.Position;

			case J3DVertexAttribute::Normal:
				if (
					format != nullptr &&
					format->ComponentCount != 0
					)
				{
					return &usage.NBT;
				}

				return &usage.Normal;

			case J3DVertexAttribute::NBT:
				return &usage.NBT;

			case J3DVertexAttribute::Color0:
				return &usage.Colors[0];

			case J3DVertexAttribute::Color1:
				return &usage.Colors[1];

			default:
				if (IsTexCoordAttribute(attribute))
				{
					return &usage.TexCoords[
						GetTexCoordChannel(attribute)
					];
				}

				return nullptr;
			}
		}

		std::uint32_t GetIndexComponentCount(
			J3DVertexAttribute attribute,
			const J3DVertexFormatDescriptor* format
		)
		{
			if (
				format != nullptr &&
				(
					attribute ==
					J3DVertexAttribute::Normal ||
					attribute ==
					J3DVertexAttribute::NBT
					) &&
				format->ComponentCount == 2
				)
			{
				// GX NBT3 :
				// normal, binormal et tangent possèdent
				// chacun leur propre index.
				return 3;
			}

			return 1;
		}
	}

	J3DShapeVertexIndexScanResult
		J3DShapeVertexIndexScanner::Scan(
			const J3DDocument& document,
			const J3DSectionInfo& shp1Section,
			const J3DSHP1Data& shp1,
			const J3DVTX1Data& vtx1,
			const J3DShapeDisplayListData& displayLists
		)
	{
		J3DShapeVertexIndexUsage usage;
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
					"Display-list group references unknown SHP1 shape"
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
					"Display-list group references unknown SHP1 matrix group"
				);
			}

			const std::uint64_t groupStart =
				static_cast<std::uint64_t>(
					shp1Section.Offset
					) +
				shp1.DisplayListDataOffset +
				matrixGroup->DisplayListOffset;

			for (
				const J3DShapePrimitiveRecord& primitive :
				displayGroup.Primitives
				)
			{
				const std::uint64_t primitiveDataStart =
					groupStart +
					primitive.VertexDataOffset;

				const std::uint64_t primitiveDataEnd =
					primitiveDataStart +
					primitive.VertexDataByteSize;

				if (
					primitiveDataEnd >
					document.Data.size()
					)
				{
					return Failure(
						"GX primitive payload exceeds file bounds"
					);
				}

				reader.Seek(
					static_cast<std::size_t>(
						primitiveDataStart
						)
				);

				for (
					std::uint16_t vertexIndex = 0;
					vertexIndex < primitive.VertexCount;
					++vertexIndex
					)
				{
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
							IsMatrixIndexAttribute(
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
									"GX matrix index is not DIRECT"
								);
							}

							J3DDirectMatrixIndexUsage* matrixUsage =
								ResolveMatrixIndexUsage(
									usage,
									descriptor.Attribute
								);

							if (matrixUsage == nullptr)
							{
								return Failure(
									"Unsupported GX matrix-index attribute"
								);
							}

							const std::uint8_t rawMatrixIndex =
								reader.ReadU8();

							matrixUsage->Observe(
								rawMatrixIndex
							);

							continue;
						}

						const J3DVertexFormatDescriptor* format =
							vtx1.FindFormat(
								descriptor.Attribute
							);

						if (format == nullptr)
						{
							return Failure(
								"GX vertex descriptor references missing VTX1 format"
							);
						}

						if (
							descriptor.InputType ==
							J3DVertexInputType::Direct
							)
						{
							// La donnée vit directement dans la
							// display list et ne référence donc pas
							// un tableau VTX1.
							reader.Skip(
								format->ElementStride
							);

							continue;
						}

						J3DVertexIndexRange* range =
							ResolveIndexRange(
								usage,
								descriptor.Attribute,
								format
							);

						if (range == nullptr)
						{
							return Failure(
								"Unsupported indexed GX vertex attribute"
							);
						}

						const std::uint32_t indexComponentCount =
							GetIndexComponentCount(
								descriptor.Attribute,
								format
							);

						for (
							std::uint32_t componentIndex = 0;
							componentIndex <
							indexComponentCount;
							++componentIndex
							)
						{
							std::uint32_t index = 0;

							switch (descriptor.InputType)
							{
							case J3DVertexInputType::Index8:
								index =
									reader.ReadU8();
								break;

							case J3DVertexInputType::Index16:
								index =
									reader.ReadU16();
								break;

							default:
								return Failure(
									"Unsupported indexed GX input type"
								);
							}

							range->Observe(index);
						}
					}
				}

				if (
					reader.Tell() !=
					primitiveDataEnd
					)
				{
					return Failure(
						"GX primitive index scan did not consume "
						"the expected payload size"
					);
				}
			}
		}

		J3DShapeVertexIndexScanResult result;
		result.Usage = usage;

		return result;
	}
}