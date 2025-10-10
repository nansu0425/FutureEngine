#include "pch.h"
#include "Render/RenderPass/Public/DecalPass.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/RenderPass/Public/RenderingContext.h"
#include "Component/Public/DecalComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Physics/Public/OBB.h"
#include "Texture/Public/Texture.h"
#include "Texture/Public/TextureRenderProxy.h"

FDecalPass::FDecalPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS_Read, ID3D11BlendState* InBlendState)
    : FRenderPass(InPipeline, InConstantBufferViewProj, nullptr),
    VS(InVS), PS(InPS), InputLayout(InLayout), DS_Read(InDS_Read), BlendState(InBlendState)
{
    ConstantBufferPrim = FRenderResourceFactory::CreateConstantBuffer<FModelConstants>();
    ConstantBufferDecal = FRenderResourceFactory::CreateConstantBuffer<FDecalConstants>();
}

void FDecalPass::Execute(FRenderingContext& Context)
{
    if (Context.Decals.empty()) { return; }

    // --- Set Pipeline State ---
    FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid }),
        DS_Read, PS, BlendState };
    Pipeline->UpdatePipeline(PipelineInfo);
    Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

    // --- Render Decals ---
    for (UDecalComponent* Decal : Context.Decals)
    {
        if (!Decal || !Decal->IsVisible()) { continue; }

        const IBoundingVolume* DecalBV = Decal->GetBoundingBox();
        if (!DecalBV || DecalBV->GetType() != EBoundingVolumeType::OBB) { continue; }

        const FOBB* DecalOBB = static_cast<const FOBB*>(DecalBV);

        // --- Update Decal Constant Buffer ---
        FDecalConstants DecalConstants;
        DecalConstants.DecalWorld = Decal->GetWorldTransformMatrix();
        DecalConstants.DecalWorldInverse = Decal->GetWorldTransformMatrixInverse();

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferDecal, DecalConstants);
        Pipeline->SetConstantBuffer(2, false, ConstantBufferDecal);

        // --- Bind Decal Texture ---
        if (UTexture* DecalTexture = Decal->GetTexture())
        {
            if (auto* Proxy = DecalTexture->GetRenderProxy())
            {
                Pipeline->SetTexture(0, false, Proxy->GetSRV());
                Pipeline->SetSamplerState(0, false, Proxy->GetSampler());
            }
        }

        for (UPrimitiveComponent* Prim : Context.DefaultPrimitives)
        {
            if (!Prim || !Prim->IsVisible()) { continue; }
            
            //const FAABB* PrimWorldAABB = static_cast<const FAABB*>(Prim->GetBoundingBox());
            
            FModelConstants ModelConstants{Prim->GetWorldTransformMatrix(), Prim->GetWorldTransformMatrixInverse().Transpose()};
            FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPrim, ModelConstants);
            Pipeline->SetConstantBuffer(0, true, ConstantBufferPrim);
            
            Pipeline->SetVertexBuffer(Prim->GetVertexBuffer(), sizeof(FNormalVertex));
            if (Prim->GetIndexBuffer() && Prim->GetIndicesData())
            {
                Pipeline->SetIndexBuffer(Prim->GetIndexBuffer(), 0);
                Pipeline->DrawIndexed(Prim->GetNumIndices(), 0, 0);
            }
            else
            {
                Pipeline->Draw(Prim->GetNumVertices(), 0);
            }
        }
    }
}

void FDecalPass::Release()
{
    SafeRelease(ConstantBufferPrim);
    SafeRelease(ConstantBufferDecal);
}