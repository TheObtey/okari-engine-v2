#pragma once

#include "Formats/J3D/J3DTypes.h"
#include "Formats/J3D/Sections/JNT1Types.h"

namespace Okari
{
	class J3DJNT1Parser
	{
	public:
		static J3DJNT1ParseResult Parse(
			const J3DDocument& document,
			const J3DSectionInfo& section
		);
	};
}