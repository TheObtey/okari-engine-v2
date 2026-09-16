#pragma once

#include "Formats/J3D/J3DShapeDisplayListTypes.h"
#include "Formats/J3D/J3DShapeMatrixPaletteTypes.h"
#include "Formats/J3D/J3DShapeVertexReferenceTypes.h"
#include "Formats/J3D/J3DTypes.h"
#include "Formats/J3D/Sections/SHP1Types.h"
#include "Formats/J3D/Sections/VTX1Types.h"

namespace Okari
{
	class J3DShapeVertexReferenceDecoder
	{
	public:
		static J3DShapeVertexReferenceDecodeResult Decode(
			const J3DDocument& document,
			const J3DSectionInfo& shp1Section,
			const J3DSHP1Data& shp1,
			const J3DVTX1Data& vtx1,
			const J3DShapeDisplayListData& displayLists,
			const J3DShapeMatrixPalette& matrixPalette
		);
	};
}