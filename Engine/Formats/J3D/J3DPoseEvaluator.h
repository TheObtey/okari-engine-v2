#pragma once

#include "Formats/J3D/J3DPoseTypes.h"
#include "Formats/J3D/Sections/INF1Types.h"
#include "Formats/J3D/Sections/JNT1Types.h"

namespace Okari
{
	class J3DPoseEvaluator
	{
	public:
		static J3DPoseEvaluationResult EvaluateRestPose(
			const J3DINF1Data& inf1,
			const J3DJNT1Data& jnt1
		);
	};
}