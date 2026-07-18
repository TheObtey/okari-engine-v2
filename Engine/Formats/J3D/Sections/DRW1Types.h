#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	enum class J3DDrawMatrixKind : std::uint8_t
	{
		Joint = 0x00,
		Envelope = 0x01
	};

	struct J3DDrawMatrixDefinition
	{
		// Index of definition in DRW1
		std::uint16_t Index = 0;

		J3DDrawMatrixKind Kind = J3DDrawMatrixKind::Joint;

		// Joint index if Kind == Joint
		// Envelope index if Kind == Envelope
		std::uint16_t Parameter = 0;
	};

	struct J3DDRW1Data
	{
		std::uint16_t MatrixCount = 0;
		std::uint16_t Padding = 0;

		std::uint32_t MatrixTypeTableOffset = 0;
		std::uint32_t MatrixParameterTableOffset = 0;

		std::vector<J3DDrawMatrixDefinition> Matrices;
	};

	struct J3DDRW1ParseResult
	{
		J3DDRW1Data Data;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};

	const char* ToString(J3DDrawMatrixKind kind);
}