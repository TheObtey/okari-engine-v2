#pragma once

#include "Formats/J3D/J3DModelData.h"
#include "Rendering/Mesh.h"

#include <memory>
#include <string>

namespace Okari
{
    struct J3DPreviewMeshBuildResult
    {
        std::unique_ptr<Mesh> PreviewMesh;
        std::string Error;

        bool Succeeded() const
        {
            return Error.empty() && PreviewMesh != nullptr;
        }
    };

    class J3DPreviewMeshBuilder
    {
    public:
        static J3DPreviewMeshBuildResult Build(
            const J3DModelData& model
        );
    };
}