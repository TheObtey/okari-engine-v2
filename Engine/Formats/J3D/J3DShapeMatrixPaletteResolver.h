#pragma once

#include "Formats/J3D/J3DDrawMatrixTypes.h"
#include "Formats/J3D/J3DShapeMatrixPaletteTypes.h"
#include "Formats/J3D/Sections/SHP1Types.h"

namespace Okari
{
	class J3DShapeMatrixPaletteResolver
	{
	public:
		static J3DShapeMatrixPaletteResult Resolve(
			const J3DSHP1Data& shp1,
			const J3DDrawMatrixPalette& drawPalette
		);
	};
}