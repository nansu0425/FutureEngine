#pragma once

struct FRenderingContext
{
    FRenderingContext(const FViewProjConstants* InViewProj, class UCamera* InCurrentCamera, EViewModeIndex InViewMode, uint64 InShowFlags, const D3D11_VIEWPORT& InViewport, const FVector2& InRenderTargetSize)
        : ViewProjConstants(InViewProj), CurrentCamera(InCurrentCamera), ViewMode(InViewMode), ShowFlags(InShowFlags), Viewport(InViewport), RenderTargetSize(InRenderTargetSize) {}
    
    const FViewProjConstants* ViewProjConstants;
    UCamera* CurrentCamera;
    EViewModeIndex ViewMode;
    uint64 ShowFlags;
    D3D11_VIEWPORT Viewport;
    FVector2 RenderTargetSize;

    TArray<class UPrimitiveComponent*> AllPrimitives;
    // Components By Render Pass
    TArray<class UStaticMeshComponent*> StaticMeshes;
    TArray<class UBillBoardComponent*> BillBoards;
    TArray<class UTextComponent*> Texts;
    TArray<class UUUIDTextComponent*> UUIDs;
    TArray<class UDecalComponent*> Decals;
    TArray<class UHeightFogComponent*> Fogs;
};
