#pragma once

#include <glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	struct J3DJointPose
	{
		std::int32_t ParentJointIndex = -1;

		// Transformation of the Joint to its parent
		glm::mat4 LocalMatrix { 1.0f };

		// Transformation of the Joint to model space
		glm::mat4 ModelMatrix { 1.0f };
	};

	struct J3DRestPose
	{
		std::uint16_t ScalingRule = 0;

		std::vector<J3DJointPose> Joints;
		std::vector<std::uint16_t> RootJointIndices;
	};

	struct J3DPoseEvaluationResult
	{
		J3DRestPose Pose;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}