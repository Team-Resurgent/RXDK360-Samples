//--------------------------------------------------------------------------------------
// AdvancedSegmentation.cpp
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xbdm.h>
#include <AtgApp.h>
#include <AtgHelp.h>
#include <AtgUtil.h>
#include <AtgInput.h>
#include <AtgNuiCommon.h>

#include <NuiApi.h>

#include "DepthMap.h"
#include "ColorMap.h"
#include "BinaryMap.h"
#include "IntensityMap.h"
#include "CumulativeMovingAverageBuffer.h"
#include "SegmentationProcessingProxy.h"

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------

ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Change depth display mode down" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"Change depth display mode up" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle color display full screen" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_1, L"Change color display mode" },
    { ATG::HELP_BOTTOM_CENTER,  ATG::HELP_PLACEMENT_1, L"Advanced Segmentation" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Useful constants
//--------------------------------------------------------------------------------------

#define STACK_SIZE			16384

//--------------------------------------------------------------------------------------
// Color display enum identifying strings
//--------------------------------------------------------------------------------------
const WCHAR* g_ColorDisplayNames[COLOR_DISPLAY_MAX] =
{
    L"0 Original Color Buffer",
    L"1 Select From Current Depth",
    L"2 Original Intensity",
    L"3 Sobel Original Intensity",
    L"4 CMA Color Buffer",
    L"5 Sobel CMA Color Buffer",
	L"6 Difference of Sobels",
    L"7 Final Segmentation"
};

//--------------------------------------------------------------------------------------
// Depth display enum identifying strings
//--------------------------------------------------------------------------------------
const WCHAR* g_DepthDisplayNames[DEPTH_DISPLAY_MAX] =
{
    L"0 Original Depth Map",
    L"1 ST Segmentation Mask",
    L"2 ST Segmentation Mask Erode",
    L"3 ST Segmentation Mask Dilate",
    L"4 Definite Foreground Pixels",
    L"5 Definite background Pixels",
    L"6 Pixels Containing Silhouette",
};

//--------------------------------------------------------------------------------------
// Rect used for displaying the color and depth stream
//--------------------------------------------------------------------------------------

typedef struct
{
    FLOAT fX;
    FLOAT fY;
    FLOAT fWidth;
    FLOAT fHeight;
}
Rect;


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // Standard sample components
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;

    BOOL m_bDrawHelp;                       // Drawing help screen
    BOOL m_bColorFullScreen;                // Drawing color buffer full screen

    Rect m_ColorWindow;                // Rects for the textured quads
    Rect m_FullColorWindow;
    Rect m_DepthWindow;

    ColorDisplayType m_ColorDisplayMode;    // Color map display mode
    DepthDisplayType m_DepthDisplayMode;    // Depth map display mode

    HANDLE m_hDepthStream;          // Depth stream from NUI
	HANDLE m_hNUIDepthFrameReadyEvent;
	HANDLE m_hDepthThread;

    HANDLE m_hColorStream;          // Color stream from NUI
	HANDLE m_hNUIColorFrameReadyEvent;
	HANDLE m_hColorThread;

	HANDLE m_hFrameProcessingDoneEvents[2];
	HANDLE m_hSegmentThread;
	HANDLE m_hSegmentationCompleteEvent;

    DWORD m_DepthMapFrame;		     // Timestamp for last received depth map
    DWORD m_ColorMapFrame;			 // Timestamp for last received color map

    IDirect3DVertexBuffer9* m_pColorVB;                 // Color stream vertices
    IDirect3DVertexBuffer9* m_pFullScreenColorVB;       // Full screen color stream vertices
    IDirect3DVertexBuffer9* m_pDepthVB;                 // Depth buffer vertices
    IDirect3DVertexShader9* m_pVideoVertexShader;       // Vert shader for textured quads
    IDirect3DPixelShader9* m_pVideoPixelShaderRGB;      // RGB pass through shader
    IDirect3DVertexDeclaration9* m_pVideoVertexDecl;    // Vertex decl for quads

    IDirect3DTexture9* m_pDepthTexture[2];         // D3D depth texture, double buffered
    IDirect3DTexture9* m_pColorTexture[2];         // D3D color texture, double buffered
	DWORD m_TextureBuffer;                         // Which buffer to use

	// Fast and unreadable vs slow and understandable image processing paths
	ClearSegmentationProcessingProxy* mClearProcessingProxy;
	FastSegmentationProcessingProxy*  mFastProcessingProxy;

	// Current image processing path
	SegmentationProcessingProxy* m_pProcessingProxy;

	
	
private:
    HRESULT Initialize();
    HRESULT InitializeVisualization( D3DDevice* pd3dDevice );

    HRESULT Update();
    HRESULT Render();

    HRESULT StartupCamera();
    VOID    GetColorFrame();                // Get the latest color frame from NUI
    VOID    GetDepthFrame();                // Get the latest depth frame from NUI
    VOID    SetupDepthDisplayTexture();     // Copy selected map to depth texture for display
    VOID    SetupColorDisplayTexture();     // Copy selected map to color texture for display
    VOID    Segment();                      // Actually segment the image

	static DWORD DepthHandlingThread(LPVOID parameter);
	static DWORD ColorHandlingThread(LPVOID parameter);
	static DWORD SegmentThread(LPVOID parameter);
};


//--------------------------------------------------------------------------------------
// Useful helper for translating from __mftb32() values to milliseconds
//--------------------------------------------------------------------------------------

inline FLOAT TicksToMilliseconds( const DWORD ticks )
{
    const UINT cyclesPerClock = 64;
    const FLOAT cyclesPerMillisecond = 3200000.0f;
    return ( ticks * cyclesPerClock ) / cyclesPerMillisecond;
}

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Setup the sensor array for RGB and depth streaming
//--------------------------------------------------------------------------------------
HRESULT Sample::StartupCamera()
{
    // Initializes the Natural Input system on the default thread   
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_COLOR |
                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );

    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    // Open color image stream
    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR,
                             NUI_IMAGE_RESOLUTION_640x480,
                             0,
                             1,     // This indicates how many times we can call NuiImageStreamGetNextFrame without releasing the frame
                             m_hNUIColorFrameReadyEvent,
                             &m_hColorStream );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    // Open depth stream, and register to color
    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX_IN_COLOR_SPACE,
                             NUI_IMAGE_RESOLUTION_320x240,
                             0,
                             1,     // This indicates how many times we can call NuiImageStreamGetNextFrame without releasing the frame
                             m_hNUIDepthFrameReadyEvent,
                             &m_hDepthStream );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_bColorFullScreen = FALSE;

    // Start with first stage displays for depth and color
    m_ColorDisplayMode = ORIGINAL_COLOR;
    m_DepthDisplayMode = DEPTH_MAP;

	// Threads and events
	m_hNUIDepthFrameReadyEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	m_hDepthThread = CreateThread( NULL, STACK_SIZE, &DepthHandlingThread, this, CREATE_SUSPENDED, NULL);

	m_hNUIColorFrameReadyEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	m_hColorThread = CreateThread( NULL, STACK_SIZE, &ColorHandlingThread, this, CREATE_SUSPENDED, NULL);

	m_hFrameProcessingDoneEvents[0] = CreateEvent( NULL, TRUE, FALSE, NULL );
	m_hFrameProcessingDoneEvents[1] = CreateEvent( NULL, TRUE, FALSE, NULL );

	m_hSegmentThread = CreateThread( NULL, STACK_SIZE, &SegmentThread, this, CREATE_SUSPENDED, NULL);

	m_hSegmentationCompleteEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	mClearProcessingProxy = new ClearSegmentationProcessingProxy();
	mFastProcessingProxy = new FastSegmentationProcessingProxy();

	m_pProcessingProxy = mFastProcessingProxy;

    // Init sensor and streams
    if( FAILED( StartupCamera() ) )
        return E_FAIL;

    // Set up rendering resources
    if( FAILED( InitializeVisualization( m_pd3dDevice ) ) )
        return E_FAIL;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;


	XSetThreadProcessor(m_hDepthThread,1);
	ResumeThread(m_hDepthThread);

	XSetThreadProcessor(m_hColorThread,2);
	ResumeThread(m_hColorThread);

	XSetThreadProcessor(m_hSegmentThread,3);
	ResumeThread(m_hSegmentThread);
	return S_OK;
}


//--------------------------------------------------------------------------------------
// Vertex and pixel shader for textured quads
//--------------------------------------------------------------------------------------
static const char*  g_strVideoShaderHLSL =
    " struct VS_OUT                                                              "
    " {                                                                          "
    "     float4 Position : POSITION;                                            "
    "     float2 TexCoord : TEXCOORD0;                                           "
    " };                                                                         "
    "                                                                            "
    " sampler VideoTexture : register(s0);                                       "
    "                                                                            "
    " VS_OUT VideoVertexShader( const float3 Position : POSITION,                "
    "                           const float2 TexCoord : TEXCOORD0 )              "
    " {                                                                          "
    "     VS_OUT Output;                                                         "
    "     Output.Position.x  = ( Position.x-0.5);                                "
    "     Output.Position.y  = ( Position.y-0.5);                                "
    "     Output.Position.z  = ( 0.0 );                                          "
    "     Output.Position.w  = ( 1.0 );                                          "
    "     Output.TexCoord = TexCoord;                                            "
    "     return Output;                                                         "
    " }                                                                          "
    "                                                                            "
    " float4 VideoPixelShader( VS_OUT Input ) : COLOR                            "
    " {                                                                          "
    "     return tex2D( VideoTexture, Input.TexCoord );                          "
    " }                                                                          ";

//--------------------------------------------------------------------------------------
// Vertex decl for textured quads
//--------------------------------------------------------------------------------------
struct VideoFeedVertex
{
    FLOAT vPosition[ 3 ];
    FLOAT vTexCoords[ 2 ];
};

//--------------------------------------------------------------------------------------
// Name: InitializeVisualization()
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeVisualization( D3DDevice* pd3dDevice )
{
    // Compile vertex shader.
    ID3DXBuffer* pVertexShaderCode;
    ID3DXBuffer* pVertexErrorMsg;
    HRESULT hr = D3DXCompileShader( g_strVideoShaderHLSL,
                                    ( UINT )strlen( g_strVideoShaderHLSL ),
                                    NULL,
                                    NULL,
                                    "VideoVertexShader",
                                    "vs_2_0",
                                    0,
                                    &pVertexShaderCode,
                                    &pVertexErrorMsg,
                                    NULL );
    if( FAILED( hr ) )
    {
        if( pVertexErrorMsg )
            ATG::DebugSpew( ( char* )pVertexErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create vertex shader.
    pd3dDevice->CreateVertexShader( ( DWORD* )pVertexShaderCode->GetBufferPointer(),
                                    &m_pVideoVertexShader );

    // Compile pixel shader.
    ID3DXBuffer* pPixelShaderCode;
    ID3DXBuffer* pPixelErrorMsg;

    hr = D3DXCompileShader( g_strVideoShaderHLSL,
                            ( UINT )strlen( g_strVideoShaderHLSL ),
                            NULL,
                            NULL,
                            "VideoPixelShader",
                            "ps_2_0",
                            0,
                            &pPixelShaderCode,
                            &pPixelErrorMsg,
                            NULL );
    if( FAILED( hr ) )
    {
        if( pPixelErrorMsg )
            ATG::DebugSpew( ( char* )pPixelErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create pixel shader.
    pd3dDevice->CreatePixelShader( ( DWORD* )pPixelShaderCode->GetBufferPointer(),
                                   &m_pVideoPixelShaderRGB );

    // Define the vertex elements and
    // Create a vertex declaration from the element descriptions.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVideoVertexDecl );

    for( UINT i=0; i < 2; ++i )
    {
		// Create textures for displaying color and depth streams
		if( FAILED( pd3dDevice->CreateTexture( DepthMap::DEPTH_MAP_WIDTH, DepthMap::DEPTH_MAP_HEIGHT, 1, 0,
											   D3DFMT_LIN_X8R8G8B8, 0, &m_pDepthTexture[i], NULL ) ) )
			return E_FAIL;
		if( FAILED( pd3dDevice->CreateTexture( ColorMap::COLOR_MAP_WIDTH, ColorMap::COLOR_MAP_HEIGHT, 1, 0,
											   D3DFMT_LIN_X8R8G8B8, 0, &m_pColorTexture[i], NULL ) ) )
			return E_FAIL;
    }
	m_TextureBuffer = 0;


    // Create the vertex buffer. Here we are allocating enough memory
    // (from the default pool) to hold all our 3 custom vertices. 
    if( FAILED( pd3dDevice->CreateVertexBuffer( 4 * sizeof( VideoFeedVertex ),
                                                D3DUSAGE_WRITEONLY,
                                                NULL,
                                                D3DPOOL_MANAGED,
                                                &m_pColorVB,
                                                NULL ) ) )
        return E_FAIL;

    if( FAILED( pd3dDevice->CreateVertexBuffer( 4 * sizeof( VideoFeedVertex ),
                                                D3DUSAGE_WRITEONLY,
                                                NULL,
                                                D3DPOOL_MANAGED,
                                                &m_pFullScreenColorVB,
                                                NULL ) ) )
        return E_FAIL;


    if( FAILED( pd3dDevice->CreateVertexBuffer( 4 * sizeof( VideoFeedVertex ),
                                                D3DUSAGE_WRITEONLY,
                                                NULL,
                                                D3DPOOL_MANAGED,
                                                &m_pDepthVB,
                                                NULL ) ) )
        return E_FAIL;

    // Grab display width and height
    UINT uWidth;
    UINT uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );


    XVIDEO_MODE VideoMode;
    ZeroMemory( &VideoMode, sizeof( VideoMode ) );
    XGetVideoMode( &VideoMode );

    // Set up rectangles for color, depth, and full screen color quads

    m_ColorWindow.fWidth = uWidth / 2.f * 0.8f;
    m_ColorWindow.fHeight = uWidth * 3.f / 4.f / 2.f * 0.8f;

    if( ( VideoMode.fIsWideScreen ) && ( uWidth == 640.f ) )
        m_ColorWindow.fWidth /= 1.333f;

    m_ColorWindow.fX = uWidth / 2.f - m_ColorWindow.fWidth - 10.f;
    m_ColorWindow.fY = ( uHeight - m_ColorWindow.fHeight ) / 2.f - 30.f;

    // Now we fill the vertex buffer. To do this, we need to Lock() the VB to
    // gain access to the vertices. This mechanism is required because the
    // vertex buffer may still be in use by the GPU. This can happen if the
    // CPU gets ahead of the GPU. The GPU could still be rendering the previous
    // frame.
    VideoFeedVertex* pVertices;
    VideoFeedVertex g_ColorVertices[] =
    {
        { m_ColorWindow.fX,
            m_ColorWindow.fY, 0,  0, 0 },
        { m_ColorWindow.fX + m_ColorWindow.fWidth,
            m_ColorWindow.fY, 0,  1, 0 },
        { m_ColorWindow.fX,
            m_ColorWindow.fY + m_ColorWindow.fHeight, 0,  0, 1 },
        { m_ColorWindow.fX + m_ColorWindow.fWidth,
            m_ColorWindow.fY + m_ColorWindow.fHeight, 0,  1, 1 },
    };

    if( FAILED( m_pColorVB->Lock( 0, 0, ( VOID** )&pVertices, 0 ) ) )
        return E_FAIL;
    memcpy( pVertices, g_ColorVertices, 4 * sizeof( VideoFeedVertex ) );
    m_pColorVB->Unlock();


    // Fill in the VB for full screen color view
    m_FullColorWindow.fHeight = uHeight * 0.8f;
    m_FullColorWindow.fWidth = uHeight * ( 4.0f / 3.0f );

    if( ( VideoMode.fIsWideScreen ) && ( uWidth == 640.f ) )
        m_FullColorWindow.fWidth /= 1.333f;

    m_FullColorWindow.fX = ( uWidth - m_FullColorWindow.fWidth ) / 2;
    m_FullColorWindow.fY = ( uHeight - m_FullColorWindow.fHeight ) / 2;

    VideoFeedVertex g_FullColorVertices[] =
    {
        { m_FullColorWindow.fX,
            m_FullColorWindow.fY, 0,  0, 0 },
        { m_FullColorWindow.fX + m_FullColorWindow.fWidth,
            m_FullColorWindow.fY, 0,  1, 0 },
        { m_FullColorWindow.fX,
            m_FullColorWindow.fY + m_FullColorWindow.fHeight, 0,  0, 1 },
        { m_FullColorWindow.fX + m_FullColorWindow.fWidth,
            m_FullColorWindow.fY + m_FullColorWindow.fHeight, 0,  1, 1 },
    };

    if( FAILED( m_pFullScreenColorVB->Lock( 0, 0, ( VOID** )&pVertices, 0 ) ) )
        return E_FAIL;
    memcpy( pVertices, g_FullColorVertices, 4 * sizeof( VideoFeedVertex ) );
    m_pFullScreenColorVB->Unlock();


    // Fill in the VB for depth stream texture

    m_DepthWindow.fWidth = uWidth / 2.f * 0.8f;
    m_DepthWindow.fHeight = uWidth * 3.f / 4.f / 2.f * 0.8f;

    if( ( VideoMode.fIsWideScreen ) && ( uWidth == 640.f ) )
        m_DepthWindow.fWidth /= 1.333f;

    m_DepthWindow.fX = uWidth / 2.f + 10.f;
    m_DepthWindow.fY = m_ColorWindow.fY;

    VideoFeedVertex g_DepthVertices[] =
    {
        { m_DepthWindow.fX,
            m_DepthWindow.fY, 0,  0, 0 },
        { m_DepthWindow.fX + m_DepthWindow.fWidth,
            m_DepthWindow.fY, 0,  1, 0 },
        { m_DepthWindow.fX,
            m_DepthWindow.fY + m_DepthWindow.fHeight, 0,  0, 1 },
        { m_DepthWindow.fX + m_DepthWindow.fWidth,
            m_DepthWindow.fY + m_DepthWindow.fHeight, 0,  1, 1 },
    };


    if( FAILED( m_pDepthVB->Lock( 0, 0, ( VOID** )&pVertices, 0 ) ) )
        return E_FAIL;
    memcpy( pVertices, g_DepthVertices, 4 * sizeof( VideoFeedVertex ) );
    m_pDepthVB->Unlock();

    return S_OK;
}

//--------------------------------------------------------------------------------------
DWORD Sample::DepthHandlingThread(LPVOID lpParameter)
{
    ATG::SetThreadName( GetCurrentThreadId(), "Depth processing thread." );
    Sample* pSample = (Sample*)lpParameter;

	for ( ; ; )
	{
		// Wait for next depth frame to be ready
		WaitForSingleObject(pSample->m_hNUIDepthFrameReadyEvent,INFINITE);

		const NUI_IMAGE_FRAME* pDepthFrame = NULL;

		PIXBeginNamedEvent( 0, "Get next depth frame" );
		const HRESULT grabDepth = NuiImageStreamGetNextFrame( pSample->m_hDepthStream, 0, &pDepthFrame );
		PIXEndNamedEvent();

		if( SUCCEEDED( grabDepth ) )
		{
			PIXSetMarker(0,"Depth frame %d\n",pDepthFrame->dwFrameNumber);
			PIXBeginNamedEvent(0,"Depth processing.");

			// Lock NUI texture
			D3DLOCKED_RECT LockedSrc;
			PIXBeginNamedEvent( 0, "Copy from NUI depth texture" );
			pDepthFrame->pFrameTexture->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );

			// Copy to depth map
			pSample->m_pProcessingProxy->CopyDepthFrame(( USHORT* )LockedSrc.pBits, LockedSrc.Pitch, pDepthFrame->dwFrameNumber);
			pSample->m_DepthMapFrame = pDepthFrame->dwFrameNumber;
			pDepthFrame->pFrameTexture->UnlockRect( 0 );
			NuiImageStreamReleaseFrame( pSample->m_hDepthStream, pDepthFrame );
			PIXEndNamedEvent();

			pSample->m_pProcessingProxy->DoIndependentDepthProcessing();

			// Signal depth only processing complete
			SetEvent(pSample->m_hFrameProcessingDoneEvents[1]);
			PIXEndNamedEvent(); 
		}
	}
}

//--------------------------------------------------------------------------------------
DWORD Sample::ColorHandlingThread(LPVOID lpParameter)
{
    ATG::SetThreadName( GetCurrentThreadId(), "Color processing thread." );
    Sample* pSample = (Sample*)lpParameter;

	for ( ; ; )
	{
		WaitForSingleObject(pSample->m_hNUIColorFrameReadyEvent,INFINITE);

		const NUI_IMAGE_FRAME* pColorFrame = NULL;

		PIXBeginNamedEvent( 0, "Get next color frame" );
		const HRESULT grabColor = NuiImageStreamGetNextFrame( pSample->m_hColorStream, 0, &pColorFrame );
		PIXEndNamedEvent();

		if( SUCCEEDED( grabColor ) )
		{
			PIXSetMarker(0,"Color frame %d\n",pColorFrame->dwFrameNumber);
			
			PIXBeginNamedEvent( 0, "Color processing" );

			// Lock NUI texture
			D3DLOCKED_RECT LockedSrc;
			PIXBeginNamedEvent( 0, "Copy from NUI color texture" );
			pColorFrame->pFrameTexture->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );

			// Copy to color map
			pSample->m_pProcessingProxy->CopyColorFrame(( DWORD* )LockedSrc.pBits, LockedSrc.Pitch, pColorFrame->dwFrameNumber);	        
			pSample->m_ColorMapFrame = pColorFrame->dwFrameNumber;
			pColorFrame->pFrameTexture->UnlockRect( 0 );

			NuiImageStreamReleaseFrame( pSample->m_hColorStream, pColorFrame );
			PIXEndNamedEvent();

			pSample->m_pProcessingProxy->DoIndependentColorProcessing();

			// Signal color only processing complete
			SetEvent(pSample->m_hFrameProcessingDoneEvents[0]);
			PIXEndNamedEvent();
		}
	}
}
//--------------------------------------------------------------------------------------

DWORD Sample::SegmentThread(LPVOID lpParameter)
{
    ATG::SetThreadName( GetCurrentThreadId(), "Final segmentation thread." );
    Sample* pSample = (Sample*)lpParameter;

	for ( ; ; )
	{
		// Wait for depth and color processing to be completed
		WaitForMultipleObjects(2,pSample->m_hFrameProcessingDoneEvents,TRUE,INFINITE);

		if (pSample->m_ColorMapFrame != pSample->m_DepthMapFrame)
			printf(".");

		PIXBeginNamedEvent(0, "Final Segmentation");
   
		pSample->m_pProcessingProxy->DoFinalJointProcessing();

        PIXEndNamedEvent();
		// Reset events
		ResetEvent(pSample->m_hFrameProcessingDoneEvents[0]);
		ResetEvent(pSample->m_hFrameProcessingDoneEvents[1]);
		
		// Signal segmentation processing complete
		SetEvent(pSample->m_hSegmentationCompleteEvent);
	}
}



//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

     // Toggle help

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Switch to full screen, but not from help screen
    if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B ) && !m_bDrawHelp )
    {
        m_bColorFullScreen = !m_bColorFullScreen;
    }

    // Change depth display, except in full screen mode
    if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y ) && !m_bColorFullScreen )
    {
        m_DepthDisplayMode = (DepthDisplayType)((INT)m_DepthDisplayMode + 1 );
        if( m_DepthDisplayMode == DEPTH_DISPLAY_MAX )
            m_DepthDisplayMode = DEPTH_MAP;
    }

    // Change depth display, except in full screen mode
    if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A ) && !m_bColorFullScreen )
    {
        m_DepthDisplayMode = (DepthDisplayType)((INT)m_DepthDisplayMode - 1 );
        if( m_DepthDisplayMode < 0 )
            m_DepthDisplayMode = (DepthDisplayType)(DEPTH_DISPLAY_MAX - 1);
    }

    // Change color display up and down through list
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_ColorDisplayMode = (ColorDisplayType)((INT)m_ColorDisplayMode + 1 );
        if( m_ColorDisplayMode == COLOR_DISPLAY_MAX )
            m_ColorDisplayMode = ORIGINAL_COLOR;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_ColorDisplayMode = (ColorDisplayType)((INT)m_ColorDisplayMode - 1 );
        if( m_ColorDisplayMode < 0 )
            m_ColorDisplayMode = (ColorDisplayType)(COLOR_DISPLAY_MAX - 1);
    }


	WaitForSingleObject(m_hSegmentationCompleteEvent,30);

	m_TextureBuffer ^= 1;
	// Set up the display textures
	m_pProcessingProxy->FillColorMap(m_pColorTexture[m_TextureBuffer],m_ColorDisplayMode,m_DepthDisplayMode);
	m_pProcessingProxy->FillDepthMap(m_pDepthTexture[m_TextureBuffer],m_DepthDisplayMode);

	ResetEvent(m_hSegmentationCompleteEvent);
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff00ffff );

    // Set default states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_pd3dDevice->SetVertexShader( m_pVideoVertexShader );
        m_pd3dDevice->SetPixelShader( m_pVideoPixelShaderRGB );
        m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
        m_pd3dDevice->SetVertexDeclaration( m_pVideoVertexDecl );

        if( m_bColorFullScreen )
        {
            // Draw the full screen color quad
            m_pd3dDevice->SetStreamSource( 0, m_pFullScreenColorVB, 0, sizeof( VideoFeedVertex ) );
            m_pd3dDevice->SetTexture( 0, m_pColorTexture[m_TextureBuffer] );

            m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
        }
        else
        {
            // Render the color video stream
            m_pd3dDevice->SetStreamSource( 0, m_pColorVB, 0, sizeof( VideoFeedVertex ) );
            m_pd3dDevice->SetTexture( 0, m_pColorTexture[m_TextureBuffer] );

            m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

            // Render the depth video stream
            m_pd3dDevice->SetStreamSource( 0, m_pDepthVB, 0, sizeof( VideoFeedVertex ) );
            m_pd3dDevice->SetTexture( 0, m_pDepthTexture[m_TextureBuffer] );

            m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
        }

        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Advanced Segmentation" );

        if( m_bColorFullScreen )
        {
            // Title and color map title
            m_Font.SetScaleFactors( 1.2f, 1.2f );	
            m_Font.DrawText( 0, 40, 0xffffffff, g_ColorDisplayNames[m_ColorDisplayMode] );

            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 180, 600, 0xffffff00, L"Back to dual display: ", ATGFONT_RIGHT );
            m_Font.DrawText( 190, 600, 0xffffff00, GLYPH_B_BUTTON );
        }
        else
        {
            // Title for both color and depth
            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 0, 460, 0xffffffff, g_ColorDisplayNames[m_ColorDisplayMode] );
            m_Font.DrawText( 540, 460, 0xffffffff, g_DepthDisplayNames[m_DepthDisplayMode] );

            m_Font.SetScaleFactors( 0.7f, 0.7f );
            m_Font.DrawText( 0, 490, 0xffffffff, L"D-pad up/down to change color view");
            m_Font.DrawText( 540, 490, 0xffffffff, L"A and Y to change depth view" );

			if (!m_pProcessingProxy->PlayersInView())
				m_Font.DrawText( 0, 510, 0xffffffff, L"No players in view, some color and depth views may contain no selected pixels, ie black" );

            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 180, 600, 0xffffff00, L"Color map full screen: ", ATGFONT_RIGHT );
            m_Font.DrawText( 200, 600, 0xffffff00, GLYPH_B_BUTTON );

            m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );		
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
