#pragma once

#include "Formats/J3D/J3DTypes.h"
#include "Formats/J3D/J3DPoseEvaluator.h"
#include "Formats/J3D/J3DDrawMatrixTypes.h"
#include "Formats/J3D/J3DShapeMatrixPaletteTypes.h"
#include "Formats/J3D/J3DShapeDisplayListTypes.h"
#include "Formats/J3D/J3DVertexData.h"
#include "Formats/J3D/J3DShapeVertexReferenceTypes.h"
#include "Formats/J3D/J3DAssembledGeometryTypes.h"
#include "Formats/J3D/J3DTriangleGeometryTypes.h"

#include "Formats/J3D/Sections/INF1Types.h"
#include "Formats/J3D/Sections/VTX1Types.h"
#include "Formats/J3D/Sections/EVP1Types.h"
#include "Formats/J3D/Sections/DRW1Types.h"
#include "Formats/J3D/Sections/JNT1Types.h"
#include "Formats/J3D/Sections/SHP1Types.h"

namespace Okari
{
    struct J3DModelData
    {
        J3DDocument Document;

        J3DINF1Data Inf1;
        J3DVTX1Data Vtx1;
        J3DEVP1Data Evp1;
        J3DDRW1Data Drw1;
        J3DJNT1Data Jnt1;
        J3DSHP1Data Shp1;

        J3DRestPose RestPose;
        J3DDrawMatrixPalette DrawMatrices;
        J3DShapeMatrixPalette ShapeMatrixPalette;
        J3DShapeDisplayListData DisplayLists;
        J3DDecodedVertexData VertexData;
        J3DShapeVertexReferenceData VertexReferences;
        J3DAssembledGeometry Geometry;
        J3DTriangleGeometry TriangleGeometry;
    };
}