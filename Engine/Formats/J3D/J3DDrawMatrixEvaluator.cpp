#include "Formats/J3D/J3DDrawMatrixEvaluator.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace Okari
{
	namespace
	{
		J3DDrawMatrixEvaluationResult Failure(const std::string& message)
		{
			J3DDrawMatrixEvaluationResult result;
			result.Error = message;
			return result;
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

		void AddWeightedMatrix(
			glm::mat4& destination,
			const glm::mat4& source,
			float weight
		)
		{
			for (std::size_t column = 0; column < 4; ++column)
			{
				for (std::size_t row = 0; row < 4; ++row)
				{
					destination[column][row] += source[column][row] * weight;
				}
			}
		}

		float CalculateIdentityError(const glm::mat4& matrix)
		{
			const glm::mat4 identity(1.0f);

			float maximumError = 0.0f;

			for (std::size_t column = 0; column < 4; ++column)
			{
				for (std::size_t row = 0; row < 4; ++row)
				{
					const float error =
						std::abs(
							matrix[column][row] -
							identity[column][row]
						);

					maximumError = std::max(maximumError, error);
				}
			}

			return maximumError;
		}

		glm::mat4 ConvertInverseBindMatrix(const J3DInverseBindMatrix& source)
		{
			const auto& values = source.Values;

			glm::mat4 matrix(1.0f);

			/*
				Order in EVP1 :

				m00 m10 m20 m30
				m01 m11 m21 m31
				m02 m12 m22 m32

				GLM uses matrix[column][row].
			*/

			matrix[0][0] = values[0];
			matrix[0][1] = values[4];
			matrix[0][2] = values[8];
			matrix[0][3] = 0.0f;

			matrix[1][0] = values[1];
			matrix[1][1] = values[5];
			matrix[1][2] = values[9];
			matrix[1][3] = 0.0f;

			matrix[2][0] = values[2];
			matrix[2][1] = values[6];
			matrix[2][2] = values[10];
			matrix[2][3] = 0.0f;

			matrix[3][0] = values[3];
			matrix[3][1] = values[7];
			matrix[3][2] = values[11];
			matrix[3][3] = 1.0f;

			return matrix;
		}

		bool HasDuplicatedEnvelopeSuffix(
			const J3DDRW1Data& drw1,
			const J3DEVP1Data& evp1
		)
		{
			const std::size_t envelopeCount = evp1.Envelopes.size();

			if (envelopeCount == 0)
				return false;

			if (envelopeCount > drw1.Matrices.size() / 2)
				return false;

			const std::size_t secondCopyStart =
				drw1.Matrices.size() -
				envelopeCount;

			const std::size_t firstCopyStart =
				secondCopyStart -
				envelopeCount;

			for (
				std::size_t envelopeIndex = 0;
				envelopeIndex < envelopeCount;
				++envelopeIndex
				)
			{
				const J3DDrawMatrixDefinition& first =
					drw1.Matrices[
						firstCopyStart +
							envelopeIndex
					];

				const J3DDrawMatrixDefinition& second =
					drw1.Matrices[
						secondCopyStart +
							envelopeIndex
					];

				if (
					first.Kind !=
					J3DDrawMatrixKind::Envelope ||
					second.Kind !=
					J3DDrawMatrixKind::Envelope
					)
				{
					return false;
				}

				if (first.Parameter != second.Parameter)
					return false;
			}

			return true;
		}
	}

	J3DDrawMatrixEvaluationResult
		J3DDrawMatrixEvaluator::Evaluate(
			const J3DRestPose& restPose,
			const J3DDRW1Data& drw1,
			const J3DEVP1Data& evp1
		)
	{
		if (restPose.Joints.empty())
			return Failure("Cannot evaluate J3D draw matrices without a pose");
		
		J3DDrawMatrixPalette palette;

		palette.RawDefinitionCount = drw1.Matrices.size();

		palette.RemovedDuplicatedEnvelopeSuffix =
			HasDuplicatedEnvelopeSuffix(
				drw1,
				evp1
			);

		palette.EffectiveDefinitionCount = palette.RawDefinitionCount;

		if (palette.RemovedDuplicatedEnvelopeSuffix)
		{
			palette.EffectiveDefinitionCount -=
				evp1.Envelopes.size();
		}

		palette.Matrices.reserve(palette.EffectiveDefinitionCount);

		for (
			std::size_t definitionIndex = 0;
			definitionIndex <
			palette.EffectiveDefinitionCount;
			++definitionIndex
			)
		{
			const J3DDrawMatrixDefinition& definition = drw1.Matrices[definitionIndex];

			if (definition.Index != definitionIndex)
			{
				return Failure(
					"DRW1 definition index mismatch at entry " +
					std::to_string(definitionIndex)
				);
			}

			J3DResolvedDrawMatrix resolved;

			resolved.SourceDefinitionIndex = definition.Index;

			resolved.Kind = definition.Kind;

			resolved.Parameter = definition.Parameter;

			switch (definition.Kind)
			{
			case J3DDrawMatrixKind::Joint:
			{
				++palette.RigidMatrixCount;

				if (
					definition.Parameter >=
					restPose.Joints.size()
					)
				{
					return Failure(
						"DRW1 definition " +
						std::to_string(definition.Index) +
						" references invalid pose joint " +
						std::to_string(definition.Parameter)
					);
				}

				resolved.Matrix =
					restPose
					.Joints[definition.Parameter]
					.ModelMatrix;

				break;
			}

			case J3DDrawMatrixKind::Envelope:
			{
				++palette.EnvelopeMatrixCount;

				if (
					definition.Parameter >=
					evp1.Envelopes.size()
					)
				{
					return Failure(
						"DRW1 definition " +
						std::to_string(definition.Index) +
						" references invalid EVP1 envelope " +
						std::to_string(definition.Parameter)
					);
				}

				const J3DEnvelope& envelope = evp1.Envelopes[definition.Parameter];

				if (envelope.WeightedJoints.empty())
				{
					return Failure(
						"EVP1 envelope " +
						std::to_string(envelope.Index) +
						" contains no weighted joints"
					);
				}

				glm::mat4 envelopeMatrix(0.0f);

				for (
					const J3DWeightedJoint& weightedJoint :
					envelope.WeightedJoints
					)
				{
					if (
						weightedJoint.JointIndex >=
						restPose.Joints.size()
						)
					{
						return Failure(
							"EVP1 envelope " +
							std::to_string(envelope.Index) +
							" references invalid pose joint " +
							std::to_string(
								weightedJoint.JointIndex
							)
						);
					}

					if (
						weightedJoint.JointIndex >=
						evp1.InverseBindMatrices.size()
						)
					{
						return Failure(
							"EVP1 has no inverse-bind matrix for joint " +
							std::to_string(
								weightedJoint.JointIndex
							)
						);
					}

					if (!std::isfinite(weightedJoint.Weight))
					{
						return Failure(
							"EVP1 envelope " +
							std::to_string(envelope.Index) +
							" contains a non-finite weight"
						);
					}

					const glm::mat4 inverseBindMatrix =
						ConvertInverseBindMatrix(
							evp1.InverseBindMatrices[
								weightedJoint.JointIndex
							]
						);

					const glm::mat4 jointSkinMatrix =
						restPose
						.Joints[
							weightedJoint.JointIndex
						]
						.ModelMatrix *
						inverseBindMatrix;

					if (!IsFinite(jointSkinMatrix))
					{
						return Failure(
							"Non-finite skinning matrix generated for joint " +
							std::to_string(
								weightedJoint.JointIndex
							)
						);
					}

					AddWeightedMatrix(
						envelopeMatrix,
						jointSkinMatrix,
						weightedJoint.Weight
					);
				}

				resolved.Matrix = envelopeMatrix;

				resolved.BindPoseIdentityError =
					CalculateIdentityError(
						envelopeMatrix
					);

				palette.MaximumEnvelopeIdentityError =
					std::max(
						palette.MaximumEnvelopeIdentityError,
						resolved.BindPoseIdentityError
					);

				break;
			}
			}

			if (!IsFinite(resolved.Matrix))
			{
				return Failure(
					"Non-finite DRW1 matrix generated at definition " +
					std::to_string(definition.Index)
				);
			}

			palette.Matrices.push_back( std::move(resolved));
		}

		J3DDrawMatrixEvaluationResult result;
		result.Palette = std::move(palette);

		return result;
	}
}