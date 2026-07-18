#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	class BigEndianReader
	{
	public:
		explicit BigEndianReader(const std::vector<std::uint8_t>& data);

		std::size_t Tell() const;
		std::size_t Size() const;
		bool CanRead(std::size_t byteCount) const;

		void Seek(std::size_t offset);
		void Skip(std::size_t byteCount);

		std::uint8_t ReadU8();
		std::uint16_t ReadU16();
		std::uint32_t ReadU32();

		std::int16_t ReadS16();
		float ReadF32();

		std::string ReadFixedString(std::size_t length);
		std::string ReadCString(std::size_t maxLength);

	private:
		void EnsureAvailable(std::size_t byteCount) const;

		const std::vector<std::uint8_t>& m_Data;
		std::size_t m_Offset = 0;
	};
}