#include "IO/BigEndianReader.h"

#include <stdexcept>
#include <cstring>

namespace Okari
{
	BigEndianReader::BigEndianReader(const std::vector<std::uint8_t>& data)
		: m_Data(data)
	{ }

	std::size_t BigEndianReader::Tell() const
	{
		return m_Offset;
	}

	std::size_t BigEndianReader::Size() const
	{
		return m_Data.size();
	}

	bool BigEndianReader::CanRead(std::size_t byteCount) const
	{
		return byteCount <= m_Data.size() - m_Offset;
	}

	void BigEndianReader::Seek(std::size_t offset)
	{
		if (offset > m_Data.size())
			throw std::out_of_range("BigEndianReader seek is outside the buffer");

		m_Offset = offset;
	}

	void BigEndianReader::Skip(std::size_t byteCount)
	{
		EnsureAvailable(byteCount);
		m_Offset += byteCount;
	}

	std::uint8_t BigEndianReader::ReadU8()
	{
		EnsureAvailable(1);
		return m_Data[m_Offset++];
	}

	std::uint16_t BigEndianReader::ReadU16()
	{
		EnsureAvailable(2);

		const std::uint16_t value =
			(static_cast<std::uint16_t>(m_Data[m_Offset]) << 8) |
			static_cast<std::uint16_t>(m_Data[m_Offset + 1]);

		m_Offset += 2;
		return value;
	}

	std::uint32_t BigEndianReader::ReadU32()
	{
		EnsureAvailable(4);

		const std::uint32_t value =
			(static_cast<std::uint32_t>(m_Data[m_Offset]) << 24) |
			(static_cast<std::uint32_t>(m_Data[m_Offset + 1]) << 16) |
			(static_cast<std::uint32_t>(m_Data[m_Offset + 2]) << 8) |
			static_cast<std::uint32_t>(m_Data[m_Offset + 3]);

		m_Offset += 4;
		return value;
	}

	std::int16_t BigEndianReader::ReadS16()
	{
		const std::uint16_t rawValue = ReadU16();

		const std::int32_t signedValue =
			rawValue <= 0x7FFF
			? static_cast<std::int32_t>(rawValue)
			: static_cast<std::int32_t>(rawValue) - 0x10000;

		return static_cast<std::int16_t>(signedValue);
	}

	float BigEndianReader::ReadF32()
	{
		const std::uint32_t rawValue = ReadU32();

		float value = 0.0f;

		static_assert(
			sizeof(value) == sizeof(rawValue),
			"J3D float parsing requires 32-bit floats"
			);

		std::memcpy(
			&value,
			&rawValue,
			sizeof(value)
		);

		return value;
	}

	std::string BigEndianReader::ReadFixedString(std::size_t length)
	{
		EnsureAvailable(length);

		const char* begin = reinterpret_cast<const char*>(m_Data.data() + m_Offset);

		std::string value(begin, length);

		m_Offset += length;
		return value;
	}

	std::string BigEndianReader::ReadCString(std::size_t maxLength)
	{
		const std::size_t startOffset = m_Offset;

		for (std::size_t index = 0; index < maxLength; index++)
		{
			const char character = static_cast<char>(ReadU8());

			if (character == '\0')
			{
				const char* begin =
					reinterpret_cast<const char*>(
						m_Data.data() + startOffset
					);

				return std::string(begin, index);
			}
		}

		throw std::runtime_error(
			"BigEndianReader encountered an unterminated string"
		);
	}

	void BigEndianReader::EnsureAvailable(std::size_t byteCount) const
	{
		if (!CanRead(byteCount))
			throw std::out_of_range("BigEndianReader attempted to read beyond the buffer");
	}
}