#include "Formats/J3D/Sections/JNT1Parser.h"

#include "Formats/J3D/J3DStringTable.h"
#include "IO/BigEndianReader.h"

#include <stdexcept>
#include <utility>

namespace Okari
{
	namespace
	{
		constexpr std::size_t JNT1HeaderSize = 0x18;
		constexpr std::size_t JointRecordSize = 0x40;

		J3DJNT1ParseResult Failure(const std::string& message)
		{
			J3DJNT1ParseResult result;
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
	}

	J3DJNT1ParseResult J3DJNT1Parser::Parse(
		const J3DDocument& document,
		const J3DSectionInfo& section
	)
	{
		if (section.Tag != "JNT1")
		{
			return Failure(
				"JNT1 parser received section '" +
				section.Tag +
				"'"
			);
		}

		if (section.Size < JNT1HeaderSize)
			return Failure("JNT1 section is smaller than its 0x18-byte header");
		
		const std::uint64_t sectionEnd =
			static_cast<std::uint64_t>(section.Offset) +
			section.Size;

		if (sectionEnd > document.Data.size())
			return Failure("JNT1 section ends outside the J3D file");

		try
		{
			BigEndianReader reader(document.Data);
			reader.Seek(section.Offset);

			const std::string sectionTag = reader.ReadFixedString(4);

			const std::uint32_t serializedSectionSize = reader.ReadU32();

			if (sectionTag != "JNT1")
				return Failure("Invalid JNT1 section magic");
			
			if (serializedSectionSize != section.Size)
				return Failure("JNT1 section size does not match the section directory");

			J3DJNT1Data data;

			data.JointCount = reader.ReadU16();
			data.Padding = reader.ReadU16();

			data.JointDataOffset = reader.ReadU32();
			data.RemapTableOffset = reader.ReadU32();
			data.NameTableOffset = reader.ReadU32();

			const std::uint64_t remapTableSize =
				static_cast<std::uint64_t>(
					data.JointCount
					) *
				sizeof(std::uint16_t);

			if (!RangeFitsInsideSection(
				data.RemapTableOffset,
				remapTableSize,
				section.Size
			))
			{
				return Failure("JNT1 remap table exceeds the section");
			}

			reader.Seek(section.Offset + data.RemapTableOffset);

			data.RemapTable.reserve(data.JointCount);

			for (
				std::uint16_t index = 0;
				index < data.JointCount;
				++index
				)
			{
				data.RemapTable.push_back(
					reader.ReadU16()
				);
			}

			J3DStringTableReadResult nameTableResult =
				J3DStringTableReader::Read(
					document,
					section,
					data.NameTableOffset
				);

			if (!nameTableResult.Succeeded())
			{
				return Failure(
					"Unable to read JNT1 names: " +
					nameTableResult.Error
				);
			}

			if (
				nameTableResult.Table.Entries.size() !=
				data.JointCount
				)
			{
				return Failure("JNT1 joint count does not match its name count");
			}

			data.NameTable = std::move(nameTableResult.Table);

			data.Joints.reserve(data.JointCount);

			for (
				std::uint16_t logicalIndex = 0;
				logicalIndex < data.JointCount;
				++logicalIndex
				)
			{
				const std::uint16_t dataIndex = data.RemapTable[logicalIndex];

				const std::uint64_t recordOffset =
					static_cast<std::uint64_t>(
						data.JointDataOffset
						) +
					static_cast<std::uint64_t>(
						dataIndex
						) *
					JointRecordSize;

				if (!RangeFitsInsideSection(
					recordOffset,
					JointRecordSize,
					section.Size
				))
				{
					return Failure(
						"JNT1 joint record " +
						std::to_string(dataIndex) +
						" exceeds the section"
					);
				}

				reader.Seek(
					static_cast<std::size_t>(
						section.Offset +
						recordOffset
						)
				);

				J3DJoint joint;

				joint.LogicalIndex = logicalIndex;
				joint.DataIndex = dataIndex;

				joint.Name =
					data.NameTable
					.Entries[logicalIndex]
					.Value;

				joint.MatrixType = reader.ReadU16();
				joint.CalcFlags = reader.ReadU8();

				// 0x03 padding
				reader.Skip(1);

				joint.Transform.Scale.X = reader.ReadF32();
				joint.Transform.Scale.Y = reader.ReadF32();
				joint.Transform.Scale.Z = reader.ReadF32();

				joint.Transform.Rotation.X = reader.ReadS16();
				joint.Transform.Rotation.Y = reader.ReadS16();
				joint.Transform.Rotation.Z = reader.ReadS16();

				// Add padding after rotations
				reader.Skip(2);

				joint.Transform.Translation.X = reader.ReadF32();
				joint.Transform.Translation.Y = reader.ReadF32();
				joint.Transform.Translation.Z = reader.ReadF32();

				joint.BoundingSphereRadius = reader.ReadF32();

				joint.Bounds.Minimum.X = reader.ReadF32();
				joint.Bounds.Minimum.Y = reader.ReadF32();
				joint.Bounds.Minimum.Z = reader.ReadF32();

				joint.Bounds.Maximum.X = reader.ReadF32();
				joint.Bounds.Maximum.Y = reader.ReadF32();
				joint.Bounds.Maximum.Z = reader.ReadF32();

				data.Joints.push_back(std::move(joint));
			}

			J3DJNT1ParseResult result;
			result.Data = std::move(data);

			return result;
		}
		catch (const std::exception& exception)
		{
			return Failure(
				std::string("Malformed JNT1 section: ") +
				exception.what()
			);
		}
	}
}