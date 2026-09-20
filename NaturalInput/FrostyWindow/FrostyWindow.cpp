//--------------------------------------------------------------------------------------
// FrostyWindow.cpp
//
// Frosty Window sample
//
// Demonstrates how to use skeletal tracking to make a mini game that you can 
// draw/scratch on a frosty window.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <xaudio2.h>
#include <xbdm.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <AtgPostProcess.h>
#include <AtgNuiCommon.h>

#include <nuiapi.h>
#include <XNAmath.h>

static const CHAR* g_strX2DShader =
    "struct VS_IN {                              "
    "    float2 Pos            : POSITION;       "
    "    float2 Tex            : TEXCOORD0;      "
    "};                                          "
    "struct VS_OUT {                             "
    "    float4 Position       : POSITION;       "
    "    float2 TexCoord0      : TEXCOORD0;      "
    "};                                          "
    "                                            "
    "VS_OUT X2DVS( VS_IN In )                    "
    "{                                           "
    "    VS_OUT Out;                             "
    "    Out.Position.x  = In.Pos.x;             "
    "    Out.Position.y  = In.Pos.y;             "
    "    Out.Position.z  = 0.0;                  "
    "    Out.Position.w  = 1.0;                  "
    "    Out.TexCoord0.x = In.Tex.x;             "
    "    Out.TexCoord0.y = In.Tex.y;             "
    "    return Out;                             "
    "}                                           "
    "                                            "
    "sampler Texture : register(s0);             "
    "float4 Color    : register(c0);             "
    "                                            "
    "float4 X2DPS( VS_OUT In ) : COLOR0          "
    "{                                           "
    "    return tex2D( Texture, In.TexCoord0 )   "
    "                * Color;                    "
    "}                                           "
    "                                            "
    "float4 ColorPS( VS_OUT In ) : COLOR0        "
    "{                                           "
    "    return Color;                           "
    "}                                           ";

typedef struct
{
    FLOAT x, y;
    FLOAT tu, tv;
} T2DVertex;

typedef struct
{
    FLOAT x, y;
    FLOAT size;
} TBrush;

#define MAX_STREAMS    2
#define FULLBODY_STAMP_WAIT (30 * 5)

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_2, L"Save\npicture" },
    { ATG::HELP_Y_BUTTON, ATG::HELP_PLACEMENT_2, L"Toggle\nfull body stamp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Toggle\ninterpolation type" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_2, L"Clear\nwindow" },
    { ATG::HELP_LEFT_STICK, ATG::HELP_PLACEMENT_2, L"Adjust distance\nto drawing plane" },
    { ATG::HELP_RIGHT_STICK, ATG::HELP_PLACEMENT_2, L"Adjust distance to\nfull body stamp plane" },
    { ATG::HELP_LEFT_TRIGGER, ATG::HELP_PLACEMENT_2, L"Cycle through\ntraced joint" },
    { ATG::HELP_RIGHT_TRIGGER, ATG::HELP_PLACEMENT_2, L"Cycle through\ntraced joint" },
};
static const DWORD  NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::PackedResource m_Resource;

    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    // Holds current brush information (position and size)
    TBrush m_Brush;
    TBrush m_BrushStrokeHistory[3];

    // Vertex buffers for rendering the cursors
    T2DVertex m_CursorPos[4];    

    // Z coordinate of the cursor, which is from the joint currently being used for drawing on the frosty window
    FLOAT m_fCursorZ;
    
    // Z coordinate of the frosty window
    FLOAT m_fGlassZ;

    // How far away the frosty window is in front of you
    FLOAT m_fDistanceToGlass;

    // If the player is beyond this point, the full body stamp is generated
    FLOAT m_fFullBodyStampZ;

    D3DTexture* m_pBrushTexture;
    D3DTexture* m_pCursorTexture;    // The hand cursor
    D3DTexture* m_pColorTexture;
    D3DTexture* m_pDepthTexture;
    D3DTexture* m_pGlassTexture;

    D3DVertexShader* m_pX2DVertexShader;
    D3DPixelShader* m_pX2DPixelShader;
    D3DPixelShader* m_pColorPS;

    BOOL m_bClearGlass;        

    NUI_SKELETON_FRAME m_SkeletonFrame;
    NUI_IMAGE_FRAME* m_pImageFrame[MAX_STREAMS];
    NUI_IMAGE_FRAME* m_pDepthImageFrame[MAX_STREAMS];
    D3DPOINT m_ScreenSpace[NUI_SKELETON_POSITION_COUNT];
    DWORD m_dwLatest[2];
    DWORD m_dwDepthLatest[2];
    HANDLE m_hStream;
    HANDLE m_hDepthStream;    
    HANDLE m_hFrameEndEvent;
    
    // Which joint to use? left hand, right hand or hip center
    INT m_iWhichJoint;          

    INT m_iFullBodyStampWait;
    BOOL m_bFullBodyStampEnable;

    // Which interpolation scheme to use? linear or spline
    DWORD m_dwInterpolation;

    // Which skeleton (from NUI) is currently being used
    UINT m_uCurrentSkeletonIndex;

    ATG::PostProcess             m_PostProcess;

private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

    VOID CalcScreenSpace( DWORD dwDisplayWidth, DWORD dwDisplayHeight, D3DPOINT* pScreenSpace, XMVECTOR* pJoints );
    VOID DrawFullScreenTexturedQuad( D3DTexture* pTex, D3DXCOLOR color = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f) );
    VOID DrawFullScreenColorQuad( D3DXCOLOR color );
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;

    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.m_d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    atgApp.m_d3dpp.MultiSampleQuality = 0;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr = S_OK;

    m_uCurrentSkeletonIndex = 0;

    if( FAILED( m_PostProcess.Initialize() ) )
    {
        ATG_PrintError( "DX9Sample: Couldn't initialize the effects library\n" );
        return E_FAIL;
    }

    // Create the font
    if ( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    m_bDrawHelp = FALSE;
    if ( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the other textures
    if ( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_Brush.x = m_d3dpp.BackBufferWidth / 2.0f;
    m_Brush.y = m_d3dpp.BackBufferHeight / 2.0f;
    m_Brush.size = 0.0f;

    for ( UINT i = 0; i < 3; i++ )
        m_BrushStrokeHistory[ i ] = m_Brush;

    m_pBrushTexture = m_Resource.GetTexture( "Brush" );
    m_pCursorTexture = m_Resource.GetTexture( "Cursor" );

    // Create texture for Color & Depth buffer
    m_pd3dDevice->CreateTexture( 640, 480, 1, 0, ATG::GetAs16SRGBFormat( D3DFMT_LIN_X8R8G8B8 ), 0, &m_pColorTexture, NULL );
    m_pd3dDevice->CreateTexture( 320, 240, 1, 0, D3DFMT_LIN_A8R8G8B8, 0, &m_pDepthTexture, NULL );

    m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 1, 0, D3DFMT_A8R8G8B8, 0, &m_pGlassTexture, NULL );

    // Clear depth texture
    D3DLOCKED_RECT lock;
    m_pDepthTexture->LockRect( 0, &lock, NULL, 0 );
    
    DWORD* lpBits = ( DWORD* )lock.pBits;
    for( UINT j = 0; j < (UINT)lock.Pitch * 240 / 4; ++ j )
    {
        lpBits[ j ] = D3DCOLOR_ARGB(0, 0, 0, 0);
    }
    
    m_pDepthTexture->UnlockRect(0);

    // Initialize simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Set up shader    
    ID3DXBuffer* pShaderCode;
    if ( FAILED( D3DXCompileShader( g_strX2DShader, strlen( g_strX2DShader ), NULL, NULL, "X2DVS", "vs_2_0", 0, &pShaderCode, NULL, NULL ) ) )
        return E_FAIL;
    if ( FAILED( m_pd3dDevice->CreateVertexShader( (LPDWORD) pShaderCode->GetBufferPointer(), &m_pX2DVertexShader ) ) )
        return E_FAIL;
    pShaderCode->Release();

    if ( FAILED( D3DXCompileShader( g_strX2DShader, strlen( g_strX2DShader ), NULL, NULL, "X2DPS", "ps_2_0", 0, &pShaderCode, NULL, NULL ) ) )
        return E_FAIL;
    if ( FAILED( m_pd3dDevice->CreatePixelShader( (LPDWORD) pShaderCode->GetBufferPointer(), &m_pX2DPixelShader ) ) )
        return E_FAIL;
    pShaderCode->Release();

    if ( FAILED( D3DXCompileShader( g_strX2DShader, strlen( g_strX2DShader ), NULL, NULL, "ColorPS", "ps_2_0", 0, &pShaderCode, NULL, NULL ) ) )
        return E_FAIL;
    if ( FAILED( m_pd3dDevice->CreatePixelShader( (LPDWORD) pShaderCode->GetBufferPointer(), &m_pColorPS ) ) )
        return E_FAIL;
    pShaderCode->Release();

    m_bClearGlass = TRUE;
    m_fGlassZ = 1.7f;

    //

    m_CursorPos[0].tu = 0.0f;
    m_CursorPos[0].tv = 0.0f;

    m_CursorPos[1].tu = 1.0f;
    m_CursorPos[1].tv = 0.0f;

    m_CursorPos[2].tu = 1.0f;
    m_CursorPos[2].tv = 1.0f;

    m_CursorPos[3].tu = 0.0f;
    m_CursorPos[3].tv = 1.0f;

    // Reset image stream parameters
    m_dwLatest[0] = MAX_STREAMS - 1;
    m_dwLatest[1] = MAX_STREAMS - 2;
    m_dwDepthLatest[0] = MAX_STREAMS - 1;
    m_dwDepthLatest[1] = MAX_STREAMS - 2;
    for( DWORD dwStreamsNumber = 0; dwStreamsNumber < MAX_STREAMS; dwStreamsNumber++ )
    {
        m_pImageFrame[dwStreamsNumber] = NULL;
        m_pDepthImageFrame[dwStreamsNumber] = NULL;
    }

    // Create frame end event
    m_hFrameEndEvent = CreateEvent(NULL,
        FALSE,  // auto-reset
        FALSE,  // create unsignaled
        "NuiFrameEndEvent");
    if ( !m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    // Set up NUI
    hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON | 
                        NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | 
                        NUI_INITIALIZE_FLAG_USES_COLOR, 
                        NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }
    
    hr = NuiSkeletonTrackingEnable( NULL, 0 );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
        return E_FAIL;
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, MAX_STREAMS, NULL, &m_hStream );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL; 
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_320x240, 0, MAX_STREAMS, NULL, &m_hDepthStream );
    if ( FAILED( hr ) )
    {
        ATG_PrintError( "Failed to initialize Depth Stream\n" );
        return E_FAIL;
    }    

    m_iWhichJoint = 0;
    m_iFullBodyStampWait = FULLBODY_STAMP_WAIT;
    m_fDistanceToGlass = 0.4f;
    m_fFullBodyStampZ = 1.4f;
    m_bFullBodyStampEnable = FALSE;
    m_dwInterpolation = 1;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//       The movie is played from here.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    HRESULT hr = S_OK;

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Save the recent brushstroke coordinates
    m_BrushStrokeHistory[2] = m_BrushStrokeHistory[1];
    m_BrushStrokeHistory[1] = m_BrushStrokeHistory[0];
    m_BrushStrokeHistory[0] = m_Brush;

    if ( m_iFullBodyStampWait > 0 ) m_iFullBodyStampWait--;

    IDirect3DTexture9* pColorStream = NULL;
    IDirect3DTexture9* pDepthStream = NULL;
    const NUI_IMAGE_FRAME* pImage;
    const NUI_IMAGE_FRAME* pDepth;

    // Wait for frame end
    if ( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }
    
    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hStream, 0, &pImage );
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepthStream, 0, &pDepth );
    HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &m_SkeletonFrame );

    // Color stream
    if ( SUCCEEDED( hrImage ) )
    {
        if ( ++m_dwLatest[ 0 ] >= MAX_STREAMS )
            m_dwLatest[ 0 ] = 0;
        if ( ++m_dwLatest[ 1 ] >= MAX_STREAMS )
            m_dwLatest[ 1 ] = 0;

        m_pImageFrame[ m_dwLatest[ 0 ] ] = (NUI_IMAGE_FRAME*) pImage;
        if ( m_pImageFrame[ m_dwLatest[ 1 ] ] )
            NuiImageStreamReleaseFrame( m_hStream, (NUI_IMAGE_FRAME*) m_pImageFrame[ m_dwLatest[ 1 ] ] );

        DWORD frame = m_dwLatest[ 0 ];        
        if ( m_pImageFrame[ frame ] )
            pColorStream = m_pImageFrame[ frame ]->pFrameTexture;
    }    

    // Depth stream
    if ( SUCCEEDED( hrDepth ) )
    {
        if ( ++m_dwDepthLatest[ 0 ] >= MAX_STREAMS )
            m_dwDepthLatest[ 0 ] = 0;
        if ( ++m_dwDepthLatest[ 1 ] >= MAX_STREAMS )
            m_dwDepthLatest[ 1 ] = 0;

        m_pDepthImageFrame[ m_dwDepthLatest[ 0 ] ] = (NUI_IMAGE_FRAME*) pDepth;
        if ( m_pDepthImageFrame[ m_dwDepthLatest[ 1 ] ] )
            NuiImageStreamReleaseFrame( m_hDepthStream, (NUI_IMAGE_FRAME*) m_pDepthImageFrame[ m_dwDepthLatest[ 1 ] ] );

        DWORD frame = m_dwDepthLatest[0];        
        if ( m_pDepthImageFrame[frame] )
            pDepthStream = m_pDepthImageFrame[frame]->pFrameTexture;
    }  

    // Skeleton
    if ( SUCCEEDED( hrSkeleton ) )
    {
        // enable smoothing
        NuiTransformSmooth( &m_SkeletonFrame, NULL );

        // Find the first tracked skeleton
        if ( m_SkeletonFrame.SkeletonData[m_uCurrentSkeletonIndex].eTrackingState != NUI_SKELETON_TRACKED )
            for( UINT i = 0; i < NUI_SKELETON_COUNT; ++ i )
            {
                if ( m_SkeletonFrame.SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
                {
                    m_uCurrentSkeletonIndex = i;
                    break;
                }
            }

        // Calculate positions in screen space from joint positions 
        XMVECTOR* pSkeletonPositions = m_SkeletonFrame.SkeletonData[ m_uCurrentSkeletonIndex ].SkeletonPositions;
        CalcScreenSpace( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, m_ScreenSpace, pSkeletonPositions );

        const DWORD tableSkeleton[ 3 ] = {
            NUI_SKELETON_POSITION_HAND_RIGHT,
            NUI_SKELETON_POSITION_HAND_LEFT,
            NUI_SKELETON_POSITION_HIP_CENTER,
        };

        m_Brush.x = (FLOAT)m_ScreenSpace[ tableSkeleton[ m_iWhichJoint ] ].x;
        m_Brush.y = (FLOAT)m_ScreenSpace[ tableSkeleton[ m_iWhichJoint ] ].y;
        m_fCursorZ = pSkeletonPositions[ tableSkeleton[ m_iWhichJoint ] ].z;
        
        // Put the window at a proper distance (m_fDistanceToGlass) away from the player
        switch( tableSkeleton[ m_iWhichJoint ] )
        {
        case NUI_SKELETON_POSITION_HAND_RIGHT:
            if ( m_Brush.x < m_ScreenSpace[ NUI_SKELETON_POSITION_HIP_CENTER ].x )
                m_fGlassZ = pSkeletonPositions[ NUI_SKELETON_POSITION_HIP_RIGHT ].z - m_fDistanceToGlass;
            else
                m_fGlassZ = pSkeletonPositions[ NUI_SKELETON_POSITION_HIP_LEFT ].z - m_fDistanceToGlass;
            break;

        case NUI_SKELETON_POSITION_HAND_LEFT:
            if ( m_Brush.x < m_ScreenSpace[ NUI_SKELETON_POSITION_HIP_CENTER ].x )
                m_fGlassZ = pSkeletonPositions[ NUI_SKELETON_POSITION_HIP_LEFT ].z - m_fDistanceToGlass;
            else
                m_fGlassZ = pSkeletonPositions[ NUI_SKELETON_POSITION_HIP_RIGHT ].z - m_fDistanceToGlass;
            break;

        case NUI_SKELETON_POSITION_HIP_CENTER:
            m_fGlassZ = pSkeletonPositions[ NUI_SKELETON_POSITION_HEAD ].z - m_fDistanceToGlass;
            break;
        }

        if ( m_fCursorZ < m_fGlassZ ) 
        {
            m_Brush.size = (m_fGlassZ - m_fCursorZ) * 3.0f;
        }
        else
        {
            m_Brush.size = 0;
        }

        // "Full Body Stamp"
        if ( m_bFullBodyStampEnable && 
            pSkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ].z < m_fFullBodyStampZ &&
            pSkeletonPositions[ NUI_SKELETON_POSITION_HIP_LEFT ].z < m_fFullBodyStampZ &&
            pSkeletonPositions[ NUI_SKELETON_POSITION_HIP_RIGHT ].z < m_fFullBodyStampZ ) 
        {
            if ( m_iFullBodyStampWait < 1 && pDepthStream )
            {
                D3DLOCKED_RECT LockedDst, LockedSrc;
                
                m_pDepthTexture->LockRect( 0, &LockedDst, NULL, 0 );
                DWORD* pBitsDst = ( DWORD* )LockedDst.pBits;

                pDepthStream->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );
                USHORT* pBitsSrc = ( USHORT* )LockedSrc.pBits;

                for( UINT j = 0; j < 240; ++j )
                {
                    for( UINT i = 0; i < 320; ++i )
                    {
                        USHORT depth = *(pBitsSrc + i) & 0xFFF8;  
                        
                        if ( (*(pBitsSrc + i) & 7) == 1 )          
                            *(pBitsDst + i) = D3DCOLOR_ARGB( 64, 
                                                             0, 
                                                             0, 
                                                             (BYTE)(depth / 65535.0f * 255) * 2 );
                        else
                            *(pBitsDst + i) = D3DCOLOR_ARGB(0, 0, 0, 0);

                    }
                    pBitsSrc += LockedSrc.Pitch / sizeof(USHORT);
                    pBitsDst += LockedDst.Pitch / sizeof(DWORD);
                }

                pDepthStream->UnlockRect(0);
                m_pDepthTexture->UnlockRect(0);

                m_iFullBodyStampWait = FULLBODY_STAMP_WAIT;
            }
        }
    }

    // Copy color buffer to texture
    if ( pColorStream )
    {
        D3DLOCKED_RECT LockedDst, LockedSrc;
        m_pColorTexture->LockRect( 0, &LockedDst, NULL, 0 );
        pColorStream->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );

        XMemCpyStreaming_WriteCombined( LockedDst.pBits, LockedSrc.pBits, NUI_IMAGE_COLOR_640x480_BUFFER_SIZE );

        pColorStream->UnlockRect( 0 );
        m_pColorTexture->UnlockRect( 0 );
    }

    // Toggle help
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_dwInterpolation = 1 - m_dwInterpolation;

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bClearGlass = TRUE;

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        hr = DmMapDevkitDrive();

        if ( hr == S_OK )
        {
            // Save texture to file on devkit
            hr = D3DXSaveTextureToFile( "DEVKIT:\\FrostyWindow.png", D3DXIFF_PNG, m_pGlassTexture, NULL );
        }
    }

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        m_bFullBodyStampEnable = !m_bFullBodyStampEnable;

    if ( pGamepad->bPressedLeftTrigger )
    {
        if (--m_iWhichJoint < 0) m_iWhichJoint = 2;
    }
    if ( pGamepad->bPressedRightTrigger )
    {
        if (++m_iWhichJoint > 2) m_iWhichJoint = 0;
    }
    if ( pGamepad->sThumbLY < 0 )
    {
        m_fDistanceToGlass -= 0.01f;
    }
    if ( pGamepad->sThumbLY > 0 )
    {
        m_fDistanceToGlass += 0.01f;
    }
    if ( pGamepad->sThumbRY < 0 )
    {
        m_fFullBodyStampZ -= 0.01f;
    }
    if ( pGamepad->sThumbRY > 0 )
    {
        m_fFullBodyStampZ += 0.01f;
    }

    return S_OK;
}

VOID Sample::DrawFullScreenTexturedQuad( D3DTexture* pTex, D3DXCOLOR color /*= D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f)*/ )
{    
    m_pd3dDevice->SetPixelShader( m_pX2DPixelShader );
    m_pd3dDevice->SetPixelShaderConstantF( 0, color, 1 );
    m_pd3dDevice->SetTexture( 0, pTex );
    m_PostProcess.DrawFullScreenQuad();
}

VOID Sample::DrawFullScreenColorQuad( D3DXCOLOR color )
{
    m_pd3dDevice->SetPixelShader( m_pColorPS );
    m_pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT*)&color, 1 );    
    m_PostProcess.DrawFullScreenQuad();
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

    if ( m_bClearGlass )
    {
        // Clear the frosty window texture
        ATG::RenderBackground( 0xffffffff, 0xffffffff );
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pGlassTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );
        m_bClearGlass = FALSE;
    }

    // Clear the background
    ATG::RenderBackground( 0xffffffff, 0xffffffff );

    // Draw glass    
    DrawFullScreenTexturedQuad( m_pGlassTexture );    

    // Add frost, which make the strokes gradually disappear
    static DWORD dwFrostyWait = 0;
    if (++dwFrostyWait > 1)
    {
        dwFrostyWait = 0;
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
        m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
        
        DrawFullScreenColorQuad( D3DCOLOR_ARGB( 1, 1, 1, 1 ) );
    }

    if ( m_iFullBodyStampWait == FULLBODY_STAMP_WAIT )
    {
        // Draw "Full Body Stamp"
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCCOLOR );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );

        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
        
        DrawFullScreenTexturedQuad( m_pDepthTexture, D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f) );

        m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    }    

    // Draw brushstroke
    if ( m_SkeletonFrame.SkeletonData[ m_uCurrentSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
    {
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ZERO );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
        
        if ( m_dwInterpolation == 0 )
        {
            // Linear interpolation
            for( DWORD j = 0; j < 64; j++ )
            {
                XMVECTOR v0 = XMVectorSet( m_BrushStrokeHistory[ 0 ].x, m_BrushStrokeHistory[ 0 ].y, m_BrushStrokeHistory[ 0 ].size, 0.0f );
                XMVECTOR v1 = XMVectorSet( m_Brush.x, m_Brush.y, m_Brush.size, 0.0f );
                XMVECTOR v = XMVectorLerp( v0, v1, (FLOAT)j / 64.0f );

                D3DRECT rect;
                FLOAT fpow = v.z * 32.f;
                rect.x1 = (LONG)( v.x - fpow );
                rect.x2 = (LONG)( v.x + fpow );
                rect.y1 = (LONG)( v.y - fpow );
                rect.y2 = (LONG)( v.y + fpow );
                ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_pBrushTexture );
            }
        }
        else
        {
            // Spline interpolation
            static const XMVECTOR cvr = { 0.015873016f, 0.015873016f, 0.015873016f, 0.015873016f };
            static const XMVECTOR cvx = { 1.f, 3.f, 3.f, 1.f };
            static const XMVECTOR cvz = { -32.f, 32.f, -32.f, 32.f };
            const XMVECTOR vx = XMVectorSet( m_BrushStrokeHistory[ 2 ].x, m_BrushStrokeHistory[ 1 ].x, m_BrushStrokeHistory[ 0 ].x, m_Brush.x );
            const XMVECTOR vy = XMVectorSet( m_BrushStrokeHistory[ 2 ].y, m_BrushStrokeHistory[ 1 ].y, m_BrushStrokeHistory[ 0 ].y, m_Brush.y );
            const XMVECTOR vz = XMVectorSet( m_BrushStrokeHistory[ 2 ].size, m_BrushStrokeHistory[ 1 ].size, m_BrushStrokeHistory[ 0 ].size, m_Brush.size );
            XMVECTOR vt = XMVectorZero();
            XMVECTOR vs = g_XMOne;
            for( DWORD j = 0; j < 64; j++ )
            {
                XMVECTOR vtmp0 = XMVectorMultiply( __vrlimi(vs,vt,0x1,0), __vrlimi(vs,vt,0x3,0) );
                XMVECTOR vtmp1 = XMVectorMultiply( __vrlimi(vs,vt,0x7,0), cvx );
                XMVECTOR v = XMVectorMultiply( vtmp0, vtmp1 );
                vt = XMVectorAdd( vt, cvr );
                vs = XMVectorSubtract( vs, cvr );
                XMVECTOR vvx = XMVector4Dot( v, vx );
                XMVECTOR vvy = XMVector4Dot( v, vy );
                XMVECTOR vvz = XMVector4Dot( v, vz );
                vvx = XMVectorMultiplyAdd( vvz, cvz, vvx );
                vvy = XMVectorMultiplyAdd( vvz, cvz, vvy );

                D3DRECT rect;
                rect.x1 = (LONG)( XMVectorGetX( vvx ) );
                rect.x2 = (LONG)( XMVectorGetY( vvx ) );
                rect.y1 = (LONG)( XMVectorGetX( vvy ) );
                rect.y2 = (LONG)( XMVectorGetY( vvy ) );
                ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_pBrushTexture );
            }
        }

        D3DRECT rect;
        FLOAT fpow = m_Brush.size * 32.f;
        rect.x1 = (LONG)( m_Brush.x - fpow );
        rect.x2 = (LONG)( m_Brush.x + fpow );
        rect.y1 = (LONG)( m_Brush.y - fpow );
        rect.y2 = (LONG)( m_Brush.y + fpow );
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_pBrushTexture );
    }

    // Copy to glass
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pGlassTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );

    // Draw color buffer
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    DrawFullScreenTexturedQuad( m_pColorTexture );

    // Draw glass texture
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );

    DrawFullScreenTexturedQuad( m_pGlassTexture, D3DXCOLOR(1.0f, 1.0f, 1.0f, 0.5f) );

    // Draw hand cursor
    m_CursorPos[ 0 ].x = (FLOAT)m_Brush.x - 32;
    m_CursorPos[ 0 ].y = (FLOAT)m_Brush.y - 32;
    m_CursorPos[ 1 ].x = (FLOAT)m_Brush.x + 32;
    m_CursorPos[ 1 ].y = (FLOAT)m_Brush.y - 32;
    m_CursorPos[ 2 ].x = (FLOAT)m_Brush.x + 32;
    m_CursorPos[ 2 ].y = (FLOAT)m_Brush.y + 32;
    m_CursorPos[ 3 ].x = (FLOAT)m_Brush.x - 32;
    m_CursorPos[ 3 ].y = (FLOAT)m_Brush.y + 32;

    XMVECTOR color = XMVectorSet( 1.0f, 1.0f, 1.0f, ( m_fGlassZ + m_fDistanceToGlass - m_fCursorZ ) / m_fDistanceToGlass );

    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    m_pd3dDevice->SetTexture( 0, m_pCursorTexture );
    m_pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT*) &color, 1);
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, m_CursorPos, sizeof( m_CursorPos[ 0 ] ) );

#if 0
    // Debug draw for all the tracked joints in screen space
    for( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
    {
        XMFLOAT2 pos( (FLOAT) m_ScreenSpace[ i ].x, (FLOAT) m_ScreenSpace[ i ].y );
        XMFLOAT2 size( 8, 8 );
        ATG::DebugDraw::DrawScreenSpaceRect( pos, size, 1.0f, D3DCOLOR_ARGB( 255, 255, 255, 0 ) );
    }
#endif

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if ( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Frosty Window" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( -1, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        WCHAR temp[256];
        swprintf_s( temp, L"DistanceToGlass:%.2f", m_fDistanceToGlass);
        m_Font.DrawText( 0, 64, 0xffffff00, temp,  ATGFONT_LEFT );

        swprintf_s( temp, L"FullBodyStamp (%s):%.2f", m_bFullBodyStampEnable ? L"Enabled" : L"Disabled", m_fFullBodyStampZ);
        m_Font.DrawText( 0, 96, 0xffffff00, temp,  ATGFONT_LEFT );
        
        swprintf_s( temp, L"Cursor x:%d y:%d z:%.2f", (INT) m_Brush.x, (INT) m_Brush.y, m_fCursorZ);
        m_Font.DrawText( -1, 64, 0xffffff00, temp,  ATGFONT_RIGHT );

        swprintf_s( temp, L"Interpolation:%s", (m_dwInterpolation == 0) ? L"Linear" : L"Spline");
        m_Font.DrawText( -1, 96, 0xffffff00, temp,  ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: CalcScreenSpace()
// Desc: Calculate position in screen space from joint position.
//--------------------------------------------------------------------------------------
VOID Sample::CalcScreenSpace( DWORD dwDisplayWidth, DWORD dwDisplayHeight, D3DPOINT* pScreenSpace, XMVECTOR* pJoints )
{
    FLOAT fHalfWindowWidth = (FLOAT)dwDisplayWidth * 0.5f ;
    FLOAT fHalfWindowHeight = (FLOAT)dwDisplayHeight * 0.5f;    

    FLOAT fXColorRatioToDisplay = (FLOAT)dwDisplayWidth / 640;
    FLOAT fYColorRatioToDisplay = (FLOAT)dwDisplayHeight / 480;    
    
    // Project the world space joints into screen space
    for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; ++ i )
    {
        XMFLOAT3 vJointLocation;
        XMStoreFloat3( &vJointLocation, pJoints[ i ] );

        // Check for divide by zero
        if ( fabsf( vJointLocation.z ) > FLT_EPSILON  )
        {
            LONG plDepthX, plDepthY;
            USHORT usDepthValue;
            NuiTransformSkeletonToDepthImage( pJoints[ i ],
                &plDepthX, &plDepthY, &usDepthValue );  
                
            LONG plColorX, plColorY;
            
            HRESULT hr = NuiImageGetColorPixelCoordinatesFromDepthPixel( NUI_IMAGE_RESOLUTION_640x480,
                                                                        NULL,
                                                                        plDepthX,
                                                                        plDepthY,
                                                                        usDepthValue,
                                                                        &plColorX,
                                                                        &plColorY );
            if(FAILED(hr))
            {
                // couldn't map to color, use the depth position
                plColorX = plDepthX;
                plColorY = plDepthY;
            }
            
            pScreenSpace[i].x = (LONG)(( (FLOAT) plColorX ) * fXColorRatioToDisplay);
            pScreenSpace[i].y = (LONG)(( (FLOAT) plColorY ) * fYColorRatioToDisplay);
        }
        else
        {
            // A joint that is so close to the camera that its Z value is 0 can simply be drawn directly 
            // at the center of the 2D plane.
            pScreenSpace[ i ].x = (LONG)(fHalfWindowWidth);
            pScreenSpace[ i ].y = (LONG)(fHalfWindowHeight);
        }
    }
}
