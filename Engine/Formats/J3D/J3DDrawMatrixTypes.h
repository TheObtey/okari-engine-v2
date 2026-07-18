#pragma once

#include "Formats/J3D/Sections/DRW1Types.h"

#include <glm.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	struct J3DResolvedDrawMatrix
	{
		// Index of original definition in DRW1
		std::uint16_t SourceDefinitionIndex = 0;

		J3DDrawMatrixKind Kind = J3DDrawMatrixKind::Joint;

		// Index JNT1 or EVP1 according to Kind
		std::uint16_t Parameter = 0;

		glm::mat4 Matrix{ 1.0f };

		float BindPoseIdentityError = 0.0f;
	};

	struct J3DDrawMatrixPalette
	{
		std::size_t RawDefinitionCount = 0;
		std::size_t EffectiveDefinitionCount = 0;

		bool RemovedDuplicatedEnvelopeSuffix = false;

		std::size_t RigidMatrixCount = 0;
		std::size_t EnvelopeMatrixCount = 0;

		float MaximumEnvelopeIdentityError = 0.0f;

		std::vector<J3DResolvedDrawMatrix> Matrices;
	};

	struct J3DDrawMatrixEvaluationResult
	{
		J3DDrawMatrixPalette Palette;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}