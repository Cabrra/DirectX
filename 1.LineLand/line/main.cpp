// D3D "Line Land".

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

	ID3D11Buffer*					vertexbuffer;
	ID3D11InputLayout*				vertexLayout;
	unsigned int					counter;

	ID3D11Buffer*					gridVertexbuffer;
	unsigned int					gridCounter;

	ID3D11VertexShader*				vertexShader;
	ID3D11PixelShader*				pixelShader;

	XMFLOAT2						velocity;
	ID3D11Buffer*					constantBuffer;
	XTime							time;

	struct SEND_TO_VRAM
	{
		XMFLOAT4 Color;
		XMFLOAT2 Pos;
		XMFLOAT2 padding;
	};

	 SEND_TO_VRAM					toShader;

	 SEND_TO_VRAM					gridToShader;

public:
	struct SimpleVertex
	{
		XMFLOAT2 Pos;
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

	window = CreateWindow(	L"DirectXApplication", L"Line Land",	WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME|WS_MAXIMIZEBOX), 
							CW_USEDEFAULT, CW_USEDEFAULT, window_size.right-window_size.left, window_size.bottom-window_size.top,					
							NULL, NULL,	application, this );												

    ShowWindow( window, SW_SHOW );
	counter = 361;
	gridCounter = 1200;
	velocity = XMFLOAT2(1.0f, 0.5f);

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

	SimpleVertex myCircle [361];
	for( unsigned int i = 0; i < counter; i++ )
	{
		float angle = 3.14159f * i / 180.0f;  //2 * PI * i / NumberofVertex
		myCircle[i].Pos = XMFLOAT2(sinf( angle ), cosf( angle ));
	}

	for( unsigned int i = 0; i < counter; i++ )
	{
		myCircle[i].Pos.x *= 0.2f;
		myCircle[i].Pos.y *= 0.2f;
	}

	 D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = NULL;
    bd.ByteWidth = sizeof(SimpleVertex) * counter;
	bd.MiscFlags = 0; //unused
	bd.StructureByteStride = sizeof(SimpleVertex);

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = myCircle;

	hr = device->CreateBuffer(&bd, &InitData, &vertexbuffer);

	SimpleVertex myGrid [1200];

	XMFLOAT2 initial = XMFLOAT2(-0.9f, 1.0f);
	unsigned int index = 0;

	for (unsigned int rows = 0; rows < 20; rows++)
	{
		if (rows % 2 == 0)
			initial.x = -0.9f;
		else
			initial.x = -1.0f;

		for (unsigned int columns = 0; columns < 10; columns++)
		{
				myGrid[index].Pos = XMFLOAT2(initial.x, initial.y); 
				myGrid[index + 1].Pos = XMFLOAT2(initial.x + 0.1f, initial.y); 
				myGrid[index + 2].Pos = XMFLOAT2(initial.x + 0.1f, initial.y - 0.1f); 
			
				myGrid[index + 3].Pos = myGrid[index].Pos;
				myGrid[index + 4].Pos = myGrid[index + 2].Pos; 
				myGrid[index + 5].Pos = XMFLOAT2(initial.x , initial.y - 0.1f); 

			index += 6;
			initial.x += 0.2f;
		}
		initial.y -= 0.1f;
	}

	D3D11_BUFFER_DESC gridBd;
	ZeroMemory(&gridBd, sizeof(gridBd));
    gridBd.Usage = D3D11_USAGE_IMMUTABLE;
    gridBd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	gridBd.CPUAccessFlags = NULL;
    gridBd.ByteWidth = sizeof(SimpleVertex) * gridCounter;
	gridBd.MiscFlags = 0; //unused
	gridBd.StructureByteStride = sizeof(SimpleVertex);

	D3D11_SUBRESOURCE_DATA GridInitData;
	ZeroMemory(&GridInitData, sizeof(GridInitData));
    GridInitData.pSysMem = myGrid;

	hr = device->CreateBuffer(&gridBd, &GridInitData, &gridVertexbuffer);

	hr = device->CreateVertexShader(Trivial_VS, sizeof(Trivial_VS), nullptr, &vertexShader);
	hr = device->CreatePixelShader(Trivial_PS, sizeof(Trivial_PS), nullptr, &pixelShader);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, //Not sure of these two floats
		   D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);

    hr =  device->CreateInputLayout(layout, numElements, Trivial_VS, sizeof(Trivial_VS), &vertexLayout);

	 D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbd.ByteWidth = sizeof(SEND_TO_VRAM);

	toShader.Pos = XMFLOAT2(0.0f, 0.0f);
	toShader.Color = XMFLOAT4 (1.0f, 1.0f, 0.0f, 1.0f);
	
	gridToShader.Color =  XMFLOAT4 (0.0f, 0.0f, 0.0f, 1.0f);
	gridToShader.Pos = XMFLOAT2(0.0f, 0.0f);

	hr = device->CreateBuffer(&cbd, nullptr, &constantBuffer);
}

//************************************************************
//************ EXECUTION *************************************
//************************************************************

bool DEMO_APP::Run()
{
	time.Signal();
	double dt = time.Delta();

	if (toShader.Pos.x - 0.2 <= -1.0f || toShader.Pos.x + 0.2 >= 1.0f) // left and right
		velocity.x = -velocity.x;
	if (toShader.Pos.y - 0.2 <= -1.0f || toShader.Pos.y + 0.2 >= 1.0f) // top and down
		velocity.y = -velocity.y;

	toShader.Pos.x =  (float)(toShader.Pos.x + velocity.x * dt);
	toShader.Pos.y = (float)(toShader.Pos.y + velocity.y *dt);

	inmediateContext->OMSetRenderTargets(1, &renderTargetView, nullptr);
	inmediateContext->RSSetViewports(1, viewport);
	inmediateContext->ClearRenderTargetView(renderTargetView, Colors::MidnightBlue);
	inmediateContext->VSSetConstantBuffers(0, 1, &constantBuffer);

	D3D11_MAPPED_SUBRESOURCE  myGridResource;
	inmediateContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &myGridResource);
	memcpy(myGridResource.pData, &gridToShader, sizeof (SEND_TO_VRAM));
	inmediateContext->Unmap(constantBuffer, 0);

	UINT sstride = sizeof(SimpleVertex);
    UINT soffset = 0;
	inmediateContext->IASetVertexBuffers(0, 1, &gridVertexbuffer, &sstride, &soffset);
	inmediateContext->IASetInputLayout(vertexLayout);
	inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	inmediateContext->PSSetShader(pixelShader, nullptr, 0);
	inmediateContext->VSSetShader(vertexShader, nullptr, 0);

	inmediateContext->Draw(gridCounter, 0);

	D3D11_MAPPED_SUBRESOURCE  myResource;
	inmediateContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &myResource);
	memcpy(myResource.pData, &toShader, sizeof (SEND_TO_VRAM));
	inmediateContext->Unmap(constantBuffer, 0);

	inmediateContext->VSSetConstantBuffers(0, 1, &constantBuffer);

	UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
	inmediateContext->IASetVertexBuffers(0, 1, &vertexbuffer, &stride, &offset);

	inmediateContext->PSSetShader(pixelShader, nullptr, 0);
	inmediateContext->VSSetShader(vertexShader, nullptr, 0);
	inmediateContext->IASetInputLayout(vertexLayout);
	inmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	inmediateContext->Draw(counter, 0);

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

	SAFE_RELEASE(vertexbuffer);
	SAFE_DELETE(vertexbuffer);

	SAFE_RELEASE(vertexLayout);
	SAFE_DELETE(vertexLayout);

	SAFE_RELEASE(vertexShader);
	SAFE_DELETE(vertexShader);

	SAFE_RELEASE(pixelShader);
	SAFE_DELETE(pixelShader);

	SAFE_RELEASE(constantBuffer);
	SAFE_DELETE(constantBuffer);
	
	SAFE_RELEASE(gridVertexbuffer);
	SAFE_DELETE(gridVertexbuffer);

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
