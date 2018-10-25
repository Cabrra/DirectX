// D3D "Star".

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

#define BACKBUFFER_WIDTH	500
#define BACKBUFFER_HEIGHT	500

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

	struct SimpleVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
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

	window = CreateWindow(	L"DirectXApplication", L"Star",	WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME|WS_MAXIMIZEBOX), 
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

	//3D triangle
	SimpleVertex myTriangle [22];
	
	//front 
	myTriangle[0].Pos = XMFLOAT3(0.0f, 0.8f, 0.0f);
	myTriangle[0].Color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	myTriangle[1].Pos = XMFLOAT3(0.129f, 0.177f, 0.0f);
	myTriangle[1].Color = XMFLOAT4(1.0f, 0.5f, 0.75f, 1.0f);
	myTriangle[2].Pos = XMFLOAT3(0.76f, 0.247f, 0.0f);
	myTriangle[2].Color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	myTriangle[3].Pos = XMFLOAT3(0.2f, -0.06f, 0.0f);

	myTriangle[3].Color = XMFLOAT4(1.0f, 0.5f, 0.75f, 1.0f);
	myTriangle[4].Pos = XMFLOAT3(0.47f, -0.64f, 0.0f);
	myTriangle[4].Color = XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f);

	myTriangle[5].Pos = XMFLOAT3(0.0f, -0.22f, 0.0f);
	myTriangle[5].Color = XMFLOAT4(1.0f, 0.5f, 0.75f, 1.0f);
	myTriangle[6].Pos = XMFLOAT3(-0.47f, -0.64f, 0.0f);
	myTriangle[6].Color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);

	myTriangle[7].Pos = XMFLOAT3(-0.2f, -0.06f, 0.0f);
	myTriangle[7].Color = XMFLOAT4(1.0f, 0.5f, 0.75f, 1.0f);
	myTriangle[8].Pos = XMFLOAT3(-0.76f, 0.247f, 0.0f);
	myTriangle[8].Color = XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f);
	myTriangle[9].Pos = XMFLOAT3(-0.129f, 0.177f, 0.0f);
	myTriangle[9].Color = XMFLOAT4(1.0f, 0.5f, 0.75f, 1.0f);
	myTriangle[10].Pos = XMFLOAT3(0.0f, 0.0f, -0.1f);;
	myTriangle[10].Color = XMFLOAT4(1.0f, 0.0f, 0.25f, 1.0f);

	//back
	myTriangle[11].Pos = XMFLOAT3(0.0f, 0.8f, 0.1f);
	myTriangle[11].Color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	myTriangle[12].Pos = XMFLOAT3(0.129f, 0.177f, 0.1f);
	myTriangle[12].Color = XMFLOAT4(1.0f, 0.5f, 0.75f, 1.0f);
	myTriangle[13].Pos = XMFLOAT3(0.76f, 0.247f, 0.1f);
	myTriangle[13].Color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	myTriangle[14].Pos = XMFLOAT3(0.2f, -0.06f, 0.1f);
	myTriangle[14].Color = XMFLOAT4(1.0f, 0.5f, 0.75f, 1.0f);
	myTriangle[15].Pos = XMFLOAT3(0.47f, -0.64f, 0.1f);
	myTriangle[15].Color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	myTriangle[16].Pos = XMFLOAT3(0.0f, -0.22f, 0.1f);
	myTriangle[16].Color = XMFLOAT4(1.0f, 0.5f, 0.75f, 1.0f);
	myTriangle[17].Pos = XMFLOAT3(-0.47f, -0.64f, 0.1f);
	myTriangle[17].Color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	myTriangle[18].Pos = XMFLOAT3(-0.2f, -0.06f, 0.1f);
	myTriangle[18].Color = XMFLOAT4(1.0f, 0.5f, 0.75f, 1.0f);
	myTriangle[19].Pos = XMFLOAT3(-0.76f, 0.247f, 0.1f);
	myTriangle[19].Color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	myTriangle[20].Pos = XMFLOAT3(-0.129f, 0.177f, 0.1f);
	myTriangle[20].Color = XMFLOAT4(0.0f, 0.5f, 0.75f, 1.0f);
	myTriangle[21].Pos = XMFLOAT3(0.0f, 0.0f, 0.2f);;
	myTriangle[21].Color = XMFLOAT4(0.0f, 1.0f, 0.25f, 1.0f);

	//To VRAM
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = NULL;
    bd.ByteWidth = sizeof(SimpleVertex) * 22;
	bd.MiscFlags = 0; //unused
	bd.StructureByteStride = sizeof(SimpleVertex);

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = myTriangle;

	hr = device->CreateBuffer(&bd, &InitData,  &vertexbuffer);

	int index = 0;
	unsigned short myIndex[120];
	
	for (unsigned short i = 0; i < 10; i++)
	{
		myIndex[index] = i;
		myIndex[index + 1] = i + 1;
		myIndex[index + 2] = 10;


		index += 3;
	}

	myIndex[28] = 0;

	//back star
	index =  30;
	for (unsigned short i = 0; i < 10; i++)
	{
		myIndex[index] = i + 12;
		myIndex[index +1] = i + 11;
		myIndex[index +2] = 21;

		index += 3;
	}

	myIndex[57] = 11;

	// edges 20 = 60 triangles
	//1
	myIndex[60] = 1;
	myIndex[61] = 0;
	myIndex[62] = 12;
	myIndex[63] = 11;
	myIndex[64] = 12;
	myIndex[65] = 0;
	//2
	myIndex[66] = 2;
	myIndex[67] = 1;
	myIndex[68] = 13;
	myIndex[69] = 12;
	myIndex[70] = 13;
	myIndex[71] = 1;
	//3
	myIndex[72] = 3;
	myIndex[73] = 2;
	myIndex[74] = 14;
	myIndex[75] = 13;
	myIndex[76] = 14;
	myIndex[77] = 2;
	//4
	myIndex[78] = 4;
	myIndex[79] = 3;
	myIndex[80] = 15;
	myIndex[81] = 14;
	myIndex[82] = 15;
	myIndex[83] = 3;
	//5
	myIndex[84] = 5;
	myIndex[85] = 4;
	myIndex[86] = 16;
	myIndex[87] = 15;
	myIndex[88] = 16;
	myIndex[89] = 4;
	//6
	myIndex[90] = 6;
	myIndex[91] = 5;
	myIndex[92] = 17;
	myIndex[93] = 16;
	myIndex[94] = 17;
	myIndex[95] = 5;
	//7
	myIndex[96] = 7;
	myIndex[97] = 6;
	myIndex[98] = 18;
	myIndex[99] = 17;
	myIndex[100] = 18;
	myIndex[101] = 6;
	//8
	myIndex[102] = 8;
	myIndex[103] = 7;
	myIndex[104] = 19;
	myIndex[105] = 18;
	myIndex[106] = 19;
	myIndex[107] = 7;
	//9
	myIndex[108] = 9;
	myIndex[109] = 8;
	myIndex[110] = 20;
	myIndex[111] = 19;
	myIndex[112] = 20;
	myIndex[113] = 8;
	//10
	myIndex[114] = 0;
	myIndex[115] = 9;
	myIndex[116] = 20;
	myIndex[117] = 11;
	myIndex[118] = 0;
	myIndex[119] = 20;


	D3D11_BUFFER_DESC Ibd;
	ZeroMemory(&Ibd, sizeof(Ibd));
	Ibd.Usage = D3D11_USAGE_DEFAULT;
	Ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Ibd.CPUAccessFlags = NULL;
	Ibd.ByteWidth = sizeof(unsigned short) * 120;
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
		   { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, 
		   D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);
    hr =  device->CreateInputLayout(layout, numElements, Trivial_VS, sizeof(Trivial_VS), &vertexLayout);

	WtoShader.World = XMMatrixIdentity();

	StoShader.ProjectM = XMMatrixPerspectiveFovLH((3.14159265359 / 2), 1.0f, 0.1f, 100.0f);
	StoShader.ViewM = XMMatrixIdentity();

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
	
}

//************************************************************
//************ EXECUTION *************************************
//************************************************************

bool DEMO_APP::Run()
{
	time.Signal();
	double dt = time.Delta();
	XMFLOAT4 vec = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR aux = XMLoadFloat4(&vec);
	rotation *= XMMatrixRotationAxis(aux, (1.0f * dt));

	WtoShader.World = rotation *XMMatrixTranslation(0.0f, 0.0f, 1.0f);
	inmediateContext->OMSetRenderTargets(1, &renderTargetView, stencilView);
	inmediateContext->OMSetDepthStencilState(stencilState, 1);
	inmediateContext->RSSetViewports(1, viewport);
	inmediateContext->ClearRenderTargetView(renderTargetView, Colors::MidnightBlue);
	inmediateContext->ClearDepthStencilView(stencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0xFF);

	inmediateContext->VSSetConstantBuffers(0, 1, &WorldconstantBuffer);
	inmediateContext->VSSetConstantBuffers(1, 1, &SceneconstantBuffer);

	D3D11_MAPPED_SUBRESOURCE  Resource;

	inmediateContext->Map(WorldconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &WtoShader, sizeof (SEND_TO_WORLD));
	inmediateContext->Unmap(WorldconstantBuffer, 0);

	inmediateContext->Map(SceneconstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
	memcpy(Resource.pData, &StoShader, sizeof (SEND_TO_SCENE));
	inmediateContext->Unmap(SceneconstantBuffer, 0);

	UINT sstride = sizeof(SimpleVertex);
    UINT soffset = 0;
	inmediateContext->IASetVertexBuffers(0, 1, &vertexbuffer, &sstride, &soffset);
	inmediateContext->IASetIndexBuffer(indexbuffer, DXGI_FORMAT_R16_UINT, 0);

	inmediateContext->IASetInputLayout(vertexLayout);
	inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	inmediateContext->PSSetShader(pixelShader, nullptr, 0);
	inmediateContext->VSSetShader(vertexShader, nullptr, 0);
	inmediateContext->DrawIndexed(120, 0, 0);

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
