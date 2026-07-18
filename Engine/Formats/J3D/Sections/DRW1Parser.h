#pragma once

#include "Formats/J3D/J3DTypes.h"
#include "Formats/J3D/Sections/DRW1Types.h"

namespace Okari
{
	class J3DDRW1Parser
	{
	public:
		static J3DDRW1ParseResult Parse(
			const J3DDocument& document,
			const J3DSectionInfo& section
		);
	};
}