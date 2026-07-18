#include "Formats/J3D/Sections/EVP1Parser.h"

#include "IO/BigEndianReader.h"

#include <exception>
#include <limits>
#include <utility>

namespace Okari
{
	namespace
	{
		constexpr std::size_t EVP1HeaderSize = 0x1C;
		constexpr std::size_t InverseBindMatrixSize = 0x30;

		J3DEVP1ParseResult Failure(const std::string& message)
		{
			J3DEVP1ParseResult result;
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

	J3DEVP1ParseResult J3DEVP1Parser::Parse(
		const J3DDocument& document,
		const J3DSectionInfo& section
	)
	{
		if (section.Tag != "EVP1")
		{
			return Failure(
				"EVP1 parser received section '" +
				section.Tag +
				"'"
			);
		}

		if (section.Size < EVP1HeaderSize)
			return Failure("EVP1 section is smaller than its 0x1C-byte header");

		const std::uint64_t sectionEnd =
			static_cast<std::uint64_t>(section.Offset) +
			section.Size;

		if (sectionEnd > document.Data.size())
			return Failure("EVP1 section ends outside the J3D file");

		try
		{
			BigEndianReader reader(document.Data);
			reader.Seek(section.Offset);

			const std::string sectionTag = reader.ReadFixedString(4);

			const std::uint32_t serializedSectionSize = reader.ReadU32();

			if (sectionTag != "EVP1")
				return Failure("Invalid EVP1 section magic");

			if (serializedSectionSize != section.Size)
				return Failure("EVP1 section size does not match the section directory");

			J3DEVP1Data data;

			data.EnvelopeCount = reader.ReadU16();
			data.Padding = reader.ReadU16();

			data.WeightedJointCountTableOffset = reader.ReadU32();
			data.WeightedJointIndexTableOffset = reader.ReadU32();
			data.WeightedJointWeightTableOffset = reader.ReadU32();
			data.InverseBindMatrixTableOffset = reader.ReadU32();

			if (!RangeFitsInsideSection(
				data.WeightedJointCountTableOffset,
				data.EnvelopeCount,
				section.Size
			))
			{
				return Failure("EVP1 weighted-joint count table exceeds the section");
			}

			reader.Seek(
				static_cast<std::size_t>(
					section.Offset +
					data.WeightedJointCountTableOffset
					)
			);

			data.WeightedJointCounts.reserve(data.EnvelopeCount);

			std::uint64_t totalWeightedJointCount = 0;

			for (std::uint16_t envelopeIndex = 0; envelopeIndex < data.EnvelopeCount; ++envelopeIndex)
			{
				const std::uint8_t jointCount = reader.ReadU8();

				data.WeightedJointCounts.push_back(jointCount);

				totalWeightedJointCount += jointCount;
			}

			if (totalWeightedJointCount > std::numeric_limits<std::uint32_t>::max())
				return Failure("EVP1 weighted-joint count is too large");

			data.TotalWeightedJointCount =
				static_cast<std::uint32_t>(
					totalWeightedJointCount
					);

			const std::uint64_t jointIndexTableSize =
				totalWeightedJointCount *
				sizeof(std::uint16_t);

			const std::uint64_t jointWeightTableSize =
				totalWeightedJointCount *
				sizeof(float);

			if (!RangeFitsInsideSection(
				data.WeightedJointIndexTableOffset,
				jointIndexTableSize,
				section.Size
			))
			{
				return Failure("EVP1 weighted-joint index table exceeds the section");
			}

			if (!RangeFitsInsideSection(
				data.WeightedJointWeightTableOffset,
				jointWeightTableSize,
				section.Size
			))
			{
				return Failure("EVP1 weighted-joint weight table exceeds the section");
			}

			data.Envelopes.reserve(data.EnvelopeCount);

			std::uint32_t weightedJointIndex = 0;
			std::int32_t maximumJointIndex = -1;

			for (std::uint16_t envelopeIndex = 0; envelopeIndex < data.EnvelopeCount; ++envelopeIndex)
			{
				J3DEnvelope envelope;
				envelope.Index = envelopeIndex;

				const std::uint8_t jointCount = data.WeightedJointCounts[envelopeIndex];

				envelope.WeightedJoints.reserve(jointCount);

				for (std::uint8_t influenceIndex = 0; influenceIndex < jointCount; ++influenceIndex)
				{
					const std::uint64_t jointIndexOffset =
						static_cast<std::uint64_t>(
							section.Offset
							) +
						data.WeightedJointIndexTableOffset +
						static_cast<std::uint64_t>(
							weightedJointIndex
							) *
						sizeof(std::uint16_t);

					reader.Seek(
						static_cast<std::size_t>(
							jointIndexOffset
							)
					);

					const std::uint16_t jointIndex = reader.ReadU16();

					const std::uint64_t jointWeightOffset =
						static_cast<std::uint64_t>(
							section.Offset
							) +
						data.WeightedJointWeightTableOffset +
						static_cast<std::uint64_t>(
							weightedJointIndex
							) *
						sizeof(float);

					reader.Seek(
						static_cast<std::size_t>(
							jointWeightOffset
							)
					);

					const float weight = reader.ReadF32();

					J3DWeightedJoint weightedJoint;

					weightedJoint.JointIndex = jointIndex;
					weightedJoint.Weight = weight;

					envelope.WeightedJoints.push_back(weightedJoint);

					if (static_cast<std::int32_t>(jointIndex) > maximumJointIndex)
						maximumJointIndex = static_cast<std::int32_t>(jointIndex);

					++weightedJointIndex;
				}

				data.Envelopes.push_back(std::move(envelope));
			}

			const std::uint32_t inverseBindMatrixCount =
				maximumJointIndex < 0
				? 0
				: static_cast<std::uint32_t>(
					maximumJointIndex + 1
					);

			const std::uint64_t inverseBindTableSize =
				static_cast<std::uint64_t>(
					inverseBindMatrixCount
					) *
				InverseBindMatrixSize;

			if (!RangeFitsInsideSection(
				data.InverseBindMatrixTableOffset,
				inverseBindTableSize,
				section.Size
			))
			{
				return Failure("EVP1 inverse-bind matrix table exceeds the section");
			}

			data.InverseBindMatrices.reserve(inverseBindMatrixCount);

			for (
				std::uint32_t jointIndex = 0;
				jointIndex < inverseBindMatrixCount;
				++jointIndex
				)
			{
				const std::uint64_t matrixOffset =
					static_cast<std::uint64_t>(
						section.Offset
						) +
					data.InverseBindMatrixTableOffset +
					static_cast<std::uint64_t>(
						jointIndex
						) *
					InverseBindMatrixSize;

				reader.Seek(
					static_cast<std::size_t>(
						matrixOffset
						)
				);

				J3DInverseBindMatrix matrix;

				matrix.JointIndex = static_cast<std::uint16_t>(jointIndex);

				for (float& value : matrix.Values)
					value = reader.ReadF32();

				data.InverseBindMatrices.push_back(std::move(matrix));
			}

			J3DEVP1ParseResult result;
			result.Data = std::move(data);

			return result;
		}
		catch (const std::exception& exception)
		{
			return Failure(
				std::string("Malformed EVP1 section: ") +
				exception.what()
			);
		}
	}
}