#include "Formats/J3D/J3DPoseEvaluator.h"

#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Okari
{
	namespace
	{
		constexpr float Pi = 3.14159265358979323846f;

		constexpr float J3DRotationScale = Pi / 32767.0f;

		constexpr std::uint16_t ScalingRuleMask = 0x000F;

		constexpr std::uint16_t ScalingRuleMaya = 0x0002;

		constexpr std::uint8_t MayaSegmentScaleCompensation = 0x01;

		constexpr float MinimumScaleMagnitude = 0.000001f;

		J3DPoseEvaluationResult Failure(const std::string& message)
		{
			J3DPoseEvaluationResult result;
			result.Error = message;
			return result;
		}

		float RotationToRadians(std::int16_t rotation)
		{
			return static_cast<float>(rotation) * J3DRotationScale;
		}

		glm::mat4 BuildLocalMatrix(const J3DJointTransform& transform)
		{
			const float rotationX = RotationToRadians(transform.Rotation.X);
			const float rotationY = RotationToRadians(transform.Rotation.Y);
			const float rotationZ = RotationToRadians(transform.Rotation.Z);

			const float sinX = std::sin(rotationX);
			const float cosX = std::cos(rotationX);

			const float sinY = std::sin(rotationY);
			const float cosY = std::cos(rotationY);

			const float sinZ = std::sin(rotationZ);
			const float cosZ = std::cos(rotationZ);

			const float scaleX = transform.Scale.X;
			const float scaleY = transform.Scale.Y;
			const float scaleZ = transform.Scale.Z;

			glm::mat4 matrix(1.0f);

			matrix[0][0] = scaleX * (cosY * cosZ);
			matrix[0][1] = scaleX * (sinZ * cosY);
			matrix[0][2] = scaleX * (-sinY);
			matrix[0][3] = 0.0f;

			matrix[1][0] = scaleY * (sinX * cosZ * sinY - cosX * sinZ);
			matrix[1][1] = scaleY * (sinX * sinZ * sinY + cosX * cosZ);
			matrix[1][2] = scaleY * (sinX * cosY);
			matrix[1][3] = 0.0f;

			matrix[2][0] = scaleZ * (cosX * cosZ * sinY + sinX * sinZ);
			matrix[2][1] = scaleZ * (cosX * sinZ * sinY - sinX * cosZ);
			matrix[2][2] = scaleZ * (cosY * cosX);
			matrix[2][3] = 0.0f;

			matrix[3][0] = transform.Translation.X;
			matrix[3][1] = transform.Translation.Y;
			matrix[3][2] = transform.Translation.Z;
			matrix[3][3] = 1.0f;

			return matrix;
		}

		bool HasUsableScale(const J3DVector3F& scale)
		{
			return
				std::abs(scale.X) > MinimumScaleMagnitude &&
				std::abs(scale.Y) > MinimumScaleMagnitude &&
				std::abs(scale.Z) > MinimumScaleMagnitude;
		}

		void ApplyMayaSegmentScaleCompensation(
			glm::mat4& matrix,
			const J3DVector3F& parentScale
		)
		{
			matrix[0][0] /= parentScale.X;
			matrix[1][0] /= parentScale.X;
			matrix[2][0] /= parentScale.X;

			matrix[0][1] /= parentScale.Y;
			matrix[1][1] /= parentScale.Y;
			matrix[2][1] /= parentScale.Y;

			matrix[0][2] /= parentScale.Z;
			matrix[1][2] /= parentScale.Z;
			matrix[2][2] /= parentScale.Z;
		}

		bool IsFinite(const glm::mat4& matrix)
		{
			for (std::size_t column = 0; column < 4; ++column)
			{
				for (std::size_t row = 0; row < 4; ++row)
				{
					if (!std::isfinite(matrix[column][row]))
						return false;
				}
			}

			return true;
		}
	}

	J3DPoseEvaluationResult J3DPoseEvaluator::EvaluateRestPose(
		const J3DINF1Data& inf1,
		const J3DJNT1Data& jnt1
	)
	{
		if (jnt1.Joints.empty())
			return Failure("Cannot evaluate a J3D pose without joints");
		
		J3DRestPose pose;

		pose.ScalingRule = inf1.LoadFlags & ScalingRuleMask;

		pose.Joints.resize(jnt1.Joints.size());

		std::vector<bool> jointWasFound(jnt1.Joints.size(), false);

		for (const J3DHierarchyNode& node : inf1.Nodes)
		{
			if (node.Type != J3DHierarchyEntryType::Joint)
				continue;

			if (node.Index >= jnt1.Joints.size())
			{
				return Failure(
					"INF1 references JNT1 joint " +
					std::to_string(node.Index) +
					", but only " +
					std::to_string(jnt1.Joints.size()) +
					" joints exist"
				);
			}

			if (jointWasFound[node.Index])
			{
				return Failure(
					"INF1 contains joint " +
					std::to_string(node.Index) +
					" more than once"
				);
			}

			J3DJointPose& jointPose = pose.Joints[node.Index];

			if (node.ParentNode >= 0)
			{
				const std::size_t parentNodeIndex =
					static_cast<std::size_t>(
						node.ParentNode
						);

				if (parentNodeIndex >= inf1.Nodes.size())
					return Failure("INF1 joint parent index is outside the node table");
				
				const J3DHierarchyNode& parentNode = inf1.Nodes[parentNodeIndex];

				if (parentNode.Type != J3DHierarchyEntryType::Joint)
					return Failure("INF1 joint parent is not a joint");

				if (parentNode.Index >= jnt1.Joints.size())
					return Failure("INF1 parent joint index exceeds JNT1");
				
				jointPose.ParentJointIndex =
					static_cast<std::int32_t>(
						parentNode.Index
						);
			}
			else
			{
				pose.RootJointIndices.push_back(node.Index);
			}

			jointWasFound[node.Index] = true;
		}

		for (std::size_t jointIndex = 0; jointIndex < jointWasFound.size(); ++jointIndex)
		{
			if (!jointWasFound[jointIndex])
			{
				return Failure(
					"JNT1 joint " +
					std::to_string(jointIndex) +
					" is not present in INF1"
				);
			}
		}

		if (pose.RootJointIndices.empty())
			return Failure("The J3D skeleton has no root joint");
		
		// 0 = not visited
		// 1 = in progress
		// 2 = finished
		std::vector<std::uint8_t> visitStates(
			jnt1.Joints.size(),
			0
		);

		std::string evaluationError;

		std::function<bool(std::uint16_t)> evaluateJoint;

		evaluateJoint =
			[&](
				std::uint16_t jointIndex
				) -> bool
			{
				if (visitStates[jointIndex] == 2)
					return true;

				if (visitStates[jointIndex] == 1)
				{
					evaluationError =
						"Cycle detected in J3D joint hierarchy at joint " +
						std::to_string(jointIndex);

					return false;
				}

				visitStates[jointIndex] = 1;

				const J3DJoint& joint = jnt1.Joints[jointIndex];

				J3DJointPose& jointPose = pose.Joints[jointIndex];

				glm::mat4 parentModelMatrix(1.0f);

				J3DVector3F parentScale
				{
					1.0f,
					1.0f,
					1.0f
				};

				if (jointPose.ParentJointIndex >= 0)
				{
					const std::uint16_t parentJointIndex =
						static_cast<std::uint16_t>(
							jointPose.ParentJointIndex
							);

					if (!evaluateJoint(parentJointIndex))
						return false;

					parentModelMatrix =
						pose.Joints[parentJointIndex]
						.ModelMatrix;

					parentScale =
						jnt1.Joints[parentJointIndex]
						.Transform
						.Scale;
				}

				jointPose.LocalMatrix = BuildLocalMatrix(joint.Transform);

				const bool needsMayaScaleCompensation =
					pose.ScalingRule == ScalingRuleMaya &&
					(joint.CalcFlags &
						MayaSegmentScaleCompensation) != 0;

				if (needsMayaScaleCompensation)
				{
					if (!HasUsableScale(parentScale))
					{
						evaluationError =
							"Cannot apply Maya scale compensation to joint " +
							std::to_string(jointIndex) +
							" because its parent has a zero scale component";

						return false;
					}

					ApplyMayaSegmentScaleCompensation(
						jointPose.LocalMatrix,
						parentScale
					);
				}

				jointPose.ModelMatrix = parentModelMatrix * jointPose.LocalMatrix;

				if (
					!IsFinite(jointPose.LocalMatrix) ||
					!IsFinite(jointPose.ModelMatrix)
					)
				{
					evaluationError =
						"Non-finite matrix generated for joint " +
						std::to_string(jointIndex);

					return false;
				}

				visitStates[jointIndex] = 2;
				return true;
			};

		for (std::uint16_t jointIndex = 0; jointIndex < jnt1.Joints.size(); ++jointIndex)
		{
			if (!evaluateJoint(jointIndex))
				return Failure(evaluationError);
		}

		J3DPoseEvaluationResult result;
		result.Pose = std::move(pose);

		return result;
	}
}