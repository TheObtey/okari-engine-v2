#pragma once

#include "Formats/J3D/J3DStringTable.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	struct J3DVector3F
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;
	};

	struct J3DVector3S16
	{
		std::int16_t X = 0;
		std::int16_t Y = 0;
		std::int16_t Z = 0;
	};

	struct J3DBoundingBox
	{
		J3DVector3F Minimum;
		J3DVector3F Maximum;
	};

	struct J3DJointTransform
	{
		J3DVector3F Scale
		{
			1.0f,
			1.0f,
			1.0f
		};

		J3DVector3S16 Rotation;

		J3DVector3F Translation;
	};

	struct J3DJoint
	{
		// Index used by INF1
		std::uint16_t LogicalIndex = 0;
		
		// Physical index after reading remap table
		std::uint16_t DataIndex = 0;

		std::string Name;

		std::uint16_t MatrixType = 0;
		std::uint8_t CalcFlags = 0;

		J3DJointTransform Transform;

		float BoundingSphereRadius = 0.0f;
		J3DBoundingBox Bounds;
	};

	struct J3DJNT1Data
	{
		std::uint16_t JointCount = 0;
		std::uint16_t Padding = 0;

		std::uint32_t JointDataOffset = 0;
		std::uint32_t RemapTableOffset = 0;
		std::uint32_t NameTableOffset = 0;

		std::vector<std::uint16_t> RemapTable;
		J3DStringTable NameTable;

		std::vector<J3DJoint> Joints;
	};

	struct J3DJNT1ParseResult
	{
		J3DJNT1Data Data;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}