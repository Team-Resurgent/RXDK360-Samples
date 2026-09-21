//--------------------------------------------------------------------------------------
// HeadOrientation.cpp
//
// Microsoft Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>
#include <nuiapi.h>
#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgNuiCommon.h>
#include <AtgNuiVisualization.h>
#include <AtgSimpleShaders.h>


//--------------------------------------------------------------------------------------
// Name: Sizes and colors
// Desc: Define sizes and colors for various sample elements.
//--------------------------------------------------------------------------------------

// Skeleton colors
const D3DCOLOR SKELETON_COLOR = D3DCOLOR_ARGB( 0xff, 0x90, 0x90, 0x90 );

// Colors assigned to the different players
const D3DCOLOR g_aPlayerMainColor[ NUI_SKELETON_MAX_TRACKED_COUNT ]      = { D3DCOLOR_ARGB( 0xff, 0xff, 0x66, 0x00 ), 
                                                                             D3DCOLOR_ARGB( 0xff, 0x00, 0xa8, 0x00 )  };
const D3DCOLOR g_aPlayerHighlightColor[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { D3DCOLOR_ARGB( 0xff, 0xff, 0x98, 0x53 ), 
                                                                             D3DCOLOR_ARGB( 0xff, 0x66, 0xff, 0x66 )  };

// Size of the color image on screen
const DWORD COLOR_IMAGE_RENDER_WIDTH  = 640;
const DWORD COLOR_IMAGE_RENDER_HEIGHT = 480;


//--------------------------------------------------------------------------------------
// Name: struct PLAYER_INFO
// Desc: Holds the data specific to a player
//--------------------------------------------------------------------------------------
struct PLAYER_INFO
{
    DWORD    dwTrackingID;    // Match the NUI tracking ID from skeleton and head position APIs
    D3DCOLOR MainColor;       // Color used to represent player assets on screen
    D3DCOLOR HighlightColor;  // Color used to highlight player assets on screen
    XMFLOAT2 UIOrigin;        // Screen coordinate around which the player's UI will be centered
    XMFLOAT2 RollOrigin;      // Screen coordinate at which the roll vector will be displayer
};


//--------------------------------------------------------------------------------------
// Name: GetSkeletonDataFromTrackingID()
// Desc: Returns a pointer to the NUI_SKELETON_DATA structure belonging to the skeleton 
//       identified by dwTrackingID.
//       It returns NULL if no matching skeleton is found.
//--------------------------------------------------------------------------------------
const NUI_SKELETON_DATA* GetSkeletonDataFromTrackingID( const NUI_SKELETON_FRAME* pSkeletonFrame, DWORD dwTrackingID )
{
    assert( pSkeletonFrame != NULL );
    assert( dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID );
    
    const NUI_SKELETON_DATA* pSkeletonData = NULL;
    for( DWORD dwSkeletonIndex = 0; dwSkeletonIndex < NUI_SKELETON_COUNT; ++ dwSkeletonIndex )
    {
        if( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].dwTrackingID == dwTrackingID )
        {
            pSkeletonData = &pSkeletonFrame->SkeletonData[ dwSkeletonIndex ];
        }
    }
    
    return pSkeletonData;
}


//--------------------------------------------------------------------------------------
// Name: QuaternionToPitchYawRoll()
// Desc: Extract distinct pitch-Yaw-roll components from a quaternion.
// Note: The pitch yaw roll computed by this function is in pitch-yaw-roll order, which 
//       can not be used for XMQuaternionRotationRollPitchYaw, that function requires 
//       roll-pitch-yaw order 
//--------------------------------------------------------------------------------------
inline VOID QuaternionToPitchYawRoll( const XMVECTOR* pQ, FLOAT* pPitch, FLOAT* pYaw, FLOAT* pRoll)
{
    XMVECTOR vQ = XMQuaternionNormalize( *pQ );

    FLOAT x = XMVectorGetX( vQ );
    FLOAT y = XMVectorGetY( vQ );
    FLOAT z = XMVectorGetZ( vQ );
    FLOAT w = XMVectorGetW( vQ );

    *pPitch = atan2( 2 * ( y * z + w * x ), w * w - x * x - y * y + z * z );
    *pYaw   = asin( 2 * ( w * y - x * z ) );
    *pRoll  = atan2( 2 * ( x * y + w * z ), w * w + x * x - y * y - z * z );
}


//--------------------------------------------------------------------------------------
// Name: GetRollVector()
// Desc: Computes the normalized vector (between 3PI/2 and PI/2) representing the 
//       specified roll ratio.
//--------------------------------------------------------------------------------------
XMVECTOR GetRollVector( FLOAT fRollRatio )
{
    assert( fRollRatio >= -1.0f && fRollRatio <= 1.0f );
    FLOAT fRollAngle = fRollRatio * XM_PI * 0.5f;
    if( fRollAngle < 0.0f )
    {
        fRollAngle += 2 * XM_PI;
    }

    XMMATRIX mRollMatrix = XMMatrixRotationZ( fRollAngle );
    XMVECTOR vRoll = XMVectorSet( 0.0f, -1.0f, 0.0f, 0.0f  );
    vRoll = XMVector3Transform( vRoll, mRollMatrix );

    return vRoll;
}


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    PLAYER_INFO* GetPlayerInfoFromTrackingID( DWORD dwTrackingID );


private:

    // General sample data
    ATG::Timer                 m_Timer;       // General usage timer
    ATG::Font                  m_Font;        // Font used to display text
    ATG::NuiVisualization      m_NuiRenderer; // Used to render the depth map

    // Data used to recover Kinect depth map and head tracking information
    HANDLE                     m_hSkeletonFrameEndEvent;        // This event is signaled when a new skeleton frame is available
    HANDLE                     m_hColor640x480;                 // Handle to the color stream (used only for display in this sample)
    HANDLE                     m_hHeadOrientationFrameEndEvent; // This event is signaled when head tracking processing has completed
    NUI_HEAD_ORIENTATION_FRAME m_HeadOrientationFrame;          // Data from the latest head tracking frame

    // Player specific data for each tracked players
    PLAYER_INFO m_aPlayerInfo[ NUI_SKELETON_MAX_TRACKED_COUNT ]; 
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Sample::Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize general sample states


    // Create the font
    RETURN_ON_FAIL( m_Font.Create( "game:\\Media\\Fonts\\Arial_20.xpr" ) );

    // Confine text drawing to the safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the simple shaders, sensor and menus
    ATG::SimpleShaders::Initialize( NULL, NULL );

    ZeroMemory( &m_HeadOrientationFrame, sizeof( m_HeadOrientationFrame ) );

    // Create an event to be signaled when a new skeleton frame is available
    m_hSkeletonFrameEndEvent = CreateEvent( NULL,
                                            FALSE,  // auto-reset
                                            FALSE,  // create unsignaled
                                            "NuiSkeletonFrameEndEvent" );
    if ( ! m_hSkeletonFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiSkeletonFrameEndEvent" );
        return E_FAIL;
    }

    // Create an event to be signaled when head orientation processing ends
    m_hHeadOrientationFrameEndEvent = CreateEvent( NULL,
                                                   FALSE,  // auto-reset
                                                   FALSE,  // create unsignaled
                                                   "NuiHeadOrientationFrameEndEvent" );
    if ( ! m_hHeadOrientationFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent" );
        return E_FAIL;
    }

    // Initializes the Natural Input system on the default thread
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_HEAD_ORIENTATION | 
                                    NUI_INITIALIZE_FLAG_USES_SKELETON | 
                                    NUI_INITIALIZE_FLAG_USES_COLOR | 
                                    NUI_INITIALIZE_FLAG_NUI_GUIDE_DISABLED, 
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    // Open the 640 x 480 color stream.
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 1, NULL, &m_hColor640x480 );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen color image 640x480" );
        return E_FAIL;
    }

    // Initialize NUI skeleton tracking
    hr = NuiSkeletonTrackingEnable( m_hSkeletonFrameEndEvent, 0 );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't initialize NUI skeletal tracking\n" );
        return hr;
    }

    // Enable head orientation tracking
    hr = NuiHeadOrientationEnable( m_hHeadOrientationFrameEndEvent, 0 );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiHeadOrientationEnable" );
        return E_FAIL;
    }

    // Initialize NuiVisualization object to render the depth map
    if( FAILED( m_NuiRenderer.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_SKELETON | NUI_INITIALIZE_FLAG_USES_COLOR ) ) )
    {
        ATG_PrintError( "Picture in Picture initialization failed" );
    }

    ATG::NUI_VISUALIZATION_SKELETON_RENDER_INFO SkeletonRenderInfo;
    SkeletonRenderInfo.Initialize( SKELETON_COLOR, SKELETON_COLOR );
    for( DWORD dwSkeletonIndex = 0; dwSkeletonIndex < NUI_SKELETON_COUNT; ++ dwSkeletonIndex )
    {
        m_NuiRenderer.SetSkeletonRenderInfo( dwSkeletonIndex, &SkeletonRenderInfo );
    }

    // Initalize player data
    ZeroMemory( m_aPlayerInfo, sizeof( m_aPlayerInfo ) );
    for( DWORD i = 0; i < NUI_SKELETON_MAX_TRACKED_COUNT; ++ i )
    {
        m_aPlayerInfo[ i ].MainColor      = g_aPlayerMainColor[ i ];
        m_aPlayerInfo[ i ].HighlightColor = g_aPlayerHighlightColor[ i ];
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Process the game inputs and allow user to exit sample
    ATG::Input::GetMergedInput();

    // Wait for frame processing to end
    if( WaitForSingleObject( m_hSkeletonFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) == WAIT_OBJECT_0 )
    {
        NUI_SKELETON_FRAME SkeletonFrame;

        // Retrieve ans smooth the latest skeleton data
        HRESULT hr =  NuiSkeletonGetNextFrame( 0, &SkeletonFrame );
        if( SUCCEEDED( hr ) )
        {
            NuiTransformSmooth( &SkeletonFrame, NULL );

            m_NuiRenderer.SetSkeletons( &SkeletonFrame );

        }

        // Retrieve and smooth the latest head orientation data
        hr = NuiHeadOrientationGetNextFrame( 0, &m_HeadOrientationFrame );
        if( SUCCEEDED( hr ) )
        {
            NuiHeadOrientationSmooth( &m_HeadOrientationFrame, NULL );
        }

        // Severe link to skeleton and head tracking data whenever the tracked skeleton is lost
        for( DWORD i = 0; i < NUI_SKELETON_MAX_TRACKED_COUNT; ++ i )
        {
            if( m_aPlayerInfo[ i ].dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID && 
                GetSkeletonDataFromTrackingID( &SkeletonFrame,  m_aPlayerInfo[ i ].dwTrackingID ) == NULL )
            {
                m_aPlayerInfo[ i ].dwTrackingID = NUI_SKELETON_INVALID_TRACKING_ID;
            }
        }

        // Enssure that each tracked skeleton has been assigned to a player info structure
        for( DWORD dwSkeletonIndex = 0 ; dwSkeletonIndex < NUI_SKELETON_COUNT; ++ dwSkeletonIndex )
        {
            if( SkeletonFrame.SkeletonData[ dwSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
            {
                if( GetPlayerInfoFromTrackingID( SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwTrackingID ) == NULL )
                {
                    for( DWORD i = 0; i < NUI_SKELETON_MAX_TRACKED_COUNT; ++ i )
                    {
                        if( m_aPlayerInfo[ i ].dwTrackingID == NUI_SKELETON_INVALID_TRACKING_ID )
                        {
                            m_aPlayerInfo[ i ].dwTrackingID = SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwTrackingID;
                            break;
                        }
                    }
                }
            }
        }

        // Update player's UI location based on skeleton joints 
        for( DWORD dwSkeletonIndex = 0 ; dwSkeletonIndex < NUI_SKELETON_COUNT; ++ dwSkeletonIndex )
        {
            if( SkeletonFrame.SkeletonData[ dwSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
            {
                PLAYER_INFO* pPlayerInfo = GetPlayerInfoFromTrackingID( SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwTrackingID );
                if( pPlayerInfo != NULL )
                {
                    pPlayerInfo->UIOrigin   = m_NuiRenderer.GetJointProjectedLocation( dwSkeletonIndex, NUI_SKELETON_POSITION_HEAD, ( FLOAT )COLOR_IMAGE_RENDER_WIDTH, ( FLOAT )COLOR_IMAGE_RENDER_HEIGHT, TRUE );
                    pPlayerInfo->RollOrigin = m_NuiRenderer.GetJointProjectedLocation( dwSkeletonIndex, NUI_SKELETON_POSITION_SHOULDER_CENTER, ( FLOAT )COLOR_IMAGE_RENDER_WIDTH, ( FLOAT )COLOR_IMAGE_RENDER_HEIGHT, TRUE );
                }
            }
        }
    }

    // Check if new data is available for the color stream and update the related NuiRenderer
    CONST NUI_IMAGE_FRAME* pColorFrame640x480;
    HRESULT hr = NuiImageStreamGetNextFrame( m_hColor640x480, 0, &pColorFrame640x480 );
    if( SUCCEEDED( hr ) )
    {
        m_NuiRenderer.SetColorTexture( pColorFrame640x480->pFrameTexture, &pColorFrame640x480->ViewArea );

        NuiImageStreamReleaseFrame( m_hColor640x480, pColorFrame640x480 );
    }       


    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    
    // Default color
    const D3DCOLOR DEFAULT_COLOR = D3DCOLOR_RGBA( 255, 255, 255, 255 );

    m_Timer.MarkFrame();

    //Draw a gradient filled background
    ATG::RenderBackground( D3DCOLOR_ARGB( 255, 0, 0, 255 ), D3DCOLOR_ARGB( 255, 0, 0, 0 ) );

    // Origin of the color image on screen
    const DWORD dwColorImageX = ( DWORD )( ( m_d3dpp.BackBufferWidth - COLOR_IMAGE_RENDER_WIDTH ) * 0.5f );
    const DWORD dwColorImageY = 200;

    // Show the color image
    m_NuiRenderer.RenderColorStream( ( FLOAT )dwColorImageX, ( FLOAT )dwColorImageY, ( FLOAT )COLOR_IMAGE_RENDER_WIDTH, ( FLOAT )COLOR_IMAGE_RENDER_HEIGHT );
    
    // Show the skeletons over the color image, cliping the skeleton to the image area and converting it from depth map to color space
    m_NuiRenderer.RenderSkeletons( ( FLOAT )dwColorImageX, ( FLOAT )dwColorImageY, ( FLOAT )COLOR_IMAGE_RENDER_WIDTH, ( FLOAT )COLOR_IMAGE_RENDER_HEIGHT, TRUE, TRUE );

    for( UINT i = 0; i < NUI_HEAD_ORIENTATION_COUNT; ++ i )
    {
        // Render info for all tracked head (maximum 2)
        DWORD dwMessageCount = 0;
        if( m_HeadOrientationFrame.HeadOrientationData[ i ].TrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
        {
            const PLAYER_INFO* pPlayerInfo = GetPlayerInfoFromTrackingID( m_HeadOrientationFrame.HeadOrientationData[ i ].TrackingID );
            if( pPlayerInfo != NULL )
            {
                if( m_HeadOrientationFrame.HeadOrientationData[ i ].QualityFlags != 0 )
                {
                    m_Font.Begin();
                    D3DRECT OriginalWindow;
                    m_Font.GetWindow( OriginalWindow );
                    m_Font.SetWindow( dwColorImageX, dwColorImageY, dwColorImageX + COLOR_IMAGE_RENDER_WIDTH, dwColorImageY + COLOR_IMAGE_RENDER_HEIGHT );
                    m_Font.SetScaleFactors( 1.0f, 1.0f );
                    const WCHAR* pwszMessage = ATG::GetHeadOrientationQualityFlagPrompt(  m_HeadOrientationFrame.HeadOrientationData[ i ].QualityFlags );
                    m_Font.DrawText( COLOR_IMAGE_RENDER_WIDTH * 0.5f, COLOR_IMAGE_RENDER_HEIGHT - ( dwMessageCount + 1 ) * m_Font.GetFontHeight() * 2, pPlayerInfo->MainColor,  pwszMessage, ATGFONT_CENTER_X );
                    m_Font.SetWindow( OriginalWindow );
                    m_Font.End();
                    ++ dwMessageCount;
                }

                // If the NUI_HEAD_ORIENTATION_QUALITY_FACE_DETECT_FAILURE is set, it means that NuiHeadOrientation couldn't find the player's face and thus, 
                // the orientation data is also invalid. Thus we can skip the rest of the code and proceed with the next tracked head.
                if( m_HeadOrientationFrame.HeadOrientationData[ i ].QualityFlags & NUI_HEAD_ORIENTATION_QUALITY_FACE_DETECT_FAILURE )
                {
                    continue;
                }

                // Dispay the face bounds used by the NuiHeadOrientation API
                D3DRECT ScreenRect;
                ScreenRect.x1 = ( LONG )( ( m_HeadOrientationFrame.HeadOrientationData[ i ].FaceBounds.left + dwColorImageX ) );
                ScreenRect.y1 = ( LONG )( ( m_HeadOrientationFrame.HeadOrientationData[ i ].FaceBounds.top + dwColorImageY ) );
                ScreenRect.x2 = ( LONG )( ( m_HeadOrientationFrame.HeadOrientationData[ i ].FaceBounds.right + dwColorImageX ) );
                ScreenRect.y2 = ( LONG )( ( m_HeadOrientationFrame.HeadOrientationData[ i ].FaceBounds.bottom + dwColorImageY ) );
                ATG::DebugDraw::DrawScreenSpaceRect( ScreenRect, 3,  pPlayerInfo->MainColor );     

                // Sizes in screen coordinates of various UI elements 
                const DWORD FACE_REFERENCE_RECT_WIDTH  = 100;
                const DWORD FACE_REFERENCE_RECT_HEIGHT = 100;
                const DWORD HALF_REFERENCE_RECT_WIDTH  = ( DWORD )( FACE_REFERENCE_RECT_WIDTH * 0.5f );
                const DWORD HALF_REFERENCE_RECT_HEIGHT = ( DWORD )( FACE_REFERENCE_RECT_HEIGHT * 0.5f );
                const DWORD YAW_RECT_HEIGHT            = 20;
                const DWORD PITCH_RECT_WIDTH           = 20;

                // If NUI_HEAD_ORIENTATION_QUALITY_FAILURE is set, it means the orientation data is valid. 
                // but face recangle could be valid if NUI_HEAD_ORIENTATION_QUALITY_FACE_DETECT_FAILURE is not set.
                if ( m_HeadOrientationFrame.HeadOrientationData[ i ].QualityFlags & NUI_HEAD_ORIENTATION_QUALITY_FAILURE )
                {
                    continue;
                }

                // Obtain pitch, yaw and roll from the head orientaion data
                FLOAT fPitch;
                FLOAT fYaw;
                FLOAT fRoll;
                QuaternionToPitchYawRoll( &m_HeadOrientationFrame.HeadOrientationData[ i ].Orientation, &fPitch, &fYaw, &fRoll );
                fPitch *= -1.0f;
                fYaw   *= -1.0f;
                fRoll  *= -1.0f;

                // Compute a reference rect around the player's head. The ref rect is used to determine the 
                // location of other UI elements
                D3DRECT ReferenceRect;
                ReferenceRect.x1 = dwColorImageX + ( LONG )pPlayerInfo->UIOrigin.x - HALF_REFERENCE_RECT_WIDTH;
                ReferenceRect.y1 = dwColorImageY + ( LONG )pPlayerInfo->UIOrigin.y - HALF_REFERENCE_RECT_HEIGHT;
                ReferenceRect.x2 = ReferenceRect.x1 + FACE_REFERENCE_RECT_WIDTH;
                ReferenceRect.y2 = ReferenceRect.y1 + FACE_REFERENCE_RECT_HEIGHT;
           
                // Display the current yaw value as a gauge indicator over the player's head
                ScreenRect.x1 = ReferenceRect.x1;
                ScreenRect.y1 = ReferenceRect.y1 - YAW_RECT_HEIGHT - 5;
                ScreenRect.x2 = ReferenceRect.x2;
                ScreenRect.y2 = ReferenceRect.y1 - 5;
                ATG::DebugDraw::DrawScreenSpaceRect( ScreenRect, 3,  pPlayerInfo->MainColor ); 
                FLOAT fYawPosition = ScreenRect.x1 + HALF_REFERENCE_RECT_WIDTH + HALF_REFERENCE_RECT_WIDTH * fYaw;
                ATG::DebugDraw::DrawScreenSpaceLine( XMFLOAT2( fYawPosition, ( FLOAT )ScreenRect.y1 ), 
                                                     XMFLOAT2( fYawPosition, ( FLOAT )ScreenRect.y2 ), 
                                                     pPlayerInfo->HighlightColor, 3 );
    
                // Display the current pitch value as a gauge indicator to the left of the player's head
                ScreenRect.x1 = ReferenceRect.x1 - PITCH_RECT_WIDTH - 5;
                ScreenRect.y1 = ReferenceRect.y1;
                ScreenRect.x2 = ReferenceRect.x1 - 5;
                ScreenRect.y2 = ReferenceRect.y2;
                ATG::DebugDraw::DrawScreenSpaceRect( ScreenRect, 3,  pPlayerInfo->MainColor );     
                FLOAT fPitchPosition = ScreenRect.y1 + HALF_REFERENCE_RECT_HEIGHT + HALF_REFERENCE_RECT_HEIGHT * fPitch;
                ATG::DebugDraw::DrawScreenSpaceLine( XMFLOAT2( ( FLOAT )ScreenRect.x1, fPitchPosition ), 
                                                     XMFLOAT2( ( FLOAT )ScreenRect.x2, fPitchPosition ), 
                                                     pPlayerInfo->HighlightColor, 3 );

                // Display a 2d vector representing the roll value
                // The origin coincides with the neck joint of the skeleton
                XMFLOAT2 RollOrigin;
                RollOrigin.x = dwColorImageX + pPlayerInfo->RollOrigin.x;
                RollOrigin.y = dwColorImageY + pPlayerInfo->RollOrigin.y;

                XMVECTOR vRoll = GetRollVector( fRoll );
                vRoll = XMVectorScale( vRoll, ( FLOAT )HALF_REFERENCE_RECT_HEIGHT );
                ATG::DebugDraw::DrawScreenSpaceLine( RollOrigin, 
                                                     XMFLOAT2( RollOrigin.x + vRoll.x, RollOrigin.y + vRoll.y ),
                                                     pPlayerInfo->MainColor, 3 );
            }
        }
    }

    // Show title and frame rate
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, DEFAULT_COLOR,  L"Head Orientation" );

    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, DEFAULT_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
    m_Font.End();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::GetPlayerInfoFromTrackingID()
// Desc: Returns a pointer to the PLAYER_INFO structure correspong to belonging to the skeleton 
//       identified by dwTrackingID.
//       It returns NULL if no matching skeleton is found.
//--------------------------------------------------------------------------------------
PLAYER_INFO* Sample::GetPlayerInfoFromTrackingID( DWORD dwTrackingID )
{
    assert( dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID );
    
    PLAYER_INFO* pPlayerInfo = NULL;
    for( DWORD i = 0; i < NUI_SKELETON_MAX_TRACKED_COUNT; ++ i )
    {
        if( m_aPlayerInfo[ i ].dwTrackingID == dwTrackingID )
        {
            pPlayerInfo = &m_aPlayerInfo[ i ];
        }
    }
    
    return pPlayerInfo;
}

