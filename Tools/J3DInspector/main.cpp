#include "Formats/J3D/J3DDrawMatrixEvaluator.h"
#include "Formats/J3D/J3DShapeMatrixPaletteResolver.h"
#include "Formats/J3D/J3DFileReader.h"
#include "Formats/J3D/J3DPoseEvaluator.h"
#include "Formats/J3D/J3DVertexDecoder.h"
#include "Formats/J3D/J3DShapeDisplayListParser.h"
#include "Formats/J3D/J3DShapeVertexIndexScanner.h"
#include "Formats/J3D/J3DShapeVertexReferenceDecoder.h"
#include "Formats/J3D/J3DGeometryAssembler.h"
#include "Formats/J3D/J3DTriangleTopologyBuilder.h"

#include "Formats/J3D/J3DModelLoader.h"

#include "Formats/J3D/Sections/DRW1Parser.h"
#include "Formats/J3D/Sections/EVP1Parser.h"
#include "Formats/J3D/Sections/INF1Parser.h"
#include "Formats/J3D/Sections/JNT1Parser.h"
#include "Formats/J3D/Sections/VTX1Parser.h"
#include "Formats/J3D/Sections/SHP1Parser.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <set>

namespace
{
	constexpr std::size_t DisplayedVertexSampleCount = 3;
	constexpr std::size_t MaximumDisplayedMatrices = 32;
	constexpr std::size_t MaximumDisplayedEnvelopes = 16;
	constexpr std::size_t MaximumDisplayedPoseJoints = 20;
	constexpr std::size_t MaximumDisplayedResolvedEnvelopes = 8;

	struct DrawMatrixStats
	{
		std::size_t RigidMatrixCount = 0;
		std::size_t EnvelopeMatrixCount = 0;
	};

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

	const Okari::J3DSectionInfo* FindRequiredSection(
		const Okari::J3DDocument& document,
		const char* tag
	)
	{
		const Okari::J3DSectionInfo* section = document.FindSection(tag);

		if (section == nullptr)
		{
			std::cerr
				<< "\n[J3DInspector] The model has no "
				<< tag
				<< " section.\n";
		}

		return section;
	}

	void PrintDocumentSummary(const Okari::J3DDocument& document)
	{
		std::cout
			<< "File: "
			<< document.SourcePath.string()
			<< '\n'
			<< "Magic: "
			<< document.Header.Magic
			<< '\n'
			<< "Type: "
			<< ToString(document.Header.Type)
			<< '\n'
			<< "Declared size: "
			<< document.Header.DeclaredFileSize
			<< " bytes\n"
			<< "Actual size: "
			<< document.Data.size()
			<< " bytes\n"
			<< "Sections: "
			<< document.Header.SectionCount
			<< "\n\n";

		for (std::size_t index = 0; index < document.Sections.size(); ++index)
		{
			const Okari::J3DSectionInfo& section = document.Sections[index];

			std::cout
				<< '['
				<< index
				<< "] "
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
			PrintHierarchyNode(inf1, jnt1, childIndex, depth + 1);
		}
	}

	bool PrintVertexArrays(
		const Okari::J3DINF1Data& inf1,
		const Okari::J3DVTX1Data& vtx1
	)
	{
		const Okari::J3DVertexFormatDescriptor* positionFormat =
			vtx1.FindFormat(Okari::J3DVertexAttribute::Position);

		const Okari::J3DVertexArrayData* positionArray =
			vtx1.FindArray(Okari::J3DVertexAttribute::Position);

		if (positionFormat == nullptr || positionArray == nullptr)
		{
			std::cerr << "[J3DInspector] VTX1 has no position array.\n";
			return false;
		}

		const std::uint64_t requiredPositionByteCount =
			static_cast<std::uint64_t>(inf1.VertexPositionCount) *
			positionFormat->ElementStride;

		if (positionArray->ByteSize < requiredPositionByteCount)
		{
			std::cerr
				<< "[J3DInspector] VTX1 position array contains only "
				<< positionArray->ByteSize
				<< " bytes, but INF1 requires "
				<< requiredPositionByteCount
				<< " bytes.\n";

			return false;
		}

		const std::uint64_t bytesAfterPositionPayload =
			positionArray->ByteSize - requiredPositionByteCount;

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

		for (const Okari::J3DVertexArrayData& array : vtx1.Arrays)
		{
			const Okari::J3DVertexFormatDescriptor* format =
				vtx1.FindFormat(array.Attribute);

			if (format == nullptr)
			{
				std::cerr
					<< "[J3DInspector] Missing VTX1 format during display.\n";

				return false;
			}

			std::cout
				<< Okari::ToString(array.Attribute)
				<< " | components="
				<< Okari::DescribeComponentCount(*format)
				<< " | type="
				<< Okari::DescribeComponentType(*format)
				<< " | shift="
				<< static_cast<unsigned int>(format->FractionalBits)
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

			if (array.Attribute == Okari::J3DVertexAttribute::Position)
			{
				std::cout
					<< " | INF1 count="
					<< inf1.VertexPositionCount
					<< " | bytes after payload="
					<< bytesAfterPositionPayload;
			}

			std::cout << '\n';
		}

		return true;
	}

	bool PrintDecodedVertexData(
		const Okari::J3DDecodedVertexData& vertexData
	)
	{
		if (vertexData.Positions.empty())
		{
			std::cerr << "[J3DInspector] VTX1 decoded no positions.\n";
			return false;
		}

		glm::vec3 minimumPosition = vertexData.Positions.front();
		glm::vec3 maximumPosition = vertexData.Positions.front();

		for (const glm::vec3& position : vertexData.Positions)
		{
			minimumPosition.x = std::min(minimumPosition.x, position.x);
			minimumPosition.y = std::min(minimumPosition.y, position.y);
			minimumPosition.z = std::min(minimumPosition.z, position.z);

			maximumPosition.x = std::max(maximumPosition.x, position.x);
			maximumPosition.y = std::max(maximumPosition.y, position.y);
			maximumPosition.z = std::max(maximumPosition.z, position.z);
		}

		std::size_t zeroLengthNormalCount = 0;
		float minimumNonZeroNormalLength = std::numeric_limits<float>::max();
		float maximumNormalLength = 0.0f;

		for (const glm::vec3& normal : vertexData.Normals)
		{
			const float length = glm::length(normal);

			if (length <= 0.000001f)
			{
				++zeroLengthNormalCount;
				continue;
			}

			minimumNonZeroNormalLength =
				std::min(minimumNonZeroNormalLength, length);

			maximumNormalLength = std::max(maximumNormalLength, length);
		}

		std::cout
			<< "\nVTX1 DECODED DATA\n"
			<< "Positions: "
			<< vertexData.Positions.size()
			<< '\n'
			<< "Normals decoded from complete records: "
			<< vertexData.Normals.size()
			<< '\n'
			<< "NBT frames: "
			<< vertexData.NBTFrames.size()
			<< '\n'
			<< "CLR0 entries: "
			<< vertexData.Colors[0].size()
			<< '\n'
			<< "CLR1 entries: "
			<< vertexData.Colors[1].size()
			<< '\n';

		for (std::size_t channel = 0; channel < vertexData.TexCoords.size(); ++channel)
		{
			if (vertexData.TexCoords[channel].empty())
				continue;

			std::cout
				<< "TEX"
				<< channel
				<< " entries: "
				<< vertexData.TexCoords[channel].size()
				<< '\n';
		}

		std::cout
			<< "Position bounds: min=("
			<< minimumPosition.x
			<< ", "
			<< minimumPosition.y
			<< ", "
			<< minimumPosition.z
			<< ") max=("
			<< maximumPosition.x
			<< ", "
			<< maximumPosition.y
			<< ", "
			<< maximumPosition.z
			<< ")\n";

		if (!vertexData.Normals.empty())
		{
			std::cout
				<< "Zero-length normal records: "
				<< zeroLengthNormalCount
				<< '\n';

			if (zeroLengthNormalCount < vertexData.Normals.size())
			{
				std::cout
					<< "Non-zero normal length range: "
					<< minimumNonZeroNormalLength
					<< " -> "
					<< maximumNormalLength
					<< '\n';
			}
		}

		const std::size_t positionSampleCount = std::min(
			vertexData.Positions.size(),
			DisplayedVertexSampleCount
		);

		std::cout << "\nPosition samples\n";

		for (std::size_t index = 0; index < positionSampleCount; ++index)
		{
			const glm::vec3& position = vertexData.Positions[index];

			std::cout
				<< '['
				<< index
				<< "] ("
				<< position.x
				<< ", "
				<< position.y
				<< ", "
				<< position.z
				<< ")\n";
		}

		const std::size_t normalSampleCount = std::min(
			vertexData.Normals.size(),
			DisplayedVertexSampleCount
		);

		std::cout << "\nNormal samples\n";

		for (std::size_t index = 0; index < normalSampleCount; ++index)
		{
			const glm::vec3& normal = vertexData.Normals[index];

			std::cout
				<< '['
				<< index
				<< "] ("
				<< normal.x
				<< ", "
				<< normal.y
				<< ", "
				<< normal.z
				<< ") length="
				<< glm::length(normal)
				<< '\n';
		}

		if (!vertexData.Colors[0].empty())
		{
			const std::size_t colorSampleCount = std::min(
				vertexData.Colors[0].size(),
				DisplayedVertexSampleCount
			);

			std::cout << "\nCLR0 samples\n";

			for (std::size_t index = 0; index < colorSampleCount; ++index)
			{
				const Okari::J3DColorRGBA8& color = vertexData.Colors[0][index];

				std::cout
					<< '['
					<< index
					<< "] ("
					<< static_cast<unsigned int>(color.R)
					<< ", "
					<< static_cast<unsigned int>(color.G)
					<< ", "
					<< static_cast<unsigned int>(color.B)
					<< ", "
					<< static_cast<unsigned int>(color.A)
					<< ")\n";
			}
		}

		if (!vertexData.TexCoords[0].empty())
		{
			const std::size_t texCoordSampleCount = std::min(
				vertexData.TexCoords[0].size(),
				DisplayedVertexSampleCount
			);

			std::cout << "\nTEX0 samples\n";

			for (std::size_t index = 0; index < texCoordSampleCount; ++index)
			{
				const glm::vec2& texCoord = vertexData.TexCoords[0][index];

				std::cout
					<< '['
					<< index
					<< "] ("
					<< texCoord.x
					<< ", "
					<< texCoord.y
					<< ")\n";
			}
		}

		return true;
	}

	void PrintJoints(const Okari::J3DJNT1Data& jnt1)
	{
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
	}

	bool ValidateHierarchyJointReferences(
		const Okari::J3DINF1Data& inf1,
		const Okari::J3DJNT1Data& jnt1
	)
	{
		for (const Okari::J3DHierarchyNode& node : inf1.Nodes)
		{
			if (
				node.Type == Okari::J3DHierarchyEntryType::Joint &&
				node.Index >= jnt1.Joints.size()
				)
			{
				std::cerr
					<< "[J3DInspector] INF1 references invalid joint "
					<< node.Index
					<< '\n';

				return false;
			}
		}

		return true;
	}

	void PrintHierarchy(
		const Okari::J3DINF1Data& inf1,
		const Okari::J3DJNT1Data& jnt1
	)
	{
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
			<< "\n\n"
			<< "\nINF1 hierarchy with JNT1 names\n\n";

		for (const std::uint32_t rootNodeIndex : inf1.RootNodes)
		{
			PrintHierarchyNode(inf1, jnt1, rootNodeIndex, 0);
		}
	}

	bool ValidateDrawMatrices(
		const Okari::J3DDRW1Data& drw1,
		const Okari::J3DEVP1Data& evp1,
		const Okari::J3DJNT1Data& jnt1,
		DrawMatrixStats& stats
	)
	{
		for (const Okari::J3DDrawMatrixDefinition& matrix : drw1.Matrices)
		{
			switch (matrix.Kind)
			{
			case Okari::J3DDrawMatrixKind::Joint:
				++stats.RigidMatrixCount;

				if (matrix.Parameter >= jnt1.Joints.size())
				{
					std::cerr
						<< "[J3DInspector] DRW1 matrix "
						<< matrix.Index
						<< " references invalid joint "
						<< matrix.Parameter
						<< '\n';

					return false;
				}

				break;

			case Okari::J3DDrawMatrixKind::Envelope:
				++stats.EnvelopeMatrixCount;

				if (matrix.Parameter >= evp1.Envelopes.size())
				{
					std::cerr
						<< "[J3DInspector] DRW1 matrix "
						<< matrix.Index
						<< " references invalid envelope "
						<< matrix.Parameter
						<< '\n';

					return false;
				}

				break;
			}
		}

		return true;
	}

	bool ValidateEnvelopes(
		const Okari::J3DEVP1Data& evp1,
		const Okari::J3DJNT1Data& jnt1,
		std::size_t& invalidWeightSumCount
	)
	{
		for (const Okari::J3DEnvelope& envelope : evp1.Envelopes)
		{
			float weightSum = 0.0f;

			if (envelope.WeightedJoints.empty())
			{
				std::cerr
					<< "[J3DInspector] EVP1 envelope "
					<< envelope.Index
					<< " contains no joints.\n";

				return false;
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

					return false;
				}

				if (!std::isfinite(weightedJoint.Weight))
				{
					std::cerr
						<< "[J3DInspector] EVP1 envelope "
						<< envelope.Index
						<< " contains a non-finite weight.\n";

					return false;
				}

				weightSum += weightedJoint.Weight;
			}

			if (std::abs(weightSum - 1.0f) > 0.001f)
				++invalidWeightSumCount;
		}

		return true;
	}

	bool PrintDrawMatrices(
		const Okari::J3DDRW1Data& drw1,
		const Okari::J3DJNT1Data& jnt1,
		const DrawMatrixStats& stats
	)
	{
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
			<< stats.RigidMatrixCount
			<< '\n'
			<< "Weighted envelope matrices: "
			<< stats.EnvelopeMatrixCount
			<< "\n\n";

		if (drw1.Matrices.size() != drw1.MatrixCount)
		{
			std::cerr
				<< "[J3DInspector] DRW1 parsed matrix count does not "
				<< "match the declared matrix count.\n";

			return false;
		}

		const std::size_t displayedMatrixCount = std::min(
			drw1.Matrices.size(),
			MaximumDisplayedMatrices
		);

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

		return true;
	}

	void PrintEnvelopes(
		const Okari::J3DEVP1Data& evp1,
		const Okari::J3DJNT1Data& jnt1,
		std::size_t invalidWeightSumCount
	)
	{
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

		const std::size_t displayedEnvelopeCount = std::min(
			evp1.Envelopes.size(),
			MaximumDisplayedEnvelopes
		);

		for (std::size_t envelopeIndex = 0;
			envelopeIndex < displayedEnvelopeCount;
			++envelopeIndex)
		{
			const Okari::J3DEnvelope& envelope = evp1.Envelopes[envelopeIndex];

			std::cout
				<< '['
				<< envelope.Index
				<< "] ";

			for (std::size_t influenceIndex = 0;
				influenceIndex < envelope.WeightedJoints.size();
				++influenceIndex)
			{
				const Okari::J3DWeightedJoint& weightedJoint =
					envelope.WeightedJoints[influenceIndex];

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
	}

	void PrintRestPose(
		const Okari::J3DRestPose& restPose,
		const Okari::J3DJNT1Data& jnt1
	)
	{
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

		const std::size_t displayedPoseJointCount = std::min(
			restPose.Joints.size(),
			MaximumDisplayedPoseJoints
		);

		for (std::size_t jointIndex = 0;
			jointIndex < displayedPoseJointCount;
			++jointIndex)
		{
			const Okari::J3DJointPose& jointPose = restPose.Joints[jointIndex];

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
					static_cast<std::size_t>(jointPose.ParentJointIndex);

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
	}

	void PrintDrawMatrixPalette(const Okari::J3DDrawMatrixPalette& drawPalette)
	{
		std::cout
			<< "\nJ3D DRAW MATRIX PALETTE\n"
			<< "Raw DRW1 definitions: "
			<< drawPalette.RawDefinitionCount
			<< '\n'
			<< "Effective draw matrices: "
			<< drawPalette.EffectiveDefinitionCount
			<< '\n'
			<< "Duplicated envelope suffix removed: "
			<< (drawPalette.RemovedDuplicatedEnvelopeSuffix ? "yes" : "no")
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

		std::size_t displayedResolvedEnvelopes = 0;

		for (const Okari::J3DResolvedDrawMatrix& matrix : drawPalette.Matrices)
		{
			if (matrix.Kind != Okari::J3DDrawMatrixKind::Envelope)
				continue;

			std::cout
				<< '['
				<< matrix.SourceDefinitionIndex
				<< "] Envelope["
				<< matrix.Parameter
				<< ']'
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
	}

	void PrintShapeSectionSummary(
		const Okari::J3DSHP1Data& shp1
	)
	{
		std::size_t nonIdentityRemapCount = 0;

		for (
			std::size_t logicalIndex = 0;
			logicalIndex < shp1.RemapTable.size();
			++logicalIndex
			)
		{
			if (shp1.RemapTable[logicalIndex] != logicalIndex)
				++nonIdentityRemapCount;
		}

		std::cout
			<< "\nSHP1\n"
			<< "Shape count: "
			<< shp1.ShapeCount
			<< '\n'
			<< "Padding: 0x"
			<< std::hex
			<< std::uppercase
			<< shp1.Padding
			<< '\n'
			<< "Shape init data offset: 0x"
			<< shp1.ShapeInitDataOffset
			<< '\n'
			<< "Remap table offset: 0x"
			<< shp1.RemapTableOffset
			<< '\n'
			<< "Name table offset: 0x"
			<< shp1.NameTableOffset
			<< '\n'
			<< "Vertex descriptor table offset: 0x"
			<< shp1.VertexDescriptorTableOffset
			<< '\n'
			<< "Matrix table offset: 0x"
			<< shp1.MatrixTableOffset
			<< '\n'
			<< "Display-list data offset: 0x"
			<< shp1.DisplayListDataOffset
			<< '\n'
			<< "Matrix init data offset: 0x"
			<< shp1.MatrixInitDataOffset
			<< '\n'
			<< "Draw init data offset: 0x"
			<< shp1.DrawInitDataOffset
			<< std::dec
			<< '\n'
			<< "Remap entries: "
			<< shp1.RemapTable.size()
			<< '\n'
			<< "Non-identity remap entries: "
			<< nonIdentityRemapCount
			<< '\n';
	}

	const char* ToString(
		Okari::J3DShapeMatrixType type
	)
	{
		switch (type)
		{
		case Okari::J3DShapeMatrixType::SingleMatrix:
			return "SingleMatrix";

		case Okari::J3DShapeMatrixType::Billboard:
			return "Billboard";

		case Okari::J3DShapeMatrixType::YBillboard:
			return "YBillboard";

		case Okari::J3DShapeMatrixType::MultiMatrix:
			return "MultiMatrix";

		default:
			return "Unknown";
		}
	}

	void PrintShapeRecords(
		const Okari::J3DSHP1Data& shp1
	)
	{
		std::array<std::size_t, 4> matrixTypeCounts{};
		std::size_t totalMatrixGroupCount = 0;
		std::size_t unexpectedPaddingCount = 0;

		for (const Okari::J3DShapeRecord& shape : shp1.Shapes)
		{
			const std::size_t matrixTypeIndex =
				static_cast<std::size_t>(
					shape.MatrixType
					);

			++matrixTypeCounts[matrixTypeIndex];

			totalMatrixGroupCount +=
				shape.MatrixGroupCount;

			if (
				shape.Padding0x01 != 0xFF ||
				shape.Padding0x0A != 0xFFFF
				)
			{
				++unexpectedPaddingCount;
			}
		}

		std::cout
			<< "\nSHP1 SHAPE RECORDS\n"
			<< "Decoded records: "
			<< shp1.Shapes.size()
			<< '\n'
			<< "Total matrix groups: "
			<< totalMatrixGroupCount
			<< '\n'
			<< "Single-matrix shapes: "
			<< matrixTypeCounts[0]
			<< '\n'
			<< "Billboard shapes: "
			<< matrixTypeCounts[1]
			<< '\n'
			<< "Y-billboard shapes: "
			<< matrixTypeCounts[2]
			<< '\n'
			<< "Multi-matrix shapes: "
			<< matrixTypeCounts[3]
			<< '\n'
			<< "Unexpected padding records: "
			<< unexpectedPaddingCount
			<< '\n';

		for (const Okari::J3DShapeRecord& shape : shp1.Shapes)
		{
			std::cout
				<< '['
				<< shape.LogicalIndex
				<< "] data="
				<< shape.DataIndex
				<< " type="
				<< ToString(shape.MatrixType)
				<< " groups="
				<< shape.MatrixGroupCount
				<< " vtxDescOffset=0x"
				<< std::hex
				<< std::uppercase
				<< shape.VertexDescriptorListOffset
				<< std::dec
				<< " matrixInitIndex="
				<< shape.MatrixInitDataIndex
				<< " drawInitIndex="
				<< shape.DrawInitDataIndex
				<< " radius="
				<< shape.BoundingSphereRadius
				<< '\n'
				<< "    bounds min=("
				<< shape.Bounds.Minimum.X
				<< ", "
				<< shape.Bounds.Minimum.Y
				<< ", "
				<< shape.Bounds.Minimum.Z
				<< ") max=("
				<< shape.Bounds.Maximum.X
				<< ", "
				<< shape.Bounds.Maximum.Y
				<< ", "
				<< shape.Bounds.Maximum.Z
				<< ")\n";
		}
	}

	void PrintShapeMatrixGroups(
		const Okari::J3DSHP1Data& shp1
	)
	{
		constexpr std::uint16_t ReusePreviousMatrix =
			0xFFFF;

		std::size_t decodedGroupCount = 0;
		std::size_t totalMatrixTableEntries = 0;
		std::size_t reusedMatrixEntryCount = 0;
		std::size_t zeroMatrixCountGroups = 0;
		std::size_t singleMatrixMismatches = 0;
		std::uint64_t totalDisplayListBytes = 0;

		std::cout
			<< "\nSHP1 MATRIX GROUPS\n";

		for (const Okari::J3DShapeRecord& shape : shp1.Shapes)
		{
			for (
				const Okari::J3DShapeMatrixGroup& group :
				shape.MatrixGroups
				)
			{
				++decodedGroupCount;

				totalMatrixTableEntries +=
					group.RawMatrixTable.size();

				totalDisplayListBytes +=
					group.DisplayListSize;

				if (group.UseMatrixCount == 0)
					++zeroMatrixCountGroups;

				for (
					const std::uint16_t matrixIndex :
				group.RawMatrixTable
					)
				{
					if (
						matrixIndex ==
						ReusePreviousMatrix
						)
					{
						++reusedMatrixEntryCount;
					}
				}

				if (
					shape.MatrixType ==
					Okari::J3DShapeMatrixType::SingleMatrix
					)
				{
					const bool tableMatches =
						group.RawMatrixTable.size() == 1 &&
						group.RawMatrixTable[0] ==
						group.UseMatrixIndex;

					if (!tableMatches)
						++singleMatrixMismatches;
				}

				std::cout
					<< "shape["
					<< shape.LogicalIndex
					<< "] group["
					<< group.LocalIndex
					<< "]"
					<< " matrixRecord="
					<< group.MatrixInitDataIndex
					<< " drawRecord="
					<< group.DrawInitDataIndex
					<< " useMatrix="
					<< group.UseMatrixIndex
					<< " matrixCount="
					<< group.UseMatrixCount
					<< " firstMatrix="
					<< group.FirstUseMatrixIndex
					<< " displayListOffset=0x"
					<< std::hex
					<< std::uppercase
					<< group.DisplayListOffset
					<< " displayListSize=0x"
					<< group.DisplayListSize
					<< std::dec
					<< '\n'
					<< "    matrixTable=[";

				for (
					std::size_t matrixSlot = 0;
					matrixSlot <
					group.RawMatrixTable.size();
					++matrixSlot
					)
				{
					if (matrixSlot > 0)
						std::cout << ", ";

					const std::uint16_t matrixIndex =
						group.RawMatrixTable[matrixSlot];

					if (
						matrixIndex ==
						ReusePreviousMatrix
						)
					{
						std::cout << "0xFFFF";
					}
					else
					{
						std::cout << matrixIndex;
					}
				}

				std::cout << "]\n";
			}
		}

		std::cout
			<< "Decoded matrix groups: "
			<< decodedGroupCount
			<< '\n'
			<< "Matrix-table entries: "
			<< totalMatrixTableEntries
			<< '\n'
			<< "Reuse entries (0xFFFF): "
			<< reusedMatrixEntryCount
			<< '\n'
			<< "Zero-count groups: "
			<< zeroMatrixCountGroups
			<< '\n'
			<< "Single-matrix table mismatches: "
			<< singleMatrixMismatches
			<< '\n'
			<< "Total referenced display-list bytes: "
			<< totalDisplayListBytes
			<< '\n';
	}

	void PrintShapeVertexDescriptors(
		const Okari::J3DSHP1Data& shp1,
		const Okari::J3DVTX1Data& vtx1
	)
	{
		std::set<std::uint16_t> uniqueDescriptorOffsets;

		std::size_t totalDescriptors = 0;
		std::size_t missingPositionShapes = 0;
		std::size_t nonDirectMatrixDescriptors = 0;
		std::size_t missingIndexedArrays = 0;
		std::size_t nonZeroTerminatorTypes = 0;

		std::cout
			<< "\nSHP1 VERTEX DESCRIPTORS\n";

		for (const Okari::J3DShapeRecord& shape : shp1.Shapes)
		{
			uniqueDescriptorOffsets.insert(
				shape.VertexDescriptorListOffset
			);

			totalDescriptors +=
				shape.VertexDescriptors.size();

			bool hasPosition = false;

			if (shape.VertexDescriptorTerminatorType != 0)
				++nonZeroTerminatorTypes;

			std::cout
				<< "shape["
				<< shape.LogicalIndex
				<< "] descriptorOffset=0x"
				<< std::hex
				<< std::uppercase
				<< shape.VertexDescriptorListOffset
				<< " terminatorType=0x"
				<< shape.VertexDescriptorTerminatorType
				<< std::dec
				<< " descriptors="
				<< shape.VertexDescriptors.size()
				<< '\n'
				<< "    ";

			for (
				std::size_t descriptorIndex = 0;
				descriptorIndex <
				shape.VertexDescriptors.size();
				++descriptorIndex
				)
			{
				const Okari::J3DShapeVertexDescriptor&
					descriptor =
					shape.VertexDescriptors[
						descriptorIndex
					];

				if (descriptorIndex > 0)
					std::cout << ", ";

				std::cout
					<< Okari::ToString(
						descriptor.Attribute
					)
					<< '='
					<< Okari::ToString(
						descriptor.InputType
					);

				if (
					descriptor.Attribute ==
					Okari::J3DVertexAttribute::Position
					)
				{
					hasPosition = true;
				}

				if (
					Okari::IsMatrixIndexAttribute(
						descriptor.Attribute
					) &&
					descriptor.InputType !=
					Okari::J3DVertexInputType::Direct
					)
				{
					++nonDirectMatrixDescriptors;
				}

				const bool usesIndexedArray =
					Okari::IsVertexArrayAttribute(
						descriptor.Attribute
					) &&
					(
						descriptor.InputType ==
						Okari::J3DVertexInputType::Index8 ||
						descriptor.InputType ==
						Okari::J3DVertexInputType::Index16
						);

				if (
					usesIndexedArray &&
					vtx1.FindArray(
						descriptor.Attribute
					) == nullptr
					)
				{
					++missingIndexedArrays;
				}
			}

			if (!hasPosition)
				++missingPositionShapes;

			std::cout << '\n';
		}

		std::cout
			<< "Unique descriptor lists: "
			<< uniqueDescriptorOffsets.size()
			<< '\n'
			<< "Descriptors across shapes: "
			<< totalDescriptors
			<< '\n'
			<< "Shapes missing POS: "
			<< missingPositionShapes
			<< '\n'
			<< "Non-direct matrix descriptors: "
			<< nonDirectMatrixDescriptors
			<< '\n'
			<< "Indexed descriptors without VTX1 array: "
			<< missingIndexedArrays
			<< '\n'
			<< "Non-zero terminator types: "
			<< nonZeroTerminatorTypes
			<< '\n';
	}

	void PrintShapeDisplayListSummary(
		const Okari::J3DShapeDisplayListData& data
	)
	{
		std::size_t primitiveCount = 0;
		std::size_t vertexCount = 0;
		std::size_t embeddedNoopCount = 0;
		std::size_t trailingNoopCount = 0;

		std::array<std::size_t, 8> primitiveTypeCounts{};

		std::cout
			<< "\nSHP1 DISPLAY LIST STRUCTURE\n";

		for (
			const Okari::J3DShapeDisplayListGroup& group :
			data.Groups
			)
		{
			primitiveCount +=
				group.Primitives.size();

			embeddedNoopCount +=
				group.EmbeddedNoopByteCount;

			trailingNoopCount +=
				group.TrailingNoopByteCount;

			for (
				const Okari::J3DShapePrimitiveRecord& primitive :
				group.Primitives
				)
			{
				vertexCount += primitive.VertexCount;

				const std::size_t primitiveIndex =
					(
						static_cast<std::uint8_t>(
							primitive.Type
							) -
						0x80
						) /
					0x08;

				if (
					primitiveIndex <
					primitiveTypeCounts.size()
					)
				{
					++primitiveTypeCounts[
						primitiveIndex
					];
				}
			}

			std::cout
				<< "shape["
				<< group.ShapeIndex
				<< "] group["
				<< group.GroupIndex
				<< "] vertexSize="
				<< group.EncodedVertexSize
				<< " primitives="
				<< group.Primitives.size()
				<< " trailingNoops="
				<< group.TrailingNoopByteCount
				<< " embeddedNoops="
				<< group.EmbeddedNoopByteCount
				<< '\n';
		}

		std::cout
			<< "Decoded groups: "
			<< data.Groups.size()
			<< '\n'
			<< "Primitive records: "
			<< primitiveCount
			<< '\n'
			<< "Referenced GX vertices: "
			<< vertexCount
			<< '\n'
			<< "QUADS: "
			<< primitiveTypeCounts[0]
			<< '\n'
			<< "QUAD_STRIP: "
			<< primitiveTypeCounts[1]
			<< '\n'
			<< "TRIANGLES: "
			<< primitiveTypeCounts[2]
			<< '\n'
			<< "TRIANGLE_STRIP: "
			<< primitiveTypeCounts[3]
			<< '\n'
			<< "TRIANGLE_FAN: "
			<< primitiveTypeCounts[4]
			<< '\n'
			<< "LINES: "
			<< primitiveTypeCounts[5]
			<< '\n'
			<< "LINE_STRIP: "
			<< primitiveTypeCounts[6]
			<< '\n'
			<< "POINTS: "
			<< primitiveTypeCounts[7]
			<< '\n'
			<< "Embedded NOOP bytes: "
			<< embeddedNoopCount
			<< '\n'
			<< "Trailing NOOP bytes: "
			<< trailingNoopCount
			<< '\n';
	}

	void PrintIndexRange(
		const char* name,
		const Okari::J3DVertexIndexRange& range
	)
	{
		std::cout
			<< name
			<< ": ";

		if (!range.Used)
		{
			std::cout << "unused\n";
			return;
		}

		std::cout
			<< "references="
			<< range.ReferenceCount
			<< " maxIndex="
			<< range.MaximumIndex
			<< " requiredCount="
			<< range.RequiredElementCount()
			<< '\n';
	}

	void PrintMatrixIndexUsage(
		const char* name,
		const Okari::J3DDirectMatrixIndexUsage& usage
	)
	{
		std::cout
			<< name
			<< ": ";

		if (!usage.Used)
		{
			std::cout << "unused\n";
			return;
		}

		std::set<unsigned int> logicalSlots;

		std::cout
			<< "references="
			<< usage.ReferenceCount
			<< " minRaw="
			<< static_cast<unsigned int>(
				usage.MinimumRawValue
				)
			<< " maxRaw="
			<< static_cast<unsigned int>(
				usage.MaximumRawValue
				)
			<< " distinct="
			<< usage.DistinctValueCount()
			<< " nonMultipleOf3="
			<< usage.NonMultipleOfThreeCount
			<< '\n'
			<< "    rawValues=[";

		bool first = true;

		for (
			std::size_t rawValue = 0;
			rawValue < usage.SeenRawValues.size();
			++rawValue
			)
		{
			if (!usage.SeenRawValues[rawValue])
				continue;

			if (!first)
				std::cout << ", ";

			std::cout << rawValue;
			first = false;

			logicalSlots.insert(
				static_cast<unsigned int>(
					rawValue / 3
					)
			);
		}

		std::cout
			<< "]\n"
			<< "    candidateSlots=[";

		first = true;

		for (
			const unsigned int slot :
		logicalSlots
			)
		{
			if (!first)
				std::cout << ", ";

			std::cout << slot;
			first = false;
		}

		std::cout << "]\n";
	}

	void PrintShapeVertexIndexUsage(
		const Okari::J3DShapeVertexIndexUsage& usage,
		const Okari::J3DINF1Data& inf1
	)
	{
		std::cout
			<< "\nSHP1 VERTEX INDEX USAGE\n";

		std::cout
			<< "\nDirect matrix indices\n";

		PrintMatrixIndexUsage(
			"PNMTXIDX",
			usage.MatrixIndices.Position
		);

		for (
			std::size_t channel = 0;
			channel <
			usage.MatrixIndices.TexCoords.size();
			++channel
			)
		{
			const std::string name =
				"TEX" +
				std::to_string(channel) +
				"MTXIDX";

			PrintMatrixIndexUsage(
				name.c_str(),
				usage.MatrixIndices.TexCoords[channel]
			);
		}

		std::cout
			<< "\nIndexed vertex arrays\n";

		PrintIndexRange("POS", usage.Position);
		PrintIndexRange("NRM", usage.Normal);
		PrintIndexRange("NBT", usage.NBT);

		PrintIndexRange("CLR0", usage.Colors[0]);
		PrintIndexRange("CLR1", usage.Colors[1]);

		for (
			std::size_t channel = 0;
			channel < usage.TexCoords.size();
			++channel
			)
		{
			const std::string name =
				"TEX" +
				std::to_string(channel);

			PrintIndexRange(
				name.c_str(),
				usage.TexCoords[channel]
			);
		}

		std::cout
			<< "INF1 position count: "
			<< inf1.VertexPositionCount
			<< '\n'
			<< "SHP1 required position count: "
			<< usage.Position.RequiredElementCount()
			<< '\n';
	}

	void PrintShapeMatrixPalette(
		const Okari::J3DShapeMatrixPalette& palette,
		const Okari::J3DDrawMatrixPalette& drawPalette
	)
	{
		std::cout
			<< "\nSHP1 RESOLVED MATRIX PALETTES\n";

		for (
			const Okari::J3DResolvedShapeMatrixGroup& group :
			palette.Groups
			)
		{
			std::cout
				<< "shape["
				<< group.ShapeIndex
				<< "] group["
				<< group.GroupIndex
				<< "] loaded="
				<< group.LoadedSlotCount
				<< " reused="
				<< group.ReusedSlotCount
				<< " effective=[";

			for (
				std::size_t slot = 0;
				slot <
				group.DrawMatrixIndices.size();
				++slot
				)
			{
				if (slot > 0)
					std::cout << ", ";

				std::cout
					<< group.DrawMatrixIndices[slot];
			}

			std::cout << "]\n";
		}

		std::cout
			<< "Resolved groups: "
			<< palette.Groups.size()
			<< '\n'
			<< "Directly loaded slots: "
			<< palette.LoadedSlotCount
			<< '\n'
			<< "Reused slots: "
			<< palette.ReusedSlotCount
			<< '\n'
			<< "Maximum resolved draw-matrix index: "
			<< palette.MaximumDrawMatrixIndex
			<< '\n'
			<< "Available draw matrices: "
			<< drawPalette.Matrices.size()
			<< '\n';
	}

	void PrintShapeVertexReferences(
		const Okari::J3DShapeVertexReferenceData& data,
		std::size_t drawMatrixCount
	)
	{
		std::size_t groupCount = 0;
		std::size_t primitiveCount = 0;
		std::size_t vertexCount = 0;

		std::size_t explicitMatrixVertices = 0;
		std::size_t implicitMatrixVertices = 0;
		std::size_t invalidDrawMatrices = 0;

		std::size_t sampleCount = 0;

		std::cout
			<< "\nSHP1 DECODED VERTEX REFERENCES\n";

		for (
			const Okari::J3DDecodedShapeGroup& group :
			data.Groups
			)
		{
			++groupCount;

			for (
				const Okari::J3DDecodedShapePrimitive& primitive :
				group.Primitives
				)
			{
				++primitiveCount;

				for (
					const Okari::J3DShapeVertexReference& vertex :
					primitive.Vertices
					)
				{
					++vertexCount;

					if (
						vertex.RawPositionMatrixIndex.has_value()
						)
					{
						++explicitMatrixVertices;
					}
					else
					{
						++implicitMatrixVertices;
					}

					if (
						vertex.DrawMatrixIndex >=
						drawMatrixCount
						)
					{
						++invalidDrawMatrices;
					}

					if (sampleCount < 12)
					{
						std::cout
							<< "shape["
							<< group.ShapeIndex
							<< "] group["
							<< group.GroupIndex
							<< "]"
							<< " POS="
							<< *vertex.PositionIndex
							<< " NRM=";

						if (
							vertex.NormalIndices[0].
							has_value()
							)
						{
							std::cout
								<< *vertex.NormalIndices[0];
						}
						else
						{
							std::cout << "none";
						}

						std::cout
							<< " CLR0=";

						if (
							vertex.ColorIndices[0].
							has_value()
							)
						{
							std::cout
								<< *vertex.ColorIndices[0];
						}
						else
						{
							std::cout << "none";
						}

						std::cout
							<< " TEX0=";

						if (
							vertex.TexCoordIndices[0].
							has_value()
							)
						{
							std::cout
								<< *vertex.TexCoordIndices[0];
						}
						else
						{
							std::cout << "none";
						}

						std::cout
							<< " matrixSlot="
							<< static_cast<unsigned int>(
								vertex.PositionMatrixSlot
								)
							<< " drawMatrix="
							<< vertex.DrawMatrixIndex;

						if (
							vertex.RawPositionMatrixIndex.
							has_value()
							)
						{
							std::cout
								<< " rawPNMTXIDX="
								<< static_cast<unsigned int>(
									*vertex.
									RawPositionMatrixIndex
									);
						}
						else
						{
							std::cout
								<< " rawPNMTXIDX=implicit";
						}

						std::cout << '\n';

						++sampleCount;
					}
				}
			}
		}

		std::cout
			<< "Decoded groups: "
			<< groupCount
			<< '\n'
			<< "Decoded primitives: "
			<< primitiveCount
			<< '\n'
			<< "Decoded vertex references: "
			<< vertexCount
			<< '\n'
			<< "Vertices with PNMTXIDX: "
			<< explicitMatrixVertices
			<< '\n'
			<< "Vertices with implicit matrix: "
			<< implicitMatrixVertices
			<< '\n'
			<< "Invalid draw-matrix references: "
			<< invalidDrawMatrices
			<< '\n';
	}

	void PrintAssembledGeometry(
		const Okari::J3DAssembledGeometry& geometry
	)
	{
		std::size_t primitiveCount = 0;
		std::size_t vertexCount = 0;

		std::size_t verticesWithNormal = 0;
		std::size_t verticesWithColor0 = 0;
		std::size_t verticesWithTex0 = 0;

		std::set<std::uint16_t> usedDrawMatrices;

		std::size_t displayedSamples = 0;

		std::cout
			<< "\nJ3D ASSEMBLED CPU GEOMETRY\n";

		for (
			const Okari::J3DAssembledShapeGroup& group :
			geometry.Groups
			)
		{
			for (
				const Okari::J3DAssembledPrimitive& primitive :
				group.Primitives
				)
			{
				++primitiveCount;

				for (
					const Okari::J3DAssembledVertex& vertex :
					primitive.Vertices
					)
				{
					++vertexCount;

					if (vertex.Normal.has_value())
						++verticesWithNormal;

					if (vertex.Colors[0].has_value())
						++verticesWithColor0;

					if (vertex.TexCoords[0].has_value())
						++verticesWithTex0;

					usedDrawMatrices.insert(
						vertex.DrawMatrixIndex
					);

					if (displayedSamples < 10)
					{
						std::cout
							<< "shape["
							<< group.ShapeIndex
							<< "] group["
							<< group.GroupIndex
							<< "] pos=("
							<< vertex.Position.x
							<< ", "
							<< vertex.Position.y
							<< ", "
							<< vertex.Position.z
							<< ')';

						if (vertex.Normal.has_value())
						{
							std::cout
								<< " nrm=("
								<< vertex.Normal->x
								<< ", "
								<< vertex.Normal->y
								<< ", "
								<< vertex.Normal->z
								<< ')';
						}

						if (vertex.TexCoords[0].has_value())
						{
							std::cout
								<< " uv0=("
								<< vertex.TexCoords[0]->x
								<< ", "
								<< vertex.TexCoords[0]->y
								<< ')';
						}

						std::cout
							<< " drawMatrix="
							<< vertex.DrawMatrixIndex
							<< '\n';

						++displayedSamples;
					}
				}
			}
		}

		std::cout
			<< "Groups: "
			<< geometry.Groups.size()
			<< '\n'
			<< "Primitives: "
			<< primitiveCount
			<< '\n'
			<< "Assembled vertices: "
			<< vertexCount
			<< '\n'
			<< "Vertices with normal: "
			<< verticesWithNormal
			<< '\n'
			<< "Vertices with CLR0: "
			<< verticesWithColor0
			<< '\n'
			<< "Vertices with TEX0: "
			<< verticesWithTex0
			<< '\n'
			<< "Distinct draw matrices referenced: "
			<< usedDrawMatrices.size()
			<< '\n';
	}

	void PrintTriangleGeometry(
		const Okari::J3DTriangleGeometry& geometry
	)
	{
		std::cout
			<< "\nJ3D TRIANGLE TOPOLOGY\n"
			<< "Ranges: "
			<< geometry.Ranges.size()
			<< '\n'
			<< "Triangle vertices: "
			<< geometry.Vertices.size()
			<< '\n'
			<< "Triangles: "
			<< geometry.TriangleCount()
			<< '\n';

		std::size_t emptyRanges = 0;
		std::size_t invalidRanges = 0;

		for (
			const Okari::J3DTriangleRange& range :
			geometry.Ranges
			)
		{
			if (range.VertexCount == 0)
				++emptyRanges;

			if ((range.VertexCount % 3) != 0)
				++invalidRanges;
		}

		std::cout
			<< "Empty ranges: "
			<< emptyRanges
			<< '\n'
			<< "Ranges not divisible by 3: "
			<< invalidRanges
			<< '\n';
	}

	void PrintModelLoaderSmokeTest(
		const Okari::J3DModelData& model
	)
	{
		std::cout
			<< "\nJ3D MODEL LOADER SMOKE TEST\n"
			<< "Sections: "
			<< model.Document.Sections.size()
			<< '\n'
			<< "INF1 positions: "
			<< model.Inf1.VertexPositionCount
			<< '\n'
			<< "VTX1 arrays: "
			<< model.Vtx1.Arrays.size()
			<< '\n'
			<< "EVP1 envelopes: "
			<< model.Evp1.Envelopes.size()
			<< '\n'
			<< "DRW1 definitions: "
			<< model.Drw1.Matrices.size()
			<< '\n'
			<< "JNT1 joints: "
			<< model.Jnt1.Joints.size()
			<< '\n'
			<< "SHP1 shapes: "
			<< model.Shp1.Shapes.size()
			<< '\n'
			<< "SHP1 matrix groups: ";

		std::size_t matrixGroupCount = 0;

		for (
			const Okari::J3DShapeRecord& shape :
			model.Shp1.Shapes
			)
		{
			matrixGroupCount +=
				shape.MatrixGroups.size();
		}

		std::cout
			<< matrixGroupCount
			<< '\n';

		std::cout
			<< "Rest pose joints: "
			<< model.RestPose.Joints.size()
			<< '\n'
			<< "Rest pose roots: "
			<< model.RestPose.RootJointIndices.size()
			<< '\n'
			<< "Rest pose scaling rule: 0x"
			<< std::hex
			<< std::uppercase
			<< model.RestPose.ScalingRule
			<< std::dec
			<< '\n';

		std::cout
			<< "DRW1 raw matrices: "
			<< model.DrawMatrices.RawDefinitionCount
			<< '\n'
			<< "DRW1 effective matrices: "
			<< model.DrawMatrices.EffectiveDefinitionCount
			<< '\n'
			<< "DRW1 rigid matrices: "
			<< model.DrawMatrices.RigidMatrixCount
			<< '\n'
			<< "DRW1 envelope matrices: "
			<< model.DrawMatrices.EnvelopeMatrixCount
			<< '\n'
			<< "DRW1 duplicated envelope suffix removed: "
			<< (
				model.DrawMatrices.RemovedDuplicatedEnvelopeSuffix
				? "yes"
				: "no"
				)
			<< '\n'
			<< "DRW1 max bind-pose identity error: "
			<< model.DrawMatrices.MaximumEnvelopeIdentityError
			<< '\n';

		std::cout
			<< "DRW1 palette size: "
			<< model.DrawMatrices.Matrices.size()
			<< '\n';

		std::cout
			<< "SHP1 resolved matrix groups: "
			<< model.ShapeMatrixPalette.Groups.size()
			<< '\n'
			<< "SHP1 loaded matrix slots: "
			<< model.ShapeMatrixPalette.LoadedSlotCount
			<< '\n'
			<< "SHP1 reused matrix slots: "
			<< model.ShapeMatrixPalette.ReusedSlotCount
			<< '\n'
			<< "SHP1 maximum draw-matrix index: "
			<< model.ShapeMatrixPalette.MaximumDrawMatrixIndex
			<< '\n';

		std::size_t primitiveCount = 0;
		std::size_t referencedVertexCount = 0;

		for (const Okari::J3DShapeDisplayListGroup& group :
			model.DisplayLists.Groups)
		{
			primitiveCount += group.Primitives.size();

			for (const Okari::J3DShapePrimitiveRecord& primitive :
				group.Primitives)
			{
				referencedVertexCount += primitive.VertexCount;
			}
		}

		std::cout
			<< "GX display-list groups: "
			<< model.DisplayLists.Groups.size()
			<< '\n'
			<< "GX primitives: "
			<< primitiveCount
			<< '\n'
			<< "GX vertex references: "
			<< referencedVertexCount
			<< '\n';

		std::cout
			<< "Decoded positions: "
			<< model.VertexData.Positions.size()
			<< '\n'
			<< "Decoded normals: "
			<< model.VertexData.Normals.size()
			<< '\n'
			<< "Decoded NBT frames: "
			<< model.VertexData.NBTFrames.size()
			<< '\n'
			<< "Decoded CLR0: "
			<< model.VertexData.Colors[0].size()
			<< '\n'
			<< "Decoded CLR1: "
			<< model.VertexData.Colors[1].size()
			<< '\n'
			<< "Decoded TEX0: "
			<< model.VertexData.TexCoords[0].size()
			<< '\n';
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

	const Okari::J3DModelLoadResult modelLoadResult =
		Okari::J3DModelLoader::Load(argv[1]);

	if (!modelLoadResult.Succeeded())
	{
		std::cerr
			<< "[J3DInspector] J3DModelLoader: "
			<< modelLoadResult.Error
			<< '\n';

		return 1;
	}

	PrintModelLoaderSmokeTest(
		modelLoadResult.Model
	);

	const Okari::J3DReadResult readResult = Okari::J3DFileReader::Read(argv[1]);

	if (!readResult.Succeeded())
	{
		std::cerr
			<< "[J3DInspector] "
			<< readResult.Error
			<< std::endl;

		return 1;
	}

	const Okari::J3DDocument& document = readResult.Document;
	PrintDocumentSummary(document);

	const Okari::J3DSectionInfo* inf1Section =
		FindRequiredSection(document, "INF1");

	if (inf1Section == nullptr)
		return 1;

	const Okari::J3DINF1ParseResult inf1Result =
		Okari::J3DINF1Parser::Parse(document, *inf1Section);

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
		FindRequiredSection(document, "VTX1");

	if (vtx1Section == nullptr)
		return 1;

	const Okari::J3DVTX1ParseResult vtx1Result =
		Okari::J3DVTX1Parser::Parse(document, *vtx1Section);

	if (!vtx1Result.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< vtx1Result.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DVTX1Data& vtx1 = vtx1Result.Data;

	if (!PrintVertexArrays(inf1, vtx1))
		return 1;

	const Okari::J3DSectionInfo* shp1Section =
		FindRequiredSection(document, "SHP1");

	if (shp1Section == nullptr)
		return 1;

	const Okari::J3DSHP1ParseResult shp1Result =
		Okari::J3DSHP1Parser::Parse(document, *shp1Section);

	if (!shp1Result.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< shp1Result.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DSHP1Data& shp1 =
		shp1Result.Data;

	PrintShapeSectionSummary(shp1);

	PrintShapeRecords(shp1);
	
	PrintShapeMatrixGroups(shp1);

	PrintShapeVertexDescriptors(
		shp1,
		vtx1
	);

	const Okari::J3DShapeDisplayListParseResult
		displayListResult =
		Okari::J3DShapeDisplayListParser::Parse(
			document,
			*shp1Section,
			shp1,
			vtx1
		);

	if (!displayListResult.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< displayListResult.Error
			<< '\n';

		return 1;
	}

	PrintShapeDisplayListSummary(
		displayListResult.Data
	);

	const Okari::J3DShapeVertexIndexScanResult
		indexScanResult =
		Okari::J3DShapeVertexIndexScanner::Scan(
			document,
			*shp1Section,
			shp1,
			vtx1,
			displayListResult.Data
		);

	if (!indexScanResult.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< indexScanResult.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DShapeVertexIndexUsage&
		indexUsage =
		indexScanResult.Usage;

	PrintShapeVertexIndexUsage(
		indexUsage,
		inf1
	);

	const Okari::J3DVertexDecodeRequest
		vertexDecodeRequest =
		indexUsage.BuildDecodeRequest();

	const Okari::J3DVertexDecodeResult vertexDecodeResult =
		Okari::J3DVertexDecoder::Decode(
			vtx1,
			vertexDecodeRequest
		);

	if (!vertexDecodeResult.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< vertexDecodeResult.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DDecodedVertexData& vertexData =
		vertexDecodeResult.Data;

	if (!PrintDecodedVertexData(vertexData))
		return 1;

	const Okari::J3DSectionInfo* jnt1Section =
		FindRequiredSection(document, "JNT1");

	if (jnt1Section == nullptr)
		return 1;

	const Okari::J3DJNT1ParseResult jnt1Result =
		Okari::J3DJNT1Parser::Parse(document, *jnt1Section);

	if (!jnt1Result.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< jnt1Result.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DJNT1Data& jnt1 = jnt1Result.Data;
	PrintJoints(jnt1);

	if (!ValidateHierarchyJointReferences(inf1, jnt1))
		return 1;

	PrintHierarchy(inf1, jnt1);

	const Okari::J3DSectionInfo* drw1Section =
		FindRequiredSection(document, "DRW1");

	if (drw1Section == nullptr)
		return 1;

	const Okari::J3DDRW1ParseResult drw1Result =
		Okari::J3DDRW1Parser::Parse(document, *drw1Section);

	if (!drw1Result.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< drw1Result.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DDRW1Data& drw1 = drw1Result.Data;

	const Okari::J3DSectionInfo* evp1Section =
		FindRequiredSection(document, "EVP1");

	if (evp1Section == nullptr)
		return 1;

	const Okari::J3DEVP1ParseResult evp1Result =
		Okari::J3DEVP1Parser::Parse(document, *evp1Section);

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
		Okari::J3DPoseEvaluator::EvaluateRestPose(inf1, jnt1);

	if (!poseResult.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< poseResult.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DRestPose& restPose = poseResult.Pose;

	DrawMatrixStats drawMatrixStats;

	if (!ValidateDrawMatrices(drw1, evp1, jnt1, drawMatrixStats))
		return 1;

	std::size_t invalidWeightSumCount = 0;

	if (!ValidateEnvelopes(evp1, jnt1, invalidWeightSumCount))
		return 1;

	const Okari::J3DDrawMatrixEvaluationResult drawMatrixResult =
		Okari::J3DDrawMatrixEvaluator::Evaluate(restPose, drw1, evp1);

	if (!drawMatrixResult.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< drawMatrixResult.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DDrawMatrixPalette& drawPalette = drawMatrixResult.Palette;

	if (!PrintDrawMatrices(drw1, jnt1, drawMatrixStats))
		return 1;

	PrintEnvelopes(evp1, jnt1, invalidWeightSumCount);
	PrintRestPose(restPose, jnt1);
	PrintDrawMatrixPalette(drawPalette);

	const Okari::J3DShapeMatrixPaletteResult
		shapeMatrixPaletteResult =
		Okari::J3DShapeMatrixPaletteResolver::Resolve(
			shp1,
			drawPalette
		);

	if (!shapeMatrixPaletteResult.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< shapeMatrixPaletteResult.Error
			<< '\n';

		return 1;
	}

	const Okari::J3DShapeMatrixPalette&
		shapeMatrixPalette =
		shapeMatrixPaletteResult.Palette;

	PrintShapeMatrixPalette(
		shapeMatrixPalette,
		drawPalette
	);

	const Okari::J3DShapeVertexReferenceDecodeResult
		vertexReferenceResult =
		Okari::J3DShapeVertexReferenceDecoder::Decode(
			document,
			*shp1Section,
			shp1,
			vtx1,
			displayListResult.Data,
			shapeMatrixPalette
		);

	if (!vertexReferenceResult.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< vertexReferenceResult.Error
			<< '\n';

		return 1;
	}

	PrintShapeVertexReferences(
		vertexReferenceResult.Data,
		drawPalette.Matrices.size()
	);

	const Okari::J3DGeometryAssemblyResult
		geometryAssemblyResult =
		Okari::J3DGeometryAssembler::Assemble(
			vertexReferenceResult.Data,
			vertexData,
			drawPalette
		);

	if (!geometryAssemblyResult.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< geometryAssemblyResult.Error
			<< '\n';

		return 1;
	}

	PrintAssembledGeometry(
		geometryAssemblyResult.Geometry
	);

	const Okari::J3DTriangleTopologyResult topologyResult =
		Okari::J3DTriangleTopologyBuilder::Build(geometryAssemblyResult.Geometry);

	if (!topologyResult.Succeeded())
	{
		std::cerr
			<< "\n[J3DInspector] "
			<< topologyResult.Error
			<< '\n';

		return 1;
	}

	PrintTriangleGeometry(
		topologyResult.Geometry
	);

	if (document.Data.size() != document.Header.DeclaredFileSize)
	{
		std::cout
			<< "\nWarning: the physical file contains trailing "
			<< "bytes after the declared J3D data.\n";
	}

	return 0;
}