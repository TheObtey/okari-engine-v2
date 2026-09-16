#include "Formats/J3D/J3DModelLoader.h"

#include "Formats/J3D/J3DFileReader.h"

#include "Formats/J3D/J3DPoseEvaluator.h"
#include "Formats/J3D/J3DDrawMatrixEvaluator.h"
#include "Formats/J3D/J3DShapeMatrixPaletteResolver.h"
#include "Formats/J3D/J3DShapeDisplayListParser.h"
#include "Formats/J3D/J3DShapeVertexIndexScanner.h"
#include "Formats/J3D/J3DVertexDecoder.h"

#include "Formats/J3D/Sections/INF1Parser.h"
#include "Formats/J3D/Sections/VTX1Parser.h"
#include "Formats/J3D/Sections/EVP1Parser.h"
#include "Formats/J3D/Sections/DRW1Parser.h"
#include "Formats/J3D/Sections/JNT1Parser.h"
#include "Formats/J3D/Sections/SHP1Parser.h"

#include <string>
#include <utility>

namespace Okari
{
    namespace
    {
        J3DModelLoadResult Failure(
            const std::string& message
        )
        {
            J3DModelLoadResult result;
            result.Error = message;
            return result;
        }

        const J3DSectionInfo* FindRequiredSection(
            const J3DDocument& document,
            const char* tag
        )
        {
            return document.FindSection(tag);
        }

        std::string MissingSectionError(
            const char* tag
        )
        {
            return
                "J3D model is missing required section '" +
                std::string(tag) +
                "'";
        }

        std::string SectionParseError(
            const char* tag,
            const std::string& error
        )
        {
            return
                "Failed to parse " +
                std::string(tag) +
                ": " +
                error;
        }
    }

    J3DModelLoadResult J3DModelLoader::Load(
        const std::filesystem::path& path
    )
    {
        const J3DReadResult readResult =
            J3DFileReader::Read(path);

        if (!readResult.Succeeded())
            return Failure(readResult.Error);

        J3DModelData model;
        model.Document = std::move(readResult.Document);

        const J3DSectionInfo* inf1Section =
            FindRequiredSection(model.Document, "INF1");

        if (inf1Section == nullptr)
            return Failure(MissingSectionError("INF1"));

        const J3DINF1ParseResult inf1Result =
            J3DINF1Parser::Parse(
                model.Document,
                *inf1Section
            );

        if (!inf1Result.Succeeded())
        {
            return Failure(
                SectionParseError(
                    "INF1",
                    inf1Result.Error
                )
            );
        }

        model.Inf1 = inf1Result.Data;

        const J3DSectionInfo* vtx1Section =
            FindRequiredSection(model.Document, "VTX1");

        if (vtx1Section == nullptr)
            return Failure(MissingSectionError("VTX1"));

        const J3DVTX1ParseResult vtx1Result =
            J3DVTX1Parser::Parse(
                model.Document,
                *vtx1Section
            );

        if (!vtx1Result.Succeeded())
        {
            return Failure(
                SectionParseError(
                    "VTX1",
                    vtx1Result.Error
                )
            );
        }

        model.Vtx1 = vtx1Result.Data;

        const J3DSectionInfo* evp1Section =
            FindRequiredSection(model.Document, "EVP1");

        if (evp1Section == nullptr)
            return Failure(MissingSectionError("EVP1"));

        const J3DEVP1ParseResult evp1Result =
            J3DEVP1Parser::Parse(
                model.Document,
                *evp1Section
            );

        if (!evp1Result.Succeeded())
        {
            return Failure(
                SectionParseError(
                    "EVP1",
                    evp1Result.Error
                )
            );
        }

        model.Evp1 = evp1Result.Data;

        const J3DSectionInfo* drw1Section =
            FindRequiredSection(model.Document, "DRW1");

        if (drw1Section == nullptr)
            return Failure(MissingSectionError("DRW1"));

        const J3DDRW1ParseResult drw1Result =
            J3DDRW1Parser::Parse(
                model.Document,
                *drw1Section
            );

        if (!drw1Result.Succeeded())
        {
            return Failure(
                SectionParseError(
                    "DRW1",
                    drw1Result.Error
                )
            );
        }

        model.Drw1 = drw1Result.Data;

        const J3DSectionInfo* jnt1Section =
            FindRequiredSection(model.Document, "JNT1");

        if (jnt1Section == nullptr)
            return Failure(MissingSectionError("JNT1"));

        const J3DJNT1ParseResult jnt1Result =
            J3DJNT1Parser::Parse(
                model.Document,
                *jnt1Section
            );

        if (!jnt1Result.Succeeded())
        {
            return Failure(
                SectionParseError(
                    "JNT1",
                    jnt1Result.Error
                )
            );
        }

        model.Jnt1 = jnt1Result.Data;

        const J3DSectionInfo* shp1Section =
            FindRequiredSection(model.Document, "SHP1");

        if (shp1Section == nullptr)
            return Failure(MissingSectionError("SHP1"));

        const J3DSHP1ParseResult shp1Result =
            J3DSHP1Parser::Parse(
                model.Document,
                *shp1Section
            );

        if (!shp1Result.Succeeded())
        {
            return Failure(
                SectionParseError(
                    "SHP1",
                    shp1Result.Error
                )
            );
        }

        model.Shp1 = shp1Result.Data;

        const J3DPoseEvaluationResult poseResult =
            J3DPoseEvaluator::EvaluateRestPose(
                model.Inf1,
                model.Jnt1
            );

        if (!poseResult.Succeeded())
        {
            return Failure(
                "Failed to evaluate J3D rest pose: " +
                poseResult.Error
            );
        }

        model.RestPose = poseResult.Pose;

        const J3DDrawMatrixEvaluationResult drawMatrixResult =
            J3DDrawMatrixEvaluator::Evaluate(
                model.RestPose,
                model.Drw1,
                model.Evp1
            );

        if (!drawMatrixResult.Succeeded())
        {
            return Failure(
                "Failed to evaluate J3D draw matrices: " +
                drawMatrixResult.Error
            );
        }

        model.DrawMatrices = drawMatrixResult.Palette;

        const J3DShapeMatrixPaletteResult shapeMatrixResult =
            J3DShapeMatrixPaletteResolver::Resolve(
                model.Shp1,
                model.DrawMatrices
            );

        if (!shapeMatrixResult.Succeeded())
        {
            return Failure(
                "Failed to resolve SHP1 matrix palettes: " +
                shapeMatrixResult.Error
            );
        }

        model.ShapeMatrixPalette = shapeMatrixResult.Palette;

        const J3DShapeDisplayListParseResult displayListResult =
            J3DShapeDisplayListParser::Parse(
                model.Document,
                *shp1Section,
                model.Shp1,
                model.Vtx1
            );

        if (!displayListResult.Succeeded())
        {
            return Failure(
                "Failed to parse SHP1 GX display lists: " +
                displayListResult.Error
            );
        }

        model.DisplayLists = displayListResult.Data;

        const J3DShapeVertexIndexScanResult indexScanResult =
            J3DShapeVertexIndexScanner::Scan(
                model.Document,
                *shp1Section,
                model.Shp1,
                model.Vtx1,
                model.DisplayLists
            );

        if (!indexScanResult.Succeeded())
        {
            return Failure(
                "Failed to scan SHP1 vertex indices: " +
                indexScanResult.Error
            );
        }

        const J3DVertexDecodeRequest vertexDecodeRequest =
            indexScanResult.Usage.BuildDecodeRequest();

        if (
            vertexDecodeRequest.PositionCount !=
            model.Inf1.VertexPositionCount
            )
        {
            return Failure(
                "SHP1 position usage disagrees with INF1 position count"
            );
        }

        const J3DVertexDecodeResult vertexDecodeResult =
            J3DVertexDecoder::Decode(
                model.Vtx1,
                vertexDecodeRequest
            );

        if (!vertexDecodeResult.Succeeded())
        {
            return Failure(
                "Failed to decode VTX1 vertex data: " +
                vertexDecodeResult.Error
            );
        }

        model.VertexData = vertexDecodeResult.Data;

        J3DModelLoadResult result;
        result.Model = std::move(model);

        return result;
    }
}