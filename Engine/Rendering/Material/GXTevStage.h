#pragma once

#include "GXEnums.h"

namespace Okari
{
	struct GXTevOrder
	{
		int TexCoord = -1;
		int TexMap = -1;
		int ColorChannel = -1;
	};

	struct GXTevColorStage
	{
		GXTevColorArg A = GXTevColorArg::Zero;
		GXTevColorArg B = GXTevColorArg::Zero;
		GXTevColorArg C = GXTevColorArg::Zero;
		GXTevColorArg D = GXTevColorArg::Zero;

		GXTevOp Operation = GXTevOp::Add;

		// GX bias: 0 = +0.0, 1 = +0.5, 2 = -0.5
		int Bias = 0;
		int Scale = 0;
		bool Clamp = true;

		GXTevRegister Output = GXTevRegister::Prev;
	};

	struct GXTevAlphaStage
	{
		GXTevColorArg A = GXTevColorArg::Zero;
		GXTevColorArg B = GXTevColorArg::Zero;
		GXTevColorArg C = GXTevColorArg::Zero;
		GXTevColorArg D = GXTevColorArg::Zero;

		GXTevOp Operation = GXTevOp::Add;

		// GX bias: 0 = +0.0, 1 = +0.5, 2 = -0.5
		int Bias = 0;
		int Scale = 0;
		bool Clamp = true;

		GXTevRegister Output = GXTevRegister::Prev;
	};

	struct GXTevStage
	{
		GXTevOrder Order;

		GXTevColorStage ColorStage;
		GXTevAlphaStage AlphaStage;

		int KonstColorSelector = -1;
		int KonstAlphaSelector = -1;
	};
}