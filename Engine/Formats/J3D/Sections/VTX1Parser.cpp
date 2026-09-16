#include "Formats/J3D/Sections/VTX1Parser.h"

#include "IO/BigEndianReader.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <string>
#include <utility>

namespace Okari
{
	namespace
	{
		constexpr std::size_t VTX1HeaderSize = 0x40;
		constexpr std::size_t FormatRecordSize = 0x10;

		constexpr std::array<J3DVertexAttribute, J3DVTX1Data::ArraySlotCount> ArraySlotAttributes
		{
			J3DVertexAttribute::Position,
			J3DVertexAttribute::Normal,
			J3DVertexAttribute::NBT,
			J3DVertexAttribute::Color0,
			J3DVertexAttribute::Color1,
			J3DVertexAttribute::TexCoord0,
			J3DVertexAttribute::TexCoord1,
			J3DVertexAttribute::TexCoord2,
			J3DVertexAttribute::TexCoord3,
			J3DVertexAttribute::TexCoord4,
			J3DVertexAttribute::TexCoord5,
			J3DVertexAttribute::TexCoord6,
			J3DVertexAttribute::TexCoord7
		};

		J3DVTX1ParseResult Failure(const std::string& message)
		{
			J3DVTX1ParseResult result;
			result.Error = message;
			return result;
		}

		bool IsColorAttribute(J3DVertexAttribute attribute)
		{
			return
				attribute == J3DVertexAttribute::Color0 ||
				attribute == J3DVertexAttribute::Color1;
		}

		bool IsTextureCoordinateAttribute(J3DVertexAttribute attribute)
		{
			return
				attribute >= J3DVertexAttribute::TexCoord0 &&
				attribute <= J3DVertexAttribute::TexCoord7;
		}

		bool DecodeAttribute(std::uint32_t rawAttribute, J3DVertexAttribute& attribute)
		{
			switch (rawAttribute)
			{
			case 9:
				attribute = J3DVertexAttribute::Position;
				return true;

			case 10:
				attribute = J3DVertexAttribute::Normal;
				return true;

			case 11:
				attribute = J3DVertexAttribute::Color0;
				return true;

			case 12:
				attribute = J3DVertexAttribute::Color1;
				return true;

			case 13:
				attribute = J3DVertexAttribute::TexCoord0;
				return true;

			case 14:
				attribute = J3DVertexAttribute::TexCoord1;
				return true;

			case 15:
				attribute = J3DVertexAttribute::TexCoord2;
				return true;

			case 16:
				attribute = J3DVertexAttribute::TexCoord3;
				return true;

			case 17:
				attribute = J3DVertexAttribute::TexCoord4;
				return true;

			case 18:
				attribute = J3DVertexAttribute::TexCoord5;
				return true;

			case 19:
				attribute = J3DVertexAttribute::TexCoord6;
				return true;

			case 20:
				attribute = J3DVertexAttribute::TexCoord7;
				return true;

			case 25:
				attribute = J3DVertexAttribute::NBT;
				return true;

			default:
				return false;
			}
		}

		bool NumericComponentByteSize(
			std::uint32_t componentType,
			std::uint32_t& byteSize
		)
		{
			switch (componentType)
			{
			case 0: // U8
			case 1: // S8
				byteSize = 1;
				return true;

			case 2: // U16
			case 3: // S16
				byteSize = 2;
				return true;

			case 4: // F32
				byteSize = 4;
				return true;

			default:
				return false;
			}
		}

		bool CalculateElementStride(
			J3DVertexAttribute attribute,
			std::uint32_t componentCount,
			std::uint32_t componentType,
			std::uint32_t& stride,
			std::string& error
		)
		{
			if (IsColorAttribute(attribute))
			{
				if (componentCount > 1)
				{
					error = "GX color component count must be RGB or RGBA";

					return false;
				}

				switch (componentType)
				{
				case 0: // RGB565
					stride = 2;
					return true;

				case 1: // RGB8
					stride = 3;
					return true;

				case 2: // RGBX8
					stride = 4;
					return true;

				case 3: // RGBA4
					stride = 2;
					return true;

				case 4: // RGBA6
					stride = 3;
					return true;

				case 5: // RGBA8
					stride = 4;
					return true;

				default:
					error = "Unsupported GX color component type";

					return false;
				}
			}

			std::uint32_t componentByteSize = 0;

			if (!NumericComponentByteSize(componentType, componentByteSize))
			{
				error = "Unsupported GX numeric component type";

				return false;
			}

			std::uint32_t componentTotal = 0;

			switch (attribute)
			{
			case J3DVertexAttribute::Position:
				if (componentCount == 0)
					componentTotal = 2;
				else if (componentCount == 1)
					componentTotal = 3;
				else
				{
					error = "GX position must contain XY or XYZ";

					return false;
				}

				break;

			case J3DVertexAttribute::Normal:
			case J3DVertexAttribute::NBT:
				if (componentCount == 0)
					componentTotal = 3;
				else if ( componentCount == 1 || componentCount == 2)
				{
					componentTotal = 9;
				}
				else
				{
					error = "GX normal must use XYZ, NBT, or NBT3";

					return false;
				}

				break;

			default:
				if (IsTextureCoordinateAttribute(attribute))
				{
					if (componentCount == 0)
						componentTotal = 1;
					else if (componentCount == 1)
						componentTotal = 2;
					else
					{
						error = "GX texture coordinate must use S or ST";

						return false;
					}
				}
				else
				{
					error = "Unsupported VTX1 vertex attribute";

					return false;
				}

				break;
			}

			stride = componentTotal * componentByteSize;

			return true;
		}
	}

	std::string DescribeComponentCount(const J3DVertexFormatDescriptor& format)
	{
		switch (format.Attribute)
		{
		case J3DVertexAttribute::Position:
			if (format.ComponentCount == 0)
				return "XY";

			if (format.ComponentCount == 1)
				return "XYZ";

			break;

		case J3DVertexAttribute::Normal:
		case J3DVertexAttribute::NBT:
			if (format.ComponentCount == 0)
				return "XYZ";

			if (format.ComponentCount == 1)
				return "NBT";

			if (format.ComponentCount == 2)
				return "NBT3";

			break;

		case J3DVertexAttribute::Color0:
		case J3DVertexAttribute::Color1:
			if (format.ComponentCount == 0)
				return "RGB";

			if (format.ComponentCount == 1)
				return "RGBA";

			break;

		default:
			if (IsTextureCoordinateAttribute(format.Attribute))
			{
				if (format.ComponentCount == 0)
					return "S";

				if (format.ComponentCount == 1)
					return "ST";
			}

			break;
		}

		return "Unknown";
	}

	std::string DescribeComponentType(
		const J3DVertexFormatDescriptor& format
	)
	{
		if (IsColorAttribute(format.Attribute))
		{
			switch (format.ComponentType)
			{
			case 0:
				return "RGB565";

			case 1:
				return "RGB8";

			case 2:
				return "RGBX8";

			case 3:
				return "RGBA4";

			case 4:
				return "RGBA6";

			case 5:
				return "RGBA8";

			default:
				return "Unknown";
			}
		}

		switch (format.ComponentType)
		{
		case 0:
			return "U8";

		case 1:
			return "S8";

		case 2:
			return "U16";

		case 3:
			return "S16";

		case 4:
			return "F32";

		default:
			return "Unknown";
		}
	}

	J3DVTX1ParseResult J3DVTX1Parser::Parse(
		const J3DDocument& document,
		const J3DSectionInfo& section
	)
	{
		if (section.Tag != "VTX1")
		{
			return Failure(
				"VTX1 parser received section '" +
				section.Tag +
				"'"
			);
		}

		if (section.Size < VTX1HeaderSize)
			return Failure("VTX1 section is smaller than its 0x40-byte header");
		
		const std::uint64_t sectionEnd =
			static_cast<std::uint64_t>(section.Offset) +
			section.Size;

		if (sectionEnd > document.Data.size())
			return Failure("VTX1 section ends outside the J3D file");
		
		try
		{
			BigEndianReader reader(document.Data);
			reader.Seek(section.Offset);

			const std::string sectionTag =
				reader.ReadFixedString(4);

			const std::uint32_t serializedSectionSize =
				reader.ReadU32();

			if (sectionTag != "VTX1")
				return Failure("Invalid VTX1 section magic");
			
			if (serializedSectionSize != section.Size)
				return Failure("VTX1 section size does not match the section directory");
			
			J3DVTX1Data data;

			data.FormatTableOffset = reader.ReadU32();

			for (std::size_t slotIndex = 0; slotIndex < data.ArrayOffsets.size(); ++slotIndex)
				data.ArrayOffsets[slotIndex] = reader.ReadU32();
			
			if (data.FormatTableOffset < VTX1HeaderSize || data.FormatTableOffset >= section.Size)
				return Failure("VTX1 format table offset is outside the section");
			
			std::uint32_t firstArrayOffset = section.Size;

			std::uint32_t previousArrayOffset = 0;

			for (std::size_t slotIndex = 0; slotIndex < data.ArrayOffsets.size(); ++slotIndex)
			{
				const std::uint32_t arrayOffset = data.ArrayOffsets[slotIndex];

				if (arrayOffset == 0)
					continue;

				if (arrayOffset < VTX1HeaderSize || arrayOffset >= section.Size)
				{
					return Failure(
						"VTX1 array offset for " +
						std::string(
							ToString(
								ArraySlotAttributes[slotIndex]
							)
						) +
						" is outside the section"
					);
				}

				if (previousArrayOffset != 0 && arrayOffset <= previousArrayOffset)
					return Failure("VTX1 non-zero array offsets are not strictly ordered");
				
				previousArrayOffset = arrayOffset;

				firstArrayOffset = std::min(firstArrayOffset, arrayOffset);
			}

			if (data.FormatTableOffset >= firstArrayOffset)
				return Failure("VTX1 format table overlaps the vertex arrays");
			
			reader.Seek(section.Offset + data.FormatTableOffset);

			bool foundFormatTerminator = false;

			while (true)
			{
				const std::size_t recordStart = reader.Tell();

				const std::size_t relativeRecordStart = recordStart - section.Offset;

				if (relativeRecordStart + sizeof(std::uint32_t) > firstArrayOffset)
					break;

				const std::uint32_t rawAttribute = reader.ReadU32();

				if (rawAttribute == static_cast<std::uint32_t>(J3DVertexAttribute::Null))
				{
					foundFormatTerminator = true;
					break;
				}

				if (relativeRecordStart + FormatRecordSize > firstArrayOffset)
					return Failure("VTX1 format record overlaps the vertex arrays");
				
				J3DVertexAttribute attribute;

				if (!DecodeAttribute(rawAttribute, attribute))
				{
					return Failure(
						"Unsupported VTX1 attribute " +
						std::to_string(rawAttribute)
					);
				}

				if (data.FindFormat(attribute) != nullptr)
				{
					return Failure(
						"VTX1 contains duplicate format for " +
						std::string(ToString(attribute))
					);
				}

				J3DVertexFormatDescriptor format;

				format.Attribute = attribute;

				format.ComponentCount = reader.ReadU32();
				format.ComponentType = reader.ReadU32();
				format.FractionalBits = reader.ReadU8();

				format.Padding[0] = reader.ReadU8();
				format.Padding[1] = reader.ReadU8();
				format.Padding[2] = reader.ReadU8();

				std::string strideError;

				if (!CalculateElementStride(
					format.Attribute,
					format.ComponentCount,
					format.ComponentType,
					format.ElementStride,
					strideError
				))
				{
					return Failure(
						"Invalid VTX1 format for " +
						std::string(
							ToString(format.Attribute)
						) +
						": " +
						strideError
					);
				}

				data.Formats.push_back(std::move(format));
			}

			if (!foundFormatTerminator)
			{
				return Failure("VTX1 format table has no NULL terminator");
			}

			for (std::size_t slotIndex = 0; slotIndex < data.ArrayOffsets.size(); ++slotIndex)
			{
				const std::uint32_t arrayOffset = data.ArrayOffsets[slotIndex];

				if (arrayOffset == 0)
					continue;

				const J3DVertexAttribute attribute = ArraySlotAttributes[slotIndex];

				const J3DVertexFormatDescriptor* format = data.FindFormat(attribute);

				if (format == nullptr)
				{
					return Failure(
						"VTX1 contains data for " +
						std::string(ToString(attribute)) +
						" but no matching format descriptor"
					);
				}

				std::uint32_t arrayEnd = section.Size;

				for (std::size_t nextSlotIndex = slotIndex + 1; nextSlotIndex < data.ArrayOffsets.size(); ++nextSlotIndex)
				{
					const std::uint32_t nextOffset = data.ArrayOffsets[nextSlotIndex];

					if (nextOffset != 0)
					{
						arrayEnd = nextOffset;
						break;
					}
				}

				if (arrayEnd <= arrayOffset)
				{
					return Failure(
						"VTX1 array for " +
						std::string(ToString(attribute)) +
						" has an invalid byte range"
					);
				}

				J3DVertexArrayData array;

				array.Attribute = attribute;
				array.Offset = arrayOffset;
				array.ByteSize = arrayEnd - arrayOffset;

				array.ElementStride = format->ElementStride;

				array.ElementCapacity = array.ByteSize / array.ElementStride;

				array.RemainderByteCount = array.ByteSize % array.ElementStride;

				const std::size_t absoluteArrayStart =
					static_cast<std::size_t>(
						section.Offset +
						arrayOffset
						);

				const std::size_t absoluteArrayEnd =
					static_cast<std::size_t>(
						section.Offset +
						arrayEnd
						);

				array.RawData.assign(
					document.Data.begin() +
					absoluteArrayStart,
					document.Data.begin() +
					absoluteArrayEnd
				);

				data.Arrays.push_back(std::move(array));
			}

			J3DVTX1ParseResult result;
			result.Data = std::move(data);

			return result;
		}
		catch (const std::exception& exception)
		{
			return Failure(
				std::string("Malformed VTX1 section: ") +
				exception.what()
			);
		}
	}
}