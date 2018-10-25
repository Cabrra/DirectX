// D3D "Cube".

//************************************************************
//************ INCLUDES & DEFINES ****************************
//************************************************************

#include <windows.h>
#include <conio.h>
#include <thread>

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
#include "DDSTextureLoader.h"

#define BACKBUFFER_WIDTH	500
#define BACKBUFFER_HEIGHT	500

#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } };
#define SAFE_DELETE(a) if( (a) != NULL ) delete (a); (a) = NULL;
#define WAIT_FOR_THREAD(r) if ((r)->joinable()) (r)->join();

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

	ID3D11Texture2D*				zBuffer;
	ID3D11DepthStencilState *		stencilState;
	ID3D11DepthStencilView*			stencilView;
	ID3D11Buffer*					vertexbuffer;
	ID3D11Buffer*					indexbuffer;

	ID3D11VertexShader*				vertexShader;
	ID3D11PixelShader*				pixelShader;
	ID3D11InputLayout*				vertexLayout;

	ID3D11Buffer*					WorldconstantBuffer;
	ID3D11Buffer*					SceneconstantBuffer;
	XTime							time;
	XMMATRIX						rotation;

	ID3D11RasterizerState*			rasterState;
	ID3D11ShaderResourceView*		shaderResourceView;
	ID3D11SamplerState*				samplerState;
	ID3D11DeviceContext*			deferredcontext;
	ID3D11CommandList*				commandList;

	struct SEND_TO_WORLD
	{
		XMMATRIX World;
	};

	struct SEND_TO_SCENE
	{
		XMMATRIX ViewM;
		XMMATRIX ProjectM;
	};

	SEND_TO_WORLD					WtoShader;
	SEND_TO_SCENE					StoShader;

public:

	friend void LoadingThread(DEMO_APP* myApp);
	friend void DrawingThread(DEMO_APP* myApp);
	struct SimpleVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 UV;
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

	window = CreateWindow(L"DirectXApplication", L"Cube", WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
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


	std::thread myLoadingThread = std::thread(LoadingThread, this);

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

	//3D cube
	SimpleVertex myCube[24];

	//top
	myCube[0].Pos = XMFLOAT3(-0.3f, 0.3f, 0.3f);
	myCube[1].Pos = XMFLOAT3(0.3f, 0.3f, 0.3f);
	myCube[2].Pos = XMFLOAT3(0.3f, 0.3f, -0.3f);
	myCube[3].Pos = XMFLOAT3(-0.3f, 0.3f, -0.3f);

	myCube[0].UV  = XMFLOAT2(0.0f, 0.0f);
	myCube[1].UV  = XMFLOAT2(1.0f, 0.0f);
	myCube[2].UV  = XMFLOAT2(1.0f, 1.0f);
	myCube[3].UV  = XMFLOAT2(0.0f, 1.0f);

	//bottom
	myCube[4].Pos = XMFLOAT3(-0.3f, -0.3f, 0.3f);
	myCube[5].Pos = XMFLOAT3(0.3f, -0.3f, 0.3f);
	myCube[6].Pos = XMFLOAT3(0.3f, -0.3f, -0.3f);
	myCube[7].Pos = XMFLOAT3(-0.3f, -0.3f, -0.3f);

	myCube[4].UV = XMFLOAT2(0.0f, 1.0f);
	myCube[5].UV = XMFLOAT2(1.0f, 1.0f);
	myCube[6].UV = XMFLOAT2(1.0f, 0.0f);
	myCube[7].UV = XMFLOAT2(0.0f, 0.0f);
	//left
	myCube[8].Pos = XMFLOAT3(-0.3f, 0.3f, -0.3f);
	myCube[9].Pos = XMFLOAT3(-0.3f, 0.3f, 0.3f);
	myCube[10].Pos = XMFLOAT3(-0.3f, -0.3f, 0.3f);
	myCube[11].Pos = XMFLOAT3(-0.3f, -0.3f, -0.3f);

	myCube[8].UV = XMFLOAT2(1.0f, 0.0f);
	myCube[9].UV = XMFLOAT2(0.0f, 0.0f);
	myCube[10].UV = XMFLOAT2(0.0f, 1.0f);
	myCube[11].UV = XMFLOAT2(1.0f, 1.0f);
	//right
	myCube[12].Pos = XMFLOAT3(0.3f, 0.3f, -0.3f);
	myCube[13].Pos = XMFLOAT3(0.3f, 0.3f, 0.3f);
	myCube[14].Pos = XMFLOAT3(0.3f, -0.3f, 0.3f);
	myCube[15].Pos = XMFLOAT3(0.3f, -0.3f, -0.3f);

	myCube[12].UV = XMFLOAT2(0.0f, 0.0f);
	myCube[13].UV = XMFLOAT2(1.0f, 0.0f);
	myCube[14].UV = XMFLOAT2(1.0f, 1.0f);
	myCube[15].UV = XMFLOAT2(0.0f, 1.0f);
	//back
	myCube[16].Pos = XMFLOAT3(-0.3f, 0.3f, 0.3f);
	myCube[17].Pos = XMFLOAT3(0.3f, 0.3f, 0.3f);
	myCube[18].Pos = XMFLOAT3(0.3f, -0.3f, 0.3f);
	myCube[19].Pos = XMFLOAT3(-0.3f, -0.3f, 0.3f);

	myCube[16].UV = XMFLOAT2(1.0f, 0.0f);
	myCube[17].UV = XMFLOAT2(0.0f, 0.0f);
	myCube[18].UV = XMFLOAT2(0.0f, 1.0f);
	myCube[19].UV = XMFLOAT2(1.0f, 1.0f);
	//front
	myCube[20].Pos = XMFLOAT3(-0.3f, 0.3f, -0.3f);
	myCube[21].Pos = XMFLOAT3(0.3f, 0.3f, -0.3f);
	myCube[22].Pos = XMFLOAT3(0.3f, -0.3f, -0.3f);
	myCube[23].Pos = XMFLOAT3(-0.3f, -0.3f, -0.3f);

	myCube[20].UV = XMFLOAT2(0.0f, 0.0f);
	myCube[21].UV = XMFLOAT2(1.0f, 0.0f);
	myCube[22].UV = XMFLOAT2(1.0f, 1.0f);
	myCube[23].UV = XMFLOAT2(0.0f, 1.0f);

	//To VRAM
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = NULL;
	bd.ByteWidth = sizeof(SimpleVertex) * 24;
	bd.MiscFlags = 0; //unused
	bd.StructureByteStride = sizeof(SimpleVertex);

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = myCube;

	hr = device->CreateBuffer(&bd, &InitData, &vertexbuffer);

	unsigned short myIndex[36];

	//1t
	myIndex[0] = 0;
	myIndex[1] = 1;
	myIndex[2] = 2;
	myIndex[3] = 2;
	myIndex[4] = 3;
	myIndex[5] = 0;
	//2b
	myIndex[6] = 4;
	myIndex[7] = 6;
	myIndex[8] = 5;
	myIndex[9] = 6;
	myIndex[10] = 4;
	myIndex[11] = 7;
	//3l
	myIndex[12] = 9;
	myIndex[13] = 8;
	myIndex[14] = 10;
	myIndex[15] = 10;
	myIndex[16] = 8;
	myIndex[17] = 11;
	//4r
	myIndex[18] = 12;
	myIndex[19] = 13;
	myIndex[20] = 14;
	myIndex[21] = 14;
	myIndex[22] = 15;
	myIndex[23] = 12;
	//5		    
	myIndex[24]= 17;
	myIndex[25]= 16;
	myIndex[26]= 18;
	myIndex[27] = 18;
	myIndex[28] = 16;
	myIndex[29] = 19;
	//6
	myIndex[30] = 20;
	myIndex[31] = 21;
	myIndex[32] = 22;
	myIndex[33] = 22;
	myIndex[34] = 23;
	myIndex[35] = 20;

	hr = device->CreateDeferredContext(0, &deferredcontext);

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
	indexInitData.pSysMem = &myIndex;
	indexInitData.SysMemPitch = 0;
	indexInitData.SysMemSlicePitch = 0;

	hr = device->CreateBuffer(&Ibd, &indexInitData, &indexbuffer);

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

	hr = device->CreateVertexShader(Trivial_VS, sizeof(Trivial_VS), nullptr, &vertexShader);
	hr = device->CreatePixelShader(Trivial_PS, sizeof(Trivial_PS), nullptr, &pixelShader);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);
	hr = device->CreateInputLayout(layout, numElements, Trivial_VS, sizeof(Trivial_VS), &vertexLayout);

	WtoShader.World = XMMatrixIdentity();

	StoShader.ProjectM = XMMatrixPerspectiveFovLH((3.14159265359 / 2), 1.0f, 0.1f, 100.0f);
	StoShader.ViewM = XMMatrixIdentity();

	//StoShader.ViewM = XMPlaneTransform();
	rotation = XMMatrixIdentity();

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
	sampd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	//sampd.BorderColor = FLOAT4( 0.0F, 0.0F, 0.0F, 0.0F );
	sampd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampd.MaxAnisotropy = 1;
	sampd.MaxLOD = D3D11_FLOAT32_MAX;
	sampd.MinLOD = 0;
	sampd.MipLODBias = 2;

	hr = device->CreateSamplerState(&sampd, &samplerState);

	WAIT_FOR_THREAD(&myLoadingThread);
}

//************************************************************
//************ EXECUTION *************************************
//************************************************************

bool DEMO_APP::Run()
{

	std::thread myDrawingThread = std::thread(DrawingThread, this);

	time.Signal();
	double dt = time.Delta();
	XMFLOAT4 vec = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR aux = XMLoadFloat4(&vec);
	rotation *= XMMatrixRotationAxis(aux, (1.0f * dt));

	XMMATRIX mat = XMMatrixInverse(nullptr, StoShader.ViewM);

	if (GetAsyncKeyState('W'))
	{
		mat = mat * XMMatrixTranslation(0.0f, 1.0f * dt, 0.0f);
	}
	else if (GetAsyncKeyState('S'))
	{
		mat = mat * XMMatrixTranslation(0.0f, -1.0f * dt, 0.0f);
	}
	else if (GetAsyncKeyState('A'))
	{
		mat = mat * XMMatrixTranslation(-1.0f * dt, 0.0f, 0.0f);
	}
	else if (GetAsyncKeyState('D'))
	{
		mat = mat * XMMatrixTranslation(1.0f * dt, 0.0f, 0.0f);
	}

	else if (GetAsyncKeyState('X'))
	{
		XMFLOAT3 rot = XMFLOAT3(1.0f, 0.0f, 0.0f);
		XMVECTOR aux = XMLoadFloat3(&rot);
		mat *= XMMatrixRotationAxis(aux, 1.0f *dt);

	}
	else if (GetAsyncKeyState('Z'))
	{
		XMFLOAT3 rot = XMFLOAT3(1.0f, 0.0f, 0.0f);
		XMVECTOR aux = XMLoadFloat3(&rot);
		mat *= XMMatrixRotationAxis(aux, -1.0f *dt);
		
		
	}

	else if (GetAsyncKeyState('C'))
	{
		XMFLOAT3 rot = XMFLOAT3(0.0f, 1.0f, 0.0f);
		XMVECTOR aux = XMLoadFloat3(&rot);
		mat *= XMMatrixRotationAxis(aux, -1.0f *dt);

	}
	else if (GetAsyncKeyState('V'))
	{
		XMFLOAT3 rot = XMFLOAT3(0.0f, 1.0f, 0.0f);
		XMVECTOR aux = XMLoadFloat3(&rot);
		mat *= XMMatrixRotationAxis(aux, 1.0f *dt);

	}

	StoShader.ViewM = XMMatrixInverse(nullptr, mat);

	WtoShader.World = rotation *XMMatrixTranslation(0.0f, 0.0f, 1.0f);
	WAIT_FOR_THREAD(&myDrawingThread);
	if (commandList)
		inmediateContext->ExecuteCommandList(commandList, true);
	SAFE_RELEASE(commandList);
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

	SAFE_RELEASE(zBuffer);
	SAFE_DELETE(zBuffer);

	SAFE_RELEASE(stencilState);
	SAFE_DELETE(stencilState);

	SAFE_RELEASE(stencilView);
	SAFE_DELETE(stencilView);

	SAFE_RELEASE(vertexbuffer);
	SAFE_DELETE(vertexbuffer);

	SAFE_RELEASE(indexbuffer);
	SAFE_DELETE(indexbuffer);

	SAFE_RELEASE(pixelShader);
	SAFE_DELETE(pixelShader);

	SAFE_RELEASE(vertexShader);
	SAFE_DELETE(vertexShader);

	SAFE_RELEASE(vertexLayout);
	SAFE_DELETE(vertexLayout);

	SAFE_RELEASE(WorldconstantBuffer);
	SAFE_DELETE(WorldconstantBuffer);

	SAFE_RELEASE(SceneconstantBuffer);
	SAFE_DELETE(SceneconstantBuffer);

	SAFE_RELEASE(rasterState);
	SAFE_DELETE(rasterState);

	SAFE_RELEASE(shaderResourceView);
	SAFE_DELETE(shaderResourceView);

	SAFE_RELEASE(samplerState);
	SAFE_DELETE(samplerState);

	SAFE_RELEASE(deferredcontext);
	SAFE_DELETE(deferredcontext);

	SAFE_RELEASE(commandList);
	SAFE_DELETE(commandList);
	
	UnregisterClass(L"DirectXApplication", application);
	return true;
}

//************************************************************
//************ WINDOWS RELATED *******************************
//************************************************************

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int)
{
	srand(unsigned int(time(0)));
	DEMO_APP myApp(hInstance, (WNDPROC)WndProc);
	MSG msg; ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT && myApp.Run())
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	myApp.ShutDown();
	return 0;
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (GetAsyncKeyState(VK_ESCAPE))
		message = WM_DESTROY;
	switch (message)
	{
	case (WM_DESTROY) : { PostQuitMessage(0); }
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void LoadingThread(DEMO_APP* myApp)
{
	HRESULT hrT;

	hrT = CreateDDSTextureFromFile(myApp->device, L"Assets/Textures/FullSailLogo.dds", nullptr, &myApp->shaderResourceView);
}

void DrawingThread(DEMO_APP* myApp)
{
	if (myApp->shaderResourceView)
	{
		myApp->deferredcontext->OMSetRenderTargets(1, &myApp->renderTargetView, myApp->stencilView);
		myApp->deferredcontext->OMSetDepthStencilState(myApp->stencilState, 1);
		myApp->deferredcontext->RSSetViewports(1, myApp->viewport);

		myApp->deferredcontext->ClearRenderTargetView(myApp->renderTargetView, Colors::MidnightBlue);
		myApp->deferredcontext->ClearDepthStencilView(myApp->stencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0xFF);

		myApp->deferredcontext->VSSetConstantBuffers(0, 1, &myApp->WorldconstantBuffer);
		myApp->deferredcontext->VSSetConstantBuffers(1, 1, &myApp->SceneconstantBuffer);

		D3D11_MAPPED_SUBRESOURCE  Resource;

		myApp->deferredcontext->Map(myApp->WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
		memcpy(Resource.pData, &myApp->WtoShader, sizeof(DEMO_APP::SEND_TO_WORLD));
		myApp->deferredcontext->Unmap(myApp->WorldconstantBuffer, 0);

		myApp->deferredcontext->Map(myApp->SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
		memcpy(Resource.pData, &myApp->StoShader, sizeof(DEMO_APP::SEND_TO_SCENE));
		myApp->deferredcontext->Unmap(myApp->SceneconstantBuffer, 0);

		UINT sstride = sizeof(DEMO_APP::SimpleVertex);
		UINT soffset = 0;
		myApp->deferredcontext->IASetVertexBuffers(0, 1, &myApp->vertexbuffer, &sstride, &soffset);
		myApp->deferredcontext->IASetIndexBuffer(myApp->indexbuffer, DXGI_FORMAT_R16_UINT, 0);

		myApp->deferredcontext->IASetInputLayout(myApp->vertexLayout);
		myApp->deferredcontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		myApp->deferredcontext->PSSetShader(myApp->pixelShader, nullptr, 0);
		myApp->deferredcontext->VSSetShader(myApp->vertexShader, nullptr, 0);

		myApp->deferredcontext->PSSetSamplers(0, 1, &myApp->samplerState);
		myApp->deferredcontext->PSSetShaderResources(0, 1, &myApp->shaderResourceView);

		myApp->deferredcontext->DrawIndexed(36, 0, 0);
		myApp->deferredcontext->FinishCommandList(true, &myApp->commandList);

	}
}