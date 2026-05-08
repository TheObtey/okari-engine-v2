#pragma once

#include <cstdint>

namespace Okari
{
	enum class GXTevRegister : uint8_t
	{
		Prev,
		Reg0,
		Reg1,
		Reg2
	};

	enum class GXTevColorArg : uint8_t
	{
		Zero,
		One,

		TexColor,
		TexAlpha,

		RasColor,
		RasAlpha,

		KonstColor,
		KonstAlpha,

		PrevColor,
		PrevAlpha,

		Reg0Color,
		Reg0Alpha,

		Reg1Color,
		Reg1Alpha,

		Reg2Color,
		Reg2Alpha
	};

	enum class GXTevOp : uint8_t
	{
		Add,
		Subtract
	};
}