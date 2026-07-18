#pragma once

namespace Okari
{
	struct J3DVector3F
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;
	};

	struct J3DBoundingBox
	{
		J3DVector3F Minimum;
		J3DVector3F Maximum;
	};
}