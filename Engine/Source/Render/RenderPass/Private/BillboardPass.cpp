#include "pch.h"
#include "Render/RenderPass/Public/BillboardPass.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Texture/Public/Texture.h"

FBillboardPass::FBillboardPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11Buffer* InConstantBufferModel,
                               ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS, ID3D11BlendState* InBS)
        : FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel), VS(InVS), PS(InPS), InputLayout(InLayout), DS(InDS), BS(InBS)
{
}

void FBillboardPass::Execute(FRenderingContext& Context)
{
    FRenderState RenderState = UBillBoardComponent::GetClassDefaultRenderState();
    if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
    {
        RenderState.CullMode = ECullMode::None;
        RenderState.FillMode = EFillMode::WireFrame;
    }
    FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState(RenderState), DS, PS, BS };
    Pipeline->UpdatePipeline(PipelineInfo);

    if (!(Context.ShowFlags & EEngineShowFlags::SF_Billboard)) { return; }
    for (UBillBoardComponent* BillBoardComp : Context.BillBoards)
    {
        BillBoardComp->FaceCamera(Context.CurrentCamera->GetForward());
        
        FMatrix WorldMatrix;
        if (BillBoardComp->IsScreenSizeScaled())
        {
            FVector FixedWorldScale = BillBoardComp->GetRelativeScale3D(); 

            FVector BillboardLocation = BillBoardComp->GetWorldLocation();
            FQuaternion BillboardRotation = BillBoardComp->GetWorldRotationAsQuaternion();

            WorldMatrix = FMatrix::GetModelMatrix(BillboardLocation, BillboardRotation, FixedWorldScale);
        }
        else { WorldMatrix = BillBoardComp->GetWorldTransformMatrix(); }

        Pipeline->SetVertexBuffer(BillBoardComp->GetVertexBuffer(), sizeof(FNormalVertex));
        Pipeline->SetIndexBuffer(BillBoardComp->GetIndexBuffer(), 0);
		
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, WorldMatrix);
        Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

        Pipeline->SetTexture(0, false, BillBoardComp->GetSprite()->GetTextureSRV());
        Pipeline->SetSamplerState(0, false, BillBoardComp->GetSprite()->GetTextureSampler());
        Pipeline->DrawIndexed(BillBoardComp->GetNumIndices(), 0, 0);
    }
}

void FBillboardPass::Release()
{
}
