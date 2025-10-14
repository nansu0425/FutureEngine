#pragma once

struct FRenderingContext
{
    FRenderingContext(){}
    FRenderingContext(const FCameraConstants* InViewProj, class UCamera* InCurrentCamera, EViewModeIndex InViewMode, uint64 InShowFlags)
        : ViewProjConstants(InViewProj), CurrentCamera(InCurrentCamera), ViewMode(InViewMode), ShowFlags(InShowFlags) {}
    
    const FCameraConstants* ViewProjConstants= nullptr;
    UCamera* CurrentCamera = nullptr;
    EViewModeIndex ViewMode;
    uint64 ShowFlags;

    TArray<class UPrimitiveComponent*> AllPrimitives;
    // Components By Render Pass
    TArray<class UStaticMeshComponent*> StaticMeshes;
    TArray<class UBillBoardComponent*> BillBoards;
    TArray<class UTextComponent*> Texts;
    TArray<class UUUIDTextComponent*> UUIDs;
    TArray<class UDecalComponent*> Decals;
};
