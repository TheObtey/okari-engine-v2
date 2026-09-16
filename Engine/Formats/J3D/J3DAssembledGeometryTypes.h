#pragma once

#include "Formats/J3D/J3DShapeDisplayListTypes.h"
#include "Formats/J3D/J3DVertexData.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Okari
{
	struct J3DAssembledVertex
	{
		glm::vec3 Position{ 0.0f };

		std::optional<glm::vec3> Normal;

		std::array<
			std::optional<J3DColorRGBA8>,
			2
		> Colors;

		std::array<
			std::optional<glm::vec2>,
			8
		> TexCoords;

		// Matrice effectivement sélectionnée dans J3DDrawMatrixPalette.
		std::uint16_t DrawMatrixIndex = 0;

		// Informations GX conservées pour ne pas perdre la sémantique originale.
		std::uint8_t PositionMatrixSlot = 0;

		std::optional<std::uint8_t>
			RawPositionMatrixIndex;

		std::array<
			std::optional<std::uint8_t>,
			8
		> RawTextureMatrixIndices;
	};

	struct J3DAssembledPrimitive
	{
		J3DGXPrimitiveType Type =
			J3DGXPrimitiveType::Triangles;

		std::uint8_t VertexFormat = 0;

		std::uint32_t SourceCommandOffset = 0;

		std::vector<J3DAssembledVertex> Vertices;
	};

	struct J3DAssembledShapeGroup
	{
		std::uint16_t ShapeIndex = 0;
		std::uint16_t GroupIndex = 0;

		std::vector<J3DAssembledPrimitive> Primitives;
	};

	struct J3DAssembledGeometry
	{
		std::vector<J3DAssembledShapeGroup> Groups;
	};

	struct J3DGeometryAssemblyResult
	{
		J3DAssembledGeometry Geometry;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}