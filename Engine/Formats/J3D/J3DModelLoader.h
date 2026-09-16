#pragma once

#include "Formats/J3D/J3DModelData.h"

#include <filesystem>
#include <string>

namespace Okari
{
    struct J3DModelLoadResult
    {
        J3DModelData Model;
        std::string Error;

        bool Succeeded() const
        {
            return Error.empty();
        }
    };

    class J3DModelLoader
    {
    public:
        static J3DModelLoadResult Load(
            const std::filesystem::path& path
        );
    };
}