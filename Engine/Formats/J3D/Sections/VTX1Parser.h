#pragma once

#include "Formats/J3D/J3DTypes.h"
#include "Formats/J3D/Sections/VTX1Types.h"

namespace Okari
{
	class J3DVTX1Parser
	{
	public:
		static J3DVTX1ParseResult Parse(
			const J3DDocument& document,
			const J3DSectionInfo& section
		);
	};
}