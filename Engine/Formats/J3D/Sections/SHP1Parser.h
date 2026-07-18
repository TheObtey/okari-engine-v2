#pragma once

#include "Formats/J3D/J3DTypes.h"
#include "Formats/J3D/Sections/SHP1Types.h"

namespace Okari
{
	class J3DSHP1Parser
	{
	public:
		static J3DSHP1ParseResult Parse(
			const J3DDocument& document,
			const J3DSectionInfo& section
		);
	};
}