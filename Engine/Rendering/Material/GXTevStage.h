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

		GXTevRegister Output = GXTevRegister::Prev;
	};

	struct GXTevAlphaStage
	{
		GXTevColorArg A = GXTevColorArg::Zero;
		GXTevColorArg B = GXTevColorArg::Zero;
		GXTevColorArg C = GXTevColorArg::Zero;
		GXTevColorArg D = GXTevColorArg::Zero;

		GXTevOp Operation = GXTevOp::Add;

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