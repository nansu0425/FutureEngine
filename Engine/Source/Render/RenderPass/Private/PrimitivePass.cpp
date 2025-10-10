#include "pch.h"
#include "Render/RenderPass/Public/PrimitivePass.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

FPrimitivePass::FPrimitivePass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11Buffer* InConstantBufferModel,
                               ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS)
        : FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel), VS(InVS), PS(InPS), InputLayout(InLayout), DS(InDS)
{
    ConstantBufferColor = FRenderResourceFactory::CreateConstantBuffer<FVector4>();
}

void FPrimitivePass::Execute(FRenderingContext& Context)
{
    FRenderState DefaultState;
    if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
    {
       DefaultState.CullMode = ECullMode::None;
       DefaultState.FillMode = EFillMode::WireFrame;
    }

    FPipelineInfo PipelineInfo = { InputLayout, VS, nullptr, DS, PS, nullptr };
    Pipeline->UpdatePipeline(PipelineInfo);
    Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);
    Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
    Pipeline->SetConstantBuffer(2, false, ConstantBufferColor);
    if (!(Context.ShowFlags & EEngineShowFlags::SF_Primitives)) return;
    
    for (UPrimitiveComponent* PrimitiveComponent : Context.DefaultPrimitives)
    {
        if (Context.ViewMode != EViewModeIndex::VMI_Wireframe)
        {
            DefaultState = PrimitiveComponent->GetRenderState();
        }
        
        PipelineInfo.RasterizerState = FRenderResourceFactory::GetRasterizerState(DefaultState);
        Pipeline->UpdatePipeline(PipelineInfo);
        
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, PrimitiveComponent->GetWorldTransformMatrix());
        Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferColor, PrimitiveComponent->GetColor());
        Pipeline->SetConstantBuffer(2, true, ConstantBufferColor);
        Pipeline->SetConstantBuffer(2, false, ConstantBufferColor);
        Pipeline->SetVertexBuffer(PrimitiveComponent->GetVertexBuffer(), sizeof(FNormalVertex));

        if (PrimitiveComponent->GetIndexBuffer() && PrimitiveComponent->GetIndicesData())
        {
           Pipeline->SetIndexBuffer(PrimitiveComponent->GetIndexBuffer(), 0);
           Pipeline->DrawIndexed(PrimitiveComponent->GetNumIndices(), 0, 0);
        }
        else
        {
           Pipeline->Draw(PrimitiveComponent->GetNumVertices(), 0);
        }
    }
}

void FPrimitivePass::Release()
{
    SafeRelease(ConstantBufferColor);
}
