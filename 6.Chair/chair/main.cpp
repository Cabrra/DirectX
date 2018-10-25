// D3D "Chair Scene".

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
#include "newchair.h"
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

	ID3D11ShaderResourceView*		shaderResourceView[2];
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

	ID3D11Buffer*					Chairvertexbuffer;
	ID3D11Buffer*					Chairindexbuffer;
	ID3D11Buffer*					PLightconstantBuffer;
	ID3D11Buffer*					PSpotLightconstantBuffer;

	XTime							time;
	XMMATRIX						rotation;

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

	SEND_TO_WORLD					WtoShader[2];
	SEND_TO_SCENE					StoShader;
	SEND_POINT_LIGHT				LtoPShader;
	SEND_SPOT_LIGHT					sLtoPShader;

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

	window = CreateWindow(L"DirectXApplication", L"Chair Scene", WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
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

	hr = CreateDDSTextureFromFile(device, L"Assets/Textures/Green Marble Tiles.dds", nullptr, &shaderResourceView[0]);
	hr = CreateDDSTextureFromFile(device, L"Assets/Textures/chair01_texture.dds", nullptr, &shaderResourceView[1]);

	SimpleVertex myPlane[4];

	myPlane[0].Pos = XMFLOAT3(-5.0f, 0.0f, 5.0f);
	myPlane[1].Pos = XMFLOAT3(5.0f, 0.0f, 5.0f);
	myPlane[2].Pos = XMFLOAT3(5.0f, 0.0f, -5.0f);
	myPlane[3].Pos = XMFLOAT3(-5.0f, 0.0f, -5.0f);

	myPlane[0].UV = XMFLOAT3(0.0f, 0.0f, 0.0f);
	myPlane[1].UV = XMFLOAT3(2.0f, 0.0f, 0.0f);
	myPlane[2].UV = XMFLOAT3(2.0f, 2.0f, 0.0f);
	myPlane[3].UV = XMFLOAT3(0.0f, 2.0f, 0.0f);

	myPlane[0].norm = myPlane[1].norm = myPlane[2].norm = myPlane[3].norm = XMFLOAT3(0.0f, 1.0f, 0.0f);

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

	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = NULL;
	bd.ByteWidth = sizeof(newchair_data);
	bd.MiscFlags = 0; //unused
	bd.StructureByteStride = sizeof(_OBJ_VERT_);

	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = newchair_data;

	hr = device->CreateBuffer(&bd, &InitData, &Chairvertexbuffer);

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

	ZeroMemory(&Ibd, sizeof(Ibd));
	Ibd.Usage = D3D11_USAGE_DEFAULT;
	Ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Ibd.CPUAccessFlags = NULL;
	Ibd.ByteWidth = sizeof(newchair_indicies);
	Ibd.MiscFlags = 0; //unused
	Ibd.StructureByteStride = sizeof(unsigned int);

	ZeroMemory(&indexInitData, sizeof(indexInitData));
	indexInitData.pSysMem = newchair_indicies;
	indexInitData.SysMemPitch = 0;
	indexInitData.SysMemSlicePitch = 0;

	hr = device->CreateBuffer(&Ibd, &indexInitData, &Chairindexbuffer);

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
	for (int i = 0; i < 2; i++)
		WtoShader[i].World = XMMatrixIdentity();
	rotation = XMMatrixIdentity();

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
	int c = 0;
	c = getch();
	if (c)
	{
		Input();
	}

	inmediateContext->OMSetRenderTargets(1, &renderTargetView, stencilView);
	inmediateContext->RSSetViewports(1, viewport);
	inmediateContext->OMSetDepthStencilState(stencilState, 1);
	inmediateContext->ClearRenderTargetView(renderTargetView, Colors::MidnightBlue);

	inmediateContext->PSSetShader(pixelShader, nullptr, 0);
	inmediateContext->VSSetShader(vertexShader, nullptr, 0);

	inmediateContext->VSSetConstantBuffers(0, 1, &WorldconstantBuffer);
	inmediateContext->VSSetConstantBuffers(1, 1, &SceneconstantBuffer);
	//light
	inmediateContext->PSSetConstantBuffers(0, 1, &PLightconstantBuffer);
	inmediateContext->PSSetConstantBuffers(1, 1, &PSpotLightconstantBuffer);
	inmediateContext->PSSetSamplers(0, 1, &samplerState);
	inmediateContext->ClearDepthStencilView(stencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0xFF);

	D3D11_MAPPED_SUBRESOURCE  Resource;

	inmediateContext->Map(WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &WtoShader[0], sizeof(SEND_TO_WORLD));
	inmediateContext->Unmap(WorldconstantBuffer, 0);

	inmediateContext->Map(SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &StoShader, sizeof(SEND_TO_SCENE));
	inmediateContext->Unmap(SceneconstantBuffer, 0);

	inmediateContext->Map(PLightconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &LtoPShader, sizeof(SEND_POINT_LIGHT));
	inmediateContext->Unmap(PLightconstantBuffer, 0);

	inmediateContext->Map(PSpotLightconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &sLtoPShader, sizeof(SEND_SPOT_LIGHT));
	inmediateContext->Unmap(PSpotLightconstantBuffer, 0);

	UINT sstride = sizeof(SimpleVertex);
	UINT soffset = 0;
	inmediateContext->IASetVertexBuffers(0, 1, &vertexbuffer, &sstride, &soffset);
	inmediateContext->IASetIndexBuffer(indexbuffer, DXGI_FORMAT_R16_UINT, 0);

	inmediateContext->IASetInputLayout(vertexLayout);
	inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[0]);
	inmediateContext->DrawIndexed(6, 0, 0);

	//chair
	inmediateContext->ClearDepthStencilView(stencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0xFF);

	XMMATRIX translation = XMMatrixIdentity();
	translation *= XMMatrixTranslation(-1.5f, 0.0f, 0.0f);
	
	XMMATRIX scale = XMMatrixIdentity();
	scale *= XMMatrixScaling(1.5f, 1.5f, 1.5f);
	WtoShader[1].World = scale * translation;

	inmediateContext->Map(WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &WtoShader[1], sizeof(SEND_TO_WORLD));
	inmediateContext->Unmap(WorldconstantBuffer, 0);

	inmediateContext->Map(SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &StoShader, sizeof(SEND_TO_SCENE));
	inmediateContext->Unmap(SceneconstantBuffer, 0);

	sstride = sizeof(_OBJ_VERT_);
	soffset = 0;
	inmediateContext->IASetVertexBuffers(0, 1, &Chairvertexbuffer, &sstride, &soffset);
	inmediateContext->IASetIndexBuffer(Chairindexbuffer, DXGI_FORMAT_R32_UINT, 0);

	inmediateContext->IASetInputLayout(vertexLayout);
	inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	inmediateContext->PSSetShaderResources(0, 1, &shaderResourceView[1]);
	inmediateContext->DrawIndexed(2646, 0, 0);

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

	SAFE_RELEASE(vertexbuffer);
	SAFE_DELETE(vertexbuffer);

	SAFE_RELEASE(indexbuffer);
	SAFE_DELETE(indexbuffer);

	SAFE_RELEASE(vertexLayout);
	SAFE_DELETE(vertexLayout);


	SAFE_RELEASE(Chairvertexbuffer);
	SAFE_DELETE(Chairvertexbuffer);

	SAFE_RELEASE(Chairindexbuffer);
	SAFE_DELETE(Chairindexbuffer);

	SAFE_RELEASE(PLightconstantBuffer);
	SAFE_DELETE(PLightconstantBuffer);

	SAFE_RELEASE(PSpotLightconstantBuffer);
	SAFE_DELETE(PSpotLightconstantBuffer);

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
	time.Signal();
	double dt = time.Delta();
	XMFLOAT4 vec = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR aux = XMLoadFloat4(&vec);
	rotation *= XMMatrixRotationAxis(aux, (1.0f * dt));

	XMMATRIX mat = XMMatrixInverse(nullptr, StoShader.ViewM);

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

	StoShader.ViewM = XMMatrixInverse(nullptr, mat);
}