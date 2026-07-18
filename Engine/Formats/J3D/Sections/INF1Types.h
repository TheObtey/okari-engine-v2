#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Okari
{
	enum class J3DHierarchyEntryType : std::uint16_t
	{
		End = 0x00,
		Open = 0x01,
		Close = 0x02,

		Joint = 0x10,
		Material = 0x11,
		Shape = 0x12,
	};

	struct J3DHierarchyEntry
	{
		J3DHierarchyEntryType Type = J3DHierarchyEntryType::End;
		std::uint16_t Index = 0;

		std::uint32_t FileOffset = 0;
	};

	struct J3DHierarchyNode
	{
		J3DHierarchyEntryType Type = J3DHierarchyEntryType::Joint;
		std::uint16_t Index = 0;

		std::int32_t ParentNode = -1; // -1 is for root node

		std::vector<std::uint32_t> Children;

		std::uint32_t FileOffset = 0;
	};

	struct J3DINF1Data
	{
		std::uint16_t LoadFlags = 0;

		std::uint32_t PacketCount = 0;
		std::uint32_t VertexPositionCount = 0;

		std::uint32_t HierarchyOffset = 0;

		std::vector<J3DHierarchyEntry> Entries;

		std::vector<J3DHierarchyNode> Nodes;
		std::vector<std::uint32_t> RootNodes;
	};

	struct J3DINF1ParseResult
	{
		J3DINF1Data Data;
		std::string Error;

		bool Succeeded() const
		{
			return Error.empty();
		}
	};

	const char* ToString(J3DHierarchyEntryType type);

	bool IsHierarchyNode(J3DHierarchyEntryType type);
}