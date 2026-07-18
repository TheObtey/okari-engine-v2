#pragma once

#include <cstdint>;
#include <filesystem>;
#include <string>
#include <vector>;

namespace Okari
{
	enum class J3DFileType
	{
		Unknown,
		BMD,
		BDL
	};

	struct J3DFileHeader
	{
		std::string Magic;
		J3DFileType Type = J3DFileType::Unknown;

		std::uint32_t DeclaredFileSize = 0;
		std::uint32_t SectionCount = 0;
	};

	struct J3DSectionInfo
	{
		std::string Tag;

		std::uint32_t Offset = 0;
		std::uint32_t Size = 0;
	};

	struct J3DDocument
	{
		std::filesystem::path SourcePath;

		J3DFileHeader Header;
		std::vector<J3DSectionInfo> Sections;

		std::vector<std::uint8_t> Data;

		const J3DSectionInfo* FindSection(const std::string& tag) const
		{
			for (const J3DSectionInfo& section : Sections)
			{
				if (section.Tag == tag)
					return &section;
			}

			return nullptr;
		}
	};

	struct J3DReadResult
	{
		J3DDocument Document;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};
}