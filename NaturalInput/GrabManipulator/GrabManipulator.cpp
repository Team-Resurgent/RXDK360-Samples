//--------------------------------------------------------------------------------------
// GrabManipulator.cpp
//
// The GrabManipulator sample demonstrates how to use hand open-closed recognition 
// with skeletal tracking to allow the user to grab and manipulate and object in 
// 3D space.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xnamath.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgSimpleShaders.h>
#include <AtgNuiVisualization.h>
#include <AtgNuiCommon.h>
#include <AtgDebugDraw.h>
#include <AtgCamera.h>
#include <AtgMesh.h>
#include <NuiApi.h>
#include <AtgNuiCommon.h>
#include "HOCDetector.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------

ATG::HELP_CALLOUT g_HelpCallouts[] = 
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Pause" },
    { ATG::HELP_LEFTSTICK, ATG::HELP_PLACEMENT_1, L"Move Camera" },
    { ATG::HELP_RIGHTSTICK, ATG::HELP_PLACEMENT_1, L"Rotate Camera" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Toggle seated/\nstanding pipeline" },
    { ATG::HELP_LEFT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Previous Display\nTechnique" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Next Display\nTechnique" }
};

//---------------------------------------------------------------------------------------------------------
// Constant declarations
//---------------------------------------------------------------------------------------------------------

static const INT g_nDepthWidth    = 320; 
static const INT g_nDepthHeight   = 240;
static const INT g_nColorWidth    = 640;
static const INT g_nColorHeight   = 480;
static const DWORD g_dwNumHelpCallouts = ARRAYSIZE(g_HelpCallouts);
static const DWORD g_dwConfidenceHistorySize = 128;
static const FLOAT g_fMeshCenterOffset = -0.8f;
static const FLOAT g_fConfidenceScale = 10.0f;
static const FLOAT g_fConfidenceThreshhold = 0.1f;
static const FLOAT g_fModelMinDistance = 3.0f;
static const FLOAT g_fModelMaxDistance = 25.0f;
static const FLOAT g_fModelTranslateScale = 20.0f;

//--------------------------------------------------------------------------------------
// Hand enumeration
//--------------------------------------------------------------------------------------

enum NUI_HAND
{
	HAND_RIGHT = 0,
	HAND_LEFT,
};

//--------------------------------------------------------------------------------------
// Name: FilteredResult
// Desc: Filtered per classifier per hand result
//--------------------------------------------------------------------------------------
struct FilteredResult
{
    HOCFilter   m_filter[ 2 ];                  // [0] - right, [1] - left
    BOOL        m_bLastTracked[ 2 ];            // whether the confidence was set last frame

    FilteredResult()
    {
        m_bLastTracked[ 0 ] = m_bLastTracked[ 1 ] = FALSE;
    }

    void AddConfidence( UINT uHand, FLOAT fConfidence )
    {
        m_filter[ uHand ].FilterType1( fConfidence );
    }
};

//--------------------------------------------------------------------------------------
// Name: PerPlayerRecord
// Desc: All the data we need per player. Can hold information for up to two hands
//--------------------------------------------------------------------------------------
struct PerPlayerRecord
{
    HOCDataViews    m_hocData[ 2 ];            // this is transient and only kept here for display and debug purposes, [0]-right, [1]-left
    FLOAT           m_fPlayerSize;             // player size (shoulder->neck->shoulder)

    FilteredResult m_result;        // filtered classifier results

    PerPlayerRecord()
    {
        m_fPlayerSize = 0;
    }
};
    
//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer                  m_Timer;
    ATG::Font                   m_Font;
    ATG::Help                   m_Help;
    BOOL                        m_bDrawHelp;
    BOOL                        m_bPaused;
    ATG::PackedResource         m_xprResource; 

    // Picture-in-picture NUI vizualization
    ATG::NuiVisualization       m_pip;

    // Nui Handles
    HANDLE                      m_hDepthDoneEvent;
    HANDLE                      m_hSkeletonDoneEvent;
    HANDLE                      m_hDepth;

    const NUI_IMAGE_FRAME *     m_pDepthFrame;

    // Skeleton data
    NUI_SKELETON_FRAME          m_SkeletonFrame;

    // Camera variables
    FLOAT                       m_fAspectRatio;
    XMMATRIX                    m_matWorld;
    XMMATRIX                    m_matCameraView;
    XMMATRIX                    m_matCameraProj;

    // NUI FOV values
    FLOAT                       m_fNuiHorizontalFOV;
    FLOAT                       m_fNuiVerticalFOV;

    XMVECTOR                    m_vLightDir;

    // Hand open-closed stuff.
    HOCDetector                 m_Classifier;
    USHORT*                     m_pDepth;
    INT                         m_nNearestSkeletonIndex;
    PerPlayerRecord             m_player;

    // The mesh we're manipulating, and associated stuff.
    ATG::Mesh                   m_Mesh;
    LPDIRECT3DVERTEXSHADER9     m_pMeshVS;
    LPDIRECT3DPIXELSHADER9      m_pMeshPS;
    D3DTexture *                m_pTexture;

    // Manipulation variables  
    BOOL                        m_bGrabbedRight;
    BOOL                        m_bGrabbedLeft;

    XMVECTOR                    m_vecLastFrameArmDirection;
    XMVECTOR                    m_vecRotationQuaternion;
    XMVECTOR                    m_vecLastFrameHandPosition;
    FLOAT                       m_fDepthTranslate;

    FLOAT                       m_aLeftHandConfidenceHistory[ g_dwConfidenceHistorySize ];
    FLOAT                       m_aRightHandConfidenceHistory[ g_dwConfidenceHistorySize ];
    FLOAT                       m_aLeftHandUnfilteredHistory[ g_dwConfidenceHistorySize ];
    FLOAT                       m_aRightHandUnfilteredHistory[ g_dwConfidenceHistorySize ];
    UINT                        m_nConfidenceWritePosition;

private:

    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
    HRESULT InitializeNui();
    void RenderConfidenceHistory( FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight, FLOAT * aConfidenceValues, FLOAT * aUnfilteredConfidence );

    // Hand detection functions
    VOID UpdateHandsConfidence();
	VOID UpdateHandStates();
    VOID GetNearestSkeleton();
};

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    atgApp.m_d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Name: InitializeNui()
// Desc: Initialize the NUI camera
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeNui()
{
    HRESULT hr = S_OK;

	m_pDepth = new USHORT[ g_nDepthWidth * g_nDepthHeight ];
	if( NULL == m_pDepth )
	{
		ATG_PrintError( "Depth working buffer allocation failed.\n" );
		return E_FAIL;
	}

    m_hSkeletonDoneEvent = CreateEvent( NULL,
                                        FALSE,  // Auto-reset
                                        FALSE,  // Initially unsignalled
                                        "NuiSkeletonDoneEvent" );
    if( NULL == m_hSkeletonDoneEvent )
    {
        ATG_PrintError( "Initialization of NuiSkeletonDoneEvent failed\n" );
        return E_FAIL;
    }

    m_hDepthDoneEvent = CreateEvent( NULL,
                                     FALSE,  // Auto-reset
                                     FALSE,  // Initially unsignalled
                                     "NuiDepthDoneEvent" );
    if( NULL == m_hDepthDoneEvent )
    {
        ATG_PrintError( "Initialization of NuiDepthDoneEvent failed\n" );
        return E_FAIL;
    }

    // Initialize the Natural Input System on the default thread.
    hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | 
                        NUI_INITIALIZE_FLAG_USES_SKELETON |
                        NUI_INITIALIZE_FLAG_EXTRAPOLATE_FLOOR_PLANE,
                        NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return hr;
    }

    // Open the depth stream
    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, // Get depth and player (segmentation) info
                             NUI_IMAGE_RESOLUTION_320x240,          // Get depth at 320x240
                             0,                                     // No flags
                             1,                                     // Only buffer 1 frame of depth data
                             m_hDepthDoneEvent,                    // We don't use a next-frame event
                             &m_hDepth );                           // Image handle
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return hr;
    }

    m_pDepthFrame = NULL;

    // Enable skeleton tracking. Use the seated pipeline, as we only track hands.
    hr = NuiSkeletonTrackingEnable( m_hSkeletonDoneEvent, NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
        return hr;
    }

    // Initialize the Picture-in-Picture vizualization.
    hr = m_pip.Initialize( m_pd3dDevice, 
                           NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON,
                           NUI_IMAGE_RESOLUTION_640x480, 
                           TRUE );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Picture-in-Picture initialization failed!" );
        return hr;
    }

    hr = m_Classifier.Load( "game:\\Media\\opt.hoc" );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Failed to load hand open/closed classifiers!" );
        return hr;
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This initializes all objects and values used by the sample.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr = S_OK;

    m_bDrawHelp = FALSE;
    m_bPaused = FALSE;
    m_bGrabbedLeft = FALSE;
    m_bGrabbedRight = FALSE;

	XMemSet( m_aLeftHandConfidenceHistory, 0, g_dwConfidenceHistorySize * sizeof( FLOAT ) );
	XMemSet( m_aRightHandConfidenceHistory, 0, g_dwConfidenceHistorySize * sizeof( FLOAT ) );
	XMemSet( m_aLeftHandUnfilteredHistory, 0, g_dwConfidenceHistorySize * sizeof( FLOAT ) );
	XMemSet( m_aRightHandUnfilteredHistory, 0, g_dwConfidenceHistorySize * sizeof( FLOAT ) );

    hr = InitializeNui();
    if( FAILED( hr ) )
    {
        return hr;
    }

    // Initialize the skeleton
    XMemSet( &m_SkeletonFrame, 0, sizeof( m_SkeletonFrame ) );
    m_nNearestSkeletonIndex = 0;

    // Calculate NUI-camera to world space values.

    // NUI constants
    m_fNuiHorizontalFOV  = NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV;
    m_fNuiVerticalFOV    = NUI_CAMERA_DEPTH_NOMINAL_VERTICAL_FOV;

    // Double-check FOV & aspect ratio consistent with each other (should be roughly correct for default values)
    FLOAT FovAngleY      = XMConvertToRadians( m_fNuiVerticalFOV );
    FLOAT AspectRatio    = ( FLOAT )g_nDepthWidth / ( FLOAT )g_nDepthHeight;
    FLOAT FovAngleX      = 2.0f * atanf( AspectRatio * tan( FovAngleY / 2.0f ) ); 
    m_fNuiHorizontalFOV  = XMConvertToDegrees( FovAngleX ); // actually around 57.8

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        return ATGAPPERR_MEDIANOTFOUND;
    }

    if( FAILED( m_xprResource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        return ATGAPPERR_MEDIANOTFOUND;
    }

    if( FAILED( hr = m_Mesh.Create( "game:\\Media\\Meshes\\dwarf.xbg", &m_xprResource ) ) )
    {
        ATG_PrintError( "Failed to load teapot mesh!" );
        return hr;
    }

    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\TransformLitVerticesVS.xvu", &m_pMeshVS ) ) )
    {
        ATG_PrintError( "Failed to load vertex shader!" );
        return hr;
    }

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\SimplePerPixelLightingPS.xpu", &m_pMeshPS ) ) )
    {
        ATG_PrintError( "Failed to load pixel shader!" );
        return hr;
    }

    m_pTexture = m_xprResource.GetTexture( "ChessBishop.bmp" );

    m_vLightDir = XMVector3Normalize( XMVectorSet( 0.5f, 0, 1.5f, 0 ) );

    m_matCameraView = XMMatrixIdentity();

    // Set up world transform.
    m_matWorld = XMMatrixIdentity();

    // Set up projection matrix
    const FLOAT fZNear = 0.1f;
    const FLOAT fZFar = 250.0f;     // Far depth-plane at 250 meters.
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / (FLOAT)m_d3dpp.BackBufferHeight;
    m_matCameraProj = XMMatrixPerspectiveFovLH( XM_PI / 2.5f, fAspectRatio, fZNear, fZFar );

    m_vecLastFrameArmDirection = XMQuaternionIdentity();
    m_fDepthTranslate = 5.0f;

    m_vecRotationQuaternion = XMQuaternionIdentity();

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    m_nConfidenceWritePosition = 0;
    XMemSet( m_aLeftHandConfidenceHistory, 0, sizeof( m_aLeftHandConfidenceHistory ) );
    XMemSet( m_aRightHandConfidenceHistory, 0, sizeof( m_aRightHandConfidenceHistory ) );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------

HRESULT Sample::Update()
{
    HRESULT hr = S_OK;

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Pause the timer
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        m_bPaused = !m_bPaused;

        if( m_bPaused )   
        {
            m_Timer.Stop();
        }
        else
        {
            m_Timer.Start();
        }
    }

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Go no further if the app is paused.
    if( m_bPaused )
        return S_OK;

    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hSkeletonDoneEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }

    // Release any previous frame depth reference
    if( NULL != m_pDepthFrame )
    {
        NuiImageStreamReleaseFrame( m_hDepth, m_pDepthFrame );
        m_pDepthFrame = NULL;
    }

    // Grab a copy of the depth surface for this frame.
    hr = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );
    if( SUCCEEDED( hr ) )
    {
        // Copy depth frame data to our hand-tracking buffer.
        D3DLOCKED_RECT rectDepth;
        m_pDepthFrame->pFrameTexture->LockRect( 0, &rectDepth, NULL, 0 );
        for( UINT y = 0; y < g_nDepthHeight; ++y )
        {
            XMemCpy( &m_pDepth[ y * g_nDepthWidth ], ( BYTE* )rectDepth.pBits + rectDepth.Pitch * y, g_nDepthWidth * sizeof( USHORT ) );
        }

        m_pDepthFrame->pFrameTexture->UnlockRect( 0 );
    }

    // Get the next frame's skeleton data (if we have any)
    hr = NuiSkeletonGetNextFrame( NUI_CAMERA_TIMEOUT_DEFAULT, &m_SkeletonFrame );
    if( SUCCEEDED( hr ) )
    {
        NuiTransformSmooth( &m_SkeletonFrame, NULL );
        m_pip.SetSkeletons( &m_SkeletonFrame );
    }

    GetNearestSkeleton();
    UpdateHandsConfidence();
	UpdateHandStates();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------

HRESULT Sample::Render()
{
    // Draw a solid black background
    ATG::RenderBackground( 0xff000000, 0xff000000 );

    // Render the picture-in-picture components (dash-style preview, plus first tracked skeleton)
    if( m_pDepthFrame != NULL )
    {
        const FLOAT drawWidth = 200.0f;
        const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
        const FLOAT drawY = 720.0f - 50.0f - drawHeight;
        FLOAT drawX = 50.0f;

        m_pip.SetDepthTexture( m_pDepthFrame->pFrameTexture );

        // Render any tracked skeletons...
        for( INT dwSkeleton = 0; dwSkeleton < NUI_SKELETON_COUNT; ++dwSkeleton )
        {
            m_pip.RenderSingleSkeleton( dwSkeleton, drawX, drawY, drawWidth, drawHeight, FALSE, FALSE );
        }

        drawX += drawWidth + 20;
        m_pip.RenderDashStyleDepthPreview( drawX, drawY, drawWidth, drawHeight );

        m_pip.SetDepthTexture( NULL );
    }

    // Render the mesh.
    XMMATRIX matRotation = XMMatrixRotationQuaternion( m_vecRotationQuaternion );
    XMMATRIX matWorld = XMMatrixTranslationFromVector( XMVectorSet( 0.0f, g_fMeshCenterOffset, 0.0f, 0.0f ) ) * XMMatrixScaling( 1.0f, 1.0f, 1.0f ) * matRotation * XMMatrixTranslationFromVector( XMVectorSet( 0.0f, 0.0f, m_fDepthTranslate, 0.0f ) );
    XMMATRIX matSet[2];
    matSet[ 0 ] = XMMatrixTranspose( matWorld * m_matCameraProj );
    matSet[ 1 ] = XMMatrixTranspose( matRotation );

    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )matSet, 8 );

    XMVECTOR vecLight = XMVector4Normalize( XMVectorSet( 0.5f, 0.1f, 1.5f, 0.0f ) );
    XMVECTOR vecLightColor = XMVectorSet( 1.0f, 1.0f, 1.0f, 1.0f );

    m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&vecLightColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&vecLight, 1 );

    m_pd3dDevice->SetVertexShader( m_pMeshVS );
    m_pd3dDevice->SetPixelShader( m_pMeshPS );

    m_pd3dDevice->SetTexture( 0, m_pTexture );

    m_Mesh.Render( ATG::MESH_NOMATERIALS | ATG::MESH_NOTEXTURES );

    m_pd3dDevice->SetTexture( 0, NULL );

    m_pd3dDevice->SetVertexShader( NULL );
    m_pd3dDevice->SetPixelShader( NULL );

    RenderConfidenceHistory( 100.0f, 260.0f, 128.0f, 60.0f, m_aLeftHandConfidenceHistory, m_aLeftHandUnfilteredHistory );
    RenderConfidenceHistory( ( FLOAT )m_d3dpp.BackBufferWidth - 328.0f, 260.0f, 128.0f, 60.0f, m_aRightHandConfidenceHistory, m_aRightHandUnfilteredHistory );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
		m_Help.Render( &m_Font, g_HelpCallouts, g_dwNumHelpCallouts );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff,  L"GrabManipulator Sample" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderConfidenceHistory()
// Desc: Renders a line graph showing the history of hand open-closed confidence, showing
// both filtered and unfiltered values.
//--------------------------------------------------------------------------------------

void Sample::RenderConfidenceHistory( FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight, FLOAT * aValues, FLOAT * aUnfiltered )
{
    XMFLOAT2 aLineList[ g_dwConfidenceHistorySize ];
    D3DCOLOR aColors[ g_dwConfidenceHistorySize ];

    XMFLOAT2 aLineListUnfiltered[ g_dwConfidenceHistorySize ];
    D3DCOLOR aColorsUnfiltered[ g_dwConfidenceHistorySize ];

    UINT nIndex = m_nConfidenceWritePosition;
    for( UINT i = 0; i < g_dwConfidenceHistorySize; ++i )
    {
        if( aValues[ nIndex ] < 0 )
        {
            aColors[ i ] = D3DCOLOR_ARGB( 255, 255, 0, 0 );
        }
        else
        {
            aColors[ i ] = D3DCOLOR_ARGB( 255, 0, 255, 0 );
        }

        aLineList[ i ].x = fX + ( FLOAT )i;
        aLineList[ i ].y = fY + ( 1 - ( ( aValues[ nIndex ] + 1 ) * 0.5f ) ) * fHeight;

        if( aUnfiltered != NULL )
        {
            if( aUnfiltered[ nIndex ] < 0 )
            {
                aColorsUnfiltered[ i ] = D3DCOLOR_ARGB( 255, 255, 255, 0 );
            }
            else
            {
                aColorsUnfiltered[ i ] = D3DCOLOR_ARGB( 255, 0, 0, 255 );
            }

            aLineListUnfiltered[ i ].x = fX + ( FLOAT )i;
            aLineListUnfiltered[ i ].y = fY + ( 1 - ( ( aUnfiltered[ nIndex ] + 1 ) * 0.5f ) ) * fHeight;
        }

        ++nIndex;
        nIndex %= g_dwConfidenceHistorySize;
    }

    XMFLOAT2 Origin( fX, fY + fHeight );
    XMFLOAT2 xAxisEnd( fX + fWidth, fY + fHeight );
    XMFLOAT2 yAxisEnd( fX, fY );
    ATG::DebugDraw::DrawScreenSpaceLine( Origin, xAxisEnd, D3DCOLOR_ARGB( 255, 255, 255, 255 ) );
    ATG::DebugDraw::DrawScreenSpaceLine( Origin, yAxisEnd, D3DCOLOR_ARGB( 255, 255, 255, 255 ) );

    Origin.y = fY + fHeight / 2.0f;
    ++Origin.x;
    xAxisEnd.y = Origin.y;
    ATG::DebugDraw::DrawScreenSpaceLine( Origin, xAxisEnd, D3DCOLOR_ARGB( 255, 0, 0, 255 ) );

    for( UINT i = 0; i < g_dwConfidenceHistorySize - 1; ++i )
    {
        ATG::DebugDraw::DrawScreenSpaceLine( aLineList[ i ], aColors[ i ], aLineList[ i + 1 ], aColors[ i + 1 ] );
    }

    for( UINT i = 0; i < g_dwConfidenceHistorySize - 1 && NULL != aUnfiltered; ++i )
    {
        ATG::DebugDraw::DrawScreenSpaceLine( aLineListUnfiltered[ i ], aColorsUnfiltered[ i ], aLineListUnfiltered[ i + 1 ], aColorsUnfiltered[ i + 1 ] );
    }
}

//--------------------------------------------------------------------------------------
// Name: GetNearestSkeleton()
// Desc: Obtains the index of the skeleton nearest the camera.
//--------------------------------------------------------------------------------------

VOID Sample::GetNearestSkeleton()
{
    FLOAT fSkeletonPositionZ = FLT_MAX;
    for( UINT uCurrentSkeleton = 0; uCurrentSkeleton < NUI_SKELETON_COUNT; ++uCurrentSkeleton )
    {
        if( m_SkeletonFrame.SkeletonData[ uCurrentSkeleton ].eTrackingState == NUI_SKELETON_TRACKED )
        {
            FLOAT fZ = XMVectorGetZ( m_SkeletonFrame.SkeletonData[ uCurrentSkeleton ].Position );
            if( fZ < fSkeletonPositionZ )
            {
                fSkeletonPositionZ = fZ;
                m_nNearestSkeletonIndex = ( INT )uCurrentSkeleton;
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: UpdateHandsConfidence()
// Desc: Updates the confidence values of both left and right hand states.
//--------------------------------------------------------------------------------------

VOID Sample::UpdateHandsConfidence()
{
    PIXBeginNamedEvent( 0, "Acquire and Detect Hand(s)" );

    CONST NUI_SKELETON_DATA * pSkeleton = &m_SkeletonFrame.SkeletonData[ m_nNearestSkeletonIndex ];
    CONST XMVECTOR* __restrict pPositions = pSkeleton->SkeletonPositions;

    FLOAT fSize;
    if( HOCDetector::GetPlayerSize( fSize, pSkeleton ) )
    {
        // Apply low-pass filter to player size.
        m_player.m_fPlayerSize = 0.98f * m_player.m_fPlayerSize + 0.02f * fSize;
    }


    for( UINT uHand = 0; uHand < 2; ++uHand )
    {
        m_player.m_result.m_bLastTracked[ uHand ] = FALSE;

        const BOOL bRightHand = ( 0 == uHand ) ? TRUE : FALSE;

        const NUI_SKELETON_POSITION_INDEX nHand  = bRightHand ? NUI_SKELETON_POSITION_HAND_RIGHT : NUI_SKELETON_POSITION_HAND_LEFT;
        const NUI_SKELETON_POSITION_INDEX nWrist = bRightHand ? NUI_SKELETON_POSITION_WRIST_RIGHT : NUI_SKELETON_POSITION_WRIST_LEFT;
        const NUI_SKELETON_POSITION_INDEX nElbow = bRightHand ? NUI_SKELETON_POSITION_ELBOW_RIGHT : NUI_SKELETON_POSITION_ELBOW_LEFT;

        // Use ST to tell us where everything is.
        if( pSkeleton->eSkeletonPositionTrackingState[ nHand ] == NUI_SKELETON_POSITION_TRACKED )
        {
            const BOOL bElbowTracked = pSkeleton->eSkeletonPositionTrackingState[ nElbow ] == NUI_SKELETON_POSITION_TRACKED;
            const BOOL bWristTracked = pSkeleton->eSkeletonPositionTrackingState[ nWrist ] == NUI_SKELETON_POSITION_TRACKED;
                
            if( HOCDetector::GetFrameData(  m_player.m_hocData[ uHand ],  // can have a temporary of this type instead, only keeping for display
                                            bElbowTracked, bWristTracked,
                                            pPositions[ nElbow ],
                                            pPositions[ nWrist ],
                                            pPositions[ nHand ],
                                            m_pDepth,
                                            m_player.m_fPlayerSize ) )
            {
                // now run our classifiers
                m_player.m_result.m_bLastTracked[ uHand ] = TRUE;

                // filter confidence
                m_player.m_result.AddConfidence( uHand, m_Classifier.Detect( m_player.m_hocData[ uHand ] ) );
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: UpdateHandStates()
// Desc: Updates the current state (open/closed) of the left and right hands.
//--------------------------------------------------------------------------------------

VOID Sample::UpdateHandStates()
{
    // We want to filter (or smooth) out false negatives. To do this, we'll compare the confidence with the previous value, and
    // if it's over our threshold, we'll just do a small adjustment to this frame's value.
    UINT nNewWritePosition = ( m_nConfidenceWritePosition + 1 ) % g_dwConfidenceHistorySize;

    FLOAT fConfidence = m_player.m_result.m_filter[ HAND_RIGHT ].m_fFilteredConfidence;
    m_aRightHandConfidenceHistory[ nNewWritePosition ] = fConfidence;
    m_aRightHandUnfilteredHistory[ nNewWritePosition ] = m_player.m_result.m_filter[ HAND_RIGHT ].m_fConfidence * g_fConfidenceScale;

    // Right hand grabs...
    if( fConfidence > g_fConfidenceThreshhold && 
            m_SkeletonFrame.SkeletonData[ m_nNearestSkeletonIndex ].eTrackingState != NUI_SKELETON_NOT_TRACKED && 
            m_SkeletonFrame.SkeletonData[ m_nNearestSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HAND_RIGHT ] == NUI_SKELETON_POSITION_TRACKED &&
            m_SkeletonFrame.SkeletonData[ m_nNearestSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ] == NUI_SKELETON_POSITION_TRACKED )
    {
		// Get the "arm" vector as the normalized vector from the shoulder to the hand.
        XMVECTOR vecArm = m_SkeletonFrame.SkeletonData[ m_nNearestSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ] -
                            m_SkeletonFrame.SkeletonData[ m_nNearestSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
        vecArm = XMVector3Normalize( vecArm );

        // When we detect a grab, just store this frame's arm direction.
        if( !m_bGrabbedRight )
        {
            m_bGrabbedRight = TRUE;

            m_vecLastFrameArmDirection = vecArm;
        }
        else
        {
            // Only do something if there's a meaningful angle between the vectors.
            FLOAT flAngle = XMVectorGetX( XMVector3AngleBetweenNormals( vecArm, m_vecLastFrameArmDirection ) );
            if( flAngle > FLT_EPSILON )
            {
                XMVECTOR vecAxisOfRotation = XMVector3Cross( vecArm, m_vecLastFrameArmDirection );
                m_vecRotationQuaternion = XMQuaternionMultiply( m_vecRotationQuaternion, XMQuaternionRotationAxis( vecAxisOfRotation, -flAngle * 2 ) );
                m_vecRotationQuaternion = XMVector4Normalize( m_vecRotationQuaternion );
                m_vecLastFrameArmDirection = vecArm;
            }
        }
    }
    else
    {
        if( m_bGrabbedRight )
        {
            m_bGrabbedRight = FALSE;
        }
    }

    fConfidence = m_player.m_result.m_filter[ HAND_LEFT ].m_fFilteredConfidence;
    m_aLeftHandConfidenceHistory[ nNewWritePosition ] = fConfidence;
    m_aLeftHandUnfilteredHistory[ nNewWritePosition ] = m_player.m_result.m_filter[ HAND_LEFT ].m_fConfidence * g_fConfidenceScale;

    // The left hand controls position...
    if( fConfidence > g_fConfidenceThreshhold && 
        m_SkeletonFrame.SkeletonData[ m_nNearestSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HAND_LEFT ] == NUI_SKELETON_POSITION_TRACKED )
    {
        if( !m_bGrabbedLeft )
        {
            m_vecLastFrameHandPosition = m_SkeletonFrame.SkeletonData[ m_nNearestSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ];
            m_bGrabbedLeft = TRUE;
        }
        else
        {
            XMVECTOR vecHand = m_SkeletonFrame.SkeletonData[ m_nNearestSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ];
            XMVECTOR vecHandTranslate = vecHand - m_vecLastFrameHandPosition;
            m_fDepthTranslate = min( max( g_fModelMinDistance, m_fDepthTranslate - XMVectorGetZ( vecHandTranslate ) * g_fModelTranslateScale ), g_fModelMaxDistance );
            m_vecLastFrameHandPosition = vecHand;
        }
    }
    else
    {
        if( m_bGrabbedLeft )
        {
            m_bGrabbedLeft = FALSE;
        }
    }

    ++m_nConfidenceWritePosition;
    m_nConfidenceWritePosition %= g_dwConfidenceHistorySize;
}

