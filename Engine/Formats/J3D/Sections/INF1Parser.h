#pragma once

#include "Formats/J3D/J3DTypes.h"
#include "Formats/J3D/Sections/INF1Types.h"

namespace Okari
{
	class J3DINF1Parser
	{
	public:
		static J3DINF1ParseResult Parse(
			const J3DDocument& document,
			const J3DSectionInfo& section
		);
	};
}