#include "Formats/J3D/Sections/INF1Parser.h"

#include "IO/BigEndianReader.h"

#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace Okari
{
	namespace
	{
		constexpr std::size_t INF1HeaderSize = 0x18;
		constexpr std::size_t HierarchyEntrySize = 0x04;

		J3DINF1ParseResult Failure(const std::string& message)
		{
			J3DINF1ParseResult result;
			result.Error = message;
			return result;
		}

		std::string FormatHex(std::uint64_t value)
		{
			std::ostringstream stream;

			stream
				<< "0x"
				<< std::hex
				<< std::uppercase
				<< value;

			return stream.str();
		}

		bool DecodeEntryType(
			std::uint16_t rawType,
			J3DHierarchyEntryType& type
		)
		{
			switch (rawType)
			{
			case 0x00:
				type = J3DHierarchyEntryType::End;
				return true;

			case 0x01:
				type = J3DHierarchyEntryType::Open;
				return true;

			case 0x02:
				type = J3DHierarchyEntryType::Close;
				return true;

			case 0x10:
				type = J3DHierarchyEntryType::Joint;
				return true;

			case 0x11:
				type = J3DHierarchyEntryType::Material;
				return true;

			case 0x12:
				type = J3DHierarchyEntryType::Shape;
				return true;

			default:
				return false;
			}
		}
	}

	const char* ToString(J3DHierarchyEntryType type)
	{
		switch (type)
		{
		case J3DHierarchyEntryType::End:
			return "End";

		case J3DHierarchyEntryType::Open:
			return "Open";

		case J3DHierarchyEntryType::Close:
			return "Close";

		case J3DHierarchyEntryType::Joint:
			return "Joint";

		case J3DHierarchyEntryType::Material:
			return "Material";

		case J3DHierarchyEntryType::Shape:
			return "Shape";

		default:
			return "Unknown";
		}
	}

	bool IsHierarchyNode(J3DHierarchyEntryType type)
	{
		return
			type == J3DHierarchyEntryType::Joint ||
			type == J3DHierarchyEntryType::Material ||
			type == J3DHierarchyEntryType::Shape;
	}

	J3DINF1ParseResult J3DINF1Parser::Parse(
		const J3DDocument& document,
		const J3DSectionInfo& section
	)
	{
		if (section.Tag != "INF1")
		{
			return Failure(
				"INF1 parser received section '" +
				section.Tag +
				"'"
			);
		}

		if (section.Size < INF1HeaderSize)
		{
			return Failure("INF1 section is smaller than its 0x18-byte header");
		}

		const std::uint64_t sectionEnd64 =
			static_cast<std::uint64_t>(section.Offset) +
			section.Size;

		if (sectionEnd64 > document.Data.size())
		{
			return Failure("INF1 section ends outside the J3D file");
		}

		const std::size_t sectionEnd = static_cast<std::size_t>(sectionEnd64);

		BigEndianReader reader(document.Data);

		try
		{
			reader.Seek(section.Offset);

			const std::string sectionTag = reader.ReadFixedString(4);

			const std::uint32_t serializedSectionSize = reader.ReadU32();

			if (sectionTag != "INF1")
			{
				return Failure(
					"Invalid INF1 magic at " +
					FormatHex(section.Offset)
				);
			}

			if (serializedSectionSize != section.Size)
			{
				return Failure("INF1 section size does not match the section directory");
			}

			J3DINF1Data data;

			data.LoadFlags = reader.ReadU16();

			reader.Skip(2); // 0x0A Padding

			data.PacketCount = reader.ReadU32();
			data.VertexPositionCount = reader.ReadU32();
			data.HierarchyOffset = reader.ReadU32();

			const std::uint64_t hierarchyStart64 =
				static_cast<std::uint64_t>(section.Offset) +
				data.HierarchyOffset;

			const std::uint64_t minimumHierarchyStart =
				static_cast<std::uint64_t>(section.Offset) +
				INF1HeaderSize;

			if (hierarchyStart64 < minimumHierarchyStart)
			{
				return Failure("INF1 hierarchy offset points inside its header");
			}

			if (hierarchyStart64 + HierarchyEntrySize > sectionEnd64)
			{
				return Failure("INF1 hierarchy offset points outside the section");
			}

			reader.Seek(
				static_cast<std::size_t>(hierarchyStart64)
			);

			std::vector<std::uint32_t> jointStack;

			// Index in data.Nodes from last encountered real Joint
			std::optional<std::uint32_t> lastJointNodeIndex;

			bool foundEnd = false;

			while (reader.Tell() + HierarchyEntrySize <= sectionEnd)
			{
				const std::size_t entryOffset = reader.Tell();

				const std::uint16_t rawType = reader.ReadU16();
				const std::uint16_t entryIndex = reader.ReadU16();

				J3DHierarchyEntryType entryType;

				if (!DecodeEntryType(rawType, entryType))
				{
					return Failure(
						"Unknown INF1 hierarchy entry type " +
						FormatHex(rawType) +
						" at " +
						FormatHex(entryOffset)
					);
				}

				J3DHierarchyEntry entry;

				entry.Type = entryType;
				entry.Index = entryIndex;
				entry.FileOffset = static_cast<std::uint32_t>(entryOffset);

				data.Entries.push_back(entry);

				if (entryType == J3DHierarchyEntryType::End)
				{
					if (!jointStack.empty())
						return Failure("INF1 hierarchy ended with unclosed joints");

					foundEnd = true;
					break;
				}

				if (entryType == J3DHierarchyEntryType::Open)
				{
					if (!lastJointNodeIndex.has_value())
					{
						return Failure(
							"INF1 Open command has no preceding joint at " +
							FormatHex(entryOffset)
						);
					}

					jointStack.push_back(*lastJointNodeIndex);
					continue;
				}

				if (entryType == J3DHierarchyEntryType::Close)
				{
					if (jointStack.empty())
					{
						return Failure(
							"INF1 Close command has no open joint at " +
							FormatHex(entryOffset)
						);
					}

					jointStack.pop_back();
					continue;
				}

				if (!IsHierarchyNode(entryType))
				{
					return Failure(
						"Unsupported INF1 hierarchy entry at " +
						FormatHex(entryOffset)
					);
				}

				J3DHierarchyNode node;

				node.Type = entryType;
				node.Index = entryIndex;
				node.FileOffset = static_cast<std::uint32_t>(entryOffset);

				if (!jointStack.empty())
					node.ParentNode = static_cast<std::int32_t>(jointStack.back());

				const std::uint32_t nodeIndex = static_cast<std::uint32_t>(data.Nodes.size());

				data.Nodes.push_back(std::move(node));

				if (jointStack.empty())
				{
					data.RootNodes.push_back(nodeIndex);
				}
				else
				{
					data.Nodes[jointStack.back()]
						.Children
						.push_back(nodeIndex);
				}

				if (entryType == J3DHierarchyEntryType::Joint)
					lastJointNodeIndex = nodeIndex;
			}

			if (!foundEnd)
				return Failure("INF1 hierarchy has no End command");

			J3DINF1ParseResult result;
			result.Data = std::move(data);

			return result;
		}
		catch (const std::out_of_range& exception)
		{
			return Failure(
				std::string("Malformed INF1 section: ") +
				exception.what()
			);
		}
	}
}