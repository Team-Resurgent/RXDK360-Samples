//--------------------------------------------------------------------------------------
// Segmentation.cpp
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgApp.h>
#include <AtgHelp.h>
#include <AtgUtil.h>
#include <AtgInput.h>
#include <AtgNuiCommon.h>

#include <NuiApi.h>

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Toggle display modes" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

// Rect used for displaying the color and depth stream
typedef struct
{
    FLOAT fX;
    FLOAT fY;
    FLOAT fWidth;
    FLOAT fHeight;
} Rect;

enum DemoMode
{
    DEMOMODE_BACKGROUNDREMOVED,
    DEMOMODE_PLAYEROVERLAY,
    DEMOMODE_NORMAL,
    DEMOMODE_COUNT
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer                   m_Timer;
    ATG::Font                    m_Font;
    ATG::Help                    m_Help;
    BOOL                         m_bDrawHelp;

    HANDLE                       m_hDepthStream;
    HANDLE                       m_hColorStream;
    HANDLE                       m_hFrameEndEvent;

    IDirect3DVertexShader9*      m_pVideoVertexShader;
    IDirect3DPixelShader9*       m_pVideoPixelShaderRGB;
    IDirect3DVertexDeclaration9* m_pVideoVertexDecl;
    IDirect3DTexture9*           m_pDepthTexture;
    IDirect3DTexture9*           m_pColorTexture;

    Rect                         m_ColorWindow;
    Rect                         m_DepthWindow;

    DemoMode                     m_DemoMode;

    USHORT*                      m_pDepthBuf;

    INT                          m_ColorWidth;
    INT                          m_ColorHeight;
    INT                          m_DepthWidth;
    INT                          m_DepthHeight;

    XMFLOAT4                     m_SegmentationColorTable[NUI_SKELETON_COUNT + 1];

private:
    HRESULT Initialize();
    HRESULT InitializeVisualization( D3DDevice* pd3dDevice );

    VOID SubmitVertexData( BOOL bColor );

    HRESULT Update();
    HRESULT Render();

    HRESULT StartupCamera();    
};

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Setup the sensor array for RGB and depth streaming
//--------------------------------------------------------------------------------------
HRESULT Sample::StartupCamera()
{
    // Create event which will be signaled when frame processing ends
    m_hFrameEndEvent = CreateEvent( NULL,
                                    FALSE,  // auto-reset
                                    FALSE,  // create unsignaled
                                    "NuiFrameEndEvent" );

    if ( !m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    // Initializes the Natural Input system on the default thread   
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_COLOR |
                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                NUI_INITIALIZE_FLAG_USES_SKELETON,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if ( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }    

    // Register frame end event with NUI
    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    if( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR_IN_DEPTH_SPACE,
                             NUI_IMAGE_RESOLUTION_640x480,
                             0,
                             1, 
                             NULL,
                             &m_hColorStream );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
                             NUI_IMAGE_RESOLUTION_320x240,
                             0,
                             1,     // This indicates how many times we can call NuiImageStreamGetNextFrame without releasing the frame
                             NULL,
                             &m_hDepthStream );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }
    
    hr = NuiSkeletonTrackingEnable( NULL, NUI_SKELETON_TRACKING_FLAG_TITLE_SETS_TRACKED_SKELETONS );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
        return E_FAIL;
    }
    
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_pDepthTexture = NULL;
    m_pColorTexture = NULL;
    m_DemoMode = DEMOMODE_BACKGROUNDREMOVED;
    m_bDrawHelp = FALSE;
    m_ColorWidth = 640;
    m_ColorHeight = 480;
    m_DepthWidth = 320;
    m_DepthHeight = 240;

    // Colors to use for player data - maps to PLAYER_INDEX in DEPTH_AND_PLAYER_INDEX NUI outputs
    //    0 is mapped to 0 to indicate grayscale for normal background
    //    0 is interpretered as black in the background removed mode
    m_SegmentationColorTable[0] = XMFLOAT4( 255, 255, 255, 255);
    m_SegmentationColorTable[1] = XMFLOAT4( 255,   0,   0, 255);
    m_SegmentationColorTable[2] = XMFLOAT4(   0, 255,   0, 255);
    m_SegmentationColorTable[3] = XMFLOAT4(  64, 255, 255, 255);
    m_SegmentationColorTable[4] = XMFLOAT4( 255, 255,  64, 255);
    m_SegmentationColorTable[5] = XMFLOAT4( 255,  64, 255, 255);
    m_SegmentationColorTable[6] = XMFLOAT4( 128, 128, 255, 255);

    m_pDepthBuf = new USHORT[NUI_IMAGE_DEPTH_BUFFER_SIZE/sizeof(USHORT)];
    
    if ( FAILED( StartupCamera() ) )
        return E_FAIL;

    if ( FAILED( InitializeVisualization( m_pd3dDevice ) ) )
        return E_FAIL;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;    

    return S_OK;
}

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

    // Create textures for displaying color and depth streams
    if ( FAILED( pd3dDevice->CreateTexture( m_DepthWidth, m_DepthHeight, 0, 0, D3DFMT_LIN_X8R8G8B8, 0, &m_pDepthTexture, NULL ) ) )
        return E_FAIL;
    if ( FAILED( pd3dDevice->CreateTexture( m_ColorWidth, m_ColorHeight, 0, 0, ATG::GetAs16SRGBFormat( D3DFMT_LIN_X8R8G8B8 ), 0, &m_pColorTexture, NULL ) ) )
        return E_FAIL;
    
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    NUI_SKELETON_FRAME SkeletonFrame;
    
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_DemoMode = (DemoMode)( (INT)m_DemoMode + 1 );
        m_DemoMode = (DemoMode)( (INT)m_DemoMode % DEMOMODE_COUNT );
    }

    const NUI_IMAGE_FRAME* pImageFrame;    
    const NUI_IMAGE_FRAME* pDepthFrame;    

    // Wait for frame processing to end
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }

    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hColorStream, 0, &pImageFrame );
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepthStream, 0, &pDepthFrame );
    HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &SkeletonFrame );

    if ( SUCCEEDED( hrDepth ) )
    {
        if( SUCCEEDED( hrImage ) && SUCCEEDED( hrSkeleton ) && 
                       pImageFrame->dwFrameNumber == pDepthFrame->dwFrameNumber && 
                       SkeletonFrame.dwFrameNumber == pDepthFrame->dwFrameNumber )
        {
            D3DLOCKED_RECT Locked;
            m_pDepthTexture->LockRect( 0, &Locked, NULL, 0 );
            DWORD* pBitsDst = (DWORD*)Locked.pBits;

            D3DLOCKED_RECT LockedSrc;
            pDepthFrame->pFrameTexture->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );
            const USHORT* pBitsSrc = (const USHORT*)LockedSrc.pBits;

            // Make a local copy of the depth & player index buffer
            const USHORT* pBitsSrcCur = (const USHORT*)LockedSrc.pBits;
            USHORT* pBitsDstCur = m_pDepthBuf;
            for (INT y = 0; y < m_DepthHeight; y++)
            {
                XMemCpy( pBitsDstCur, pBitsSrcCur, m_DepthWidth * sizeof(USHORT) );
                
                pBitsSrcCur += LockedSrc.Pitch / sizeof(USHORT);
                pBitsDstCur += m_DepthWidth;
            }

            switch ( m_DemoMode )
            {
                case DEMOMODE_BACKGROUNDREMOVED:
                    for ( INT j = 0; j < m_DepthHeight; ++j )
                    {
                        for ( INT i = 0; i < m_DepthWidth; ++i )
                        {
                            USHORT depth =*(pBitsSrc + i) >> 3;  // The lower 3 bits indicate player idx
                            
                            // generate a color for the depth value
                            float fDepth = depth/65535.0f * 8.0f;

                            // get the player index for the current pixel
                            UINT SegmentationValue = (*(pBitsSrc + i) & 7);
                            
                            // if the pixel is not part of a player, treat as black
                            if (SegmentationValue == 0)
                            {
                                fDepth = 0.0f;
                            } else
                            {
                                // else it is a player pixel, brighten it up a bit
                                fDepth *= 2.0f;
                            }

                            D3DCOLOR* pColor = (D3DCOLOR*)pBitsDst + i;
                            *pColor = D3DCOLOR_ARGB((UINT)(fDepth * m_SegmentationColorTable[SegmentationValue].w), 
                                                    (UINT)(fDepth * m_SegmentationColorTable[SegmentationValue].x), 
                                                    (UINT)(fDepth * m_SegmentationColorTable[SegmentationValue].y), 
                                                    (UINT)(fDepth * m_SegmentationColorTable[SegmentationValue].z) );
                        }
                        pBitsSrc += LockedSrc.Pitch / sizeof(USHORT);
                        pBitsDst += Locked.Pitch / sizeof(DWORD);
                    }
                    break;

                case DEMOMODE_PLAYEROVERLAY:
                    for ( INT j = 0; j < m_DepthHeight; ++j )
                    {
                        for ( INT i = 0; i < m_DepthWidth; ++i )
                        {
                            USHORT depth =*(pBitsSrc + i) >> 3;  // The lower 3 bits indicate player idx
                            
                            // generate a color for the depth value
                            FLOAT fDepth = depth/65535.0f * 8.0f;

                            // get the player index for the current pixel
                            UINT SegmentationValue = (*(pBitsSrc + i) & 7);
                            
                            // if it is a player pixel, brighten it up a bit
                            if ( SegmentationValue > 0 )
                                fDepth *= 2.0f;

                            D3DCOLOR* pColor = (D3DCOLOR*)pBitsDst + i;
                            *pColor = D3DCOLOR_ARGB((UINT)(fDepth * m_SegmentationColorTable[SegmentationValue].w), 
                                                    (UINT)(fDepth * m_SegmentationColorTable[SegmentationValue].x), 
                                                    (UINT)(fDepth * m_SegmentationColorTable[SegmentationValue].y), 
                                                    (UINT)(fDepth * m_SegmentationColorTable[SegmentationValue].z) );

                        }
                        pBitsSrc += LockedSrc.Pitch / sizeof(USHORT);
                        pBitsDst += Locked.Pitch / sizeof(DWORD);
                    }
                    break;

                case DEMOMODE_NORMAL:
                    for ( INT j = 0; j < m_DepthHeight; ++j )
                    {
                        for ( INT i = 0; i < m_DepthWidth; ++i )
                        {
                            USHORT depth =*(pBitsSrc + i) >> 3;  // The lower 3 bits indicate player idx
                            
                            *(pBitsDst + i) = D3DCOLOR_XRGB( (BYTE)(depth/65535.0f * 255 * 8), 
                                                             (BYTE)(depth/65535.0f * 255 * 8), 
                                                             (BYTE)(depth/65535.0f * 255 * 8) );
                        }
                        pBitsSrc += LockedSrc.Pitch / sizeof(USHORT);
                        pBitsDst += Locked.Pitch / sizeof(DWORD);
                    }
                    break;
            }        

            pDepthFrame->pFrameTexture->UnlockRect( 0 );
            m_pDepthTexture->UnlockRect( 0 );
        }
        NuiImageStreamReleaseFrame( m_hDepthStream, pDepthFrame );
    }
    
    if ( SUCCEEDED( hrImage ) )
    {    
        if( SUCCEEDED( hrDepth ) && SUCCEEDED( hrSkeleton ) && 
                       pImageFrame->dwFrameNumber == pDepthFrame->dwFrameNumber && 
                       SkeletonFrame.dwFrameNumber == pDepthFrame->dwFrameNumber )
        {

            D3DLOCKED_RECT Locked;
            m_pColorTexture->LockRect( 0, &Locked, NULL, 0 );
            
            D3DLOCKED_RECT LockedSrc;
            pImageFrame->pFrameTexture->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );
            
            const DWORD* pBitsSrc = (const DWORD*)LockedSrc.pBits;
            DWORD* pBitsDst = (DWORD*)Locked.pBits;
            
            switch ( m_DemoMode )
            {
                case DEMOMODE_BACKGROUNDREMOVED:
                    for ( INT j = 0; j < m_ColorHeight; ++j )
                    {
                        for ( INT i = 0; i < m_ColorWidth; ++i )
                        {
                            // Only show the pixels which are player data, all other pixels are marked black
                            if (m_pDepthBuf[i/2 + j/2*m_DepthWidth] & 7)
                                *(pBitsDst + i) = *(pBitsSrc + i);
                            else
                                *(pBitsDst + i) = 0;
                        }
                        pBitsSrc += LockedSrc.Pitch / sizeof(DWORD);
                        pBitsDst += Locked.Pitch / sizeof(DWORD);
                    }
                    break;

                case DEMOMODE_PLAYEROVERLAY:                
                case DEMOMODE_NORMAL:
                    memcpy( pBitsDst, pBitsSrc, NUI_IMAGE_COLOR_640x480_BUFFER_SIZE );                
                    break;
            }        

            pImageFrame->pFrameTexture->UnlockRect( 0 );
            m_pColorTexture->UnlockRect( 0 );
        }

        NuiImageStreamReleaseFrame( m_hColorStream, pImageFrame );
    }    

    // For this sample we do not show a skeleton,
    //    but we do need to get the skeleton frame in order to use NuiSkeletonSetTrackedSkeletons to track more than 2 players with segmentation
    if ( SUCCEEDED( hrSkeleton ))
    {
        // intialize to no skeletons active
        DWORD OneSkeletonTracked[NUI_SKELETON_MAX_TRACKED_COUNT] = {0, 0};
        
        INT iClosestBody = -1;
        float fMinDistance = FLT_MAX;

        // find the closest skeleton to the center of the playspace
        for (UINT i = 0; i < NUI_SKELETON_COUNT; i++)
        {
            // if not tracked, then ignore
            if (SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_NOT_TRACKED)
                continue;


            // find the positions closest to the camera
            XMVECTOR vTemp = XMVectorSubtract(SkeletonFrame.SkeletonData[i].Position, XMVectorZero());

            // calculate the distance between the center and tracked position
            float CurDistance = XMVector3LengthSq(vTemp).x; 
            if ( CurDistance < fMinDistance )
            {
                iClosestBody = i;
                fMinDistance = CurDistance;
            }              
        }

        // only swap if there is a closest and it is not already in the first slot
        if (iClosestBody != -1)
        {
            OneSkeletonTracked[0] = SkeletonFrame.SkeletonData[iClosestBody].dwTrackingID;
        }

        // this call will either specify no players to have skeletons for if none have been found
        //    or the one closest to the 'center' as the one to have a skeleton
        NuiSkeletonSetTrackedSkeletons(OneSkeletonTracked);
        
    }
    
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SubmitVertexData()
// Desc: Create the vertices used to render the video on screen, and feed them inline
// to the D3D command buffer.
//--------------------------------------------------------------------------------------
VOID Sample::SubmitVertexData( BOOL bColor )
{
    UINT uWidth;
    UINT uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );

    XVIDEO_MODE VideoMode;
    ZeroMemory( &VideoMode, sizeof( VideoMode ) );
    XGetVideoMode( &VideoMode );

    if( bColor )
    {
        m_ColorWindow.fWidth = uWidth / 2.f * 0.8f;
        m_ColorWindow.fHeight = uWidth * 3.f / 4.f / 2.f * 0.8f;

        if( ( VideoMode.fIsWideScreen ) && ( uWidth == 640.f ) )
            m_ColorWindow.fWidth /= 1.333f;

        m_ColorWindow.fX = uWidth / 2.f - m_ColorWindow.fWidth - 10.f;
        m_ColorWindow.fY = ( uHeight - m_ColorWindow.fHeight ) / 2.f - 30.f;

        // Now we fill the vertex buffer. 
        VideoFeedVertex g_SnapshotVertices[] =
        {
            { m_ColorWindow.fX,                                                         
            m_ColorWindow.fY, 0,  0, 0 },
            { m_ColorWindow.fX + m_ColorWindow.fWidth,               
            m_ColorWindow.fY, 0,  1, 0 },
            { m_ColorWindow.fX,                                                         
            m_ColorWindow.fY + m_ColorWindow.fHeight, 0,  0, 1 },
            //{ m_ColorWindow.fX + m_ColorWindow.fWidth, 
            //  m_ColorWindow.fY + m_ColorWindow.fHeight, 0,  1, 1 },
        };

        VideoFeedVertex* pVertices;

        m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( *g_SnapshotVertices ), &(VOID*&)pVertices );
        memcpy( pVertices, g_SnapshotVertices, sizeof( g_SnapshotVertices ) );
        m_pd3dDevice->EndVertices();
    }
    else    // depth
    {
        m_DepthWindow.fWidth = uWidth / 2.f * 0.8f;
        m_DepthWindow.fHeight = uWidth * 3.f / 4.f / 2.f * 0.8f;

        if( ( VideoMode.fIsWideScreen ) && ( uWidth == 640.f ) )
            m_DepthWindow.fWidth /= 1.333f;

        m_DepthWindow.fX = uWidth / 2.f + 10.f;
        m_DepthWindow.fY = m_ColorWindow.fY;

        // Fill in the VB for depth stream texture
        VideoFeedVertex g_SnapshotVertices[] =
        {
            { m_DepthWindow.fX,                                          
            m_DepthWindow.fY, 0,  0, 0 },
            { m_DepthWindow.fX + m_DepthWindow.fWidth, 
            m_DepthWindow.fY, 0,  1, 0 },
            { m_DepthWindow.fX,                                          
            m_DepthWindow.fY + m_DepthWindow.fHeight, 0,  0, 1 },
            //{ m_DepthWindow.fX + m_DepthWindow.fWidth, 
            //  m_DepthWindow.fY + m_DepthWindow.fHeight, 0,  1, 1 },
        };

        VideoFeedVertex* pVertices;

        m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( *g_SnapshotVertices ), &(VOID*&)pVertices );
        memcpy( pVertices, g_SnapshotVertices, sizeof( g_SnapshotVertices ) );
        m_pd3dDevice->EndVertices();
    }
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
        // Render the color video stream
        {
            m_pd3dDevice->SetVertexShader( m_pVideoVertexShader );
            m_pd3dDevice->SetPixelShader( m_pVideoPixelShaderRGB );

            m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
            m_pd3dDevice->SetVertexDeclaration( m_pVideoVertexDecl );            
            m_pd3dDevice->SetTexture( 0, m_pColorTexture );

            SubmitVertexData( TRUE );
        }
        
        // Render the depth video stream
        {
            m_pd3dDevice->SetVertexShader( m_pVideoVertexShader );
            m_pd3dDevice->SetPixelShader( m_pVideoPixelShaderRGB );

            m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
            m_pd3dDevice->SetVertexDeclaration( m_pVideoVertexDecl );            
            m_pd3dDevice->SetTexture( 0, m_pDepthTexture );

            SubmitVertexData( FALSE );
        }
                
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        switch ( m_DemoMode )
        {
            case DEMOMODE_BACKGROUNDREMOVED:
                m_Font.DrawText( 0, 0, 0xffffffff,  L"Segmentation - background removal" );
                break;

            case DEMOMODE_PLAYEROVERLAY:
                m_Font.DrawText( 0, 0, 0xffffffff,  L"Segmentation - player id overlay" );
                break;

            case DEMOMODE_NORMAL:
                m_Font.DrawText( 0, 0, 0xffffffff,  L"Segmentation - normal" );
                break;
        }
        
        m_Font.SetScaleFactors( 1.0f, 1.0f ); 
        m_Font.DrawText( 180, 500, 0xffffff00, L"Toggle modes: ", ATGFONT_RIGHT );
        m_Font.DrawText( 200, 500, 0xffffff00, GLYPH_A_BUTTON );

        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
