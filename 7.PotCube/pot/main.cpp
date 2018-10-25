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
#include "teapot.h"
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
	D3D11_VIEWPORT*					RTTviewport;


	ID3D11VertexShader*				vertexShader;
	ID3D11PixelShader*				pixelShader;

	ID3D11ShaderResourceView*		shaderResourceView[2];
	ID3D11InputLayout*				vertexLayout;

	ID3D11Texture2D*				zBuffer;
	ID3D11DepthStencilState *		stencilState;
	ID3D11DepthStencilView*			stencilView;
	ID3D11RasterizerState*			rasterState;
	ID3D11SamplerState*				samplerState;
	ID3D11Buffer*					WorldconstantBuffer;
	ID3D11Buffer*					SceneconstantBuffer;

	ID3D11Buffer*					PotVertexbuffer;
	ID3D11Buffer*					PotIndexbuffer;
	ID3D11Buffer*					PLightconstantBuffer;
	ID3D11Buffer*					PSpotLightconstantBuffer;
	ID3D11Buffer*					PSTypeconstantBuffer;

	ID3D11Texture2D*				RTTTextureMap;
	ID3D11ShaderResourceView*		shaderResourceViewMap;
	ID3D11DepthStencilView*			RTTstencilView;
	ID3D11RenderTargetView*			RTTrenderTargetView;
	ID3D11Texture2D*				RTTzBuffer;

	ID3D11Buffer*					cubeVertexbuffer;
	ID3D11Buffer*					cubeIndexbuffer;

	XTime							time;
	XMMATRIX						rotation;
	double							dt;

	struct SEND_TO_WORLD
	{
		XMMATRIX World;
	};

	struct SEND_TO_SCENE
	{
		XMMATRIX ViewM;
		XMMATRIX ProjectM;
	};

	struct SEND_POINT_LIGHT
	{
		XMFLOAT3 pos;
		float padding;
		XMFLOAT3 dir;
		float rPoint;
		XMFLOAT4 col;
	};

	struct SEND_SPOT_LIGHT
	{
		XMFLOAT3 pos;
		float padding;
		XMFLOAT3 dir;
		float sPoint;
		XMFLOAT4 col;
		float inner;
		float outer;
		XMFLOAT2 morepadding;
	};
	struct SEND_TYPE
	{
		bool	mytype;
		bool	pad1;
		bool	pad2;
		bool	padd;
		XMFLOAT3 pad3;
	};

	SEND_TO_WORLD					WtoShader[3];
	SEND_TO_SCENE					StoShader[2];
	SEND_POINT_LIGHT				LtoPShader;
	SEND_SPOT_LIGHT					sLtoPShader;
	SEND_TYPE						TypetoPShader;

public:
	struct SimpleVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT3 UV;
		XMFLOAT3 norm;
	};
	
	DEMO_APP(HINSTANCE hinst, WNDPROC proc);
	bool Run();
	bool ShutDown();
	void Input();
};

//************************************************************
//************ CREATION OF OBJECTS & RESOURCES ***************
//************************************************************

DEMO_APP::DEMO_APP(HINSTANCE hinst, WNDPROC proc)
{
	application = hinst;
	appWndProc = proc;

	WNDCLASSEX  wndClass;
	ZeroMemory(&wndClass, sizeof(wndClass));
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.lpfnWndProc = appWndProc;
	wndClass.lpszClassName = L"DirectXApplication";
	wndClass.hInstance = application;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOWFRAME);
	RegisterClassEx(&wndClass);

	RECT window_size = { 0, 0, BACKBUFFER_WIDTH, BACKBUFFER_HEIGHT };
	AdjustWindowRect(&window_size, WS_OVERLAPPEDWINDOW, false);

	window = CreateWindow(L"DirectXApplication", L"Pot Cube", WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
		CW_USEDEFAULT, CW_USEDEFAULT, window_size.right - window_size.left, window_size.bottom - window_size.top,
		NULL, NULL, application, this);

	ShowWindow(window, SW_SHOW);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = BACKBUFFER_WIDTH;
	sd.BufferDesc.Height = BACKBUFFER_HEIGHT;
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
	hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));

	hr = device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);

	backBuffer->Release();

	viewport = new D3D11_VIEWPORT;
	viewport->Width = (FLOAT)sd.BufferDesc.Width;	//swapchain->GetDesc(&sd);
	viewport->Height = (FLOAT)sd.BufferDesc.Height;	//swapchain->GetDesc(&sd);
	viewport->MinDepth = 0.0f;
	viewport->MaxDepth = 1.0f;
	viewport->TopLeftX = 0;
	viewport->TopLeftY = 0;

	RTTviewport = new D3D11_VIEWPORT;
	RTTviewport->Width = (FLOAT)400;	//swapchain->GetDesc(&sd);
	RTTviewport->Height = (FLOAT)400;	//swapchain->GetDesc(&sd);
	RTTviewport->MinDepth = 0.0f;
	RTTviewport->MaxDepth = 1.0f;
	RTTviewport->TopLeftX = 0;
	RTTviewport->TopLeftY = 0;

	hr = device->CreateVertexShader(Trivial_VS, sizeof(Trivial_VS), nullptr, &vertexShader);
	hr = device->CreatePixelShader(Trivial_PS, sizeof(Trivial_PS), nullptr, &pixelShader);

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

	ZeroMemory(&dbDesc, sizeof(dbDesc));

	dbDesc.Width = 400;
	dbDesc.Height = 400;
	dbDesc.MipLevels = 1;
	dbDesc.ArraySize = 1;
	dbDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dbDesc.SampleDesc.Count = 1;
	dbDesc.SampleDesc.Quality = 0;
	dbDesc.Usage = D3D11_USAGE_DEFAULT;
	dbDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	dbDesc.CPUAccessFlags = 0;
	dbDesc.MiscFlags = 0;

	hr = device->CreateTexture2D(&dbDesc, NULL, &RTTzBuffer);

	//RTT z buffer
	ZeroMemory(&dbDesc, sizeof(dbDesc));

	dbDesc.Width = 400;
	dbDesc.Height = 400;
	dbDesc.MipLevels = 1;
	dbDesc.ArraySize = 1;
	dbDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	dbDesc.SampleDesc.Count = 1;
	dbDesc.SampleDesc.Quality = 0;
	dbDesc.Usage = D3D11_USAGE_DEFAULT;
	dbDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	dbDesc.CPUAccessFlags = 0;
	dbDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	hr = device->CreateTexture2D(&dbDesc, NULL, &RTTTextureMap);

	D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
	renderTargetViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	renderTargetViewDesc.Texture2D.MipSlice = 0;

	hr = device->CreateRenderTargetView(RTTTextureMap, &renderTargetViewDesc, &RTTrenderTargetView);

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

	hr =  device->CreateDepthStencilView(zBuffer, NULL, &stencilView);
	hr = device->CreateDepthStencilView(RTTzBuffer, NULL, &RTTstencilView);

	hr = CreateDDSTextureFromFile(device, L"Assets/Textures/checkerboard.dds", nullptr, &shaderResourceView[0]);
	hr = CreateDDSTextureFromFile(device, L"Assets/Textures/Default_reflection.dds", nullptr, &shaderResourceView[1]);

	D3D11_SHADER_RESOURCE_VIEW_DESC srDesc;
	srDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srDesc.Texture2D.MostDetailedMip = 0;
	srDesc.Texture2D.MipLevels = 1;

	hr = device->CreateShaderResourceView(RTTTextureMap, &srDesc, &shaderResourceViewMap);

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = NULL;
	bd.ByteWidth = sizeof(teapot_data);
	bd.MiscFlags = 0; //unused
	bd.StructureByteStride = sizeof(_OBJ_VERT_);

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = teapot_data;

	hr = device->CreateBuffer(&bd, &InitData, &PotVertexbuffer);

	//CUBE
	SimpleVertex myCube[24];

	//top
	myCube[0].Pos = XMFLOAT3(-0.3f, 0.3f, 0.3f);
	myCube[1].Pos = XMFLOAT3(0.3f, 0.3f, 0.3f);
	myCube[2].Pos = XMFLOAT3(0.3f, 0.3f, -0.3f);
	myCube[3].Pos = XMFLOAT3(-0.3f, 0.3f, -0.3f);

	myCube[0].UV = XMFLOAT3(0.0f, 0.0f, 0.0f);
	myCube[1].UV = XMFLOAT3(1.0f, 0.0f, 0.0f);
	myCube[2].UV = XMFLOAT3(1.0f, 1.0f, 0.0f);
	myCube[3].UV = XMFLOAT3(0.0f, 1.0f, 0.0f);

	myCube[0].norm = XMFLOAT3(0.0f, 1.0f, 0.0f);
	myCube[1].norm = XMFLOAT3(0.0f, 1.0f, 0.0f);
	myCube[2].norm = XMFLOAT3(0.0f, 1.0f, 0.0f);
	myCube[3].norm = XMFLOAT3(0.0f, 1.0f, 0.0f);

	//bottom
	myCube[4].Pos = XMFLOAT3(-0.3f, -0.3f, 0.3f);
	myCube[5].Pos = XMFLOAT3(0.3f, -0.3f, 0.3f);
	myCube[6].Pos = XMFLOAT3(0.3f, -0.3f, -0.3f);
	myCube[7].Pos = XMFLOAT3(-0.3f, -0.3f, -0.3f);

	myCube[4].UV = XMFLOAT3(0.0f, 1.0f, 0.0f);
	myCube[5].UV = XMFLOAT3(1.0f, 1.0f, 0.0f);
	myCube[6].UV = XMFLOAT3(1.0f, 0.0f, 0.0f);
	myCube[7].UV = XMFLOAT3(0.0f, 0.0f, 0.0f);

	myCube[4].norm = XMFLOAT3(0.0f, -1.0f, 0.0f);
	myCube[5].norm = XMFLOAT3(0.0f, -1.0f, 0.0f);
	myCube[6].norm = XMFLOAT3(0.0f, -1.0f, 0.0f);
	myCube[7].norm = XMFLOAT3(0.0f, -1.0f, 0.0f);

	//left
	myCube[8].Pos = XMFLOAT3(-0.3f, 0.3f, -0.3f);
	myCube[9].Pos = XMFLOAT3(-0.3f, 0.3f, 0.3f);
	myCube[10].Pos = XMFLOAT3(-0.3f, -0.3f, 0.3f);
	myCube[11].Pos = XMFLOAT3(-0.3f, -0.3f, -0.3f);

	myCube[8].UV = XMFLOAT3(1.0f, 0.0f, 0.0f);
	myCube[9].UV = XMFLOAT3(0.0f, 0.0f, 0.0f);
	myCube[10].UV = XMFLOAT3(0.0f, 1.0f, 0.0f);
	myCube[11].UV = XMFLOAT3(1.0f, 1.0f, 0.0f);

	myCube[8].norm = XMFLOAT3(-1.0f, 0.0f, 0.0f);
	myCube[9].norm = XMFLOAT3(-1.0f, 0.0f, 0.0f);
	myCube[10].norm = XMFLOAT3(-1.0f, 0.0f, 0.0f);
	myCube[11].norm = XMFLOAT3(-1.0f, 0.0f, 0.0f);

	//right
	myCube[12].Pos = XMFLOAT3(0.3f, 0.3f, -0.3f);
	myCube[13].Pos = XMFLOAT3(0.3f, 0.3f, 0.3f);
	myCube[14].Pos = XMFLOAT3(0.3f, -0.3f, 0.3f);
	myCube[15].Pos = XMFLOAT3(0.3f, -0.3f, -0.3f);

	myCube[12].UV = XMFLOAT3(0.0f, 0.0f, 0.0f);
	myCube[13].UV = XMFLOAT3(1.0f, 0.0f, 0.0f);
	myCube[14].UV = XMFLOAT3(1.0f, 1.0f, 0.0f);
	myCube[15].UV = XMFLOAT3(0.0f, 1.0f, 0.0f);

	myCube[12].norm = XMFLOAT3(1.0f, 0.0f, 0.0f);
	myCube[13].norm = XMFLOAT3(1.0f, 0.0f, 0.0f);
	myCube[14].norm = XMFLOAT3(1.0f, 0.0f, 0.0f);
	myCube[15].norm = XMFLOAT3(1.0f, 0.0f, 0.0f);

	//back
	myCube[16].Pos = XMFLOAT3(-0.3f, 0.3f, 0.3f);
	myCube[17].Pos = XMFLOAT3(0.3f, 0.3f, 0.3f);
	myCube[18].Pos = XMFLOAT3(0.3f, -0.3f, 0.3f);
	myCube[19].Pos = XMFLOAT3(-0.3f, -0.3f, 0.3f);

	myCube[16].UV = XMFLOAT3(1.0f, 0.0f, 0.0f);
	myCube[17].UV = XMFLOAT3(0.0f, 0.0f, 0.0f);
	myCube[18].UV = XMFLOAT3(0.0f, 1.0f, 0.0f);
	myCube[19].UV = XMFLOAT3(1.0f, 1.0f, 0.0f);

	myCube[16].norm = XMFLOAT3(0.0f, 0.0f, 1.0f);
	myCube[17].norm = XMFLOAT3(0.0f, 0.0f, 1.0f);
	myCube[18].norm = XMFLOAT3(0.0f, 0.0f, 1.0f);
	myCube[19].norm = XMFLOAT3(0.0f, 0.0f, 1.0f);

	//front
	myCube[20].Pos = XMFLOAT3(-0.3f, 0.3f, -0.3f);
	myCube[21].Pos = XMFLOAT3(0.3f, 0.3f, -0.3f);
	myCube[22].Pos = XMFLOAT3(0.3f, -0.3f, -0.3f);
	myCube[23].Pos = XMFLOAT3(-0.3f, -0.3f, -0.3f);

	myCube[20].UV = XMFLOAT3(0.0f, 0.0f, 0.0f);
	myCube[21].UV = XMFLOAT3(1.0f, 0.0f, 0.0f);
	myCube[22].UV = XMFLOAT3(1.0f, 1.0f, 0.0f);
	myCube[23].UV = XMFLOAT3(0.0f, 1.0f, 0.0f);
	
	myCube[20].norm = XMFLOAT3(0.0f, 0.0f, -1.0f);
	myCube[21].norm = XMFLOAT3(0.0f, 0.0f, -1.0f);
	myCube[22].norm = XMFLOAT3(0.0f, 0.0f, -1.0f);
	myCube[23].norm = XMFLOAT3(0.0f, 0.0f, -1.0f);

	//To VRAM
	D3D11_BUFFER_DESC cubd;
	ZeroMemory(&cubd, sizeof(cubd));
	cubd.Usage = D3D11_USAGE_IMMUTABLE;
	cubd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	cubd.CPUAccessFlags = NULL;
	cubd.ByteWidth = sizeof(SimpleVertex) * 24;
	cubd.MiscFlags = 0; //unused
	cubd.StructureByteStride = sizeof(SimpleVertex);

	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = myCube;

	hr = device->CreateBuffer(&cubd, &InitData, &cubeVertexbuffer);
	unsigned short myCubeIndex[36];

	//1t
	myCubeIndex[0] = 0;
	myCubeIndex[1] = 1;
	myCubeIndex[2] = 2;
	myCubeIndex[3] = 2;
	myCubeIndex[4] = 3;
	myCubeIndex[5] = 0;
	//2b
	myCubeIndex[6] = 4;
	myCubeIndex[7] = 6;
	myCubeIndex[8] = 5;
	myCubeIndex[9] = 6;
	myCubeIndex[10] = 4;
	myCubeIndex[11] = 7;
	//3l
	myCubeIndex[12] = 9;
	myCubeIndex[13] = 8;
	myCubeIndex[14] = 10;
	myCubeIndex[15] = 10;
	myCubeIndex[16] = 8;
	myCubeIndex[17] = 11;
	//4r
	myCubeIndex[18] = 12;
	myCubeIndex[19] = 13;
	myCubeIndex[20] = 14;
	myCubeIndex[21] = 14;
	myCubeIndex[22] = 15;
	myCubeIndex[23] = 12;
	//5		    
	myCubeIndex[24] = 17;
	myCubeIndex[25] = 16;
	myCubeIndex[26] = 18;
	myCubeIndex[27] = 18;
	myCubeIndex[28] = 16;
	myCubeIndex[29] = 19;
	//6
	myCubeIndex[30] = 20;
	myCubeIndex[31] = 21;
	myCubeIndex[32] = 22;
	myCubeIndex[33] = 22;
	myCubeIndex[34] = 23;
	myCubeIndex[35] = 20;

	D3D11_BUFFER_DESC Ibd;
	ZeroMemory(&Ibd, sizeof(Ibd));
	Ibd.Usage = D3D11_USAGE_DEFAULT;
	Ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Ibd.CPUAccessFlags = NULL;
	Ibd.ByteWidth = sizeof(unsigned short) * 36;
	Ibd.MiscFlags = 0; //unused
	Ibd.StructureByteStride = sizeof(unsigned short);

	D3D11_SUBRESOURCE_DATA indexInitData;
	ZeroMemory(&indexInitData, sizeof(indexInitData));
	indexInitData.pSysMem = &myCubeIndex;
	indexInitData.SysMemPitch = 0;
	indexInitData.SysMemSlicePitch = 0;

	hr = device->CreateBuffer(&Ibd, &indexInitData, &cubeIndexbuffer);

	ZeroMemory(&Ibd, sizeof(Ibd));
	Ibd.Usage = D3D11_USAGE_DEFAULT;
	Ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Ibd.CPUAccessFlags = NULL;
	Ibd.ByteWidth = sizeof(teapot_indicies);
	Ibd.MiscFlags = 0; //unused
	Ibd.StructureByteStride = sizeof(unsigned int);

	ZeroMemory(&indexInitData, sizeof(indexInitData));
	indexInitData.pSysMem = teapot_indicies;
	indexInitData.SysMemPitch = 0;
	indexInitData.SysMemSlicePitch = 0;

	hr = device->CreateBuffer(&Ibd, &indexInitData, &PotIndexbuffer);

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
	cbd.ByteWidth = sizeof(SEND_POINT_LIGHT);

	hr = device->CreateBuffer(&cbd, nullptr, &PLightconstantBuffer);

	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbd.ByteWidth = sizeof(SEND_SPOT_LIGHT);

	hr = device->CreateBuffer(&cbd, nullptr, &PSpotLightconstantBuffer);

	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbd.ByteWidth = sizeof(SEND_TO_SCENE);

	hr = device->CreateBuffer(&cbd, nullptr, &SceneconstantBuffer);

	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbd.ByteWidth = sizeof(SEND_TYPE);

	hr = device->CreateBuffer(&cbd, nullptr, &PSTypeconstantBuffer);
	

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "UV", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);
	hr = device->CreateInputLayout(layout, numElements, Trivial_VS, sizeof(Trivial_VS), &vertexLayout);
	for (int i = 0; i < 3; i++)
		WtoShader[i].World = XMMatrixIdentity();
	rotation = XMMatrixIdentity();

	StoShader[0].ViewM = XMMatrixIdentity();
	StoShader[0].ProjectM = XMMatrixPerspectiveFovLH(1.13446401f, 1.0f, 0.1f, 100.0f);
	StoShader[1].ViewM = XMMatrixIdentity();
	StoShader[1].ProjectM = XMMatrixPerspectiveFovLH(1.13446401f, BACKBUFFER_WIDTH / BACKBUFFER_HEIGHT, 0.1f, 100.0f);

	XMFLOAT4 vec = XMFLOAT4(0.0f, 1.5f, -1.4f, 0.0f);
	XMVECTOR aux = XMLoadFloat4(&vec);
	XMFLOAT4 vec2 = XMFLOAT4(0.0f, 1.5f, 0.0f, 0.0f);
	XMVECTOR aux2 = XMLoadFloat4(&vec2);
	XMFLOAT4 vec3 = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR aux3 = XMLoadFloat4(&vec3);

	StoShader[0].ViewM *= XMMatrixLookAtLH(aux, aux2, aux3);
	StoShader[1].ViewM *= XMMatrixLookAtLH(aux, aux2, aux3);

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

	LtoPShader.pos = XMFLOAT3(0.0f, 2.0f, 0.0f);
	LtoPShader.dir = XMFLOAT3(0.0f, -1.0f, 0.0f);
	LtoPShader.col = XMFLOAT4(1.0f, 0.77f, 0.0f, 1.0f);
	LtoPShader.rPoint = 5.0f;

	sLtoPShader.pos = XMFLOAT3(-4.0f, 3.0f, -4.0f);
	sLtoPShader.dir = XMFLOAT3(1.0f, -1.0f, 1.0f);
	sLtoPShader.col = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	sLtoPShader.sPoint = 10.0f;
	sLtoPShader.inner = 0.92f;
	sLtoPShader.outer = 0.9f;
}

//************************************************************
//************ EXECUTION *************************************
//************************************************************

bool DEMO_APP::Run()
{

	time.Signal();
	dt = time.Delta();

	if (_getch())
	{
		Input();
	}

	inmediateContext->OMSetRenderTargets(1, &RTTrenderTargetView, RTTstencilView);
	inmediateContext->ClearRenderTargetView(RTTrenderTargetView, Colors::Red);
	inmediateContext->ClearDepthStencilView(RTTstencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0xFF);

	inmediateContext->RSSetViewports(1, RTTviewport);
	inmediateContext->OMSetDepthStencilState(stencilState, 1);

	inmediateContext->PSSetShader(pixelShader, nullptr, 0);
	inmediateContext->VSSetShader(vertexShader, nullptr, 0);

	inmediateContext->VSSetConstantBuffers(0, 1, &WorldconstantBuffer);
	inmediateContext->VSSetConstantBuffers(1, 1, &SceneconstantBuffer);
	//light
	inmediateContext->PSSetConstantBuffers(0, 1, &PLightconstantBuffer);
	inmediateContext->PSSetConstantBuffers(1, 1, &PSpotLightconstantBuffer);
	inmediateContext->PSSetConstantBuffers(2, 1, &PSTypeconstantBuffer);

	inmediateContext->PSSetSamplers(0, 1, &samplerState);
	
	D3D11_MAPPED_SUBRESOURCE  Resource;

	inmediateContext->Map(WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &WtoShader[0], sizeof(SEND_TO_WORLD));
	inmediateContext->Unmap(WorldconstantBuffer, 0);

	inmediateContext->Map(SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &StoShader[0], sizeof(SEND_TO_SCENE));
	inmediateContext->Unmap(SceneconstantBuffer, 0);

	inmediateContext->Map(PLightconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &LtoPShader, sizeof(SEND_POINT_LIGHT));
	inmediateContext->Unmap(PLightconstantBuffer, 0);

	inmediateContext->Map(PSpotLightconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &sLtoPShader, sizeof(SEND_SPOT_LIGHT));
	inmediateContext->Unmap(PSpotLightconstantBuffer, 0);

	TypetoPShader.mytype = true; // floor

	inmediateContext->Map(PSTypeconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &TypetoPShader, sizeof(bool));
	inmediateContext->Unmap(PSTypeconstantBuffer, 0);

	UINT sstride = sizeof(SimpleVertex);
	UINT soffset = 0;

	inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[1]);
	inmediateContext->PSSetShaderResources(1, 1, &shaderResourceView[0]);

	XMMATRIX translation = XMMatrixIdentity();
	translation = XMMatrixTranslation(0.0f, 1.0f, 2.5f);
	
	XMMATRIX rotateY = XMMatrixIdentity();
	XMMATRIX rotateZ = XMMatrixIdentity();
	
	XMFLOAT3 rot = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMVECTOR aux = XMLoadFloat3(&rot);
	rotateY *= XMMatrixRotationAxis(aux, dt);

	rot = XMFLOAT3(0.0f, 0.0f, 1.0f);
	aux = XMLoadFloat3(&rot);
	rotateZ *= XMMatrixRotationAxis(aux, 0.75f * dt);

	rotation *= rotateY *rotateZ;

	WtoShader[1].World = rotation * translation;

	inmediateContext->Map(WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &WtoShader[1], sizeof(SEND_TO_WORLD));
	inmediateContext->Unmap(WorldconstantBuffer, 0);

	inmediateContext->Map(SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &StoShader[0], sizeof(SEND_TO_SCENE));
	inmediateContext->Unmap(SceneconstantBuffer, 0);

	TypetoPShader.mytype = false;

	inmediateContext->Map(PSTypeconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &TypetoPShader, sizeof(bool));
	inmediateContext->Unmap(PSTypeconstantBuffer, 0);

	sstride = sizeof(_OBJ_VERT_);
	soffset = 0;
	inmediateContext->IASetVertexBuffers(0, 1, &PotVertexbuffer, &sstride, &soffset);
	inmediateContext->IASetIndexBuffer(PotIndexbuffer, DXGI_FORMAT_R32_UINT, 0);

	inmediateContext->IASetInputLayout(vertexLayout);
	inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	inmediateContext->DrawIndexed(4632, 0, 0);
	inmediateContext->GenerateMips(shaderResourceViewMap);

	//CUBE DRAW

	XMMATRIX translationCub = XMMatrixIdentity();
	translationCub = XMMatrixTranslation(0.5f, 1.0f, 0.0f);

	WtoShader[2].World = translation;

	inmediateContext->OMSetRenderTargets(1, &renderTargetView, stencilView);
	
	inmediateContext->RSSetViewports(1, viewport);
	inmediateContext->OMSetDepthStencilState(stencilState, 0);
	inmediateContext->ClearDepthStencilView(stencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0xFF);
	inmediateContext->ClearRenderTargetView(renderTargetView, Colors::Gray);
	
	inmediateContext->VSSetConstantBuffers(0, 1, &WorldconstantBuffer);
	inmediateContext->VSSetConstantBuffers(1, 1, &SceneconstantBuffer);
	//light
	inmediateContext->PSSetConstantBuffers(0, 1, &PLightconstantBuffer);
	inmediateContext->PSSetConstantBuffers(1, 1, &PSpotLightconstantBuffer);
	inmediateContext->PSSetConstantBuffers(2, 1, &PSTypeconstantBuffer);

	TypetoPShader.mytype = true; // 2dtex

	inmediateContext->PSSetSamplers(0, 1, &samplerState);
	inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[1]);
	inmediateContext->PSSetShaderResources(1, 1, &shaderResourceViewMap);


	inmediateContext->Map(WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &WtoShader[2], sizeof(SEND_TO_WORLD));
	inmediateContext->Unmap(WorldconstantBuffer, 0);

	inmediateContext->Map(SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &StoShader[1], sizeof(SEND_TO_SCENE));
	inmediateContext->Unmap(SceneconstantBuffer, 0);

	inmediateContext->Map(PLightconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &LtoPShader, sizeof(SEND_POINT_LIGHT));
	inmediateContext->Unmap(PLightconstantBuffer, 0);

	inmediateContext->Map(PSpotLightconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &sLtoPShader, sizeof(SEND_SPOT_LIGHT));
	inmediateContext->Unmap(PSpotLightconstantBuffer, 0);

	inmediateContext->Map(PSTypeconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &TypetoPShader, sizeof(bool));
	inmediateContext->Unmap(PSTypeconstantBuffer, 0);

	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;

	inmediateContext->IASetInputLayout(vertexLayout);
	inmediateContext->IASetVertexBuffers(0, 1, &cubeVertexbuffer, &stride, &offset);
	inmediateContext->IASetIndexBuffer(cubeIndexbuffer, DXGI_FORMAT_R16_UINT, 0);

	inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	inmediateContext->DrawIndexed(36, 0, 0);

	ID3D11ShaderResourceView* temp;
	temp = nullptr;
	inmediateContext->PSSetShaderResources(0, 1, &temp);
	inmediateContext->PSSetShaderResources(1, 1, &temp);
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
	
	for (int i = 0; i < 2; i++)
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

	SAFE_RELEASE(vertexLayout);
	SAFE_DELETE(vertexLayout);


	SAFE_RELEASE(PotVertexbuffer);
	SAFE_DELETE(PotVertexbuffer);

	SAFE_RELEASE(PotIndexbuffer);
	SAFE_DELETE(PotIndexbuffer);

	SAFE_RELEASE(PLightconstantBuffer);
	SAFE_DELETE(PLightconstantBuffer);

	SAFE_RELEASE(PSpotLightconstantBuffer);
	SAFE_DELETE(PSpotLightconstantBuffer);

	SAFE_RELEASE(PSTypeconstantBuffer);
	SAFE_DELETE(PSTypeconstantBuffer);

	SAFE_RELEASE(RTTTextureMap);
	SAFE_DELETE(RTTTextureMap);

	SAFE_RELEASE(shaderResourceViewMap);
	SAFE_DELETE(shaderResourceViewMap);

	SAFE_RELEASE(RTTstencilView);
	SAFE_DELETE(RTTstencilView);

	SAFE_RELEASE(RTTrenderTargetView);
	SAFE_DELETE(RTTrenderTargetView);

	SAFE_RELEASE(RTTzBuffer);
	SAFE_DELETE(RTTzBuffer);

	SAFE_RELEASE(cubeVertexbuffer);
	SAFE_DELETE(cubeVertexbuffer);

	SAFE_RELEASE(cubeIndexbuffer);
	SAFE_DELETE(cubeIndexbuffer);

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


void DEMO_APP::Input()
{
	XMMATRIX mat = XMMatrixInverse(nullptr, StoShader[1].ViewM);

	if (GetAsyncKeyState('W'))
	{
		mat = XMMatrixTranslation(0.0f, 1.0f * dt, 0.0f) * mat;


	}
	else if (GetAsyncKeyState('S'))
	{
		mat = XMMatrixTranslation(0.0f, -1.0f * dt, 0.0f) * mat;;

	}
	else if (GetAsyncKeyState('A'))
	{
		mat = XMMatrixTranslation(-1.0f * dt, 0.0f, 0.0f) * mat;

	}
	else if (GetAsyncKeyState('D'))
	{
		mat = XMMatrixTranslation(1.0f * dt, 0.0f, 0.0f) * mat;

	}

	if (GetAsyncKeyState('U'))
	{
		mat = XMMatrixTranslation(0.0f, 0.0f, 1.0f * dt) * mat;


	}
	else if (GetAsyncKeyState('J'))
	{
		mat = XMMatrixTranslation(0.0f, 0.0f, -1.0f * dt) * mat;

	}

	else if (GetAsyncKeyState('X'))
	{
		XMFLOAT3 rot = XMFLOAT3(1.0f, 0.0f, 0.0f);
		XMVECTOR aux = XMLoadFloat3(&rot);
		mat = XMMatrixRotationAxis(aux, 1.0f *dt) * mat;

	}
	else if (GetAsyncKeyState('Z'))
	{
		XMFLOAT3 rot = XMFLOAT3(1.0f, 0.0f, 0.0f);
		XMVECTOR aux = XMLoadFloat3(&rot);
		mat = XMMatrixRotationAxis(aux, -1.0f *dt)* mat;


	}

	else if (GetAsyncKeyState('C'))
	{
		XMFLOAT3 rot = XMFLOAT3(0.0f, 1.0f, 0.0f);
		XMVECTOR aux = XMLoadFloat3(&rot);
		mat = XMMatrixRotationAxis(aux, -1.0f *dt)* mat;

	}
	else if (GetAsyncKeyState('V'))
	{
		XMFLOAT3 rot = XMFLOAT3(0.0f, 1.0f, 0.0f);
		XMVECTOR aux = XMLoadFloat3(&rot);
		mat = XMMatrixRotationAxis(aux, 1.0f *dt) *mat;

	}

	StoShader[1].ViewM = XMMatrixInverse(nullptr, mat);
}