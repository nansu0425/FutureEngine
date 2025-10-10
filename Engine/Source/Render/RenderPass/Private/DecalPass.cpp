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
#include <limits>

namespace
{
	bool Intersects(const FOBB& obb, const FAABB& aabb)
	{
		// AABB의 중심과 절반 크기
		FVector aabbCenter = (aabb.Min + aabb.Max) * 0.5f;
		FVector aabbHalfSize = (aabb.Max - aabb.Min) * 0.5f;

		// OBB의 축
		FVector obbAxes[3];
		obbAxes[0] = FVector(obb.ScaleRotation.Data[0][0], obb.ScaleRotation.Data[0][1], obb.ScaleRotation.Data[0][2]);
		obbAxes[1] = FVector(obb.ScaleRotation.Data[1][0], obb.ScaleRotation.Data[1][1], obb.ScaleRotation.Data[1][2]);
		obbAxes[2] = FVector(obb.ScaleRotation.Data[2][0], obb.ScaleRotation.Data[2][1], obb.ScaleRotation.Data[2][2]);
		obbAxes[0].Normalize();
		obbAxes[1].Normalize();
		obbAxes[2].Normalize();


		FVector testAxes[15];
		// 1. AABB의 축 (3개)
		testAxes[0] = FVector(1.f, 0.f, 0.f);
		testAxes[1] = FVector(0.f, 1.f, 0.f);
		testAxes[2] = FVector(0.f, 0.f, 1.f);

		// 2. OBB의 축 (3개)
		testAxes[3] = obbAxes[0];
		testAxes[4] = obbAxes[1];
		testAxes[5] = obbAxes[2];

		// 3. 외적 축 (9개)
		testAxes[6] = testAxes[0].Cross(testAxes[3]);
		testAxes[7] = testAxes[0].Cross(testAxes[4]);
		testAxes[8] = testAxes[0].Cross(testAxes[5]);
		testAxes[9] = testAxes[1].Cross(testAxes[3]);
		testAxes[10] = testAxes[1].Cross(testAxes[4]);
		testAxes[11] = testAxes[1].Cross(testAxes[5]);
		testAxes[12] = testAxes[2].Cross(testAxes[3]);
		testAxes[13] = testAxes[2].Cross(testAxes[4]);
		testAxes[14] = testAxes[2].Cross(testAxes[5]);


		for (int i = 0; i < 15; ++i)
		{
			FVector axis = testAxes[i];
			if (axis.LengthSquared() < 0.001f) continue;
			axis.Normalize();

			float obbMin = std::numeric_limits<float>::max();
			float obbMax = -std::numeric_limits<float>::max();

			FVector obbCorners[8];
			obbCorners[0] = obb.Center + obbAxes[0] * obb.Extents.X + obbAxes[1] * obb.Extents.Y + obbAxes[2] * obb.Extents.Z;
			obbCorners[1] = obb.Center - obbAxes[0] * obb.Extents.X + obbAxes[1] * obb.Extents.Y + obbAxes[2] * obb.Extents.Z;
			obbCorners[2] = obb.Center + obbAxes[0] * obb.Extents.X - obbAxes[1] * obb.Extents.Y + obbAxes[2] * obb.Extents.Z;
			obbCorners[3] = obb.Center + obbAxes[0] * obb.Extents.X + obbAxes[1] * obb.Extents.Y - obbAxes[2] * obb.Extents.Z;
			obbCorners[4] = obb.Center - obbAxes[0] * obb.Extents.X - obbAxes[1] * obb.Extents.Y - obbAxes[2] * obb.Extents.Z;
			obbCorners[5] = obb.Center + obbAxes[0] * obb.Extents.X - obbAxes[1] * obb.Extents.Y - obbAxes[2] * obb.Extents.Z;
			obbCorners[6] = obb.Center - obbAxes[0] * obb.Extents.X + obbAxes[1] * obb.Extents.Y - obbAxes[2] * obb.Extents.Z;
			obbCorners[7] = obb.Center - obbAxes[0] * obb.Extents.X - obbAxes[1] * obb.Extents.Y + obbAxes[2] * obb.Extents.Z;

			for (int j = 0; j < 8; ++j)
			{
				float projection = obbCorners[j].Dot(axis);
				obbMin = min(obbMin, projection);
				obbMax = max(obbMax, projection);
			}

			float aabbMin = std::numeric_limits<float>::max();
			float aabbMax = -std::numeric_limits<float>::max();

			FVector aabbCorners[8];
			aabbCorners[0] = aabbCenter + FVector(aabbHalfSize.X, aabbHalfSize.Y, aabbHalfSize.Z);
			aabbCorners[1] = aabbCenter + FVector(-aabbHalfSize.X, aabbHalfSize.Y, aabbHalfSize.Z);
			aabbCorners[2] = aabbCenter + FVector(aabbHalfSize.X, -aabbHalfSize.Y, aabbHalfSize.Z);
			aabbCorners[3] = aabbCenter + FVector(aabbHalfSize.X, aabbHalfSize.Y, -aabbHalfSize.Z);
			aabbCorners[4] = aabbCenter + FVector(-aabbHalfSize.X, -aabbHalfSize.Y, -aabbHalfSize.Z);
			aabbCorners[5] = aabbCenter + FVector(aabbHalfSize.X, -aabbHalfSize.Y, -aabbHalfSize.Z);
			aabbCorners[6] = aabbCenter + FVector(-aabbHalfSize.X, aabbHalfSize.Y, -aabbHalfSize.Z);
			aabbCorners[7] = aabbCenter + FVector(-aabbHalfSize.X, -aabbHalfSize.Y, aabbHalfSize.Z);

			for (int j = 0; j < 8; ++j)
			{
				float projection = aabbCorners[j].Dot(axis);
				aabbMin = min(aabbMin, projection);
				aabbMax = max(aabbMax, projection);
			}

			if (obbMax < aabbMin || aabbMax < obbMin)
			{
				return false; // 분리 축 발견
			}
		}

		return true; // 모든 축에서 겹침
	}
}

FDecalPass::FDecalPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS_Read, ID3D11BlendState* InBlendState)
    : FRenderPass(InPipeline, InConstantBufferViewProj, nullptr),
    VS(InVS), PS(InPS), InputLayout(InLayout), DS_Read(InDS_Read), BlendState(InBlendState)
{
    ConstantBufferPrim = FRenderResourceFactory::CreateConstantBuffer<FModelConstants>();
    ConstantBufferDecal = FRenderResourceFactory::CreateConstantBuffer<FDecalConstants>();
}

void FDecalPass::Execute(FRenderingContext& Context)
{
	TIME_PROFILE(DecalPass)

    if (Context.Decals.empty()) { return; }

    // --- Set Pipeline State ---
    FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState({ ECullMode::Back, EFillMode::Solid }),
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


        //UE_LOG("%d", Context.DefaultPrimitives.size());
        for (UPrimitiveComponent* Prim : Context.DefaultPrimitives)
        {
        	if (!Prim || !Prim->IsVisible()) { continue; }
        
        	const IBoundingVolume* PrimBV = Prim->GetBoundingBox();
        	if (!PrimBV || PrimBV->GetType() != EBoundingVolumeType::AABB) { continue; }
        	const FAABB* PrimAABB = static_cast<const FAABB*>(PrimBV);
        
        	if (!Intersects(*DecalOBB, *PrimAABB))
        	{
        		continue;
        	}
        	
        	//const FAABB* PrimWorldAABB = static_cast<const FAABB*>(Prim->GetBoundingBox());
        	
        	FModelConstants ModelConstants{Prim->GetWorldTransformMatrix(), Prim->GetWorldTransformMatrixInverse().Transpose()};
        	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPrim, ModelConstants);            Pipeline->SetConstantBuffer(0, true, ConstantBufferPrim);
            
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