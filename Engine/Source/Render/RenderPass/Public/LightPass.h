#pragma once
#include "Global/Vector.h"
#include "Render/RenderPass/Public/RenderPass.h"

struct FViewClusterInfo 
{
	FMatrix ProjectionInv;
	FMatrix ViewInv;
	FMatrix ViewMat;
	float ZNear;
	float ZFar;
	uint32 ScreenSlideNumX;
	uint32 ScreenSlideNumY;
	uint32 ZSlideNum;
	uint32 LightMaxCountPerCluster;
	FVector2 padding;
};
struct FLightCountInfo
{
	uint32 PointLightCount;
	uint32 SpotLightCount;
	FVector2 padding;
};
struct FGizmoVertex
{
	FVector Pos;
	FVector4 Color;
};

class FLightPass : public FRenderPass
{
public:
	FLightPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, 
		ID3D11InputLayout* InGizmoInputLayout, ID3D11VertexShader* InGizmoVS, ID3D11PixelShader* InGizmoPS,
		ID3D11DepthStencilState* InGizmoDSS);
	void Execute(FRenderingContext& Context) override;
	void Release() override;


private:
	uint32 GetClusterCount() const { return ScreenXSlideNum * ScreenYSlideNum * ZSlideNum; }
public:
private:
	ID3D11ComputeShader* ViewClusterCS = nullptr;
	ID3D11ComputeShader* ClusteredLightCullingCS = nullptr;
	ID3D11ComputeShader* ClusterGizmoSetCS = nullptr;

	ID3D11Buffer* ViewClusterInfoConstantBuffer = nullptr;
	ID3D11Buffer* LightCountInfoConstantBuffer = nullptr;
	ID3D11Buffer* GlobalLightConstantBuffer = nullptr;
	ID3D11Buffer* PointLightStructuredBuffer = nullptr;
	ID3D11Buffer* SpotLightStructuredBuffer = nullptr;
	ID3D11Buffer* ClusterAABBRWStructuredBuffer = nullptr;
	ID3D11Buffer* LightIndicesRWStructuredBuffer = nullptr;

	ID3D11ShaderResourceView* PointLightStructuredBufferSRV = nullptr;
	ID3D11ShaderResourceView* SpotLightStructuredBufferSRV = nullptr;
	ID3D11ShaderResourceView* ClusterAABBRWStructuredBufferSRV = nullptr;
	ID3D11UnorderedAccessView* ClusterAABBRWStructuredBufferUAV = nullptr;
	ID3D11ShaderResourceView* LightIndicesRWStructuredBufferSRV = nullptr;
	ID3D11UnorderedAccessView* LightIndicesRWStructuredBufferUAV = nullptr;
	
	uint32 PointLightBufferCount = 256;
	uint32 SpotLightBufferCount = 256;
	uint32 ScreenXSlideNum = 24;
	uint32 ScreenYSlideNum = 16;
	uint32 ZSlideNum = 32;
	uint32 LightMaxCountPerCluster = 8;


	ID3D11Buffer* ClusterGizmoVertex = nullptr;
	ID3D11UnorderedAccessView* ClusterGizmoVertexUAV = nullptr;
	ID3D11ShaderResourceView* ClusterGizmoVertexSRV = nullptr;
	//해제 X
	ID3D11Buffer* CameraConstantBuffer = nullptr;
	ID3D11InputLayout* GizmoInputLayout = nullptr;
	ID3D11VertexShader* GizmoVS = nullptr;
	ID3D11PixelShader* GizmoPS = nullptr;
	ID3D11DepthStencilState* GizmoDSS = nullptr;
	bool bRenderClusterGizmo = false;





};