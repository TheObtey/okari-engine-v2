#include "Formats/J3D/J3DFileReader.h"

#include <iomanip>
#include <iostream>

namespace
{
	const char* ToString(Okari::J3DFileType type)
	{
		switch (type)
		{
		case Okari::J3DFileType::BMD:
			return "BMD";

		case Okari::J3DFileType::BDL:
			return "BDL";

		default:
			return "Unknown";
		}
	}
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr
			<< "Usage: J3DInspector <model.bmd|model.bdl>"
			<< std::endl;

		return 1;
	}

	Okari::J3DReadResult result =
		Okari::J3DFileReader::Read(argv[1]);

	if (!result.Succeeded())
	{
		std::cerr
			<< "[J3DInspector] "
			<< result.Error
			<< std::endl;

		return 1;
	}

	const Okari::J3DDocument& document = result.Document;

	std::cout << "File: "
		<< document.SourcePath.string()
		<< '\n';

	std::cout << "Magic: "
		<< document.Header.Magic
		<< '\n';

	std::cout << "Type: "
		<< ToString(document.Header.Type)
		<< '\n';

	std::cout << "Declared size: "
		<< document.Header.DeclaredFileSize
		<< " bytes\n";

	std::cout << "Actual size: "
		<< document.Data.size()
		<< " bytes\n";

	std::cout << "Sections: "
		<< document.Header.SectionCount
		<< "\n\n";

	for (
		std::size_t index = 0;
		index < document.Sections.size();
		++index
		)
	{
		const Okari::J3DSectionInfo& section =
			document.Sections[index];

		std::cout
			<< '[' << index << "] "
			<< section.Tag
			<< " | offset=0x"
			<< std::hex
			<< std::uppercase
			<< section.Offset
			<< " | size=0x"
			<< section.Size
			<< std::dec
			<< " ("
			<< section.Size
			<< " bytes)\n";
	}

	if (document.Data.size() !=
		document.Header.DeclaredFileSize)
	{
		std::cout
			<< "\nWarning: the physical file contains trailing "
			<< "bytes after the declared J3D data.\n";
	}

	return 0;
}