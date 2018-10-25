// D3D "Slime".

//************************************************************
//************ INCLUDES & DEFINES ****************************
//************************************************************

#include <iostream>
#include <ctime>
#include "XTime.h"

using namespace std;
#include <d3d11.h> 
#include <DirectXMath.h>
#pragma comment (lib, "d3d11.lib")
using namespace DirectX;
#include <directxcolors.h>

#include "Trivial_PS.csh"
#include "Trivial_VS.csh"
#include "Trivial_VSSlimey.csh"
#include <cstdlib>.
#include <conio.h>
#include <stdlib.h> 
#include "DDSTextureLoader.h"

#define BACKBUFFER_WIDTH	1280.0f
#define BACKBUFFER_HEIGHT	768.0f

#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }
#define SAFE_DELETE(a) if( (a) != NULL ) delete (a); (a) = NULL;

//************************************************************
//************ SIMPLE WINDOWS APP CLASS **********************
//************************************************************

class DEMO_APP
{	
	HINSTANCE						application;
	WNDPROC							appWndProc;
	HWND							window;

	ID3D11Device*					device;
	ID3D11RenderTargetView*			renderTargetView;
	ID3D11DeviceContext*			inmediateContext;
	IDXGISwapChain*					swapchain;
	D3D11_VIEWPORT*					viewport;

	ID3D11VertexShader*				vertexShader;
	ID3D11PixelShader*				pixelShader;

	ID3D11ShaderResourceView*		shaderResourceView[5];
	ID3D11Buffer*					vertexbuffer;
	ID3D11Buffer*					indexbuffer;
	ID3D11InputLayout*				vertexLayout;

	ID3D11Texture2D*				zBuffer;
	ID3D11DepthStencilState *		stencilState;
	ID3D11DepthStencilView*			stencilView;
	ID3D11RasterizerState*			rasterState;
	ID3D11SamplerState*				samplerState;
	ID3D11Buffer*					WorldconstantBuffer;
	ID3D11Buffer*					SceneconstantBuffer;
	ID3D11Buffer*					TimeconstantBuffer;

	ID3D11VertexShader*				SlimeShader;

	ID3D11Buffer*					HUDvertexbuffer;
	ID3D11Buffer*					HUDindexbuffer;

	ID3D11Buffer*					Mossvertexbuffer;
	ID3D11Buffer*					Mossindexbuffer;
	ID3D11Buffer*					Slimevertexbuffer;

	XTime							time;

	struct SEND_TO_WORLD
	{
		XMMATRIX World;
	};

	struct SEND_TO_SCENE
	{
		XMMATRIX ViewM;
		XMMATRIX ProjectM;
	};

	struct SEND_TO_SLIME
	{
		XMFLOAT4 myTime;
	};

	SEND_TO_WORLD					WtoShader[8];
	SEND_TO_SCENE					StoShader;
	SEND_TO_SLIME					TtoShader;

public:
	struct SimpleVertex
	{
		XMFLOAT2 UV;
		XMFLOAT3 Pos;
		XMFLOAT4 Col;
	};
	
	DEMO_APP(HINSTANCE hinst, WNDPROC proc);
	bool Run();
	bool ShutDown();
};

//************************************************************
//************ CREATION OF OBJECTS & RESOURCES ***************
//************************************************************

DEMO_APP::DEMO_APP(HINSTANCE hinst, WNDPROC proc)
{
	application = hinst; 
	appWndProc = proc; 

	WNDCLASSEX  wndClass;
    ZeroMemory( &wndClass, sizeof( wndClass ) );
    wndClass.cbSize         = sizeof( WNDCLASSEX );             
    wndClass.lpfnWndProc    = appWndProc;						
    wndClass.lpszClassName  = L"DirectXApplication";            
	wndClass.hInstance      = application;		               
    wndClass.hCursor        = LoadCursor( NULL, IDC_ARROW );    
    wndClass.hbrBackground  = ( HBRUSH )( COLOR_WINDOWFRAME ); 
    RegisterClassEx( &wndClass );

	RECT window_size = { 0, 0, BACKBUFFER_WIDTH, BACKBUFFER_HEIGHT };
	AdjustWindowRect(&window_size, WS_OVERLAPPEDWINDOW, false);

	window = CreateWindow(	L"DirectXApplication", L"Slime Dungeon",	WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME|WS_MAXIMIZEBOX), 
							CW_USEDEFAULT, CW_USEDEFAULT, window_size.right-window_size.left, window_size.bottom-window_size.top,					
							NULL, NULL,	application, this );												

    ShowWindow( window, SW_SHOW );
		
	DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width =	BACKBUFFER_WIDTH;
    sd.BufferDesc.Height =	BACKBUFFER_HEIGHT;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = window;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

	HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG, 
		nullptr, 0, D3D11_SDK_VERSION, &sd, &swapchain, &device, nullptr, &inmediateContext); 

	 ID3D11Texture2D* backBuffer = nullptr;
    hr = swapchain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &backBuffer ) );
	
    hr = device->CreateRenderTargetView( backBuffer, nullptr, &renderTargetView );
    backBuffer->Release();

	viewport = new D3D11_VIEWPORT;
    viewport->Width =	(FLOAT)sd.BufferDesc.Width;	//swapchain->GetDesc(&sd);
    viewport->Height =	(FLOAT)sd.BufferDesc.Height;	//swapchain->GetDesc(&sd);
    viewport->MinDepth = 0.0f;
	viewport->MaxDepth = 1.0f;
    viewport->TopLeftX = 0;
    viewport->TopLeftY = 0;

	hr = device->CreateVertexShader(Trivial_VS, sizeof(Trivial_VS), nullptr, &vertexShader);
	hr = device->CreatePixelShader(Trivial_PS, sizeof(Trivial_PS), nullptr, &pixelShader);
	hr = device->CreateVertexShader(Trivial_VSSlimey, sizeof(Trivial_VSSlimey), nullptr, &SlimeShader);

	// Z BUFFER
	D3D11_TEXTURE2D_DESC dbDesc;

	ZeroMemory(&dbDesc, sizeof(dbDesc));

	dbDesc.Width = BACKBUFFER_WIDTH;
	dbDesc.Height = BACKBUFFER_HEIGHT;
	dbDesc.MipLevels = 1;
	dbDesc.ArraySize = 1;
	dbDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dbDesc.SampleDesc.Count = 1;
	dbDesc.SampleDesc.Quality = 0;
	dbDesc.Usage = D3D11_USAGE_DEFAULT;
	dbDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	dbDesc.CPUAccessFlags = 0;
	dbDesc.MiscFlags = 0;

	hr = device->CreateTexture2D(&dbDesc, NULL, &zBuffer);

	//STELCIL
	D3D11_DEPTH_STENCIL_DESC dsDesc;
	ZeroMemory(&dsDesc, sizeof(dsDesc));
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

	dsDesc.StencilEnable = false;
	dsDesc.StencilReadMask = 0xFF;
	dsDesc.StencilWriteMask = 0xFF;

	dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	hr = device->CreateDepthStencilState(&dsDesc, &stencilState);

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;

	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	device->CreateDepthStencilView(zBuffer, NULL, &stencilView);

	hr = CreateDDSTextureFromFile(device, L"Assets/Textures/DungeonFloor.dds", nullptr, &shaderResourceView[0]);
	hr = CreateDDSTextureFromFile(device, L"Assets/Textures/DungeonWall.dds", nullptr, &shaderResourceView[1]);
	hr = CreateDDSTextureFromFile(device, L"Assets/Textures/DungeonHud.dds", nullptr, &shaderResourceView[2]);
	hr = CreateDDSTextureFromFile(device, L"Assets/Textures/DungeonMoss.dds", nullptr, &shaderResourceView[3]);
	hr = CreateDDSTextureFromFile(device, L"Assets/Textures/DungeonSlime.dds", nullptr, &shaderResourceView[4]);

	SimpleVertex myPlane[4];

	myPlane[0].Pos = XMFLOAT3(-1.5f, 0.0f, 3.0f);
	myPlane[1].Pos = XMFLOAT3(1.5f, 0.0f, 3.0f);
	myPlane[2].Pos = XMFLOAT3(1.5f, 0.0f, 0.0f);
	myPlane[3].Pos = XMFLOAT3(-1.5f, 0.0f, 0.0f);

	myPlane[0].UV = XMFLOAT2(0.0f, 0.0f);
	myPlane[1].UV = XMFLOAT2(2.0f, 0.0f);
	myPlane[2].UV = XMFLOAT2(2.0f, 2.0f);
	myPlane[3].UV = XMFLOAT2(0.0f, 2.0f);

	myPlane[0].Col = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	myPlane[1].Col = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	myPlane[2].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	myPlane[3].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	//To VRAM
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = NULL;
	bd.ByteWidth = sizeof(SimpleVertex) * 4;
	bd.MiscFlags = 0; //unused
	bd.StructureByteStride = sizeof(SimpleVertex);

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = myPlane;

	hr = device->CreateBuffer(&bd, &InitData, &vertexbuffer);

	unsigned short myIndex[6];

	myIndex[0] = 0;
	myIndex[1] = 1;
	myIndex[2] = 2;
	myIndex[3] = 3;
	myIndex[4] = 2;
	myIndex[5] = 0;

	D3D11_BUFFER_DESC Ibd;
	ZeroMemory(&Ibd, sizeof(Ibd));
	Ibd.Usage = D3D11_USAGE_DEFAULT;
	Ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Ibd.CPUAccessFlags = NULL;
	Ibd.ByteWidth = sizeof(unsigned short) * 6;
	Ibd.MiscFlags = 0; //unused
	Ibd.StructureByteStride = sizeof(unsigned short);

	D3D11_SUBRESOURCE_DATA indexInitData;
	ZeroMemory(&indexInitData, sizeof(indexInitData));
	indexInitData.pSysMem = &myIndex;
	indexInitData.SysMemPitch = 0;
	indexInitData.SysMemSlicePitch = 0;

	hr = device->CreateBuffer(&Ibd, &indexInitData, &indexbuffer);

	//HUD BUFFER
	SimpleVertex myHUD[4];
	//floor
	myHUD[0].Pos = XMFLOAT3(-1.49f, 2.39f, 0.0f);
	myHUD[1].Pos = XMFLOAT3(1.5f, 2.39f, 0.0f);
	myHUD[2].Pos = XMFLOAT3(1.5f, 0.6f, 0.0f);
	myHUD[3].Pos = XMFLOAT3(-1.49f, 0.6f, 0.0f);

	myHUD[0].UV = XMFLOAT2(0.0f, 0.0f);
	myHUD[1].UV = XMFLOAT2(1.0f, 0.0f);
	myHUD[2].UV = XMFLOAT2(1.0f, 1.0f);
	myHUD[3].UV = XMFLOAT2(0.0f, 1.0f);

	myHUD[0].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	myHUD[1].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	myHUD[2].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	myHUD[3].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	//To VRAM
	D3D11_BUFFER_DESC hbd;
	ZeroMemory(&hbd, sizeof(hbd));
	hbd.Usage = D3D11_USAGE_IMMUTABLE;
	hbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	hbd.CPUAccessFlags = NULL;
	hbd.ByteWidth = sizeof(SimpleVertex) * 4;
	hbd.MiscFlags = 0; //unused
	hbd.StructureByteStride = sizeof(SimpleVertex);

	D3D11_SUBRESOURCE_DATA HudInitData;
	ZeroMemory(&HudInitData, sizeof(HudInitData));
	HudInitData.pSysMem = myHUD;

	hr = device->CreateBuffer(&hbd, &HudInitData, &HUDvertexbuffer);

	unsigned short myHudIndex[6];

	myHudIndex[0] = 0;
	myHudIndex[1] = 1;
	myHudIndex[2] = 2;
	myHudIndex[3] = 3;
	myHudIndex[4] = 2;
	myHudIndex[5] = 0;

	ZeroMemory(&Ibd, sizeof(Ibd));
	Ibd.Usage = D3D11_USAGE_DEFAULT;
	Ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Ibd.CPUAccessFlags = NULL;
	Ibd.ByteWidth = sizeof(unsigned short) * 6;
	Ibd.MiscFlags = 0; //unused
	Ibd.StructureByteStride = sizeof(unsigned short);

	ZeroMemory(&indexInitData, sizeof(indexInitData));
	indexInitData.pSysMem = &myHudIndex;
	indexInitData.SysMemPitch = 0;
	indexInitData.SysMemSlicePitch = 0;

	hr = device->CreateBuffer(&Ibd, &indexInitData, &HUDindexbuffer);

	//Moss BUFFER
	SimpleVertex mymoss[4];
	//floor
	//from -0.3x -1.75y 0z to 0.3x 0y 0z
	mymoss[0].Pos = XMFLOAT3(-0.4f, -1.75f, 0.0f);
	mymoss[1].Pos = XMFLOAT3(-0.4f, 0.0f, 0.0f);
	mymoss[2].Pos = XMFLOAT3(0.4f, 0.0f, 0.0f);
	mymoss[3].Pos = XMFLOAT3(0.4f, -1.75f, 0.0f);

	mymoss[0].UV = XMFLOAT2(0.0f, 1.0f);
	mymoss[1].UV = XMFLOAT2(0.0f, 0.0f);
	mymoss[2].UV = XMFLOAT2(1.0f, 0.0f);
	mymoss[3].UV = XMFLOAT2(1.0f, 1.0f);

	mymoss[0].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mymoss[1].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mymoss[2].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mymoss[3].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	//To VRAM
	ZeroMemory(&hbd, sizeof(hbd));
	hbd.Usage = D3D11_USAGE_IMMUTABLE;
	hbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	hbd.CPUAccessFlags = NULL;
	hbd.ByteWidth = sizeof(SimpleVertex) * 4;
	hbd.MiscFlags = 0; //unused
	hbd.StructureByteStride = sizeof(SimpleVertex);

	ZeroMemory(&HudInitData, sizeof(HudInitData));
	HudInitData.pSysMem = mymoss;

	hr = device->CreateBuffer(&hbd, &HudInitData, &Mossvertexbuffer);

	unsigned short myMossIndex[6];

	myMossIndex[0] = 0;
	myMossIndex[1] = 1;
	myMossIndex[2] = 2;
	myMossIndex[3] = 2;
	myMossIndex[4] = 0;
	myMossIndex[5] = 3;

	ZeroMemory(&Ibd, sizeof(Ibd));
	Ibd.Usage = D3D11_USAGE_DEFAULT;
	Ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Ibd.CPUAccessFlags = NULL;
	Ibd.ByteWidth = sizeof(unsigned short) * 6;
	Ibd.MiscFlags = 0; //unused
	Ibd.StructureByteStride = sizeof(unsigned short);

	ZeroMemory(&indexInitData, sizeof(indexInitData));
	indexInitData.pSysMem = &myMossIndex;
	indexInitData.SysMemPitch = 0;
	indexInitData.SysMemSlicePitch = 0;

	hr = device->CreateBuffer(&Ibd, &indexInitData, &Mossindexbuffer);

	//Slime
	SimpleVertex myslime[3760];

	XMFLOAT2 initial = XMFLOAT2(-1.0f, 2.0f);
	unsigned int index = 0;

	for (unsigned int rows = 0; rows < 25 ; rows++)
	{
		initial.x = -1.0f;

		for (unsigned int columns = 0; columns < 25; columns++)
		{
			myslime[index].Pos		= XMFLOAT3(initial.x, initial.y, 0.0f);
			myslime[index + 1].Pos	= XMFLOAT3(initial.x + 0.08f, initial.y, 0.0f);
			myslime[index + 2].Pos	= XMFLOAT3(initial.x + 0.08f, initial.y - 0.08f, 0.0f);

			myslime[index + 3].Pos = XMFLOAT3(initial.x, initial.y, 0.0f);
			myslime[index + 4].Pos	= XMFLOAT3(initial.x + 0.08f, initial.y - 0.08f, 0.0f);
			myslime[index + 5].Pos = XMFLOAT3(initial.x, initial.y - 0.08f, 0.0f);

			myslime[index].UV		= XMFLOAT2(columns*0.04f, rows*0.04f);
			myslime[index + 1].UV	= XMFLOAT2((columns+1)*0.04f, rows*0.04f);
			myslime[index + 2].UV = XMFLOAT2((columns + 1)*0.04f, (rows+1)*0.04f);

			myslime[index + 3].UV	= XMFLOAT2(columns*0.04f, rows*0.04f);
			myslime[index + 4].UV = XMFLOAT2((columns + 1)*0.04f, (rows + 1)*0.04f);
			myslime[index + 5].UV = XMFLOAT2(columns*0.04f, (rows + 1)*0.04f);

			myslime[index].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			myslime[index+1].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			myslime[index+2].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			myslime[index+3].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			myslime[index + 4].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			myslime[index + 5].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

			myslime[index + 3].Col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
					
			index += 6;
			initial.x += 0.08f;
		}
		initial.y -= 0.08f;
	}

	//To VRAM
	ZeroMemory(&hbd, sizeof(hbd));
	hbd.Usage = D3D11_USAGE_IMMUTABLE;
	hbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	hbd.CPUAccessFlags = NULL;
	hbd.ByteWidth = sizeof(SimpleVertex) * 3760;
	hbd.MiscFlags = 0; //unused
	hbd.StructureByteStride = sizeof(SimpleVertex);

	ZeroMemory(&HudInitData, sizeof(HudInitData));
	HudInitData.pSysMem = myslime;

	hr = device->CreateBuffer(&hbd, &HudInitData, &Slimevertexbuffer);

	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbd.ByteWidth = sizeof(SEND_TO_WORLD);

	hr = device->CreateBuffer(&cbd, nullptr, &WorldconstantBuffer);

	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbd.ByteWidth = sizeof(SEND_TO_SCENE);

	hr = device->CreateBuffer(&cbd, nullptr, &SceneconstantBuffer);

	D3D11_BUFFER_DESC sbd;
	ZeroMemory(&sbd, sizeof(sbd));
	sbd.Usage = D3D11_USAGE_DYNAMIC;
	sbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	sbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	sbd.ByteWidth = sizeof(SEND_TO_SLIME);

	hr = device->CreateBuffer(&sbd, nullptr, &TimeconstantBuffer);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		D3D11_INPUT_PER_VERTEX_DATA, 0 }

	};
	UINT numElements = ARRAYSIZE(layout);
	hr = device->CreateInputLayout(layout, numElements, Trivial_VS, sizeof(Trivial_VS), &vertexLayout);
	
	for (int i = 0; i < 7; i++)
		WtoShader[i].World = XMMatrixIdentity();
	
	StoShader.ProjectM = XMMatrixPerspectiveFovLH(1.13446401f, BACKBUFFER_WIDTH / BACKBUFFER_HEIGHT, 0.1f, 100.0f);
	StoShader.ViewM = XMMatrixIdentity();
	
	XMFLOAT4 vec = XMFLOAT4(0.0f, 1.5f, -1.4f, 0.0f);
	XMVECTOR aux = XMLoadFloat4(&vec);
	XMFLOAT4 vec2 = XMFLOAT4(0.0f, 1.5f, 0.0f, 0.0f);
	XMVECTOR aux2 = XMLoadFloat4(&vec2);
	XMFLOAT4 vec3 = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR aux3 = XMLoadFloat4(&vec3);

	StoShader.ViewM *= XMMatrixLookAtLH(aux, aux2, aux3);
	//set view

	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	hr = device->CreateRasterizerState(&rasterDesc, &rasterState);
	inmediateContext->RSSetState(rasterState);

	D3D11_SAMPLER_DESC sampd;
	ZeroMemory(&sampd, sizeof(sampd));
	sampd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

	sampd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampd.MaxLOD = D3D11_FLOAT32_MAX;
	sampd.MinLOD = 0;

	hr = device->CreateSamplerState(&sampd, &samplerState);

}

//************************************************************
//************ EXECUTION *************************************
//************************************************************

bool DEMO_APP::Run()
{

	time.Signal();
	double dt = time.TotalTime();

	TtoShader.myTime.x = (float)dt;

	inmediateContext->OMSetRenderTargets(1, &renderTargetView, nullptr);
	inmediateContext->RSSetViewports(1, viewport);
	inmediateContext->OMSetDepthStencilState(stencilState, 1);
	inmediateContext->ClearRenderTargetView(renderTargetView, Colors::Black);

	inmediateContext->PSSetShader(pixelShader, nullptr, 0);
	inmediateContext->VSSetShader(vertexShader, nullptr, 0);

	inmediateContext->VSSetConstantBuffers(0, 1, &WorldconstantBuffer);
	inmediateContext->VSSetConstantBuffers(1, 1, &SceneconstantBuffer);
	inmediateContext->VSSetConstantBuffers(2, 1, &TimeconstantBuffer);

	inmediateContext->PSSetSamplers(0, 1, &samplerState);
	
	for (int i = 0; i < 4; i++)
	{
		inmediateContext->ClearDepthStencilView(stencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0xFF);


		D3D11_MAPPED_SUBRESOURCE  Resource;

		inmediateContext->Map(WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
		memcpy(Resource.pData, &WtoShader[i], sizeof(SEND_TO_WORLD));
		inmediateContext->Unmap(WorldconstantBuffer, 0);

		inmediateContext->Map(SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
		memcpy(Resource.pData, &StoShader, sizeof(SEND_TO_SCENE));
		inmediateContext->Unmap(SceneconstantBuffer, 0);

		UINT sstride = sizeof(SimpleVertex);
		UINT soffset = 0;
		inmediateContext->IASetVertexBuffers(0, 1, &vertexbuffer, &sstride, &soffset);
		inmediateContext->IASetIndexBuffer(indexbuffer, DXGI_FORMAT_R16_UINT, 0);

		inmediateContext->IASetInputLayout(vertexLayout);
		inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		if (i == 0)
			inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[0]);
		else if (i == 1)
		{
			//left
			XMFLOAT4 vec = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
			XMVECTOR aux = XMLoadFloat4(&vec);
			XMMATRIX rotation = XMMatrixIdentity();
			rotation *= XMMatrixRotationAxis(aux, -1.57079633f);

			XMMATRIX translation = XMMatrixIdentity();
			translation *= XMMatrixTranslation(-1.5f, 1.5f, 0.0f);
			WtoShader[i].World = rotation * translation;

			inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[1]);

		}
		else if (i == 2)
		{
			//right
			XMFLOAT4 vec = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
			XMVECTOR aux = XMLoadFloat4(&vec);
			XMMATRIX rotation = XMMatrixIdentity();
			rotation *= XMMatrixRotationAxis(aux, 1.57079633f);

			XMMATRIX translation = XMMatrixIdentity();
			translation *= XMMatrixTranslation(1.5f, 1.5f, 0.0f);
			WtoShader[i].World = rotation * translation;

			inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[1]);

		}
		else if (i == 3)
		{
			//top
			XMFLOAT4 vec = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
			XMVECTOR aux = XMLoadFloat4(&vec);
			XMMATRIX rotation = XMMatrixIdentity();
			rotation *= XMMatrixRotationAxis(aux, 3.14159265f);

			XMMATRIX translation = XMMatrixIdentity();
			translation *= XMMatrixTranslation(0.0f, 3.0f, 0.0f);
			WtoShader[i].World = rotation * translation;

			inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[1]);
		}
		inmediateContext->DrawIndexed(6, 0, 0);
	}

	//HUD
	inmediateContext->ClearDepthStencilView(stencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0xFF);


	D3D11_MAPPED_SUBRESOURCE  Resource;

	inmediateContext->Map(WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &WtoShader[4], sizeof(SEND_TO_WORLD));
	inmediateContext->Unmap(WorldconstantBuffer, 0);

	inmediateContext->Map(SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &StoShader, sizeof(SEND_TO_SCENE));
	inmediateContext->Unmap(SceneconstantBuffer, 0);

	UINT sstride = sizeof(SimpleVertex);
	UINT soffset = 0;
	inmediateContext->IASetVertexBuffers(0, 1, &HUDvertexbuffer, &sstride, &soffset);
	inmediateContext->IASetIndexBuffer(HUDindexbuffer, DXGI_FORMAT_R16_UINT, 0);

	inmediateContext->IASetInputLayout(vertexLayout);
	inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[2]);
	
	inmediateContext->DrawIndexed(6, 0, 0);

	//Moss
	//0.75x 3y 1z rotation 25 degrees in the Y
	XMFLOAT4 vec = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR aux = XMLoadFloat4(&vec);
	XMMATRIX rotation = XMMatrixIdentity();
	rotation *= XMMatrixRotationAxis(aux, 0.436332313f);

	XMMATRIX translation = XMMatrixIdentity();
	translation *= XMMatrixTranslation(0.75f, 3.0f, 1.0f);
	WtoShader[5].World = rotation * translation;

	inmediateContext->ClearDepthStencilView(stencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0xFF);

	inmediateContext->Map(WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &WtoShader[5], sizeof(SEND_TO_WORLD));
	inmediateContext->Unmap(WorldconstantBuffer, 0);

	inmediateContext->Map(SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &StoShader, sizeof(SEND_TO_SCENE));
	inmediateContext->Unmap(SceneconstantBuffer, 0);

	inmediateContext->IASetVertexBuffers(0, 1, &Mossvertexbuffer, &sstride, &soffset);
	inmediateContext->IASetIndexBuffer(Mossindexbuffer, DXGI_FORMAT_R16_UINT, 0);

	inmediateContext->IASetInputLayout(vertexLayout);
	inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[3]);

	inmediateContext->DrawIndexed(6, 0, 0);

	//slime
	inmediateContext->VSSetShader(SlimeShader, nullptr, 0);

	translation = XMMatrixIdentity();
	translation *= XMMatrixTranslation(0.0f, 0.0f, 2.0f);
	WtoShader[7].World = translation;

	inmediateContext->ClearDepthStencilView(stencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0xFF);

	inmediateContext->Map(WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &WtoShader[7], sizeof(SEND_TO_WORLD));
	inmediateContext->Unmap(WorldconstantBuffer, 0);

	inmediateContext->Map(SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &StoShader, sizeof(SEND_TO_SCENE));
	inmediateContext->Unmap(SceneconstantBuffer, 0);

	inmediateContext->Map(TimeconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &TtoShader, sizeof(SEND_TO_SLIME));
	inmediateContext->Unmap(TimeconstantBuffer, 0);

	inmediateContext->IASetVertexBuffers(0, 1, &Slimevertexbuffer, &sstride, &soffset);
	inmediateContext->IASetInputLayout(vertexLayout);
	inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[4]);

	inmediateContext->Draw(3760, 0);

	//moss 2
	inmediateContext->VSSetShader(vertexShader, nullptr, 0);
	//-1x 3y 2z rotation -20 degrees in the Y
	vec = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	aux = XMLoadFloat4(&vec);
	rotation = XMMatrixIdentity();
	rotation *= XMMatrixRotationAxis(aux, -0.34906585f);

	translation = XMMatrixIdentity();
	translation *= XMMatrixTranslation(-1.0f, 3.0f, 2.0f);
	WtoShader[6].World = rotation * translation ;
	//WtoShader[6].World *= rotation;

	inmediateContext->ClearDepthStencilView(stencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0xFF);

	inmediateContext->Map(WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &WtoShader[6], sizeof(SEND_TO_WORLD));
	inmediateContext->Unmap(WorldconstantBuffer, 0);

	inmediateContext->Map(SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &StoShader, sizeof(SEND_TO_SCENE));
	inmediateContext->Unmap(SceneconstantBuffer, 0);

	inmediateContext->IASetVertexBuffers(0, 1, &Mossvertexbuffer, &sstride, &soffset);
	inmediateContext->IASetIndexBuffer(Mossindexbuffer, DXGI_FORMAT_R16_UINT, 0);

	inmediateContext->IASetInputLayout(vertexLayout);
	inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[3]);

	inmediateContext->DrawIndexed(6, 0, 0);

	
	
	swapchain->Present(0, 0);

	return true; 
}

//************************************************************
//************ DESTRUCTION ***********************************
//************************************************************

bool DEMO_APP::ShutDown()
{
	SAFE_RELEASE(renderTargetView);
	SAFE_DELETE(renderTargetView);

	SAFE_RELEASE(swapchain);
	SAFE_DELETE(swapchain);

	SAFE_RELEASE(device);
	SAFE_DELETE(device);

	SAFE_RELEASE(inmediateContext);
	SAFE_DELETE(inmediateContext);

	SAFE_DELETE(viewport);

	SAFE_RELEASE(vertexShader);
	SAFE_DELETE(vertexShader);

	SAFE_RELEASE(pixelShader);
	SAFE_DELETE(pixelShader);

	SAFE_RELEASE(zBuffer);
	SAFE_DELETE(zBuffer);

	SAFE_RELEASE(stencilState);
	SAFE_DELETE(stencilState);

	SAFE_RELEASE(stencilView);
	SAFE_DELETE(stencilView);
	
	SAFE_RELEASE(rasterState);
	SAFE_DELETE(rasterState);
	
	for (int i = 0; i < 5; i++)
	{
		SAFE_RELEASE(shaderResourceView[i]);
		SAFE_DELETE(shaderResourceView[i]);
	}
	
	SAFE_RELEASE(WorldconstantBuffer);
	SAFE_DELETE(WorldconstantBuffer);

	SAFE_RELEASE(SceneconstantBuffer);
	SAFE_DELETE(SceneconstantBuffer);

	SAFE_RELEASE(samplerState);
	SAFE_DELETE(samplerState);

	SAFE_RELEASE(vertexbuffer);
	SAFE_DELETE(vertexbuffer);

	SAFE_RELEASE(indexbuffer);
	SAFE_DELETE(indexbuffer);

	SAFE_RELEASE(vertexLayout);
	SAFE_DELETE(vertexLayout);

	SAFE_RELEASE(HUDvertexbuffer);
	SAFE_DELETE(HUDvertexbuffer);

	SAFE_RELEASE(HUDindexbuffer);
	SAFE_DELETE(HUDindexbuffer);

	SAFE_RELEASE(Mossvertexbuffer);
	SAFE_DELETE(Mossvertexbuffer);

	SAFE_RELEASE(Mossindexbuffer);
	SAFE_DELETE(Mossindexbuffer);

	SAFE_RELEASE(Slimevertexbuffer);
	SAFE_DELETE(Slimevertexbuffer);

	SAFE_RELEASE(TimeconstantBuffer);
	SAFE_DELETE(TimeconstantBuffer);

	SAFE_RELEASE(SlimeShader);
	SAFE_DELETE(SlimeShader);

	UnregisterClass(L"DirectXApplication", application); 
	return true;
}

//************************************************************
//************ WINDOWS RELATED *******************************
//************************************************************

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine,	int nCmdShow );						   
LRESULT CALLBACK WndProc(HWND hWnd,	UINT message, WPARAM wparam, LPARAM lparam );		
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int)
{
	srand(unsigned int(time(0)));
	DEMO_APP myApp(hInstance,(WNDPROC)WndProc);	
    MSG msg; ZeroMemory( &msg, sizeof( msg ) );
    while ( msg.message != WM_QUIT && myApp.Run() )
    {	
	    if ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
        { 
            TranslateMessage( &msg );
            DispatchMessage( &msg ); 
        }
    }
	myApp.ShutDown(); 
	return 0; 
}
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    if(GetAsyncKeyState(VK_ESCAPE))
		message = WM_DESTROY;
    switch ( message )
    {
        case ( WM_DESTROY ): { PostQuitMessage( 0 ); }
        break;
    }
    return DefWindowProc( hWnd, message, wParam, lParam );
}
