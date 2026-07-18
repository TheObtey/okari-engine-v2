#include "Formats/J3D/J3DFileReader.h"
#include "Formats/J3D/J3DPoseEvaluator.h"
#include "Formats/J3D/J3DDrawMatrixEvaluator.h"

#include "Formats/J3D/Sections/INF1Parser.h"
#include "Formats/J3D/Sections/JNT1Parser.h"
#include "Formats/J3D/Sections/DRW1Parser.h"
#include "Formats/J3D/Sections/EVP1Parser.h"
#include "Formats/J3D/Sections/VTX1Parser.h"

#include <iostream>
#include <string>
#include <cstdint>
#include <cmath>

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

	const Okari::J3DSectionInfo* vtx1Section =
		document.FindSection("VTX1");

	if (vtx1Section == nullptr)
	{
		std::cerr
			<< "\n[J3DInspector] The model has no VTX1 section.\n";

		return 1;
	}

	const Okari::J3DVTX1ParseResult vtx1Result =
		Okari::J3DVTX1Parser::Parse(
			document,
			*vtx1Section
		);

	if (!vtx1Result.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< vtx1Result.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DVTX1Data& vtx1 =
		vtx1Result.Data;

	const Okari::J3DVertexFormatDescriptor* positionFormat =
		vtx1.FindFormat(
			Okari::J3DVertexAttribute::Position
		);

	const Okari::J3DVertexArrayData* positionArray =
		vtx1.FindArray(
			Okari::J3DVertexAttribute::Position
		);

	if (
		positionFormat == nullptr ||
		positionArray == nullptr
		)
	{
		std::cerr
			<< "[J3DInspector] VTX1 has no position array.\n";

		return 1;
	}

	const std::uint64_t requiredPositionByteCount =
		static_cast<std::uint64_t>(
			inf1.VertexPositionCount
			) *
		positionFormat->ElementStride;

	if (positionArray->ByteSize < requiredPositionByteCount)
	{
		std::cerr
			<< "[J3DInspector] VTX1 position array contains only "
			<< positionArray->ByteSize
			<< " bytes, but INF1 requires "
			<< requiredPositionByteCount
			<< " bytes.\n";

		return 1;
	}

	const std::uint64_t bytesAfterPositionPayload =
		positionArray->ByteSize -
		requiredPositionByteCount;

	std::cout
		<< "\nVTX1\n"
		<< "Format table offset: 0x"
		<< std::hex
		<< std::uppercase
		<< vtx1.FormatTableOffset
		<< std::dec
		<< '\n'
		<< "Format descriptors: "
		<< vtx1.Formats.size()
		<< '\n'
		<< "Vertex arrays: "
		<< vtx1.Arrays.size()
		<< "\n\n";

	for (
		const Okari::J3DVertexArrayData& array :
		vtx1.Arrays
		)
	{
		const Okari::J3DVertexFormatDescriptor* format =
			vtx1.FindFormat(array.Attribute);

		if (format == nullptr)
		{
			std::cerr
				<< "[J3DInspector] Missing VTX1 format during display.\n";

			return 1;
		}

		std::cout
			<< Okari::ToString(array.Attribute)
			<< " | components="
			<< Okari::DescribeComponentCount(*format)
			<< " | type="
			<< Okari::DescribeComponentType(*format)
			<< " | shift="
			<< static_cast<unsigned int>(
				format->FractionalBits
				)
			<< " | offset=0x"
			<< std::hex
			<< std::uppercase
			<< array.Offset
			<< std::dec
			<< " | bytes="
			<< array.ByteSize
			<< " | stride="
			<< array.ElementStride
			<< " | capacity="
			<< array.ElementCapacity
			<< " | remainder="
			<< array.RemainderByteCount;

		if (
			array.Attribute ==
			Okari::J3DVertexAttribute::Position
			)
		{
			std::cout
				<< " | INF1 count="
				<< inf1.VertexPositionCount
				<< " | bytes after payload="
				<< bytesAfterPositionPayload;
		}

		std::cout << '\n';
	}

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

	const Okari::J3DSectionInfo* evp1Section = document.FindSection("EVP1");

	if (evp1Section == nullptr)
	{
		std::cerr << "\n[J3DInspector] The model has no EVP1 section.\n";

		return 1;
	}

	const Okari::J3DEVP1ParseResult evp1Result =
		Okari::J3DEVP1Parser::Parse(
			document,
			*evp1Section
		);

	if (!evp1Result.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< evp1Result.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DEVP1Data& evp1 = evp1Result.Data;

	const Okari::J3DPoseEvaluationResult poseResult =
		Okari::J3DPoseEvaluator::EvaluateRestPose(
			inf1,
			jnt1
		);

	if (!poseResult.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< poseResult.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DRestPose& restPose =
		poseResult.Pose;

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

			if (matrix.Parameter >= evp1.Envelopes.size())
			{
				std::cerr
					<< "[J3DInspector] DRW1 matrix "
					<< matrix.Index
					<< " references invalid envelope "
					<< matrix.Parameter
					<< '\n';

				return 1;
			}

			break;
		}
	}

	std::size_t invalidWeightSumCount = 0;

	for (const Okari::J3DEnvelope& envelope : evp1.Envelopes)
	{
		float weightSum = 0.0f;

		if (envelope.WeightedJoints.empty())
		{
			std::cerr
				<< "[J3DInspector] EVP1 envelope "
				<< envelope.Index
				<< " contains no joints.\n";

			return 1;
		}

		for (const Okari::J3DWeightedJoint& weightedJoint : envelope.WeightedJoints)
		{
			if (weightedJoint.JointIndex >= jnt1.Joints.size())
			{
				std::cerr
					<< "[J3DInspector] EVP1 envelope "
					<< envelope.Index
					<< " references invalid joint "
					<< weightedJoint.JointIndex
					<< '\n';

				return 1;
			}

			if (!std::isfinite(weightedJoint.Weight))
			{
				std::cerr
					<< "[J3DInspector] EVP1 envelope "
					<< envelope.Index
					<< " contains a non-finite weight.\n";

				return 1;
			}

			weightSum += weightedJoint.Weight;
		}

		if (std::abs(weightSum - 1.0f) > 0.001f)
			++invalidWeightSumCount;
	}

	const Okari::J3DDrawMatrixEvaluationResult
		drawMatrixResult =
		Okari::J3DDrawMatrixEvaluator::Evaluate(
			restPose,
			drw1,
			evp1
		);

	if (!drawMatrixResult.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< drawMatrixResult.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DDrawMatrixPalette& drawPalette = drawMatrixResult.Palette;

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

	std::cout
		<< "\nEVP1\n"
		<< "Envelope count: "
		<< evp1.EnvelopeCount
		<< '\n'
		<< "Total weighted joints: "
		<< evp1.TotalWeightedJointCount
		<< '\n'
		<< "Inverse-bind matrices: "
		<< evp1.InverseBindMatrices.size()
		<< '\n'
		<< "Non-normalized envelopes: "
		<< invalidWeightSumCount
		<< "\n\n";

	constexpr std::size_t MaximumDisplayedEnvelopes = 16;

	const std::size_t displayedEnvelopeCount =
		evp1.Envelopes.size() < MaximumDisplayedEnvelopes
		? evp1.Envelopes.size()
		: MaximumDisplayedEnvelopes;

	for (std::size_t envelopeIndex = 0; envelopeIndex < displayedEnvelopeCount; ++envelopeIndex)
	{
		const Okari::J3DEnvelope& envelope = evp1.Envelopes[envelopeIndex];

		std::cout
			<< '['
			<< envelope.Index
			<< "] ";

		for (std::size_t influenceIndex = 0; influenceIndex < envelope.WeightedJoints.size(); ++influenceIndex)
		{
			const Okari::J3DWeightedJoint& weightedJoint = envelope.WeightedJoints[influenceIndex];

			if (influenceIndex > 0)
				std::cout << " + ";

			std::cout
				<< jnt1.Joints[weightedJoint.JointIndex].Name
				<< '['
				<< weightedJoint.JointIndex
				<< "]="
				<< weightedJoint.Weight;
		}

		std::cout << '\n';
	}

	if (evp1.Envelopes.size() > displayedEnvelopeCount)
	{
		std::cout
			<< "... "
			<< evp1.Envelopes.size() - displayedEnvelopeCount
			<< " additional envelopes omitted\n";
	}

	std::cout
		<< "\nJ3D REST POSE\n"
		<< "Joint matrices: "
		<< restPose.Joints.size()
		<< '\n'
		<< "Root joints: "
		<< restPose.RootJointIndices.size()
		<< '\n'
		<< "Scaling rule: 0x"
		<< std::hex
		<< std::uppercase
		<< restPose.ScalingRule
		<< std::dec
		<< "\n\n";

	constexpr std::size_t MaximumDisplayedPoseJoints = 20;

	const std::size_t displayedPoseJointCount =
		restPose.Joints.size() < MaximumDisplayedPoseJoints
		? restPose.Joints.size()
		: MaximumDisplayedPoseJoints;

	for (
		std::size_t jointIndex = 0;
		jointIndex < displayedPoseJointCount;
		++jointIndex
		)
	{
		const Okari::J3DJointPose& jointPose =
			restPose.Joints[jointIndex];

		std::cout
			<< '['
			<< jointIndex
			<< "] "
			<< jnt1.Joints[jointIndex].Name
			<< " | parent=";

		if (jointPose.ParentJointIndex < 0)
		{
			std::cout << "<root>";
		}
		else
		{
			const std::size_t parentJointIndex =
				static_cast<std::size_t>(
					jointPose.ParentJointIndex
					);

			std::cout
				<< jnt1.Joints[parentJointIndex].Name
				<< '['
				<< parentJointIndex
				<< ']';
		}

		std::cout
			<< " | modelPosition=("
			<< jointPose.ModelMatrix[3][0]
			<< ", "
			<< jointPose.ModelMatrix[3][1]
			<< ", "
			<< jointPose.ModelMatrix[3][2]
			<< ")\n";
	}

	if (restPose.Joints.size() > displayedPoseJointCount)
	{
		std::cout
			<< "... "
			<< restPose.Joints.size() - displayedPoseJointCount
			<< " additional pose joints omitted\n";
	}

	std::cout
		<< "\nJ3D DRAW MATRIX PALETTE\n"
		<< "Raw DRW1 definitions: "
		<< drawPalette.RawDefinitionCount
		<< '\n'
		<< "Effective draw matrices: "
		<< drawPalette.EffectiveDefinitionCount
		<< '\n'
		<< "Duplicated envelope suffix removed: "
		<< (
			drawPalette.RemovedDuplicatedEnvelopeSuffix
			? "yes"
			: "no"
			)
		<< '\n'
		<< "Rigid matrices: "
		<< drawPalette.RigidMatrixCount
		<< '\n'
		<< "Envelope matrices: "
		<< drawPalette.EnvelopeMatrixCount
		<< '\n'
		<< "Maximum envelope identity error: "
		<< drawPalette.MaximumEnvelopeIdentityError
		<< "\n\n";

	constexpr std::size_t MaximumDisplayedResolvedEnvelopes = 8;

	std::size_t displayedResolvedEnvelopes = 0;

	for (
		const Okari::J3DResolvedDrawMatrix& matrix :
		drawPalette.Matrices
		)
	{
		if (matrix.Kind != Okari::J3DDrawMatrixKind::Envelope)
			continue;

		std::cout
			<< '['
			<< matrix.SourceDefinitionIndex
			<< "] Envelope["
			<< matrix.Parameter
			<< "]"
			<< " | translation=("
			<< matrix.Matrix[3][0]
			<< ", "
			<< matrix.Matrix[3][1]
			<< ", "
			<< matrix.Matrix[3][2]
			<< ')'
			<< " | identityError="
			<< matrix.BindPoseIdentityError
			<< '\n';

		++displayedResolvedEnvelopes;

		if (displayedResolvedEnvelopes >= MaximumDisplayedResolvedEnvelopes)
			break;
	}

	if (document.Data.size() != document.Header.DeclaredFileSize)
	{
		std::cout
			<< "\nWarning: the physical file contains trailing "
			<< "bytes after the declared J3D data.\n";
	}

	return 0;
}