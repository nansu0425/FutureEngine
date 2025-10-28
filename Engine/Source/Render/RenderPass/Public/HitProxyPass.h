#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

/**
 * @brief HitProxy 렌더링 패스
 * 컴포넌트와 기즈모를 고유한 색상(HitProxyId)으로 렌더링하여 color picking을 가능하게 함
 * 마우스 클릭 시에만 온디맨드로 실행
 */
class FHitProxyPass :
	public FRenderPass
{
public:
	FHitProxyPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, ID3D11Buffer* InConstantBufferModel,
		ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS);
	void Execute(FRenderingContext& Context) override;
	void Release() override;

	void SetShaders(ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout);

private:
	ID3D11VertexShader* VS = nullptr;
	ID3D11PixelShader* PS = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;
	ID3D11DepthStencilState* DS = nullptr;
	ID3D11Buffer* ConstantBufferHitProxyColor = nullptr;
};
