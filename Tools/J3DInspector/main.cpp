#include "Formats/J3D/J3DFileReader.h"
#include "Formats/J3D/Sections/INF1Parser.h"

#include <iomanip>
#include <iostream>
#include <string>

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

void PrintHierarchyNode(
	const Okari::J3DINF1Data& inf1,
	std::uint32_t nodeIndex,
	std::size_t depth
)
{
	const Okari::J3DHierarchyNode& node =
		inf1.Nodes[nodeIndex];

	std::cout
		<< std::string(depth * 2, ' ')
		<< Okari::ToString(node.Type)
		<< '['
		<< node.Index
		<< ']'
		<< " @0x"
		<< std::hex
		<< std::uppercase
		<< node.FileOffset
		<< std::dec
		<< '\n';

	for (const std::uint32_t childIndex : node.Children)
	{
		PrintHierarchyNode(
			inf1,
			childIndex,
			depth + 1
		);
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

	const Okari::J3DSectionInfo* inf1Section =
		document.FindSection("INF1");

	if (inf1Section == nullptr)
	{
		std::cerr
			<< "\n[J3DInspector] The model has no INF1 section.\n";

		return 1;
	}

	const Okari::J3DINF1ParseResult inf1Result =
		Okari::J3DINF1Parser::Parse(
			document,
			*inf1Section
		);

	if (!inf1Result.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< inf1Result.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DINF1Data& inf1 = inf1Result.Data;

	std::cout
		<< "\nINF1\n"
		<< "Load flags: 0x"
		<< std::hex
		<< std::uppercase
		<< inf1.LoadFlags
		<< std::dec
		<< '\n'
		<< "Packet count: "
		<< inf1.PacketCount
		<< '\n'
		<< "Vertex position count: "
		<< inf1.VertexPositionCount
		<< '\n'
		<< "Hierarchy offset: 0x"
		<< std::hex
		<< std::uppercase
		<< inf1.HierarchyOffset
		<< std::dec
		<< '\n'
		<< "Hierarchy entries: "
		<< inf1.Entries.size()
		<< '\n'
		<< "Hierarchy nodes: "
		<< inf1.Nodes.size()
		<< '\n'
		<< "Root nodes: "
		<< inf1.RootNodes.size()
		<< "\n\n";

	for (const std::uint32_t rootNodeIndex : inf1.RootNodes)
	{
		PrintHierarchyNode(
			inf1,
			rootNodeIndex,
			0
		);
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