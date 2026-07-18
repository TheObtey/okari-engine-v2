#pragma once

#include <glm.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	struct J3DColorRGBA8
	{
		std::uint8_t R = 255;
		std::uint8_t G = 255;
		std::uint8_t B = 255;
		std::uint8_t A = 255;
	};

	struct J3DNBTFrame
	{
		glm::vec3 Normal { 0.0f };
		glm::vec3 Binormal { 0.0f };
		glm::vec3 Tangent { 0.0f };
	};

	struct J3DDecodedVertexData
	{
		std::vector<glm::vec3> Positions;
		std::vector<glm::vec3> Normals;
		std::vector<J3DNBTFrame> NBTFrames;

		std::array<std::vector<J3DColorRGBA8>, 2> Colors;

		std::array<std::vector<glm::vec2>, 8> TexCoords;
	};

	struct J3DVertexDecodeRequest
	{
		std::uint32_t PositionCount = 0;
		std::uint32_t NormalCount = 0;
		std::uint32_t NBTFrameCount = 0;

		std::array<std::uint32_t, 2> ColorCounts {};
		std::array<std::uint32_t, 8> TexCoordCounts{};
	};

	struct J3DVertexDecodeResult
	{
		J3DDecodedVertexData Data;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}