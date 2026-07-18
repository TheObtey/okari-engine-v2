#pragma once

#include "Formats/J3D/J3DDrawMatrixTypes.h"
#include "Formats/J3D/J3DPoseTypes.h"
#include "Formats/J3D/Sections/DRW1Types.h"
#include "Formats/J3D/Sections/EVP1Types.h"

namespace Okari
{
	class J3DDrawMatrixEvaluator
	{
	public:
		static J3DDrawMatrixEvaluationResult Evaluate(
			const J3DRestPose& restPose,
			const J3DDRW1Data& drw1,
			const J3DEVP1Data& evp1
		);
	};
}