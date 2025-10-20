#pragma once
#include "Global/Vector.h"
#include "Render/RenderPass/Public/RenderPass.h"

struct ViewClusterInfo 
{
	FMatrix ProjectionInv;
	float ZNear;
	float ZFar;
	uint32 ScreenSlideNumX;
	uint32 ScreenSlideNumY;
	uint32 ZSlideNum;
	FVector padding;
};

class FLightPass : public FRenderPass
{
public:
	FLightPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera);
	void Execute(FRenderingContext& Context) override;
	void Release() override;


private:
	uint32 GetClusterCount() const { return ScreenXSlideNum * ScreenYSlideNum * ZSlideNum; }
public:
private:
	ID3D11ComputeShader* ViewClusterCS = nullptr;

	ID3D11Buffer* ViewClusterInfoConstantBuffer = nullptr;
	ID3D11Buffer* GlobalLightConstantBuffer = nullptr;
	ID3D11Buffer* PointLightStructuredBuffer = nullptr;
	ID3D11Buffer* SpotLightStructuredBuffer = nullptr;
	ID3D11Buffer* ClusterAABBRWStructuredBuffer = nullptr;

	ID3D11ShaderResourceView* PointLightStructuredBufferSRV = nullptr;
	ID3D11ShaderResourceView* SpotLightStructuredBufferSRV = nullptr;
	ID3D11ShaderResourceView* ClusterAABBRWStructuredBufferSRV = nullptr;
	ID3D11UnorderedAccessView* ClusterAABBRWStructuredBufferUAV = nullptr;

	
	uint32 PointLightBufferCount = 256;
	uint32 SpotLightBufferCount = 256;
	uint32 ScreenXSlideNum = 24;
	uint32 ScreenYSlideNum = 16;
	uint32 ZSlideNum = 32;



};