#include "J3DPreviewMeshBuilder.h"

#include <glm.hpp>

#include <vector>

namespace Okari
{
    namespace
    {
        J3DPreviewMeshBuildResult Failure(
            const std::string& error
        )
        {
            J3DPreviewMeshBuildResult result;
            result.Error = error;
            return result;
        }
    }

    J3DPreviewMeshBuildResult J3DPreviewMeshBuilder::Build(
        const J3DModelData& model
    )
    {
        std::vector<Vertex> vertices;

        vertices.reserve(
            model.TriangleGeometry.Vertices.size()
        );

        for (const J3DAssembledVertex& sourceVertex :
            model.TriangleGeometry.Vertices)
        {
            if (
                sourceVertex.DrawMatrixIndex >=
                model.DrawMatrices.Matrices.size()
                )
            {
                return Failure(
                    "Triangle vertex references an invalid draw matrix"
                );
            }

            const J3DResolvedDrawMatrix& drawMatrix =
                model.DrawMatrices.Matrices[
                    sourceVertex.DrawMatrixIndex
                ];

            const glm::vec4 transformedPosition =
                drawMatrix.Matrix *
                glm::vec4(sourceVertex.Position, 1.0f);

            Vertex vertex{};

            vertex.Position[0] = transformedPosition.x;
            vertex.Position[1] = transformedPosition.y;
            vertex.Position[2] = transformedPosition.z;

            if (sourceVertex.TexCoords[0].has_value())
            {
                const glm::vec2& uv =
                    sourceVertex.TexCoords[0].value();

                vertex.UV[0] = uv.x;
                vertex.UV[1] = uv.y;
            }

            if (sourceVertex.Normal.has_value())
            {
                const glm::mat3 normalMatrix =
                    glm::transpose(
                        glm::inverse(
                            glm::mat3(drawMatrix.Matrix)
                        )
                    );

                const glm::vec3 transformedNormal =
                    glm::normalize(
                        normalMatrix *
                        sourceVertex.Normal.value()
                    );

                vertex.Normal[0] = transformedNormal.x;
                vertex.Normal[1] = transformedNormal.y;
                vertex.Normal[2] = transformedNormal.z;
            }
            else
            {
                vertex.Normal[0] = 0.0f;
                vertex.Normal[1] = 1.0f;
                vertex.Normal[2] = 0.0f;
            }

            vertices.push_back(vertex);
        }

        if (vertices.empty())
        {
            return Failure(
                "J3D model produced no preview vertices"
            );
        }

        J3DPreviewMeshBuildResult result;

        result.PreviewMesh =
            std::make_unique<Mesh>(vertices);

        return result;
    }
}