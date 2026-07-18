#pragma once

#include "Formats/J3D/J3DVertexData.h"
#include "Formats/J3D/Sections/VTX1Types.h"

#include <cstdint>

namespace Okari
{
	class J3DVertexDecoder
	{
	public:
		static J3DVertexDecodeResult Decode(
			const J3DVTX1Data& vtx1,
			const J3DVertexDecodeRequest& request
		);
	};
}