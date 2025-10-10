#pragma once

struct FRenderingContext
{
    FRenderingContext(const FViewProjConstants* InViewProj, class UCamera* InCurrentCamera, EViewModeIndex InViewMode, uint64 InShowFlags)
        : ViewProjConstants(InViewProj), CurrentCamera(InCurrentCamera), ViewMode(InViewMode), ShowFlags(InShowFlags) {}
    
    const FViewProjConstants* ViewProjConstants;
    UCamera* CurrentCamera;
    EViewModeIndex ViewMode;
    uint64 ShowFlags;

    TArray<class UPrimitiveComponent*> AllPrimitives;
    // Components By Render Pass
    TArray<class UStaticMeshComponent*> StaticMeshes;
    TArray<class UBillBoardComponent*> BillBoards;
    TArray<class UTextComponent*> Texts;
    TArray<class UDecalComponent*> Decals;
    TArray<class UPrimitiveComponent*> DefaultPrimitives;
};
