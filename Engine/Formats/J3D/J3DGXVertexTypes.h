#pragma once

#include <cstdint>

namespace Okari
{
	enum class J3DVertexAttribute : std::uint32_t
	{
		PositionMatrixIndex = 0,

		TexCoord0MatrixIndex = 1,
		TexCoord1MatrixIndex = 2,
		TexCoord2MatrixIndex = 3,
		TexCoord3MatrixIndex = 4,
		TexCoord4MatrixIndex = 5,
		TexCoord5MatrixIndex = 6,
		TexCoord6MatrixIndex = 7,
		TexCoord7MatrixIndex = 8,

		Position = 9,
		Normal = 10,
		Color0 = 11,
		Color1 = 12,

		TexCoord0 = 13,
		TexCoord1 = 14,
		TexCoord2 = 15,
		TexCoord3 = 16,
		TexCoord4 = 17,
		TexCoord5 = 18,
		TexCoord6 = 19,
		TexCoord7 = 20,

		PositionMatrixArray = 21,
		NormalMatrixArray = 22,
		TextureMatrixArray = 23,
		LightArray = 24,

		NBT = 25,

		Null = 0xFF
	};

	enum class J3DVertexInputType : std::uint32_t
	{
		None = 0,
		Direct = 1,
		Index8 = 2,
		Index16 = 3
	};

	constexpr bool IsMatrixIndexAttribute(
		J3DVertexAttribute attribute
	)
	{
		return
			attribute >=
			J3DVertexAttribute::PositionMatrixIndex &&
			attribute <=
			J3DVertexAttribute::TexCoord7MatrixIndex;
	}

	constexpr bool IsVertexArrayAttribute(
		J3DVertexAttribute attribute
	)
	{
		return
			(
				attribute >= J3DVertexAttribute::Position &&
				attribute <= J3DVertexAttribute::TexCoord7
				) ||
			attribute == J3DVertexAttribute::NBT;
	}

	constexpr const char* ToString(
		J3DVertexAttribute attribute
	)
	{
		switch (attribute)
		{
		case J3DVertexAttribute::PositionMatrixIndex:
			return "PNMTXIDX";

		case J3DVertexAttribute::TexCoord0MatrixIndex:
			return "TEX0MTXIDX";

		case J3DVertexAttribute::TexCoord1MatrixIndex:
			return "TEX1MTXIDX";

		case J3DVertexAttribute::TexCoord2MatrixIndex:
			return "TEX2MTXIDX";

		case J3DVertexAttribute::TexCoord3MatrixIndex:
			return "TEX3MTXIDX";

		case J3DVertexAttribute::TexCoord4MatrixIndex:
			return "TEX4MTXIDX";

		case J3DVertexAttribute::TexCoord5MatrixIndex:
			return "TEX5MTXIDX";

		case J3DVertexAttribute::TexCoord6MatrixIndex:
			return "TEX6MTXIDX";

		case J3DVertexAttribute::TexCoord7MatrixIndex:
			return "TEX7MTXIDX";

		case J3DVertexAttribute::Position:
			return "POS";

		case J3DVertexAttribute::Normal:
			return "NRM";

		case J3DVertexAttribute::Color0:
			return "CLR0";

		case J3DVertexAttribute::Color1:
			return "CLR1";

		case J3DVertexAttribute::TexCoord0:
			return "TEX0";

		case J3DVertexAttribute::TexCoord1:
			return "TEX1";

		case J3DVertexAttribute::TexCoord2:
			return "TEX2";

		case J3DVertexAttribute::TexCoord3:
			return "TEX3";

		case J3DVertexAttribute::TexCoord4:
			return "TEX4";

		case J3DVertexAttribute::TexCoord5:
			return "TEX5";

		case J3DVertexAttribute::TexCoord6:
			return "TEX6";

		case J3DVertexAttribute::TexCoord7:
			return "TEX7";

		case J3DVertexAttribute::PositionMatrixArray:
			return "POSMTXARRAY";

		case J3DVertexAttribute::NormalMatrixArray:
			return "NRMMTXARRAY";

		case J3DVertexAttribute::TextureMatrixArray:
			return "TEXMTXARRAY";

		case J3DVertexAttribute::LightArray:
			return "LIGHTARRAY";

		case J3DVertexAttribute::NBT:
			return "NBT";

		case J3DVertexAttribute::Null:
			return "NULL";

		default:
			return "UNKNOWN";
		}
	}

	constexpr const char* ToString(
		J3DVertexInputType inputType
	)
	{
		switch (inputType)
		{
		case J3DVertexInputType::None:
			return "NONE";

		case J3DVertexInputType::Direct:
			return "DIRECT";

		case J3DVertexInputType::Index8:
			return "INDEX8";

		case J3DVertexInputType::Index16:
			return "INDEX16";

		default:
			return "UNKNOWN";
		}
	}
}