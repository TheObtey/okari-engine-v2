#include "Formats/J3D/J3DShapeDisplayListParser.h"

#include "IO/BigEndianReader.h"

#include <limits>
#include <utility>

namespace Okari
{
	namespace
	{
		J3DShapeDisplayListParseResult Failure(
			const std::string& message
		)
		{
			J3DShapeDisplayListParseResult result;
			result.Error = message;
			return result;
		}

		bool DecodePrimitiveType(
			std::uint8_t rawValue,
			J3DGXPrimitiveType& type
		)
		{
			switch (rawValue)
			{
			case 0x80:
				type = J3DGXPrimitiveType::Quads;
				return true;

			case 0x88:
				type = J3DGXPrimitiveType::Quads2;
				return true;

			case 0x90:
				type = J3DGXPrimitiveType::Triangles;
				return true;

			case 0x98:
				type = J3DGXPrimitiveType::TriangleStrip;
				return true;

			case 0xA0:
				type = J3DGXPrimitiveType::TriangleFan;
				return true;

			case 0xA8:
				type = J3DGXPrimitiveType::Lines;
				return true;

			case 0xB0:
				type = J3DGXPrimitiveType::LineStrip;
				return true;

			case 0xB8:
				type = J3DGXPrimitiveType::Points;
				return true;

			default:
				return false;
			}
		}

		bool GetAttributeEncodedByteSize(
			const J3DShapeVertexDescriptor& descriptor,
			const J3DVTX1Data& vtx1,
			std::uint32_t& byteSize,
			std::string& error
		)
		{
			byteSize = 0;

			if (
				descriptor.InputType ==
				J3DVertexInputType::None
				)
			{
				return true;
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
					error =
						"GX matrix-index attribute must be DIRECT";

					return false;
				}

				byteSize = 1;
				return true;
			}

			if (
				!IsVertexArrayAttribute(
					descriptor.Attribute
				)
				)
			{
				error =
					"Unsupported GX vertex attribute";

				return false;
			}

			const J3DVertexFormatDescriptor* format =
				vtx1.FindFormat(
					descriptor.Attribute
				);

			if (format == nullptr)
			{
				error =
					"GX descriptor references a missing VTX1 format";

				return false;
			}

			std::uint32_t indexComponentCount = 1;

			// GX NBT3 stores three separate indices.
			if (
				(
					descriptor.Attribute ==
					J3DVertexAttribute::Normal ||
					descriptor.Attribute ==
					J3DVertexAttribute::NBT
					) &&
				format->ComponentCount == 2
				)
			{
				indexComponentCount = 3;
			}

			switch (descriptor.InputType)
			{
			case J3DVertexInputType::Direct:
				byteSize = format->ElementStride;
				return true;

			case J3DVertexInputType::Index8:
				byteSize = indexComponentCount;
				return true;

			case J3DVertexInputType::Index16:
				byteSize =
					2 * indexComponentCount;

				return true;

			default:
				error =
					"Unsupported GX vertex input type";

				return false;
			}
		}

		bool ComputeEncodedVertexSize(
			const J3DShapeRecord& shape,
			const J3DVTX1Data& vtx1,
			std::uint32_t& vertexSize,
			std::string& error
		)
		{
			std::uint64_t totalSize = 0;

			for (
				const J3DShapeVertexDescriptor& descriptor :
				shape.VertexDescriptors
				)
			{
				std::uint32_t attributeSize = 0;

				if (!GetAttributeEncodedByteSize(
					descriptor,
					vtx1,
					attributeSize,
					error
				))
				{
					return false;
				}

				totalSize += attributeSize;
			}

			if (
				totalSize >
				std::numeric_limits<std::uint32_t>::max()
				)
			{
				error =
					"GX encoded vertex size overflows uint32";

				return false;
			}

			vertexSize =
				static_cast<std::uint32_t>(
					totalSize
					);

			return true;
		}
	}

	const char* ToString(
		J3DGXPrimitiveType type
	)
	{
		switch (type)
		{
		case J3DGXPrimitiveType::Quads:
			return "QUADS";

		case J3DGXPrimitiveType::Quads2:
			return "QUADS_2";

		case J3DGXPrimitiveType::Triangles:
			return "TRIANGLES";

		case J3DGXPrimitiveType::TriangleStrip:
			return "TRIANGLE_STRIP";

		case J3DGXPrimitiveType::TriangleFan:
			return "TRIANGLE_FAN";

		case J3DGXPrimitiveType::Lines:
			return "LINES";

		case J3DGXPrimitiveType::LineStrip:
			return "LINE_STRIP";

		case J3DGXPrimitiveType::Points:
			return "POINTS";

		default:
			return "UNKNOWN";
		}
	}

	J3DShapeDisplayListParseResult
		J3DShapeDisplayListParser::Parse(
			const J3DDocument& document,
			const J3DSectionInfo& shp1Section,
			const J3DSHP1Data& shp1,
			const J3DVTX1Data& vtx1
		)
	{
		J3DShapeDisplayListData output;

		BigEndianReader reader(document.Data);

		for (
			const J3DShapeRecord& shape :
			shp1.Shapes
			)
		{
			std::uint32_t encodedVertexSize = 0;
			std::string vertexSizeError;

			if (!ComputeEncodedVertexSize(
				shape,
				vtx1,
				encodedVertexSize,
				vertexSizeError
			))
			{
				return Failure(
					"SHP1 shape " +
					std::to_string(shape.LogicalIndex) +
					": " +
					vertexSizeError
				);
			}

			if (encodedVertexSize == 0)
			{
				return Failure(
					"SHP1 shape " +
					std::to_string(shape.LogicalIndex) +
					" has an empty encoded vertex"
				);
			}

			for (
				const J3DShapeMatrixGroup& matrixGroup :
				shape.MatrixGroups
				)
			{
				const std::uint64_t displayListStart =
					static_cast<std::uint64_t>(
						shp1Section.Offset
						) +
					shp1.DisplayListDataOffset +
					matrixGroup.DisplayListOffset;

				const std::uint64_t displayListEnd =
					displayListStart +
					matrixGroup.DisplayListSize;

				if (
					displayListEnd >
					document.Data.size()
					)
				{
					return Failure(
						"SHP1 display list exceeds file bounds"
					);
				}

				J3DShapeDisplayListGroup group;

				group.ShapeIndex =
					shape.LogicalIndex;

				group.GroupIndex =
					matrixGroup.LocalIndex;

				group.DisplayListSize =
					matrixGroup.DisplayListSize;

				group.EncodedVertexSize =
					encodedVertexSize;

				reader.Seek(
					static_cast<std::size_t>(
						displayListStart
						)
				);

				std::uint32_t noopCount = 0;
				std::uint32_t lastPrimitiveEnd = 0;

				while (
					reader.Tell() <
					displayListEnd
					)
				{
					const std::uint64_t commandAbsoluteOffset =
						reader.Tell();

					const std::uint32_t commandOffset =
						static_cast<std::uint32_t>(
							commandAbsoluteOffset -
							displayListStart
							);

					const std::uint8_t command =
						reader.ReadU8();

					// GX NOOP. J3D commonly uses these as
					// trailing alignment bytes.
					if (command == 0x00)
					{
						++noopCount;
						continue;
					}

					const std::uint8_t primitiveOpcode =
						command & 0xF8;

					const std::uint8_t vertexFormat =
						command & 0x07;

					J3DGXPrimitiveType primitiveType;

					if (!DecodePrimitiveType(
						primitiveOpcode,
						primitiveType
					))
					{
						return Failure(
							"SHP1 shape " +
							std::to_string(
								shape.LogicalIndex
							) +
							", group " +
							std::to_string(
								matrixGroup.LocalIndex
							) +
							" contains unsupported GX command " +
							std::to_string(command)
						);
					}

					// J3D's VTX1 gives us the VAT used by
					// this shape path. Supporting another
					// VAT is a separate feature.
					if (vertexFormat != 0)
					{
						return Failure(
							"SHP1 shape " +
							std::to_string(
								shape.LogicalIndex
							) +
							", group " +
							std::to_string(
								matrixGroup.LocalIndex
							) +
							" uses unsupported GX vertex format " +
							std::to_string(vertexFormat)
						);
					}

					if (
						reader.Tell() + 2 >
						displayListEnd
						)
					{
						return Failure(
							"GX primitive header exceeds display list"
						);
					}

					const std::uint16_t vertexCount =
						reader.ReadU16();

					const std::uint64_t vertexDataByteSize =
						static_cast<std::uint64_t>(
							encodedVertexSize
							) *
						vertexCount;

					if (
						reader.Tell() +
						vertexDataByteSize >
						displayListEnd
						)
					{
						return Failure(
							"GX primitive vertex payload exceeds display list"
						);
					}

					J3DShapePrimitiveRecord primitive;

					primitive.Type = primitiveType;
					primitive.VertexFormat =
						vertexFormat;

					primitive.VertexCount =
						vertexCount;

					primitive.CommandOffset =
						commandOffset;

					primitive.VertexDataOffset =
						commandOffset + 3;

					primitive.VertexDataByteSize =
						static_cast<std::uint32_t>(
							vertexDataByteSize
							);

					group.Primitives.push_back(
						primitive
					);

					reader.Skip(
						static_cast<std::size_t>(
							vertexDataByteSize
							)
					);

					lastPrimitiveEnd =
						static_cast<std::uint32_t>(
							reader.Tell() -
							displayListStart
							);
				}

				group.TrailingNoopByteCount =
					group.DisplayListSize -
					lastPrimitiveEnd;

				if (
					noopCount <
					group.TrailingNoopByteCount
					)
				{
					return Failure(
						"Internal GX NOOP accounting error"
					);
				}

				group.EmbeddedNoopByteCount =
					noopCount -
					group.TrailingNoopByteCount;

				output.Groups.push_back(
					std::move(group)
				);
			}
		}

		J3DShapeDisplayListParseResult result;
		result.Data = std::move(output);
		return result;
	}
}