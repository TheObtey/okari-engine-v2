#include "Formats/J3D/J3DFileReader.h"
#include "Formats/J3D/Sections/INF1Parser.h"
#include "Formats/J3D/Sections/JNT1Parser.h"
#include "Formats/J3D/Sections/DRW1Parser.h"

#include <iostream>
#include <string>
#include <cstddef>
#include <cstdint>

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

	void PrintHierarchyNode(
		const Okari::J3DINF1Data& inf1,
		const Okari::J3DJNT1Data& jnt1,
		std::uint32_t nodeIndex,
		std::size_t depth
	)
	{
		const Okari::J3DHierarchyNode& node = inf1.Nodes[nodeIndex];

		std::cout
			<< std::string(depth * 2, ' ')
			<< Okari::ToString(node.Type)
			<< '['
			<< node.Index
			<< ']';

		if (node.Type == Okari::J3DHierarchyEntryType::Joint)
		{
			if (node.Index < jnt1.Joints.size())
			{
				std::cout
					<< " \""
					<< jnt1.Joints[node.Index].Name
					<< '"';
			}
			else
			{
				std::cout << " <invalid JNT1 index>";
			}
		}

		std::cout
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
				jnt1,
				childIndex,
				depth + 1
			);
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

	Okari::J3DReadResult result = Okari::J3DFileReader::Read(argv[1]);

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

	for (std::size_t index = 0; index < document.Sections.size(); ++index)
	{
		const Okari::J3DSectionInfo& section = document.Sections[index];

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

	const Okari::J3DSectionInfo* inf1Section = document.FindSection("INF1");

	if (inf1Section == nullptr)
	{
		std::cerr << "\n[J3DInspector] The model has no INF1 section.\n";

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

	const Okari::J3DSectionInfo* jnt1Section = document.FindSection("JNT1");

	if (jnt1Section == nullptr)
	{
		std::cerr
			<< "\n[J3DInspector] The model has no JNT1 section.\n";

		return 1;
	}

	const Okari::J3DJNT1ParseResult jnt1Result =
		Okari::J3DJNT1Parser::Parse(
			document,
			*jnt1Section
		);

	if (!jnt1Result.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< jnt1Result.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DJNT1Data& jnt1 = jnt1Result.Data;

	std::cout
		<< "\nJNT1\n"
		<< "Joint count: "
		<< jnt1.JointCount
		<< '\n'
		<< "Joint data offset: 0x"
		<< std::hex
		<< std::uppercase
		<< jnt1.JointDataOffset
		<< '\n'
		<< "Remap table offset: 0x"
		<< jnt1.RemapTableOffset
		<< '\n'
		<< "Name table offset: 0x"
		<< jnt1.NameTableOffset
		<< std::dec
		<< "\n\n";

	for (const Okari::J3DJoint& joint : jnt1.Joints)
	{
		std::cout
			<< '['
			<< joint.LogicalIndex
			<< "] "
			<< joint.Name
			<< " | data="
			<< joint.DataIndex
			<< " | scale=("
			<< joint.Transform.Scale.X
			<< ", "
			<< joint.Transform.Scale.Y
			<< ", "
			<< joint.Transform.Scale.Z
			<< ") | rotationRaw=("
			<< joint.Transform.Rotation.X
			<< ", "
			<< joint.Transform.Rotation.Y
			<< ", "
			<< joint.Transform.Rotation.Z
			<< ") | translation=("
			<< joint.Transform.Translation.X
			<< ", "
			<< joint.Transform.Translation.Y
			<< ", "
			<< joint.Transform.Translation.Z
			<< ")\n";
	}

	for (const Okari::J3DHierarchyNode& node : inf1.Nodes)
	{
		if (node.Type == Okari::J3DHierarchyEntryType::Joint && node.Index >= jnt1.Joints.size())
		{
			std::cerr
				<< "[J3DInspector] INF1 references invalid joint "
				<< node.Index
				<< '\n';

			return 1;
		}
	}

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

	std::cout << "\nINF1 hierarchy with JNT1 names\n\n";

	for (const std::uint32_t rootNodeIndex : inf1.RootNodes)
	{
		PrintHierarchyNode(
			inf1,
			jnt1,
			rootNodeIndex,
			0
		);
	}

	const Okari::J3DSectionInfo* drw1Section = document.FindSection("DRW1");

	if (drw1Section == nullptr)
	{
		std::cerr << "\n[J3DInspector] The model has no DRW1 section.\n";

		return 1;
	}

	const Okari::J3DDRW1ParseResult drw1Result =
		Okari::J3DDRW1Parser::Parse(
			document,
			*drw1Section
		);

	if (!drw1Result.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< drw1Result.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DDRW1Data& drw1 = drw1Result.Data;

	std::size_t rigidMatrixCount = 0;
	std::size_t envelopeMatrixCount = 0;

	for (const Okari::J3DDrawMatrixDefinition& matrix : drw1.Matrices)
	{
		switch (matrix.Kind)
		{
		case Okari::J3DDrawMatrixKind::Joint:
			++rigidMatrixCount;

			if (matrix.Parameter >= jnt1.Joints.size())
			{
				std::cerr
					<< "[J3DInspector] DRW1 matrix "
					<< matrix.Index
					<< " references invalid joint "
					<< matrix.Parameter
					<< '\n';

				return 1;
			}

			break;

		case Okari::J3DDrawMatrixKind::Envelope:
			++envelopeMatrixCount;
			break;
		}
	}

	std::cout
		<< "\nDRW1\n"
		<< "Matrix count: "
		<< drw1.MatrixCount
		<< '\n'
		<< "Type table offset: 0x"
		<< std::hex
		<< std::uppercase
		<< drw1.MatrixTypeTableOffset
		<< '\n'
		<< "Parameter table offset: 0x"
		<< drw1.MatrixParameterTableOffset
		<< std::dec
		<< '\n'
		<< "Rigid joint matrices: "
		<< rigidMatrixCount
		<< '\n'
		<< "Weighted envelope matrices: "
		<< envelopeMatrixCount
		<< "\n\n";

	if (drw1.Matrices.size() != drw1.MatrixCount)
	{
		std::cerr
			<< "[J3DInspector] DRW1 parsed matrix count does not "
			<< "match the declared matrix count.\n";

		return 1;
	}

	constexpr std::size_t MaximumDisplayedMatrices = 32;

	const std::size_t displayedMatrixCount =
		drw1.Matrices.size() < MaximumDisplayedMatrices
		? drw1.Matrices.size()
		: MaximumDisplayedMatrices;

	for (std::size_t index = 0; index < displayedMatrixCount; ++index)
	{
		const Okari::J3DDrawMatrixDefinition& matrix = drw1.Matrices[index];

		std::cout
			<< '['
			<< matrix.Index
			<< "] "
			<< Okari::ToString(matrix.Kind)
			<< '['
			<< matrix.Parameter
			<< ']';

		if (matrix.Kind == Okari::J3DDrawMatrixKind::Joint)
		{
			std::cout
				<< " \""
				<< jnt1.Joints[matrix.Parameter].Name
				<< '"';
		}

		std::cout << '\n';
	}

	if (drw1.Matrices.size() > displayedMatrixCount)
	{
		std::cout
			<< "... "
			<< drw1.Matrices.size() - displayedMatrixCount
			<< " additional matrices omitted\n";
	}

	if (document.Data.size() != document.Header.DeclaredFileSize)
	{
		std::cout
			<< "\nWarning: the physical file contains trailing "
			<< "bytes after the declared J3D data.\n";
	}

	return 0;
}