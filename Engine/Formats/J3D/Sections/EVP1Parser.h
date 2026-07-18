#pragma once

#include "Formats/J3D/J3DTypes.h"
#include "Formats/J3D/Sections/EVP1Types.h"

namespace Okari
{
	class J3DEVP1Parser
	{
	public:
		static J3DEVP1ParseResult Parse(
			const J3DDocument& document,
			const J3DSectionInfo& section
		);
	};
}