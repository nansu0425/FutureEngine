#include "pch.h"
#include "Physics/Public/OBB.h"

#include "Physics/Public/AABB.h"

void FOBB::Update(const FMatrix& WorldMatrix)
{
    Center = WorldMatrix.GetLocation();
    ScaleRotation = WorldMatrix;
    ScaleRotation.Data[3][0] = ScaleRotation.Data[3][1] = ScaleRotation.Data[3][2] = 0;
}

FAABB FOBB::ToWorldAABB() const
{
    FVector Corners[8];
    Corners[0] = FVector(-Extents.X, -Extents.Y, -Extents.Z);
    Corners[1] = FVector( Extents.X, -Extents.Y, -Extents.Z);
    Corners[2] = FVector(-Extents.X,  Extents.Y, -Extents.Z);
    Corners[3] = FVector( Extents.X,  Extents.Y, -Extents.Z);
    Corners[4] = FVector(-Extents.X, -Extents.Y,  Extents.Z);
    Corners[5] = FVector( Extents.X, -Extents.Y,  Extents.Z);
    Corners[6] = FVector(-Extents.X,  Extents.Y,  Extents.Z);
    Corners[7] = FVector( Extents.X,  Extents.Y,  Extents.Z);

    FMatrix ObbTransform = ScaleRotation * FMatrix::TranslationMatrix(Center);

    FVector NewMin(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector NewMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (int i = 0; i < 8; ++i)
    {
        FVector TransformedCorner = ObbTransform.TransformPosition(Corners[i]);
        NewMin.X = std::min(NewMin.X, TransformedCorner.X);
        NewMin.Y = std::min(NewMin.Y, TransformedCorner.Y);
        NewMin.Z = std::min(NewMin.Z, TransformedCorner.Z);
        NewMax.X = std::max(NewMax.X, TransformedCorner.X);
        NewMax.Y = std::max(NewMax.Y, TransformedCorner.Y);
        NewMax.Z = std::max(NewMax.Z, TransformedCorner.Z);
    }

    return FAABB(NewMin, NewMax);
}
