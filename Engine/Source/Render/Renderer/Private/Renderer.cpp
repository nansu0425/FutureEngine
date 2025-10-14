#include "pch.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/Viewport.h"
#include "Editor/Public/ViewportClient.h"
#include "Editor/Public/Camera.h"
#include "Level/Public/Level.h"
#include "Manager/UI/Public/UIManager.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Render/RenderPass/Public/RenderPass.h"
#include "Component/Mesh/Public/StaticMesh.h"
#include "Optimization/Public/OcclusionCuller.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/RenderPass/Public/BillboardPass.h"
#include "Render/RenderPass/Public/StaticMeshPass.h"
#include "Render/RenderPass/Public/TextPass.h"
#include "Render/RenderPass/Public/DecalPass.h"
#include "Render/RenderPass/Public/FXAAPass.h"

IMPLEMENT_SINGLETON_CLASS_BASE(URenderer)

URenderer::URenderer() = default;

URenderer::~URenderer() = default;


void URenderer::Init(HWND InWindowHandle)
{
	DeviceResources = new UDeviceResources(InWindowHandle);
	Pipeline = new UPipeline(GetDeviceContext());
	ViewportClient = new FViewport();
	
	
	// 렌더링 상태 및 리소스 생성
	CreateDepthStencilState();
	CreateBlendState();
	CreateDefaultShader();
	CreateTextureShader();
	CreateDecalShader();
	CreateConstantBuffers();
	CreateFXAAShader();
	

	ViewportClient->InitializeLayout(DeviceResources->GetViewportInfo());

	FStaticMeshPass* StaticMeshPass = new FStaticMeshPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		TextureVertexShader, TexturePixelShader, TextureInputLayout, DefaultDepthStencilState);
	RenderPasses.push_back(StaticMeshPass);

	FDecalPass* DecalPass = new FDecalPass(Pipeline, ConstantBufferViewProj,
		DecalVertexShader, DecalPixelShader, DecalInputLayout, DecalDepthStencilState, AlphaBlendState);
	RenderPasses.push_back(DecalPass);

	FBillboardPass* BillboardPass = new FBillboardPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		TextureVertexShader, TexturePixelShader, TextureInputLayout, DefaultDepthStencilState, AlphaBlendState);
	RenderPasses.push_back(BillboardPass);

	FTextPass* TextPass = new FTextPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels);
	RenderPasses.push_back(TextPass);

	// UPipeline* InPipeline, UDeviceResources* InDeviceResources, ID3D11VertexShader* InVS,
	// ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11SamplerState* InSampler
	FXAAPass = new FFXAAPass(Pipeline, DeviceResources, FXAAVertexShader, FXAAPixelShader, FXAAInputLayout, FXAASamplerState);
	//RenderPasses.push_back(FXAAPass);
	
}

void URenderer::Release()
{
	ReleaseConstantBuffers();
	ReleaseDefaultShader();
	ReleaseDepthStencilState();
	ReleaseBlendState();
	ReleaseSamplerState();
	FRenderResourceFactory::ReleaseRasterizerState();
	for (auto& RenderPass : RenderPasses)
	{
		RenderPass->Release();
		SafeDelete(RenderPass);
	}

	SafeDelete(ViewportClient);
	SafeDelete(Pipeline);
	SafeDelete(DeviceResources);
}

void URenderer::CreateDepthStencilState()
{
	// 3D Default Depth Stencil (Depth O, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DefaultDescription = {};
	DefaultDescription.DepthEnable = TRUE;
	DefaultDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DefaultDescription.DepthFunc = D3D11_COMPARISON_LESS;
	DefaultDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DefaultDescription, &DefaultDepthStencilState);

	// Decal Depth Stencil (Depth Read, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DecalDescription = {};
	DecalDescription.DepthEnable = TRUE;
	DecalDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DecalDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DecalDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DecalDescription, &DecalDepthStencilState);


	// Disabled Depth Stencil (Depth X, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DisabledDescription = {};
	DisabledDescription.DepthEnable = FALSE;
	DisabledDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DisabledDescription, &DisabledDepthStencilState);
}

void URenderer::CreateBlendState()
{
    // Alpha Blending
    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    GetDevice()->CreateBlendState(&BlendDesc, &AlphaBlendState);
}

void URenderer::CreateDefaultShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DefaultLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/SampleShader.hlsl", DefaultLayout, &DefaultVertexShader, &DefaultInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/SampleShader.hlsl", &DefaultPixelShader);
	Stride = sizeof(FNormalVertex);
}

void URenderer::CreateTextureShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> TextureLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/TextureVS.hlsl", TextureLayout, &TextureVertexShader, &TextureInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/TexturePS.hlsl", &TexturePixelShader);
}

void URenderer::CreateDecalShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DecalLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/DecalShader.hlsl", DecalLayout, &DecalVertexShader, &DecalInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/DecalShader.hlsl", &DecalPixelShader);
}

void URenderer::CreateFXAAShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> FXAALayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/FXAAShader.hlsl", FXAALayout, &FXAAVertexShader, &FXAAInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/FXAAShader.hlsl", &FXAAPixelShader);
	
	FXAASamplerState = FRenderResourceFactory::CreateFXAASamplerState();
}

void URenderer::ReleaseDefaultShader()
{
	SafeRelease(DefaultInputLayout);
	SafeRelease(DefaultPixelShader);
	SafeRelease(DefaultVertexShader);
	SafeRelease(TextureInputLayout);
	SafeRelease(TexturePixelShader);
	SafeRelease(TextureVertexShader);
	SafeRelease(DecalVertexShader);
	SafeRelease(DecalPixelShader);
	SafeRelease(FXAAVertexShader);
	SafeRelease(FXAAPixelShader);
	SafeRelease(FXAAInputLayout);
}

void URenderer::ReleaseDepthStencilState()
{
	SafeRelease(DefaultDepthStencilState);
	SafeRelease(DecalDepthStencilState);
	SafeRelease(DisabledDepthStencilState);
	if (GetDeviceContext())
	{
		GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
	}
}

void URenderer::ReleaseBlendState()
{
    SafeRelease(AlphaBlendState);
}

void URenderer::ReleaseSamplerState()
{
	SafeRelease(FXAASamplerState);
}

void URenderer::Update()
{
	// 토글에 따라서 FXAA bool값 세팅
    if (const ULevel* CurrentLevel = GWorld->GetLevel())
    {
        bFXAAEnabled = (CurrentLevel->GetShowFlags() & EEngineShowFlags::SF_FXAA) != 0;
    }
    else
    {
        bFXAAEnabled = true;
    }

    RenderBegin();

    for (FViewportClient& ViewportClient : ViewportClient->GetViewports())
    {
        if (ViewportClient.GetViewportInfo().Width < 1.0f || ViewportClient.GetViewportInfo().Height < 1.0f) { continue; }

        ViewportClient.Apply(GetDeviceContext());

        UCamera* CurrentCamera = &ViewportClient.Camera;
        CurrentCamera->Update(ViewportClient.GetViewportInfo());
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj, CurrentCamera->GetFViewProjConstants());
        Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

        {
            TIME_PROFILE(RenderEditor)
            GEditor->GetEditorModule()->RenderEditor();
        }
        {
            TIME_PROFILE(RenderLevel)
            RenderLevel(CurrentCamera);
        }
        // Gizmo는 최종적으로 렌더
        GEditor->GetEditorModule()->RenderGizmo(CurrentCamera);
    }

    // 모든 지오메트리 패스가 끝난 직후, UI/오버레이를 그리기 전 실행
	// FXAAPass->Execute의 RenderingContext는 쓰레기 값
	// TODO : 포스트 프로세스 패스를 따로 파야할지도
    if (bFXAAEnabled)
    {
        ID3D11RenderTargetView* nullRTV[] = { nullptr };
        GetDeviceContext()->OMSetRenderTargets(1, nullRTV, nullptr);

        FRenderingContext RenderingContext;
        FXAAPass->Execute(RenderingContext);
    }

    {
        TIME_PROFILE(UUIManager)
        UUIManager::GetInstance().Render();
    }
    {
        TIME_PROFILE(UStatOverlay)
        UStatOverlay::GetInstance().Render();
    }

    RenderEnd();
}

void URenderer::RenderBegin() const
{
    auto* DepthStencilView = DeviceResources->GetDepthStencilView();

	// FXAA bool값 변수에 따라서 RTV세팅
    if (bFXAAEnabled)
    {
        auto* SceneColorRenderTargetView = DeviceResources->GetSceneColorRenderTargetView();
        GetDeviceContext()->ClearRenderTargetView(SceneColorRenderTargetView, ClearColor);
        GetDeviceContext()->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
        ID3D11RenderTargetView* rtvs[] = { SceneColorRenderTargetView };
        GetDeviceContext()->OMSetRenderTargets(1, rtvs, DepthStencilView);
    }
    else
    {
        auto* RenderTargetView = DeviceResources->GetRenderTargetView();
        GetDeviceContext()->ClearRenderTargetView(RenderTargetView, ClearColor);
        GetDeviceContext()->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
        ID3D11RenderTargetView* rtvs[] = { RenderTargetView };
        GetDeviceContext()->OMSetRenderTargets(1, rtvs, DepthStencilView);
    }

    DeviceResources->UpdateViewport();
}


void URenderer::RenderLevel(UCamera* InCurrentCamera)
{
	const ULevel* CurrentLevel = GWorld->GetLevel();
	if (!CurrentLevel) { return; }

	// 오클루전 컬링
	TIME_PROFILE(Occlusion)
	// static COcclusionCuller Culler;
	const FCameraConstants& ViewProj = InCurrentCamera->GetFViewProjConstants();
	// Culler.InitializeCuller(ViewProj.View, ViewProj.Projection);
	TArray<UPrimitiveComponent*> FinalVisiblePrims = InCurrentCamera->GetViewVolumeCuller().GetRenderableObjects();
	// Culler.PerformCulling(
	// 	InCurrentCamera->GetViewVolumeCuller().GetRenderableObjects(),
	// 	InCurrentCamera->GetLocation()
	// );
	TIME_PROFILE_END(Occlusion)

	FRenderingContext RenderingContext(&ViewProj, InCurrentCamera, GEditor->GetEditorModule()->GetViewMode(), CurrentLevel->GetShowFlags());
	RenderingContext.AllPrimitives = FinalVisiblePrims;
	for (auto& Prim : FinalVisiblePrims)
	{
		if (auto StaticMesh = Cast<UStaticMeshComponent>(Prim))
		{
			RenderingContext.StaticMeshes.push_back(StaticMesh);
		}
		else if (auto BillBoard = Cast<UBillBoardComponent>(Prim))
		{
			RenderingContext.BillBoards.push_back(BillBoard);
		}
		else if (auto Text = Cast<UTextComponent>(Prim))
		{
			if (!Text->IsExactly(UUUIDTextComponent::StaticClass())) { RenderingContext.Texts.push_back(Text); }
			else { RenderingContext.UUIDs.push_back(Cast<UUUIDTextComponent>(Text)); }
		}
		else if (auto Decal = Cast<UDecalComponent>(Prim))
		{
			RenderingContext.Decals.push_back(Decal);
		}
	}

	for (auto RenderPass: RenderPasses)
	{
		RenderPass->Execute(RenderingContext);
	}
}

void URenderer::RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride, uint32 InIndexBufferStride)
{
    // Use the global stride if InStride is 0
    const uint32 FinalStride = (InStride == 0) ? Stride : InStride;

    // Allow for custom shaders, fallback to default
    FPipelineInfo PipelineInfo = {
        InPrimitive.InputLayout ? InPrimitive.InputLayout : DefaultInputLayout,
        InPrimitive.VertexShader ? InPrimitive.VertexShader : DefaultVertexShader,
		FRenderResourceFactory::GetRasterizerState(InRenderState),
        InPrimitive.bShouldAlwaysVisible ? DisabledDepthStencilState : DefaultDepthStencilState,
        InPrimitive.PixelShader ? InPrimitive.PixelShader : DefaultPixelShader,
        nullptr,
        InPrimitive.Topology
    };
    Pipeline->UpdatePipeline(PipelineInfo);

    // Update constant buffers
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModels, FMatrix::GetModelMatrix(InPrimitive.Location, FVector::GetDegreeToRadian(InPrimitive.Rotation), InPrimitive.Scale));
	Pipeline->SetConstantBuffer(0, true, ConstantBufferModels);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
	
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferColor, InPrimitive.Color);
	Pipeline->SetConstantBuffer(2, false, ConstantBufferColor);
	Pipeline->SetConstantBuffer(2, true, ConstantBufferColor);
	
    Pipeline->SetVertexBuffer(InPrimitive.VertexBuffer, FinalStride);

    // The core logic: check for an index buffer
    if (InPrimitive.IndexBuffer && InPrimitive.NumIndices > 0)
    {
        Pipeline->SetIndexBuffer(InPrimitive.IndexBuffer, InIndexBufferStride);
        Pipeline->DrawIndexed(InPrimitive.NumIndices, 0, 0);
    }
    else
    {
        Pipeline->Draw(InPrimitive.NumVertices, 0);
    }
}

void URenderer::RenderEnd() const
{
	TIME_PROFILE(DrawCall)
	GetSwapChain()->Present(0, 0);
	TIME_PROFILE_END(DrawCall)
}

void URenderer::OnResize(uint32 InWidth, uint32 InHeight) const
{
    if (!DeviceResources || !GetDeviceContext() || !GetSwapChain()) return;

    DeviceResources->ReleaseSceneColorTarget();
    DeviceResources->ReleaseFrameBuffer();
    DeviceResources->ReleaseDepthBuffer();
    GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);

    if (FAILED(GetSwapChain()->ResizeBuffers(2, InWidth, InHeight, DXGI_FORMAT_UNKNOWN, 0)))
    {
        UE_LOG("OnResize Failed");
        return;
    }

    DeviceResources->UpdateViewport();
    DeviceResources->CreateFrameBuffer();
    DeviceResources->CreateDepthBuffer();
    DeviceResources->CreateSceneColorTarget();

    ID3D11RenderTargetView* targetView = bFXAAEnabled
        ? DeviceResources->GetSceneColorRenderTargetView()
        : DeviceResources->GetRenderTargetView();
    ID3D11RenderTargetView* targetViews[] = { targetView };
    GetDeviceContext()->OMSetRenderTargets(1, targetViews, DeviceResources->GetDepthStencilView());
}


void URenderer::CreateConstantBuffers()
{
	ConstantBufferModels = FRenderResourceFactory::CreateConstantBuffer<FMatrix>();
	ConstantBufferColor = FRenderResourceFactory::CreateConstantBuffer<FVector4>();
	ConstantBufferViewProj = FRenderResourceFactory::CreateConstantBuffer<FCameraConstants>();
}


void URenderer::ReleaseConstantBuffers()
{
	SafeRelease(ConstantBufferModels);
	SafeRelease(ConstantBufferColor);
	SafeRelease(ConstantBufferViewProj);
}
