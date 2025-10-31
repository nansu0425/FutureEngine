#include "pch.h"
#include "Render/Renderer/Public/DeviceResources.h"

UDeviceResources::UDeviceResources(HWND InWindowHandle)
{
	Create(InWindowHandle);
}

UDeviceResources::~UDeviceResources()
{
	Release();
}

void UDeviceResources::Create(HWND InWindowHandle)
{
	RECT ClientRect;
	GetClientRect(InWindowHandle, &ClientRect);
	Width = ClientRect.right - ClientRect.left;
	Height = ClientRect.bottom - ClientRect.top;

	CreateDeviceAndSwapChain(InWindowHandle);
	CreateFrameBuffer();
	CreateNormalBuffer();
	CreateDepthBuffer();
	CreateSceneColorTarget();
	CreateHitProxyTarget();
	CreateFactories();
}

void UDeviceResources::Release()
{
	ReleaseFactories();
	ReleaseHitProxyTarget();
	ReleaseSceneColorTarget();
	ReleaseFrameBuffer();
	ReleaseNormalBuffer();
	ReleaseDepthBuffer();
	ReleaseDeviceAndSwapChain();
}

/**
 * @brief Direct3D 장치 및 스왑 체인을 생성하는 함수
 * @param InWindowHandle
 */
void UDeviceResources::CreateDeviceAndSwapChain(HWND InWindowHandle)
{
	// 지원하는 Direct3D 기능 레벨을 정의
	D3D_FEATURE_LEVEL featurelevels[] = {D3D_FEATURE_LEVEL_11_0};

	// 스왑 체인 설정 구조체 초기화
	DXGI_SWAP_CHAIN_DESC SwapChainDescription = {};
	SwapChainDescription.BufferDesc.Width = 0; // 창 크기에 맞게 자동으로 설정
	SwapChainDescription.BufferDesc.Height = 0; // 창 크기에 맞게 자동으로 설정
	SwapChainDescription.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 색상 포맷
	SwapChainDescription.SampleDesc.Count = 1; // 멀티 샘플링 비활성화
	SwapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT; // 렌더 타겟으로 사용
	SwapChainDescription.BufferCount = 2; // 더블 버퍼링
	SwapChainDescription.OutputWindow = InWindowHandle; // 렌더링할 창 핸들
	SwapChainDescription.Windowed = TRUE; // 창 모드
	SwapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 스왑 방식

	// Direct3D 장치와 스왑 체인을 생성
	HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
	                                           D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG ,
	                                           featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
	                                           &SwapChainDescription, &SwapChain, &Device, nullptr, &DeviceContext);

	if (FAILED(hr))
	{
		assert(!"Failed To Create SwapChain");
	}

	// 생성된 스왑 체인의 정보 가져오기
	SwapChain->GetDesc(&SwapChainDescription);

	// Viewport Info 업데이트
	ViewportInfo = {
		0.0f, 0.0f, static_cast<float>(SwapChainDescription.BufferDesc.Width),
		static_cast<float>(SwapChainDescription.BufferDesc.Height), 0.0f, 1.0f
	};
}

/**
 * @brief Direct3D 장치 및 스왑 체인을 해제하는 함수
 */
void UDeviceResources::ReleaseDeviceAndSwapChain()
{
	if (DeviceContext)
	{
		DeviceContext->ClearState();
		// 남아있는 GPU 명령 실행
		DeviceContext->Flush();
	}

	if (SwapChain)
	{
		SwapChain->Release();
		SwapChain = nullptr;
	}

	if (DeviceContext)
	{
		DeviceContext->Release();
		DeviceContext = nullptr;
	}

	// DX 메모리 Leak 디버깅용 함수
	// DXGI 누출 로그 출력 시 주석 해제로 타입 확인 가능
	// ID3D11Debug* DebugPointer = nullptr;
	// HRESULT Result = GetDevice()->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&DebugPointer));
	// if (SUCCEEDED(Result))
	// {
	// 	DebugPointer->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
	// 	DebugPointer->Release();
	// }
	
	if (Device)
	{
		Device->Release();
		Device = nullptr;
	}
}

/**
 * @brief FrameBuffer 생성 함수
 */
void UDeviceResources::CreateFrameBuffer()
{
	// 스왑 체인으로부터 백 버퍼 텍스처 가져오기
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

	// 렌더 타겟 뷰 생성
	D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
	framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // 색상 포맷
	framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // 2D 텍스처

	Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);

	// 셰이더 리소스 뷰 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	Device->CreateShaderResourceView(FrameBuffer, &srvDesc, &FrameBufferSRV);
}

/**
 * @brief 프레임 버퍼를 해제하는 함수
 */
void UDeviceResources::ReleaseFrameBuffer()
{
	SafeRelease(FrameBufferSRV);
	SafeRelease(FrameBufferRTV);
	SafeRelease(FrameBuffer);
}

void UDeviceResources::CreateNormalBuffer()
{
	D3D11_TEXTURE2D_DESC Texture2DDesc = {};
	Texture2DDesc.Width = Width;
	Texture2DDesc.Height = Height;
	Texture2DDesc.MipLevels = 1;
	Texture2DDesc.ArraySize = 1;
	Texture2DDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	Texture2DDesc.SampleDesc.Count = 1;
	Texture2DDesc.SampleDesc.Quality = 0;
	Texture2DDesc.Usage = D3D11_USAGE_DEFAULT;
	Texture2DDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	Texture2DDesc.CPUAccessFlags = 0;
	Texture2DDesc.MiscFlags = 0;

	Device->CreateTexture2D(&Texture2DDesc, nullptr, &NormalBuffer);

	D3D11_RENDER_TARGET_VIEW_DESC RenderTargetViewDesc = {};
	RenderTargetViewDesc.Format = Texture2DDesc.Format;
	RenderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RenderTargetViewDesc.Texture2D.MipSlice = 0;
	
	Device->CreateRenderTargetView(NormalBuffer, &RenderTargetViewDesc, &NormalBufferRTV);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = Texture2DDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	Device->CreateShaderResourceView(NormalBuffer, &srvDesc, &NormalBufferSRV);
}

void UDeviceResources::ReleaseNormalBuffer()
{
	SafeRelease(NormalBufferSRV);
	SafeRelease(NormalBufferRTV);
	SafeRelease(NormalBuffer);
}


/**
 * @brief Scene Color Texture, SRV, RTV 생성 함수
 */
void UDeviceResources::CreateSceneColorTarget()
{
	ReleaseSceneColorTarget();

	if (!Device || Width == 0 || Height == 0)
	{
		return;
	}

	D3D11_TEXTURE2D_DESC SceneDesc = {};
	SceneDesc.Width = Width;
	SceneDesc.Height = Height;
	SceneDesc.MipLevels = 1;
	SceneDesc.ArraySize = 1;
	SceneDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	SceneDesc.SampleDesc.Count = 1;
	SceneDesc.SampleDesc.Quality = 0;
	SceneDesc.Usage = D3D11_USAGE_DEFAULT;
	SceneDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	SceneDesc.CPUAccessFlags = 0;
	SceneDesc.MiscFlags = 0;


	HRESULT Result = Device->CreateTexture2D(&SceneDesc, nullptr, &SceneColorTexture);
	if (FAILED(Result))
	{
		UE_LOG_ERROR("DeviceResources: SceneColor Texture 생성 실패");
		ReleaseSceneColorTarget();
		return;
	}

	Result = Device->CreateRenderTargetView(SceneColorTexture, nullptr, &SceneColorTextureRTV);
	if (FAILED(Result))
	{
		UE_LOG_ERROR("DeviceResources: SceneColor RTV 생성 실패");
		ReleaseSceneColorTarget();
		return;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = SceneDesc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;

	Result = Device->CreateShaderResourceView(SceneColorTexture, &SRVDesc, &SceneColorTextureSRV);
	if (FAILED(Result))
	{
		UE_LOG_ERROR("DeviceResources: SceneColor SRV 생성 실패");
		ReleaseSceneColorTarget();
	}
}

/**
 * @brief Scene Color Texture, SRV, RTV를 해제하는 함수
 */
void UDeviceResources::ReleaseSceneColorTarget()
{
	SafeRelease(SceneColorTextureSRV);
	SafeRelease(SceneColorTextureRTV);
	SafeRelease(SceneColorTexture);
}

void UDeviceResources::CreateHitProxyTarget()
{
	ReleaseHitProxyTarget();

	if (!Device || Width == 0 || Height == 0)
	{
		return;
	}

	D3D11_TEXTURE2D_DESC HitProxyDesc = {};
	HitProxyDesc.Width = Width;
	HitProxyDesc.Height = Height;
	HitProxyDesc.MipLevels = 1;
	HitProxyDesc.ArraySize = 1;
	HitProxyDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	HitProxyDesc.SampleDesc.Count = 1;
	HitProxyDesc.SampleDesc.Quality = 0;
	HitProxyDesc.Usage = D3D11_USAGE_DEFAULT;
	HitProxyDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	HitProxyDesc.CPUAccessFlags = 0;
	HitProxyDesc.MiscFlags = 0;

	HRESULT Result = Device->CreateTexture2D(&HitProxyDesc, nullptr, &HitProxyTexture);
	if (FAILED(Result))
	{
		UE_LOG_ERROR("DeviceResources: HitProxy Texture 생성 실패");
		ReleaseHitProxyTarget();
		return;
	}

	Result = Device->CreateRenderTargetView(HitProxyTexture, nullptr, &HitProxyTextureRTV);
	if (FAILED(Result))
	{
		UE_LOG_ERROR("DeviceResources: HitProxy RTV 생성 실패");
		ReleaseHitProxyTarget();
		return;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = HitProxyDesc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;

	Result = Device->CreateShaderResourceView(HitProxyTexture, &SRVDesc, &HitProxyTextureSRV);
	if (FAILED(Result))
	{
		UE_LOG_ERROR("DeviceResources: HitProxy SRV 생성 실패");
		ReleaseHitProxyTarget();
	}
}

void UDeviceResources::ReleaseHitProxyTarget()
{
	SafeRelease(HitProxyTextureSRV);
	SafeRelease(HitProxyTextureRTV);
	SafeRelease(HitProxyTexture);
}


void UDeviceResources::CreateDepthBuffer()
{
	D3D11_TEXTURE2D_DESC dsDesc = {};
	dsDesc.Width = Width;
	dsDesc.Height = Height;
	dsDesc.MipLevels = 1;
	dsDesc.ArraySize = 1;
	dsDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	dsDesc.SampleDesc.Count = 1;
	dsDesc.SampleDesc.Quality = 0;
	dsDesc.Usage = D3D11_USAGE_DEFAULT;
	dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	dsDesc.CPUAccessFlags = 0;
	dsDesc.MiscFlags = 0;

	Device->CreateTexture2D(&dsDesc, nullptr, &DepthBuffer);

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	Device->CreateDepthStencilView(DepthBuffer, &dsvDesc, &DepthStencilView);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	Device->CreateShaderResourceView(DepthBuffer, &srvDesc, &DepthBufferSRV);
	Device->CreateShaderResourceView(DepthBuffer, &srvDesc, &DepthStencilSRV);
}

void UDeviceResources::ReleaseDepthBuffer()
{
	SafeRelease(DepthStencilView);
	SafeRelease(DepthStencilSRV);
	SafeRelease(DepthBufferSRV);
	SafeRelease(DepthBuffer);
}



void UDeviceResources::UpdateViewport(float InMenuBarHeight)
{
	DXGI_SWAP_CHAIN_DESC SwapChainDescription = {};
	SwapChain->GetDesc(&SwapChainDescription);

	// 전체 화면 크기
	float FullWidth = static_cast<float>(SwapChainDescription.BufferDesc.Width);
	float FullHeight = static_cast<float>(SwapChainDescription.BufferDesc.Height);

	// 메뉴바 아래에 위치하도록 뷰포트 조정
	ViewportInfo = {
		0.f,
		0.f,
		FullWidth,
		FullHeight,
		0.0f,
		1.0f
	};

	Width = SwapChainDescription.BufferDesc.Width;
	Height = SwapChainDescription.BufferDesc.Height;
}

uint64 UDeviceResources::GetTotalRenderTargetMemory() const
{
	uint64 TotalBytes = 0;
	const uint64 PixelCount = static_cast<uint64>(Width) * Height;

	// FrameBuffer: BGRA8 SRGB (4 bytes per pixel)
	if (FrameBuffer)
	{
		TotalBytes += PixelCount * 4;
	}

	// NormalBuffer: RGBA16F (8 bytes per pixel)
	if (NormalBuffer)
	{
		TotalBytes += PixelCount * 8;
	}

	// DepthBuffer: D24_UNORM_S8_UINT (4 bytes per pixel)
	if (DepthBuffer)
	{
		TotalBytes += PixelCount * 4;
	}

	// SceneColorTexture: RGBA16F (8 bytes per pixel)
	if (SceneColorTexture)
	{
		TotalBytes += PixelCount * 8;
	}

	// HitProxyTexture: RGBA8 (4 bytes per pixel)
	if (HitProxyTexture)
	{
		TotalBytes += PixelCount * 4;
	}

	return TotalBytes;
}

void UDeviceResources::CreateFactories()
{
	// Direct2D 팩토리 생성
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &D2DFactory);

	// DirectWrite 팩토리 생성
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
	                    reinterpret_cast<IUnknown**>(&DWriteFactory));

	// D2D RenderTarget 생성 (FrameBuffer와 연동)
	if (D2DFactory && FrameBuffer)
	{
		IDXGISurface* DxgiSurface = nullptr;
		FrameBuffer->QueryInterface(__uuidof(IDXGISurface), reinterpret_cast<void**>(&DxgiSurface));

		if (DxgiSurface)
		{
			D2D1_RENDER_TARGET_PROPERTIES Props = D2D1::RenderTargetProperties(
				D2D1_RENDER_TARGET_TYPE_DEFAULT,
				D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
			);

			D2DFactory->CreateDxgiSurfaceRenderTarget(DxgiSurface, &Props, &D2DRenderTarget);
			DxgiSurface->Release();
		}
	}
}

void UDeviceResources::ReleaseFactories()
{
	// CRITICAL: D2DRenderTarget은 FrameBuffer에 의존하므로 FrameBuffer 해제 전에 먼저 해제해야 함
	if (D2DRenderTarget)
	{
		D2DRenderTarget->Release();
		D2DRenderTarget = nullptr;
	}

	if (D2DFactory)
	{
		D2DFactory->Release();
		D2DFactory = nullptr;
	}

	if (DWriteFactory)
	{
		DWriteFactory->Release();
		DWriteFactory = nullptr;
	}
}
