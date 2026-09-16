#pragma once

#include "Formats/J3D/J3DAssembledGeometryTypes.h"
#include "Formats/J3D/J3DTriangleGeometryTypes.h"

namespace Okari
{
	class J3DTriangleTopologyBuilder
	{
	public:
		static J3DTriangleTopologyResult Build(
			const J3DAssembledGeometry& geometry
		);
	};
}