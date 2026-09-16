#include "Formats/J3D/J3DTriangleTopologyBuilder.h"

#include <string>
#include <utility>

namespace Okari
{
	namespace
	{
		J3DTriangleTopologyResult Failure(
			const std::string& message
		)
		{
			J3DTriangleTopologyResult result;
			result.Error = message;
			return result;
		}

		void EmitTriangle(
			J3DTriangleGeometry& output,
			const J3DAssembledVertex& a,
			const J3DAssembledVertex& b,
			const J3DAssembledVertex& c
		)
		{
			output.Vertices.push_back(a);
			output.Vertices.push_back(b);
			output.Vertices.push_back(c);
		}

		bool AppendPrimitive(
			J3DTriangleGeometry& output,
			const J3DAssembledPrimitive& primitive,
			std::string& error
		)
		{
			const auto& vertices =
				primitive.Vertices;

			switch (primitive.Type)
			{
			case J3DGXPrimitiveType::Triangles:
			{
				if ((vertices.size() % 3) != 0)
				{
					error = "GX TRIANGLES vertex count is not a multiple of 3";

					return false;
				}

				output.Vertices.insert(
					output.Vertices.end(),
					vertices.begin(),
					vertices.end()
				);

				return true;
			}

			case J3DGXPrimitiveType::Quads:
			case J3DGXPrimitiveType::Quads2:
			{
				if ((vertices.size() % 4) != 0)
				{
					error = "GX QUADS vertex count is not a multiple of 4";

					return false;
				}

				for (
					std::size_t i = 0;
					i < vertices.size();
					i += 4
					)
				{
					EmitTriangle(
						output,
						vertices[i + 0],
						vertices[i + 1],
						vertices[i + 2]
					);

					EmitTriangle(
						output,
						vertices[i + 0],
						vertices[i + 2],
						vertices[i + 3]
					);
				}

				return true;
			}

			case J3DGXPrimitiveType::TriangleStrip:
			{
				if (vertices.size() < 3)
				{
					error = "GX TRIANGLE_STRIP contains fewer than 3 vertices";

					return false;
				}

				for (
					std::size_t i = 2;
					i < vertices.size();
					++i
					)
				{
					// GX/OpenGL strips flip winding after each generated triangle.
					if ((i & 1) == 0)
					{
						EmitTriangle(
							output,
							vertices[i - 2],
							vertices[i - 1],
							vertices[i]
						);
					}
					else
					{
						EmitTriangle(
							output,
							vertices[i - 2],
							vertices[i],
							vertices[i - 1]
						);
					}
				}

				return true;
			}

			case J3DGXPrimitiveType::TriangleFan:
			{
				if (vertices.size() < 3)
				{
					error = "GX TRIANGLE_FAN contains fewer than 3 vertices";

					return false;
				}

				for (
					std::size_t i = 2;
					i < vertices.size();
					++i
					)
				{
					EmitTriangle(
						output,
						vertices[0],
						vertices[i - 1],
						vertices[i]
					);
				}

				return true;
			}

			case J3DGXPrimitiveType::Lines:
			case J3DGXPrimitiveType::LineStrip:
			case J3DGXPrimitiveType::Points:
				error = "Non-triangle GX primitive encountered in model geometry";

				return false;
			}

			error = "Unknown GX primitive type";
			return false;
		}
	}

	J3DTriangleTopologyResult
		J3DTriangleTopologyBuilder::Build(
			const J3DAssembledGeometry& geometry
		)
	{
		J3DTriangleGeometry output;

		for (
			const J3DAssembledShapeGroup& group :
			geometry.Groups
			)
		{
			const std::uint32_t firstVertex =
				static_cast<std::uint32_t>(
					output.Vertices.size()
					);

			for (
				const J3DAssembledPrimitive& primitive :
				group.Primitives
				)
			{
				std::string error;

				if (!AppendPrimitive(
					output,
					primitive,
					error
				))
				{
					return Failure(
						"Shape " +
						std::to_string(
							group.ShapeIndex
						) +
						", group " +
						std::to_string(
							group.GroupIndex
						) +
						": " +
						error
					);
				}
			}

			const std::uint32_t endVertex =
				static_cast<std::uint32_t>(
					output.Vertices.size()
					);

			J3DTriangleRange range;

			range.ShapeIndex =
				group.ShapeIndex;

			range.GroupIndex =
				group.GroupIndex;

			range.FirstVertex =
				firstVertex;

			range.VertexCount =
				endVertex - firstVertex;

			output.Ranges.push_back(range);
		}

		J3DTriangleTopologyResult result;
		result.Geometry = std::move(output);

		return result;
	}
}