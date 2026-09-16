#pragma once

#include "Formats/J3D/J3DShapeDisplayListTypes.h"
#include "Formats/J3D/J3DTypes.h"
#include "Formats/J3D/Sections/SHP1Types.h"
#include "Formats/J3D/Sections/VTX1Types.h"

namespace Okari
{
	class J3DShapeDisplayListParser
	{
	public:
		static J3DShapeDisplayListParseResult Parse(
			const J3DDocument& document,
			const J3DSectionInfo& shp1Section,
			const J3DSHP1Data& shp1,
			const J3DVTX1Data& vtx1
		);
	};
}