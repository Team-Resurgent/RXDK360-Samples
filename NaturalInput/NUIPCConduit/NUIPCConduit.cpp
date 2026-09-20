//--------------------------------------------------------------------------------------
// NUIPCConduit.cpp
//
// This sample demonstrates how to use the Xbox Studio data conduit to retrive NUI data from
// a devkit to a PC and to produce data from a PC to the NUI pipeline.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <tchar.h>
#include <assert.h>

#include <XnaMath.h>
#include <NuiTools.h>
#include <XStudio.h>
#include <utility>
#include <string>

#define SAFE_RELEASE(x) { if(x != NULL) x->Release(); }
#define V_RETURN(x)    { hr = x; if( FAILED(hr) ) { return hr; } }

//----------------------------------------------------------------------------------------------------------------------
// NuiTransformSkeletonToDepthImage() (taken from the XBox NuiSkeleton.h header)
//----------------------------------------------------------------------------------------------------------------------
// Assuming a pixel resolution of 320x240
// x_meters = (x_pixelcoord - 160) * NUI_CAMERA_DEPTH_IMAGE_TO_SKELETON_MULTIPLIER_320x240 * z_meters;
// y_meters = (y_pixelcoord - 120) * NUI_CAMERA_DEPTH_IMAGE_TO_SKELETON_MULTIPLIER_320x240 * z_meters;
#ifndef NUI_CAMERA_DEPTH_IMAGE_TO_SKELETON_MULTIPLIER_320x240
#define NUI_CAMERA_DEPTH_IMAGE_TO_SKELETON_MULTIPLIER_320x240 (NUI_CAMERA_DEPTH_NOMINAL_INVERSE_FOCAL_LENGTH_IN_PIXELS)
#endif

// Assuming a pixel resolution of 320x240
// x_pixelcoord = (x_meters) * NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 / z_meters + 160;
// y_pixelcoord = (y_meters) * NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 / z_meters + 120;
#ifndef NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240
#define NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 (NUI_CAMERA_DEPTH_NOMINAL_FOCAL_LENGTH_IN_PIXELS)
#endif

inline
VOID
NuiTransformSkeletonToDepthImage(
    XMVECTOR vPos,
    _Out_ LONG *plDepthX,
    _Out_ LONG *plDepthY,
    _Out_ USHORT *pusDepthValue
    )
{
    if((plDepthX == NULL) || (plDepthY == NULL) || (pusDepthValue == NULL))
    {
        return;
    }
    
    //
    // Requires a valid depth value.
    //
    
    XMFLOAT4 vPoint;
    XMStoreFloat4( &vPoint, vPos);
    if(vPoint.z > FLT_EPSILON)
    {
        //
        // Center of depth sensor is at (0,0,0) in skeleton space, and
        // and (160,120) in depth image coordinates.  Note that positive Y
        // is up in skeleton space and down in image coordinates.
        //
        
        *plDepthX = static_cast<INT>( 160 + vPoint.x * NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 / vPoint.z );
        *plDepthY = static_cast<INT>( 120 - vPoint.y * NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 / vPoint.z );
        
        //
        //  Depth is in meters in skeleton space.
        //  The depth image pixel format has depth in millimeters shifted left by 3.
        //
        
        *pusDepthValue = static_cast<USHORT>(vPoint.z*1000) << 3;
    } else
    {
        *plDepthX = 0;
        *plDepthY = 0;
        *pusDepthValue = 0;
    }
}


//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------
#define VERTS_PER_EDGE 2
#define DEPTH_WIDTH 320
#define DEPTH_HEIGHT 240
#define COLOR_WIDTH 640
#define COLOR_HEIGHT 480

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
LPDIRECT3D9                     g_pD3D = NULL; 
LPDIRECT3DDEVICE9               g_pd3dDevice = NULL; 

// Scene
LPDIRECT3DVERTEXBUFFER9         g_pVB = NULL;
LPDIRECT3DINDEXBUFFER9          g_pIB = NULL;
DWORD                           g_dwNumVertices = VERTS_PER_EDGE * VERTS_PER_EDGE;
DWORD                           g_dwNumIndices = 6 * ( VERTS_PER_EDGE - 1 ) * ( VERTS_PER_EDGE - 1 );
LPDIRECT3DVERTEXSHADER9         g_pVertexShader = NULL;
LPDIRECT3DPIXELSHADER9          g_pPixelShader = NULL;
LPDIRECT3DVERTEXSHADER9         g_pSimpleVertexShader = NULL;
LPDIRECT3DPIXELSHADER9          g_pSimplePixelShader = NULL;
LPDIRECT3DVERTEXDECLARATION9    g_pVertexDeclaration = NULL;
IDirect3DTexture9*              g_pDepthTexture = NULL;  
IDirect3DTexture9*              g_pColorTexture = NULL;  

// Double buffered data
WORD                            g_depthBuffer[2][DEPTH_WIDTH*DEPTH_HEIGHT];
WORD                            g_playerIndexBuffer[2][DEPTH_WIDTH*DEPTH_HEIGHT];
DWORD                           g_colorBuffer[2][COLOR_WIDTH*COLOR_HEIGHT];
NUI_SKELETON_FRAME              g_skeletonFrame[2];

UINT                            g_nCurDepthBuffer = 0;
UINT                            g_nCurPlayerIndexBuffer = 0;
UINT                            g_nCurColorBuffer = 0;
UINT                            g_nCurSkeletonFrame = 0;

XSTUDIO_HANDLE                  g_hProducerCallback;
XSTUDIO_HANDLE                  g_hConsumerCallback;

BOOL                            g_bInitAsProducer = FALSE;

UINT                            g_nFrameNumberColor = 0;
UINT                            g_nFrameNumberDepth = 0;
UINT                            g_nMicrosecondsSkel = 0;
UINT                            g_nMicrosecondsColor = 0;
UINT                            g_nMicrosecondsDepth = 0;

HRESULT InitApp();
VOID Update();
VOID Render();
VOID Shutdown();

struct Vertex
{
    D3DXVECTOR2 pos;
    D3DXVECTOR2 uv;
    DWORD color;
};

//-----------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D
//-----------------------------------------------------------------------------
HRESULT InitD3D( HWND hWnd )
{
    // Create the D3D object.
    if( NULL == ( g_pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) )
        return E_FAIL;

    // Set up the structure used to create the D3DDevice. 
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof( d3dpp ) );
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    // Create the D3DDevice
    if( FAILED( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                      &d3dpp, &g_pd3dDevice ) ) )
    {
        return E_FAIL;
    }

    // Setup render states
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Releases all previously initialized objects
//-----------------------------------------------------------------------------
VOID Cleanup()
{
    XStudioDisconnect();   // implicitly stops and unmaps all streams, and closes the callback handle. 

    SAFE_RELEASE( g_pVB );
    SAFE_RELEASE( g_pIB );
    SAFE_RELEASE( g_pVertexShader );
    SAFE_RELEASE( g_pPixelShader );
    SAFE_RELEASE( g_pSimpleVertexShader );
    SAFE_RELEASE( g_pSimplePixelShader );
    SAFE_RELEASE( g_pVertexDeclaration );
    SAFE_RELEASE( g_pDepthTexture );
    SAFE_RELEASE( g_pColorTexture );
    SAFE_RELEASE( g_pd3dDevice );
    SAFE_RELEASE( g_pD3D );
}


//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------
LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_DESTROY:
            Cleanup();
            PostQuitMessage( 0 );
            return 0;
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}


//-----------------------------------------------------------------------------
// Name: ConsumerFunc()
// Desc: Receives and handles XStudio events.  The amount of processing done in
//       this callback should be minimal as this callback is synchronous and
//       can affect performance if excessive processing is done.
//-----------------------------------------------------------------------------
HRESULT CALLBACK ConsumerFunc( XSTUDIO_STREAM_EVENT* pEvent, VOID* pContext )
{
    pContext;       // Unused

    if( pEvent->StreamID == XSTUDIO_STREAM_ID_NUIAPI_SKELETON)
    {
        assert( pEvent->cbDataBufferUsed == sizeof(NUI_SKELETON_FRAME) );

        NUI_SKELETON_FRAME* pFrame = (NUI_SKELETON_FRAME*)(pEvent->pvDataBuffer);
        memcpy( &g_skeletonFrame[(g_nCurSkeletonFrame+1)%2], pFrame, sizeof(NUI_SKELETON_FRAME) );
        g_nCurSkeletonFrame = (g_nCurSkeletonFrame+1)%2;
    }
    else if( pEvent->StreamID == XSTUDIO_STREAM_ID_NUICAM_COLOR)
    {
        assert( pEvent->cbDataBufferUsed == COLOR_WIDTH*COLOR_HEIGHT*sizeof(DWORD) &&
                pEvent->cbIndexBufferUsed == sizeof(XSTUDIO_NUICAM_IMAGE_INDEX_DATA) );

        XSTUDIO_NUICAM_IMAGE_INDEX_DATA* pIndexBuffer = NULL;
        pIndexBuffer = (XSTUDIO_NUICAM_IMAGE_INDEX_DATA*)pEvent->pvIndexBuffer;
        
        assert( pIndexBuffer->Height == COLOR_HEIGHT &&
                pIndexBuffer->Width == COLOR_WIDTH &&
                pIndexBuffer->ImageType == NUI_IMAGE_TYPE_COLOR );

        DWORD* pBuffer = (DWORD*)(pEvent->pvDataBuffer);
        memcpy(g_colorBuffer[(g_nCurColorBuffer+1)%2], pBuffer, COLOR_WIDTH*COLOR_HEIGHT*sizeof(DWORD));
        g_nCurColorBuffer = (g_nCurColorBuffer+1)%2;
    }
    else if( pEvent->StreamID == XSTUDIO_STREAM_ID_NUICAM_DEPTH )
    {
        assert( pEvent->cbDataBufferUsed == DEPTH_WIDTH*DEPTH_HEIGHT*sizeof(WORD) &&
                pEvent->cbIndexBufferUsed == sizeof(XSTUDIO_NUICAM_IMAGE_INDEX_DATA) );

        XSTUDIO_NUICAM_IMAGE_INDEX_DATA* pIndexBuffer = NULL;
        pIndexBuffer = (XSTUDIO_NUICAM_IMAGE_INDEX_DATA*)pEvent->pvIndexBuffer;
        
        assert( pIndexBuffer->Height == DEPTH_HEIGHT &&
                pIndexBuffer->Width == DEPTH_WIDTH &&
                pIndexBuffer->ImageType == NUI_IMAGE_TYPE_DEPTH );

        WORD* pDepthBuffer = (WORD*)(pEvent->pvDataBuffer);
        memcpy(g_depthBuffer[(g_nCurDepthBuffer+1)%2], pDepthBuffer, DEPTH_WIDTH*DEPTH_HEIGHT*sizeof(WORD));
        g_nCurDepthBuffer = (g_nCurDepthBuffer+1)%2;
    }
    else if( pEvent->StreamID == XSTUDIO_STREAM_ID_NUIAPI_PLAYER_INDEX )
    {
        assert( pEvent->cbDataBufferUsed == DEPTH_WIDTH*DEPTH_HEIGHT*sizeof(WORD) );

        XSTUDIO_NUICAM_IMAGE_INDEX_DATA* pIndexBuffer = NULL;
        pIndexBuffer = (XSTUDIO_NUICAM_IMAGE_INDEX_DATA*)pEvent->pvIndexBuffer;
        
        assert( pIndexBuffer->Height == DEPTH_HEIGHT &&
                pIndexBuffer->Width == DEPTH_WIDTH );

        WORD* pPlayerIndexBuffer = (WORD*)(pEvent->pvDataBuffer);
        memcpy(g_playerIndexBuffer[(g_nCurPlayerIndexBuffer+1)%2], pPlayerIndexBuffer, DEPTH_WIDTH*DEPTH_HEIGHT*sizeof(WORD));
        g_nCurPlayerIndexBuffer = (g_nCurPlayerIndexBuffer+1)%2;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: ProducerFunc()
// Desc: Produce data for the NUI streams to send to the devkit.  To demonstrate 
//       the functionality of a producer callback, we create some simple debugging data.
//-----------------------------------------------------------------------------
HRESULT CALLBACK ProducerFunc( XSTUDIO_STREAM_EVENT* pEvent, VOID* pContext )
{
    pContext;  // Unused

    if( pEvent->StreamID == XSTUDIO_STREAM_ID_NUIAPI_SKELETON)
    {
        assert( pEvent->cbDataBufferSize >= sizeof(NUI_SKELETON_FRAME) );

        NUI_SKELETON_FRAME* pFrame = (NUI_SKELETON_FRAME*)(pEvent->pvDataBuffer);

        ZeroMemory(pFrame, sizeof(NUI_SKELETON_FRAME));
        for(UINT i = 0; i < NUI_SKELETON_COUNT; i++)
        {
            pFrame->SkeletonData[i].eTrackingState = NUI_SKELETON_NOT_TRACKED;
        }

        pFrame->SkeletonData[0].eTrackingState = NUI_SKELETON_TRACKED;
        pFrame->SkeletonData[0].dwTrackingID   = 1;

        for( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
        {
            pFrame->SkeletonData[0].SkeletonPositions[i] = XMVectorSet((FLOAT)i/10.0f,(FLOAT)i/10.0f, 1.0f,1);
            pFrame->SkeletonData[0].eSkeletonPositionTrackingState[i] = NUI_SKELETON_POSITION_TRACKED;
        }

        pEvent->cbDataBufferUsed = sizeof(NUI_SKELETON_FRAME);
        pEvent->uEventMicroseconds = g_nMicrosecondsSkel;
        g_nMicrosecondsSkel += 33;  // Assume these are requested at 30 Hz
        pEvent->cbIndexBufferUsed = 0;

        memcpy( &g_skeletonFrame[(g_nCurSkeletonFrame+1)%2], pFrame, sizeof(NUI_SKELETON_FRAME) );
        g_nCurSkeletonFrame = (g_nCurSkeletonFrame+1)%2;
    }
    else if( pEvent->StreamID == XSTUDIO_STREAM_ID_NUICAM_COLOR)
    {
        assert( pEvent->cbDataBufferSize >= COLOR_WIDTH*COLOR_HEIGHT*sizeof(DWORD) ) ;

        DWORD* pBuffer = (DWORD*)(pEvent->pvDataBuffer);
        ZeroMemory(pBuffer, COLOR_WIDTH*COLOR_HEIGHT*sizeof(DWORD));
        for(int i = 0; i < COLOR_HEIGHT; i++)
        {
            for(int j = 0; j < COLOR_WIDTH; j++)
            {
                pBuffer[i*COLOR_WIDTH+j] = D3DCOLOR_ARGB(255,i*255/COLOR_HEIGHT,j*255/COLOR_WIDTH,255);
            }
        }

        memcpy(g_colorBuffer[(g_nCurColorBuffer+1)%2], pEvent->pvDataBuffer, COLOR_WIDTH*COLOR_HEIGHT*sizeof(DWORD));
        g_nCurColorBuffer = (g_nCurColorBuffer+1)%2;

        pEvent->cbDataBufferUsed = COLOR_WIDTH*COLOR_HEIGHT*sizeof(DWORD);
        pEvent->uEventMicroseconds = g_nMicrosecondsColor;
        g_nMicrosecondsColor += 33;  // Assume these are requested at 30 Hz

        XSTUDIO_NUICAM_IMAGE_INDEX_DATA* pIndexBuffer = (XSTUDIO_NUICAM_IMAGE_INDEX_DATA*)pEvent->pvIndexBuffer;
        pIndexBuffer->Width = COLOR_WIDTH;
        pIndexBuffer->Height = COLOR_HEIGHT;
        pIndexBuffer->ImageType = NUI_IMAGE_TYPE_COLOR;
        pIndexBuffer->FrameNumber = g_nFrameNumberColor++;
    	pEvent->cbIndexBufferUsed = sizeof(XSTUDIO_NUICAM_IMAGE_INDEX_DATA);

    }
    else if( pEvent->StreamID == XSTUDIO_STREAM_ID_NUICAM_DEPTH )
    {
        assert( pEvent->cbDataBufferSize >= DEPTH_WIDTH*DEPTH_HEIGHT*sizeof(WORD) ) ;

        WORD* pDepthBuffer = (WORD*)(pEvent->pvDataBuffer);
        
        ZeroMemory(pDepthBuffer, DEPTH_WIDTH*DEPTH_HEIGHT*sizeof(WORD));
        for(int i = 0; i < DEPTH_HEIGHT; i++)
        {
            for(int j = 0; j < DEPTH_WIDTH; j++)
            {
                // Multiply by 8 for abetter visual range
                // Shift left by 3 bits to give room for the player index data
                *pDepthBuffer = (WORD)(i*8) << 3;
                pDepthBuffer++;
            }
        }
        
        memcpy(g_depthBuffer[(g_nCurDepthBuffer+1)%2], pEvent->pvDataBuffer, DEPTH_WIDTH*DEPTH_HEIGHT*sizeof(WORD));
        g_nCurDepthBuffer = (g_nCurDepthBuffer+1)%2;

        pEvent->cbDataBufferUsed = DEPTH_WIDTH*DEPTH_HEIGHT*sizeof(WORD);
        pEvent->uEventMicroseconds = g_nMicrosecondsDepth;
        g_nMicrosecondsDepth += 33;  // Assume these are requested at 30 Hz

        XSTUDIO_NUICAM_IMAGE_INDEX_DATA* pIndexBuffer = (XSTUDIO_NUICAM_IMAGE_INDEX_DATA*)pEvent->pvIndexBuffer;
        pIndexBuffer->Width = DEPTH_WIDTH;
        pIndexBuffer->Height = DEPTH_HEIGHT;
        pIndexBuffer->ImageType = NUI_IMAGE_TYPE_DEPTH;
        pIndexBuffer->FrameNumber = g_nFrameNumberDepth++;
        pEvent->cbIndexBufferUsed = sizeof(XSTUDIO_NUICAM_IMAGE_INDEX_DATA);
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR cmdLine, INT )
{
    // set up the environment path so that xstudio.dll can be delay loaded correctly 
    char* path;
    _dupenv_s( &path, NULL, "path" );

    char* xedkDir;
    _dupenv_s( &xedkDir, NULL, "xedk" );

    if( !xedkDir )
        xedkDir = "";
    std::string newpath = "path=" + std::string( path ) + ";" + std::string( xedkDir ) + "\\bin\\win32";
    _putenv( newpath.c_str() );

    // Register the window class
    WNDCLASSEX wc =
    {
        sizeof( WNDCLASSEX ), CS_CLASSDC, MsgProc, 0L, 0L,
        GetModuleHandle( NULL ), NULL, NULL, NULL, NULL,
        L"NUI PC Conduit Sample", NULL
    };
    RegisterClassEx( &wc );

    // Create the application's window
    HWND hWnd = CreateWindow( L"NUI PC Conduit Sample", L"NUI PC Conduit Sample",
                              WS_OVERLAPPEDWINDOW, 100, 100, 740, 480,
                              NULL, NULL, wc.hInstance, NULL );

    if( _wcsicmp(cmdLine, L"-producer" ) == 0 )
    {
        g_bInitAsProducer = TRUE;
    }
    else
    {
        g_bInitAsProducer = FALSE;
    }

    // Initialize Direct3D
    if( SUCCEEDED( InitD3D( hWnd ) ) )
    {
        // Initialize the application
        if( SUCCEEDED( InitApp() ) )
        {
            // Show the window
            ShowWindow( hWnd, SW_SHOWDEFAULT );
            UpdateWindow( hWnd );

            // Enter the message loop
            MSG msg;
            ZeroMemory( &msg, sizeof( msg ) );
            while( msg.message != WM_QUIT )
            {
                if( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) )
                {
                    TranslateMessage( &msg );
                    DispatchMessage( &msg );
                }
                else
                {
                    Update();
                    Render();
                }
            }
        }
    }

    UnregisterClass( L"NUI PC Conduit Sample", wc.hInstance );
    return 0;
}


//--------------------------------------------------------------------------------------
// Initialize XStudio and create the NUI-to-PC Conduit
//--------------------------------------------------------------------------------------
HRESULT InitXStudio( BOOL bProducer )
{

    HRESULT hr = S_OK;

    // Connect XStudio over USB.
    hr = XStudioConnect( NULL, XSTUDIO_CONNECTION_TYPE_USB );
    if( FAILED(hr) )
    {
        // If USB fails, try a TCP connection.  TCP has lower bandwidth, so some data loss may occur.
        hr = XStudioConnect( NULL, XSTUDIO_CONNECTION_TYPE_TCP );
        if( FAILED(hr) )
        {
            MessageBox(NULL, L"XStudio connection failed.  For a USB connection, make sure your USB Easy Transfer cable is connnected to both the PC and the dev kit.  For a TCP connection, make sure a USB Easy Transfer Cable is not connected to the dev kit.", 
                              NULL, MB_OK | MB_ICONWARNING );
            return hr;
        }
    }

    if( bProducer )
    {
        // Produce skeleton, color, and depth.  Producing player index data is not supported at this time.
        XSTUDIO_STREAM_FLAGS streamFlags = XSTUDIO_STREAM_FLAG_NUIAPI_SKELETON | XSTUDIO_STREAM_FLAG_NUICAM_DEPTH | XSTUDIO_STREAM_FLAG_NUICAM_COLOR ;
        hr = XStudioRegisterStreamCallback( ProducerFunc, XSTUDIO_STREAM_CALLBACK_REGISTRATION_FLAG_PRODUCER, NULL, &g_hProducerCallback );
        if( FAILED(hr) )
        {
            MessageBox(NULL, L"Stream callback registration failed", NULL, MB_OK | MB_ICONWARNING );
            return hr;
        }
        hr = XStudioMapStreams( streamFlags, g_hProducerCallback );
        if( FAILED(hr) )
        {
            MessageBox(NULL, L"XStudioMapStreams failed", NULL, MB_OK | MB_ICONWARNING );
            return hr;
        }
        hr = XStudioStart( streamFlags );
        if( FAILED(hr) )
        {
            MessageBox(NULL, L"XStudioStart failed", NULL, MB_OK | MB_ICONWARNING );
            return hr;
        }
    }
    else
    {
        XSTUDIO_STREAM_FLAGS streamFlags = XSTUDIO_STREAM_FLAG_NUIAPI_SKELETON | XSTUDIO_STREAM_FLAG_NUICAM_DEPTH | XSTUDIO_STREAM_FLAG_NUICAM_COLOR | XSTUDIO_STREAM_FLAG_NUIAPI_PLAYER_INDEX ;
        hr = XStudioRegisterStreamCallback( ConsumerFunc, XSTUDIO_STREAM_CALLBACK_REGISTRATION_FLAG_CONSUMER, NULL, &g_hConsumerCallback );
        if( FAILED(hr) )
        {
            MessageBox(NULL, L"Stream callback registration failed", NULL, MB_OK | MB_ICONWARNING );
            return hr;
        }

        hr = XStudioMapStreams( streamFlags, g_hConsumerCallback );
        if( FAILED(hr) )
        {
            MessageBox(NULL, L"XStudioMapStreams failed", NULL, MB_OK | MB_ICONWARNING );
            return hr;
        }
        hr = XStudioStart( streamFlags );
        if( FAILED(hr) )
        {
            MessageBox(NULL, L"XStudioStart failed", NULL, MB_OK | MB_ICONWARNING );
            return hr;
        }
    }


    return hr;
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
HRESULT InitApp()
{
    HRESULT hr = InitXStudio( g_bInitAsProducer );

    // Create vertex declaration
    WCHAR strPath[512];
    LPD3DXBUFFER pCode;

    D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        D3DDECL_END()
    };

    V_RETURN( g_pd3dDevice->CreateVertexDeclaration( decl, &g_pVertexDeclaration ) );

    V_RETURN( D3DXCreateTexture( g_pd3dDevice, DEPTH_WIDTH, DEPTH_HEIGHT, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_pDepthTexture ) );
    V_RETURN( D3DXCreateTexture( g_pd3dDevice, COLOR_WIDTH, COLOR_HEIGHT, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_pColorTexture ) );

    // Create pixel shader
    swprintf_s( strPath, L".\\NUIPCConduit.hlsl", 512);
    LPD3DXBUFFER errorMsgs;
    DWORD dwShaderFlags = 0;
    hr = ( D3DXCompileShaderFromFile( strPath, NULL, NULL, "PS",
                                         "ps_2_0", dwShaderFlags, &pCode,
                                         &errorMsgs, NULL ) );
    if( FAILED( hr ) )
        return hr;

    // Create the vertex shader
    hr = g_pd3dDevice->CreatePixelShader( ( DWORD* )pCode->GetBufferPointer(),
                                         &g_pPixelShader );
    pCode->Release();
    if( FAILED( hr ) )
        return hr;

    V_RETURN( D3DXCompileShaderFromFile( strPath, NULL, NULL, "SimplePS",
                                         "ps_2_0", dwShaderFlags, &pCode,
                                         &errorMsgs, NULL ) );
    // Create the vertex shader
    hr = g_pd3dDevice->CreatePixelShader( ( DWORD* )pCode->GetBufferPointer(),
                                         &g_pSimplePixelShader );
    pCode->Release();
    if( FAILED( hr ) )
        return hr;

    // Assemble the vertex shader from the file
    V_RETURN( D3DXCompileShaderFromFile( strPath, NULL, NULL, "VS",
                                         "vs_2_0", dwShaderFlags, &pCode,
                                         NULL, NULL ) );

    // Create the vertex shader
    hr = g_pd3dDevice->CreateVertexShader( ( DWORD* )pCode->GetBufferPointer(),
                                         &g_pVertexShader );
    pCode->Release();
    if( FAILED( hr ) )
        return hr;

    // Assemble the vertex shader from the file
    V_RETURN( D3DXCompileShaderFromFile( strPath, NULL, NULL, "SimpleVS",
                                         "vs_2_0", dwShaderFlags, &pCode,
                                         NULL, NULL ));

    // Create the vertex shader
    hr = g_pd3dDevice->CreateVertexShader( ( DWORD* )pCode->GetBufferPointer(),
                                         &g_pSimpleVertexShader );
    pCode->Release();
    if( FAILED( hr ) )
        return hr;

    // Create and initialize index buffer
    WORD* pIndices;

    V_RETURN( g_pd3dDevice->CreateIndexBuffer( g_dwNumIndices * sizeof( WORD ),
                                             D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                             D3DPOOL_DEFAULT, &g_pIB, NULL ) );

    V_RETURN( g_pIB->Lock( 0, 0, ( VOID** )&pIndices, 0 ) );

    DWORD y;
    for( y = 1; y < VERTS_PER_EDGE; y++ )
    {
        for( DWORD x = 1; x < VERTS_PER_EDGE; x++ )
        {
            *pIndices++ = ( WORD )( ( y - 1 ) * VERTS_PER_EDGE + ( x - 1 ) );
            *pIndices++ = ( WORD )( ( y - 0 ) * VERTS_PER_EDGE + ( x - 1 ) );
            *pIndices++ = ( WORD )( ( y - 1 ) * VERTS_PER_EDGE + ( x - 0 ) );

            *pIndices++ = ( WORD )( ( y - 1 ) * VERTS_PER_EDGE + ( x - 0 ) );
            *pIndices++ = ( WORD )( ( y - 0 ) * VERTS_PER_EDGE + ( x - 1 ) );
            *pIndices++ = ( WORD )( ( y - 0 ) * VERTS_PER_EDGE + ( x - 0 ) );
        }
    }

    V_RETURN( g_pIB->Unlock() );

    // Create and initialize vertex buffer
    V_RETURN( g_pd3dDevice->CreateVertexBuffer( g_dwNumVertices * sizeof( Vertex ),
                                              D3DUSAGE_WRITEONLY, 0,
                                              D3DPOOL_DEFAULT, &g_pVB, NULL ) );

    Vertex* pVertices;
    V_RETURN( g_pVB->Lock( 0, 0, ( VOID** )&pVertices, 0 ) );

    for( y = 0; y < VERTS_PER_EDGE; y++ )
    {
        for( DWORD x = 0; x < VERTS_PER_EDGE; x++ )
        {
            pVertices->pos = D3DXVECTOR2( ( ( FLOAT )x / ( FLOAT )( VERTS_PER_EDGE - 1 ) - 0.5f ),                                  
                                        (( ( FLOAT )y / ( FLOAT )( VERTS_PER_EDGE - 1 ) - 0.5f)  * DEPTH_HEIGHT/DEPTH_WIDTH ));     
            pVertices->uv = D3DXVECTOR2(( 1.0f - ( FLOAT )x / ( FLOAT )( VERTS_PER_EDGE - 1 ) ),                                    
                                        ( 1.0f - ( FLOAT )y / ( FLOAT )( VERTS_PER_EDGE - 1 ) ));                                  
            pVertices++;

        }
    }

    V_RETURN( hr = g_pVB->Unlock() );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Update()
//
// Update the scene 
//--------------------------------------------------------------------------------------
void Update()
{
    // Set up the vertex shader constants
    D3DXMATRIXA16 mWorldViewProj;
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;

    D3DLOCKED_RECT lockedRect;

    D3DSURFACE_DESC desc;
    if( g_pDepthTexture)
    {
        g_pDepthTexture->GetLevelDesc(0, &desc);
        g_pDepthTexture->LockRect(0, &lockedRect, NULL, 0 );
        for( UINT i = 0; i < desc.Height; i++ )
        {
            DWORD* row = ((DWORD*)((BYTE*)lockedRect.pBits+i*lockedRect.Pitch));
            for( UINT j = 0; j < desc.Width; j++ )
            {
                DWORD* pixel = &(row[j]);
                BYTE value = (BYTE)(g_depthBuffer[g_nCurDepthBuffer][desc.Width - j+i*desc.Width] / 128 );
                     
                
                WORD playerIndex = g_playerIndexBuffer[g_nCurPlayerIndexBuffer][desc.Width - j+i*desc.Width];
                if( playerIndex == 0)
                {
                    *pixel = D3DCOLOR_ARGB(255, value, value, value);
                }
                else
                {
                    // If there is a player index available, tint the depth map based on the index
                    BYTE red[7]   = { 255,   0,   0, 255,   0, 128, 255 };
                    BYTE green[7] = {   0, 255,   0, 255, 255, 255, 128 };
                    BYTE blue[7]  = {   0,   0, 255,   0, 255,   0,   0 };
                   
                    if( playerIndex > 6 )
                    {
                        playerIndex = 6;
                    }

                    *pixel = D3DCOLOR_ARGB( 255, (value + red[playerIndex])/2, (value + green[playerIndex])/2, (value + blue[playerIndex])/2 );
                }
            }
        }
        g_pDepthTexture->UnlockRect(0);
    }

    if( g_pColorTexture)
    {
        g_pColorTexture->GetLevelDesc(0, &desc);
        g_pColorTexture->LockRect(0, &lockedRect, NULL, 0 );
        for( UINT i = 0; i < desc.Height; i++)
        {
            DWORD* row = ((DWORD*)((BYTE*)lockedRect.pBits+i*lockedRect.Pitch));
            for( UINT j = 0; j < desc.Width; j++)
            {
                DWORD* pixel = &(row[j]);
                *pixel = g_colorBuffer[g_nCurColorBuffer][desc.Width - j+i*desc.Width];
            }
        }
        g_pColorTexture->UnlockRect(0);
    }

}


//--------------------------------------------------------------------------------------
// Render()
//--------------------------------------------------------------------------------------
void Render()
{
    // Clear the render target and the zbuffer 
    g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(255,104,109,119), 1.0f, 0 );

    // Render the scene
    if( SUCCEEDED( g_pd3dDevice->BeginScene() ) )
    {
        g_pd3dDevice->SetVertexDeclaration( g_pVertexDeclaration );
        g_pd3dDevice->SetVertexShader( g_pVertexShader );
        g_pd3dDevice->SetPixelShader( g_pPixelShader );
        g_pd3dDevice->SetTexture(0, g_pColorTexture );
        g_pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof( Vertex ) );
        g_pd3dDevice->SetIndices( g_pIB );

        D3DXMATRIX mWorld, mProj, mWorldViewProj, mView;
    
        //Set up the view matrix
        D3DXVECTOR3 eyePt(0.0f, 0.0f, -1.0f);
        D3DXVECTOR3 lookAtPt(0.0f, 0.0f, 0.0f);
        D3DXVECTOR3 vUp(0.0f, 1.0f, 0.0f);
        D3DXMatrixLookAtLH(&mView, &eyePt, &lookAtPt, &vUp);

        // Set up the projection matrix
        IDirect3DSurface9* pBackBuffer;
        g_pd3dDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
        D3DSURFACE_DESC backBufferSurfaceDesc;
        pBackBuffer->GetDesc(&backBufferSurfaceDesc);
        FLOAT fAspectRatio = backBufferSurfaceDesc.Width / ( FLOAT )backBufferSurfaceDesc.Height;
        pBackBuffer->Release();
        D3DXMatrixPerspectiveFovLH( &mProj, D3DX_PI / 4, fAspectRatio, 1.0f, 1000.0f );
        
        // Draw the Color Buffer
        D3DXMatrixIdentity(&mWorld);
        D3DXMatrixTranslation(&mWorld, -.55f, 0.0f, 1.0f);  // Color buffer is rendered on the left side of the view
        mWorldViewProj = mWorld * mView * mProj;
        g_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&mWorldViewProj, 4);

        g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, g_dwNumVertices,
                                             0, g_dwNumIndices / 3 ) ;

        // Draw the Depth buffer
        D3DXMatrixTranslation(&mWorld, .55f, 0.0f, 1.0f);
        mWorldViewProj = mWorld * mView * mProj;
        g_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&mWorldViewProj, 4);
        g_pd3dDevice->SetTexture(0, g_pDepthTexture );

        g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, g_dwNumVertices,
                                             0, g_dwNumIndices / 3 ) ;

 
        // Draw the skeleton
        g_pd3dDevice->SetVertexShader( g_pSimpleVertexShader );
        g_pd3dDevice->SetPixelShader( g_pSimplePixelShader );
        g_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&mWorldViewProj, 4);

        const UINT nNumBones = 19;
        Vertex verts[nNumBones*2];

        const NUI_SKELETON_POSITION_INDEX bones[nNumBones][2] = {
                                            {NUI_SKELETON_POSITION_HIP_CENTER,      NUI_SKELETON_POSITION_SPINE             },
                                            {NUI_SKELETON_POSITION_SPINE,           NUI_SKELETON_POSITION_SHOULDER_CENTER   },
                                            {NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_HEAD              },
                                            {NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT     },
                                            {NUI_SKELETON_POSITION_SHOULDER_LEFT,   NUI_SKELETON_POSITION_ELBOW_LEFT        },
                                            {NUI_SKELETON_POSITION_ELBOW_LEFT,      NUI_SKELETON_POSITION_WRIST_LEFT        },
                                            {NUI_SKELETON_POSITION_WRIST_LEFT,      NUI_SKELETON_POSITION_HAND_LEFT         },
                                            {NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT    },
                                            {NUI_SKELETON_POSITION_SHOULDER_RIGHT,  NUI_SKELETON_POSITION_ELBOW_RIGHT       },
                                            {NUI_SKELETON_POSITION_ELBOW_RIGHT,     NUI_SKELETON_POSITION_WRIST_RIGHT       },
                                            {NUI_SKELETON_POSITION_WRIST_RIGHT,     NUI_SKELETON_POSITION_HAND_RIGHT        },
                                            {NUI_SKELETON_POSITION_HIP_CENTER,      NUI_SKELETON_POSITION_HIP_LEFT          },
                                            {NUI_SKELETON_POSITION_HIP_LEFT,        NUI_SKELETON_POSITION_KNEE_LEFT         },
                                            {NUI_SKELETON_POSITION_KNEE_LEFT,       NUI_SKELETON_POSITION_ANKLE_LEFT        },
                                            {NUI_SKELETON_POSITION_ANKLE_LEFT,      NUI_SKELETON_POSITION_FOOT_LEFT         },
                                            {NUI_SKELETON_POSITION_HIP_CENTER,      NUI_SKELETON_POSITION_HIP_RIGHT         },
                                            {NUI_SKELETON_POSITION_HIP_RIGHT,       NUI_SKELETON_POSITION_KNEE_RIGHT        },
                                            {NUI_SKELETON_POSITION_KNEE_RIGHT,      NUI_SKELETON_POSITION_ANKLE_RIGHT       },
                                            {NUI_SKELETON_POSITION_ANKLE_RIGHT,     NUI_SKELETON_POSITION_FOOT_RIGHT        } };


        for( UINT skelIndex = 0; skelIndex < NUI_SKELETON_COUNT; skelIndex++)
        {
            UINT nNumBonesToDraw = 0;
            if( g_skeletonFrame[g_nCurSkeletonFrame].SkeletonData[skelIndex].eTrackingState == NUI_SKELETON_TRACKED )
            {
                for( UINT i = 0; i < nNumBones; i++)
                {
                    NUI_SKELETON_POSITION_INDEX startJointIndex = bones[i][0];
                    NUI_SKELETON_POSITION_INDEX endJointIndex = bones[i][1];
                    
                    const DWORD dwTrackedColor = 0xFF00FF00;
                    const DWORD dwInferredColor = 0xFFFF0000;

                    // Determine the color of the end points of the bone
                    DWORD dwStartColor;
                    if( g_skeletonFrame[g_nCurSkeletonFrame].SkeletonData[skelIndex].eSkeletonPositionTrackingState[startJointIndex] == NUI_SKELETON_POSITION_TRACKED )
                    {
                        dwStartColor = dwTrackedColor;
                    }
                    else if(  g_skeletonFrame[g_nCurSkeletonFrame].SkeletonData[skelIndex].eSkeletonPositionTrackingState[startJointIndex] == NUI_SKELETON_POSITION_INFERRED )
                    {
                        dwStartColor = dwInferredColor;
                    }
                    else
                    {
                        continue;       // Not tracked, so don't draw the bone.
                    }

                    DWORD dwEndColor;
                    if( g_skeletonFrame[g_nCurSkeletonFrame].SkeletonData[skelIndex].eSkeletonPositionTrackingState[endJointIndex] == NUI_SKELETON_POSITION_TRACKED )
                    {
                        dwEndColor = dwTrackedColor;
                    }
                    else if(  g_skeletonFrame[g_nCurSkeletonFrame].SkeletonData[skelIndex].eSkeletonPositionTrackingState[endJointIndex] == NUI_SKELETON_POSITION_INFERRED )
                    {
                        dwEndColor = dwInferredColor;
                    }
                    else
                    {
                        continue;       // Not tracked, so don't draw the bone.
                    }
                       
                    // Transform the joint position to the depth image
                    LONG x, y;
                    USHORT z;
                    NuiTransformSkeletonToDepthImage(g_skeletonFrame[g_nCurSkeletonFrame].SkeletonData[skelIndex].SkeletonPositions[startJointIndex], &x,&y,&z);
                   
                    // Scale the position to the [-.5,.5] range to match the depth image
                    XMFLOAT4 startJoint, endJoint;
                    startJoint = XMFLOAT4( ((FLOAT)x)/DEPTH_WIDTH - .5f, ((FLOAT)y)/DEPTH_WIDTH - (.5f*DEPTH_HEIGHT/DEPTH_WIDTH),z,1.0f);
                    NuiTransformSkeletonToDepthImage(g_skeletonFrame[g_nCurSkeletonFrame].SkeletonData[skelIndex].SkeletonPositions[endJointIndex], &x,&y,&z);
                    endJoint = XMFLOAT4( ((FLOAT)x)/DEPTH_WIDTH - .5f, ((FLOAT)y)/DEPTH_WIDTH - (.5f*DEPTH_HEIGHT/DEPTH_WIDTH),z,1.0f);
                    
                    // Add the verts
                    verts[nNumBonesToDraw*2].pos = D3DXVECTOR2(startJoint.x, -startJoint.y);
                    verts[nNumBonesToDraw*2].color = dwStartColor;
                    verts[nNumBonesToDraw*2+1].pos = D3DXVECTOR2(endJoint.x, -endJoint.y);
                    verts[nNumBonesToDraw*2+1].color = dwEndColor;

                    nNumBonesToDraw++;
                }

                // Draw bones
                g_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, nNumBonesToDraw, verts, sizeof(Vertex));
            }
        }

         g_pd3dDevice->EndScene();
    }

    g_pd3dDevice->Present(NULL,NULL,NULL,NULL);
}
