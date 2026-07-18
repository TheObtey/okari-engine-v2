#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	struct J3DWeightedJoint
	{
		std::uint16_t JointIndex = 0;
		float Weight = 0.0f;
	};

	struct J3DEnvelope
	{
		std::uint16_t Index = 0;
		std::vector<J3DWeightedJoint> WeightedJoints;
	};

	struct J3DInverseBindMatrix
	{
		// JNT1 index of corresponding matrix
		std::uint16_t JointIndex = 0;

		std::array<float, 12> Values{};
	};

	struct J3DEVP1Data
	{
		std::uint16_t EnvelopeCount = 0;
		std::uint16_t Padding = 0;

		std::uint32_t WeightedJointCountTableOffset = 0;
		std::uint32_t WeightedJointIndexTableOffset = 0;
		std::uint32_t WeightedJointWeightTableOffset = 0;
		std::uint32_t InverseBindMatrixTableOffset = 0;

		std::uint32_t TotalWeightedJointCount = 0;

		std::vector<std::uint8_t> WeightedJointCounts;
		std::vector<J3DEnvelope> Envelopes;
		std::vector<J3DInverseBindMatrix> InverseBindMatrices;
	};

	struct J3DEVP1ParseResult
	{
		J3DEVP1Data Data;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}