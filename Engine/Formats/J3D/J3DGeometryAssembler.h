#pragma once

#include "Formats/J3D/J3DAssembledGeometryTypes.h"
#include "Formats/J3D/J3DDrawMatrixTypes.h"
#include "Formats/J3D/J3DShapeVertexReferenceTypes.h"
#include "Formats/J3D/J3DVertexData.h"

namespace Okari
{
	class J3DGeometryAssembler
	{
	public:
		static J3DGeometryAssemblyResult Assemble(
			const J3DShapeVertexReferenceData& references,
			const J3DDecodedVertexData& vertexData,
			const J3DDrawMatrixPalette& drawPalette
		);
	};
}