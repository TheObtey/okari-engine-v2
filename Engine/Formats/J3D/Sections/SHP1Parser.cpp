#include "Formats/J3D/Sections/SHP1Parser.h"

#include "IO/BigEndianReader.h"

#include <array>
#include <exception>
#include <cmath>
#include <string>
#include <utility>

namespace Okari
{
	namespace
	{
		constexpr std::size_t SHP1HeaderSize = 0x2C;
		constexpr std::size_t ShapeRecordSize = 0x28;
		constexpr std::size_t MatrixInitRecordSize = 0x08;
		constexpr std::size_t DrawInitRecordSize = 0x08;
		constexpr std::uint16_t ReusePreviousMatrix = 0xFFFF;

		J3DSHP1ParseResult Failure(const std::string& message)
		{
			J3DSHP1ParseResult result;
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

		bool IsValidTableOffset(
			std::uint32_t offset,
			std::uint32_t sectionSize
		)
		{
			return
				offset == 0 ||
				(
					offset >= SHP1HeaderSize &&
					offset < sectionSize
					);
		}

		bool IsKnownMatrixType(std::uint8_t value)
		{
			return value <=
				static_cast<std::uint8_t>(
					J3DShapeMatrixType::MultiMatrix
					);
		}

		bool IsFinite(const J3DVector3F& value)
		{
			return
				std::isfinite(value.X) &&
				std::isfinite(value.Y) &&
				std::isfinite(value.Z);
		}

		bool AreBoundsOrdered(const J3DBoundingBox& bounds)
		{
			return
				bounds.Minimum.X <= bounds.Maximum.X &&
				bounds.Minimum.Y <= bounds.Maximum.Y &&
				bounds.Minimum.Z <= bounds.Maximum.Z;
		}
	}

	J3DSHP1ParseResult J3DSHP1Parser::Parse(
		const J3DDocument& document,
		const J3DSectionInfo& section
	)
	{
		if (section.Tag != "SHP1")
		{
			return Failure(
				"SHP1 parser received section '" +
				section.Tag +
				"'"
			);
		}

		if (section.Size < SHP1HeaderSize)
			return Failure("SHP1 section is smaller than its 0x2C-byte header");

		const std::uint64_t sectionEnd =
			static_cast<std::uint64_t>(section.Offset) +
			section.Size;

		if (sectionEnd > document.Data.size())
			return Failure("SHP1 section ends outside the J3D file");

		try
		{
			BigEndianReader reader(document.Data);
			reader.Seek(section.Offset);

			const std::string sectionTag =
				reader.ReadFixedString(4);

			const std::uint32_t serializedSectionSize =
				reader.ReadU32();

			if (sectionTag != "SHP1")
				return Failure("Invalid SHP1 section magic");

			if (serializedSectionSize != section.Size)
				return Failure("SHP1 section size does not match the section directory");

			J3DSHP1Data data;

			data.ShapeCount = reader.ReadU16();
			data.Padding = reader.ReadU16();

			data.ShapeInitDataOffset = reader.ReadU32();
			data.RemapTableOffset = reader.ReadU32();
			data.NameTableOffset = reader.ReadU32();
			data.VertexDescriptorTableOffset = reader.ReadU32();
			data.MatrixTableOffset = reader.ReadU32();
			data.DisplayListDataOffset = reader.ReadU32();
			data.MatrixInitDataOffset = reader.ReadU32();
			data.DrawInitDataOffset = reader.ReadU32();

			const std::array<std::pair<const char*, std::uint32_t>, 8>
				tableOffsets
			{ {
				{ "shape init data", data.ShapeInitDataOffset },
				{ "remap table", data.RemapTableOffset },
				{ "name table", data.NameTableOffset },
				{ "vertex descriptor table", data.VertexDescriptorTableOffset },
				{ "matrix table", data.MatrixTableOffset },
				{ "display-list data", data.DisplayListDataOffset },
				{ "matrix init data", data.MatrixInitDataOffset },
				{ "draw init data", data.DrawInitDataOffset }
			} };

			for (const auto& tableOffset : tableOffsets)
			{
				if (!IsValidTableOffset(tableOffset.second, section.Size))
				{
					return Failure(
						"SHP1 " +
						std::string(tableOffset.first) +
						" offset is outside the section"
					);
				}
			}

			if (data.ShapeCount > 0)
			{
				if (data.ShapeInitDataOffset == 0)
					return Failure("SHP1 has shapes but no shape init data");

				if (data.RemapTableOffset == 0)
					return Failure("SHP1 has shapes but no remap table");
			}

			const std::uint64_t remapTableByteCount =
				static_cast<std::uint64_t>(data.ShapeCount) *
				sizeof(std::uint16_t);

			if (!RangeFitsInsideSection(
				data.RemapTableOffset,
				remapTableByteCount,
				section.Size
			))
			{
				return Failure("SHP1 remap table exceeds the section");
			}

			const std::uint64_t shapeTableByteCount =
				static_cast<std::uint64_t>(data.ShapeCount) *
				ShapeRecordSize;

			if (!RangeFitsInsideSection(
				data.ShapeInitDataOffset,
				shapeTableByteCount,
				section.Size
			))
			{
				return Failure("SHP1 shape init table exceeds the section");
			}

			reader.Seek(section.Offset + data.RemapTableOffset);

			data.RemapTable.reserve(data.ShapeCount);

			for (
				std::uint16_t logicalIndex = 0;
				logicalIndex < data.ShapeCount;
				++logicalIndex
				)
			{
				const std::uint16_t dataIndex =
					reader.ReadU16();

				if (dataIndex >= data.ShapeCount)
				{
					return Failure(
						"SHP1 remap entry " +
						std::to_string(logicalIndex) +
						" references invalid shape record " +
						std::to_string(dataIndex)
					);
				}

				data.RemapTable.push_back(dataIndex);
			}

			data.Shapes.reserve(data.ShapeCount);

			for (
				std::uint16_t logicalIndex = 0;
				logicalIndex < data.ShapeCount;
				++logicalIndex
				)
			{
				const std::uint16_t dataIndex =
					data.RemapTable[logicalIndex];

				const std::uint64_t recordOffset =
					static_cast<std::uint64_t>(
						data.ShapeInitDataOffset
						) +
					static_cast<std::uint64_t>(
						dataIndex
						) *
					ShapeRecordSize;

				if (!RangeFitsInsideSection(
					recordOffset,
					ShapeRecordSize,
					section.Size
				))
				{
					return Failure(
						"SHP1 shape record " +
						std::to_string(dataIndex) +
						" exceeds the section"
					);
				}

				reader.Seek(
					static_cast<std::size_t>(
						section.Offset + recordOffset
						)
				);

				const std::uint8_t rawMatrixType =
					reader.ReadU8();

				if (!IsKnownMatrixType(rawMatrixType))
				{
					return Failure(
						"SHP1 shape " +
						std::to_string(logicalIndex) +
						" uses unknown matrix type " +
						std::to_string(rawMatrixType)
					);
				}

				J3DShapeRecord shape;

				shape.LogicalIndex = logicalIndex;
				shape.DataIndex = dataIndex;

				shape.MatrixType =
					static_cast<J3DShapeMatrixType>(
						rawMatrixType
						);

				shape.Padding0x01 = reader.ReadU8();

				shape.MatrixGroupCount = reader.ReadU16();

				shape.VertexDescriptorListOffset =
					reader.ReadU16();

				shape.MatrixInitDataIndex =
					reader.ReadU16();

				shape.DrawInitDataIndex =
					reader.ReadU16();

				shape.Padding0x0A = reader.ReadU16();

				shape.BoundingSphereRadius =
					reader.ReadF32();

				shape.Bounds.Minimum.X = reader.ReadF32();
				shape.Bounds.Minimum.Y = reader.ReadF32();
				shape.Bounds.Minimum.Z = reader.ReadF32();

				shape.Bounds.Maximum.X = reader.ReadF32();
				shape.Bounds.Maximum.Y = reader.ReadF32();
				shape.Bounds.Maximum.Z = reader.ReadF32();

				if (
					!std::isfinite(shape.BoundingSphereRadius) ||
					shape.BoundingSphereRadius < 0.0f
					)
				{
					return Failure(
						"SHP1 shape " +
						std::to_string(logicalIndex) +
						" has an invalid bounding sphere radius"
					);
				}

				if (
					!IsFinite(shape.Bounds.Minimum) ||
					!IsFinite(shape.Bounds.Maximum)
					)
				{
					return Failure(
						"SHP1 shape " +
						std::to_string(logicalIndex) +
						" has non-finite bounds"
					);
				}

				if (!AreBoundsOrdered(shape.Bounds))
				{
					return Failure(
						"SHP1 shape " +
						std::to_string(logicalIndex) +
						" has inverted bounds"
					);
				}

				data.Shapes.push_back(
					std::move(shape)
				);
			}

			for (J3DShapeRecord& shape : data.Shapes)
			{
				shape.MatrixGroups.reserve(
					shape.MatrixGroupCount
				);

				for (
					std::uint16_t localGroupIndex = 0;
					localGroupIndex < shape.MatrixGroupCount;
					++localGroupIndex
					)
				{
					const std::uint32_t matrixInitDataIndex =
						static_cast<std::uint32_t>(
							shape.MatrixInitDataIndex
							) +
						localGroupIndex;

					const std::uint32_t drawInitDataIndex =
						static_cast<std::uint32_t>(
							shape.DrawInitDataIndex
							) +
						localGroupIndex;

					const std::uint64_t matrixInitRecordOffset =
						static_cast<std::uint64_t>(
							data.MatrixInitDataOffset
							) +
						static_cast<std::uint64_t>(
							matrixInitDataIndex
							) *
						MatrixInitRecordSize;

					if (!RangeFitsInsideSection(
						matrixInitRecordOffset,
						MatrixInitRecordSize,
						section.Size
					))
					{
						return Failure(
							"SHP1 matrix-init record " +
							std::to_string(matrixInitDataIndex) +
							" exceeds the section"
						);
					}

					const std::uint64_t drawInitRecordOffset =
						static_cast<std::uint64_t>(
							data.DrawInitDataOffset
							) +
						static_cast<std::uint64_t>(
							drawInitDataIndex
							) *
						DrawInitRecordSize;

					if (!RangeFitsInsideSection(
						drawInitRecordOffset,
						DrawInitRecordSize,
						section.Size
					))
					{
						return Failure(
							"SHP1 draw-init record " +
							std::to_string(drawInitDataIndex) +
							" exceeds the section"
						);
					}

					J3DShapeMatrixGroup group;

					group.LocalIndex = localGroupIndex;
					group.MatrixInitDataIndex =
						matrixInitDataIndex;
					group.DrawInitDataIndex =
						drawInitDataIndex;

					reader.Seek(
						static_cast<std::size_t>(
							section.Offset +
							matrixInitRecordOffset
							)
					);

					group.UseMatrixIndex =
						reader.ReadU16();

					group.UseMatrixCount =
						reader.ReadU16();

					group.FirstUseMatrixIndex =
						reader.ReadU32();

					const std::uint64_t matrixTableByteOffset =
						static_cast<std::uint64_t>(
							data.MatrixTableOffset
							) +
						static_cast<std::uint64_t>(
							group.FirstUseMatrixIndex
							) *
						sizeof(std::uint16_t);

					const std::uint64_t matrixTableByteCount =
						static_cast<std::uint64_t>(
							group.UseMatrixCount
							) *
						sizeof(std::uint16_t);

					if (!RangeFitsInsideSection(
						matrixTableByteOffset,
						matrixTableByteCount,
						section.Size
					))
					{
						return Failure(
							"SHP1 matrix table for shape " +
							std::to_string(shape.LogicalIndex) +
							", group " +
							std::to_string(localGroupIndex) +
							" exceeds the section"
						);
					}

					group.RawMatrixTable.reserve(
						group.UseMatrixCount
					);

					reader.Seek(
						static_cast<std::size_t>(
							section.Offset +
							matrixTableByteOffset
							)
					);

					for (
						std::uint16_t matrixSlot = 0;
						matrixSlot < group.UseMatrixCount;
						++matrixSlot
						)
					{
						group.RawMatrixTable.push_back(
							reader.ReadU16()
						);
					}

					reader.Seek(
						static_cast<std::size_t>(
							section.Offset +
							drawInitRecordOffset
							)
					);

					group.DisplayListSize =
						reader.ReadU32();

					group.DisplayListOffset =
						reader.ReadU32();

					const std::uint64_t displayListOffset =
						static_cast<std::uint64_t>(
							data.DisplayListDataOffset
							) +
						group.DisplayListOffset;

					if (!RangeFitsInsideSection(
						displayListOffset,
						group.DisplayListSize,
						section.Size
					))
					{
						return Failure(
							"SHP1 display list for shape " +
							std::to_string(shape.LogicalIndex) +
							", group " +
							std::to_string(localGroupIndex) +
							" exceeds the section"
						);
					}

					shape.MatrixGroups.push_back(
						std::move(group)
					);
				}
			}

			J3DSHP1ParseResult result;
			result.Data = std::move(data);

			return result;
		}
		catch (const std::exception& exception)
		{
			return Failure(
				std::string("Malformed SHP1 section: ") +
				exception.what()
			);
		}
	}
}