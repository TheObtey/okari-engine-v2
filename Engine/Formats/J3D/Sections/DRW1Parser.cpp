#include "Formats/J3D/Sections/DRW1Parser.h"

#include "IO/BigEndianReader.h"

#include <exception>
#include <utility>

namespace Okari
{
	namespace
	{
		constexpr std::size_t DRW1HeaderSize = 0x14;

		J3DDRW1ParseResult Failure(const std::string& message)
		{
			J3DDRW1ParseResult result;
			result.Error = message;
			return result;
		}

		bool RangeFitsInsideSection(
			std::uint64_t relativeOffset,
			std::uint64_t byteCount,
			std::uint64_t sectionSize
		)
		{
			return
				relativeOffset <= sectionSize &&
				byteCount <= sectionSize - relativeOffset;
		}

		bool DecodeMatrixKind(std::uint8_t rawKind, J3DDrawMatrixKind& kind)
		{
			switch (rawKind)
			{
			case 0x00:
				kind = J3DDrawMatrixKind::Joint;
				return true;

			case 0x01:
				kind = J3DDrawMatrixKind::Envelope;
				return true;

			default:
				return false;
			}
		}
	}

	const char* ToString(J3DDrawMatrixKind kind)
	{
		switch (kind)
		{
		case J3DDrawMatrixKind::Joint:
			return "Joint";

		case J3DDrawMatrixKind::Envelope:
			return "Envelope";

		default:
			return "Unknown";
		}
	}

	J3DDRW1ParseResult J3DDRW1Parser::Parse(const J3DDocument& document, const J3DSectionInfo& section)
	{
		if (section.Tag != "DRW1")
		{
			return Failure(
				"DRW1 parser received section '" +
				section.Tag +
				"'"
			);
		}

		if (section.Size < DRW1HeaderSize)
			return Failure("DRW1 section is smaller than its 0x14-byte header");

		const std::uint64_t sectionEnd =
			static_cast<std::uint64_t>(section.Offset) +
			section.Size;

		if (sectionEnd > document.Data.size())
			return Failure("DRW1 section ends outside the J3D file");

		try
		{
			BigEndianReader reader(document.Data);
			reader.Seek(section.Offset);

			const std::string sectionTag = reader.ReadFixedString(4);

			const std::uint32_t serializedSectionSize = reader.ReadU32();

			if (sectionTag != "DRW1")
				return Failure("Invalid DRW1 section magic");

			if (serializedSectionSize != section.Size)
				return Failure("DRW1 section size does not match the section directory");

			J3DDRW1Data data;

			data.MatrixCount = reader.ReadU16();
			data.Padding = reader.ReadU16();

			data.MatrixTypeTableOffset = reader.ReadU32();

			data.MatrixParameterTableOffset = reader.ReadU32();

			const std::uint64_t typeTableSize = data.MatrixCount;

			const std::uint64_t parameterTableSize =
				static_cast<std::uint64_t>(data.MatrixCount) *
				sizeof(std::uint16_t);

			if (!RangeFitsInsideSection(
				data.MatrixTypeTableOffset,
				typeTableSize,
				section.Size
			))
			{
				return Failure("DRW1 matrix type table exceeds the section");
			}

			if (!RangeFitsInsideSection(
				data.MatrixParameterTableOffset,
				parameterTableSize,
				section.Size
			))
			{
				return Failure("DRW1 matrix parameter table exceeds the section");
			}

			data.Matrices.reserve(data.MatrixCount);

			for (
				std::uint16_t matrixIndex = 0;
				matrixIndex < data.MatrixCount;
				++matrixIndex
				)
			{
				const std::uint64_t typeOffset =
					static_cast<std::uint64_t>(section.Offset) +
					data.MatrixTypeTableOffset +
					matrixIndex;

				reader.Seek(
					static_cast<std::size_t>(typeOffset)
				);

				const std::uint8_t rawKind = reader.ReadU8();

				J3DDrawMatrixKind kind;

				if (!DecodeMatrixKind(rawKind, kind))
				{
					return Failure(
						"Unknown DRW1 matrix kind " +
						std::to_string(rawKind) +
						" at matrix " +
						std::to_string(matrixIndex)
					);
				}

				const std::uint64_t parameterOffset =
					static_cast<std::uint64_t>(section.Offset) +
					data.MatrixParameterTableOffset +
					static_cast<std::uint64_t>(matrixIndex) *
					sizeof(std::uint16_t);

				reader.Seek(
					static_cast<std::size_t>(parameterOffset)
				);

				J3DDrawMatrixDefinition definition;

				definition.Index = matrixIndex;
				definition.Kind = kind;
				definition.Parameter = reader.ReadU16();

				data.Matrices.push_back(definition);
			}

			J3DDRW1ParseResult result;
			result.Data = std::move(data);

			return result;
		}
		catch (const std::exception& exception)
		{
			return Failure(
				std::string("Malformed DRW1 section: ") +
				exception.what()
			);
		}
	}
}