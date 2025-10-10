#include "pch.h"
#include "Render/RenderPass/Public/BillboardPass.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

FBillboardPass::FBillboardPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11Buffer* InConstantBufferModel,
                               ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS)
        : FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel), VS(InVS), PS(InPS), InputLayout(InLayout), DS(InDS)
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
    static FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState(RenderState), DS, PS, nullptr };
    Pipeline->UpdatePipeline(PipelineInfo);

    if (!(Context.ShowFlags & EEngineShowFlags::SF_Billboard)) return;
    for (UBillBoardComponent* BillBoardComp : Context.BillBoards)
    {
        BillBoardComp->FaceCamera(Context.CurrentCamera->GetLocation(), Context.CurrentCamera->GetUp(), Context.CurrentCamera->GetRight());

        Pipeline->SetVertexBuffer(BillBoardComp->GetVertexBuffer(), sizeof(FNormalVertex));
        Pipeline->SetIndexBuffer(BillBoardComp->GetIndexBuffer(), 0);
		
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, BillBoardComp->GetWorldTransformMatrix());
        Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

        Pipeline->SetTexture(0, false, BillBoardComp->GetSprite().second);
        Pipeline->SetSamplerState(0, false, BillBoardComp->GetSampler());
        Pipeline->DrawIndexed(BillBoardComp->GetNumIndices(), 0, 0);
    }
}

void FBillboardPass::Release()
{
}
