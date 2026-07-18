#include "Formats/J3D/J3DFileReader.h";

#include "IO/BigEndianReader.h";

#include <fstream>;
#include <sstream>;
#include <stdexcept>;

namespace Okari
{
	namespace
	{
		constexpr std::size_t J3DHeaderSize = 0x20;
		constexpr std::size_t J3DSectionHeaderSize = 0x08;

		J3DReadResult Failure(const std::string& message)
		{
			J3DReadResult result;
			result.Error = message;
			return result;
		}

		std::string FormatHex(std::uint64_t value)
		{
			std::ostringstream stream;
			stream << "0x" << std::hex << std::uppercase << value;
			return stream.str();
		}

		J3DFileType GetFileType(const std::string& magic)
		{
			if (magic == "J3D2bmd3")
				return J3DFileType::BMD;
			if (magic == "J3D2bdl14")
				return J3DFileType::BDL;

			return J3DFileType::Unknown;
		}
	}

	J3DReadResult J3DFileReader::Read(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);

		if (!file)
			return Failure("Unable to open J3D file: " + path.string());

		const std::streamsize actualFileSize = file.tellg();

		if (actualFileSize < 0)
		{
			return Failure("Unable to determine J3D file size: " + path.string());
		}

		if (actualFileSize < static_cast<std::streamsize>(J3DHeaderSize))
		{
			return Failure("J3D file is smaller than its 0x20-byte header: " + path.string());
		}

		J3DReadResult result;

		result.Document.SourcePath = path;
		result.Document.Data.resize(static_cast<std::size_t>(actualFileSize));

		file.seekg(0, std::ios::beg);

		file.read(reinterpret_cast<char*>(result.Document.Data.data()), actualFileSize);

		if (!file)
		{
			return Failure("Unable to read the completee J3D file: " + path.string());
		}

		try
		{
			BigEndianReader reader(result.Document.Data);

			result.Document.Header.Magic = reader.ReadFixedString(8);

			result.Document.Header.Type = GetFileType(result.Document.Header.Magic);

			result.Document.Header.DeclaredFileSize = reader.ReadU32();

			result.Document.Header.SectionCount = reader.ReadU32();

			if (result.Document.Header.Type == J3DFileType::Unknown)
			{
				return Failure(
					"Unsupported J3D model magic '" +
					result.Document.Header.Magic +
					"' int " +
					path.string()
				);
			}

			const std::uint32_t declaredFileSize = result.Document.Header.DeclaredFileSize;

			if (declaredFileSize < J3DHeaderSize)
			{
				return Failure("J3D declared file size is smaller than the header");
			}

			if (declaredFileSize > result.Document.Data.size())
			{
				return Failure(
					"J3D declares " +
					std::to_string(declaredFileSize) +
					" bytes, but the file only contains " +
					std::to_string(result.Document.Data.size()) +
					" bytes"
				);
			}

			const std::uint32_t maxPossibleSectionCount =
				(declaredFileSize -
					static_cast<std::uint32_t>(J3DHeaderSize)) /
				static_cast<std::uint32_t>(J3DSectionHeaderSize);

			if (result.Document.Header.SectionCount > maxPossibleSectionCount)
			{
				return Failure("J3D section count cannot fit inside the declared file size");
			}

			reader.Seek(J3DHeaderSize);

			result.Document.Sections.reserve(result.Document.Header.SectionCount);

			for (std::uint32_t sectionIndex = 0; sectionIndex < result.Document.Header.SectionCount; sectionIndex++)
			{
				const std::size_t sectionOffset = reader.Tell();

				if (sectionOffset + J3DSectionHeaderSize > declaredFileSize)
				{
					return Failure(
						"Section " +
						std::to_string(sectionIndex) +
						" header exceeds the declared file size at " +
						FormatHex(sectionOffset)
					);
				}

				J3DSectionInfo section;

				section.Tag = reader.ReadFixedString(4);
				section.Size = reader.ReadU32();
				section.Offset = static_cast<std::uint32_t>(sectionOffset);

				if (section.Size < J3DSectionHeaderSize)
				{
					return Failure(
						"Section '" +
						section.Tag +
						"' at " +
						FormatHex(sectionOffset) +
						" is smaller than its header"
					);
				}

				const std::uint64_t sectionEnd = static_cast<std::uint64_t>(sectionOffset) + section.Size;

				if (sectionEnd > declaredFileSize)
				{
					return Failure(
						"Section '" +
						section.Tag +
						"' at " +
						FormatHex(sectionOffset) +
						" ends outside the declared file size"
					);
				}

				result.Document.Sections.push_back(section);

				reader.Seek(
					static_cast<std::size_t>(sectionEnd)
				);
			}
		}
		catch (const std::out_of_range& exception)
		{
			return Failure(
				"Malformed J3D file '" +
				path.string() +
				"': " +
				exception.what()
			);
		}

		return result;
	}
}