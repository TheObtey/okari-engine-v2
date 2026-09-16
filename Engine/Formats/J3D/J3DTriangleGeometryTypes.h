#pragma once

#include "Formats/J3D/J3DAssembledGeometryTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	struct J3DTriangleRange
	{
		std::uint16_t ShapeIndex = 0;
		std::uint16_t GroupIndex = 0;

		std::uint32_t FirstVertex = 0;
		std::uint32_t VertexCount = 0;
	};

	struct J3DTriangleGeometry
	{
		std::vector<J3DAssembledVertex> Vertices;
		std::vector<J3DTriangleRange> Ranges;

		std::uint32_t TriangleCount() const
		{
			return static_cast<std::uint32_t>(
				Vertices.size() / 3
				);
		}
	};

	struct J3DTriangleTopologyResult
	{
		J3DTriangleGeometry Geometry;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}