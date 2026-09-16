#pragma once

#include "Formats/J3D/J3DVertexData.h"

#include <array>
#include <cstdint>
#include <string>

namespace Okari
{
	struct J3DVertexIndexRange
	{
		bool Used = false;

		std::uint32_t MaximumIndex = 0;
		std::uint64_t ReferenceCount = 0;

		void Observe(std::uint32_t index)
		{
			if (!Used || index > MaximumIndex)
				MaximumIndex = index;

			Used = true;
			++ReferenceCount;
		}

		std::uint32_t RequiredElementCount() const
		{
			return Used
				? MaximumIndex + 1
				: 0;
		}
	};

	struct J3DShapeVertexIndexUsage
	{
		J3DVertexIndexRange Position;
		J3DVertexIndexRange Normal;
		J3DVertexIndexRange NBT;

		std::array<J3DVertexIndexRange, 2> Colors;
		std::array<J3DVertexIndexRange, 8> TexCoords;

		J3DVertexDecodeRequest BuildDecodeRequest() const
		{
			J3DVertexDecodeRequest request;

			request.PositionCount =
				Position.RequiredElementCount();

			request.NormalCount =
				Normal.RequiredElementCount();

			request.NBTFrameCount =
				NBT.RequiredElementCount();

			for (
				std::size_t channel = 0;
				channel < Colors.size();
				++channel
				)
			{
				request.ColorCounts[channel] =
					Colors[channel].RequiredElementCount();
			}

			for (
				std::size_t channel = 0;
				channel < TexCoords.size();
				++channel
				)
			{
				request.TexCoordCounts[channel] =
					TexCoords[channel].RequiredElementCount();
			}

			return request;
		}
	};

	struct J3DShapeVertexIndexScanResult
	{
		J3DShapeVertexIndexUsage Usage;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}