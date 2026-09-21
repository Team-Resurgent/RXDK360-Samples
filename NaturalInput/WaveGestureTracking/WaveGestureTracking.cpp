//--------------------------------------------------------------------------------------
// WaveGestureTracking.cpp
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <nuiapi.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <AtgNuiCommon.h>
#include <AtgNuiVisualization.h>
#include <AtgNuiRelativeCoordinates.h>
#include <AtgDebugDraw.h>

//-------------------------------------------------------------------------------------
// Name: Lerp()
// Desc: Linear interpolation between two floats
//-------------------------------------------------------------------------------------
inline FLOAT Lerp( FLOAT f1, FLOAT f2, FLOAT fBlend )
{
    return f1 + (f2-f1) * fBlend;
}

class ScreenReletiveCoordinateSystem
{
public:



    ScreenReletiveCoordinateSystem( BOOL bApplyFiltering = TRUE );

    VOID Update( const NUI_SKELETON_DATA* pSkeletonData );
    VOID Reset() { m_bResetSkeletonParams = TRUE; }

    XMFLOAT2 GetRightHandScreenReletive();
    XMFLOAT2 GetLeftHandScreenReletive();

private:

    static const FLOAT m_fSmoothingFactor; 
    static const FLOAT m_fCenterXOffsetToSpine;

    ATG::Timer  m_Timer;
    BOOL        m_bApplyFilteringToHands;

    ATG::FilterAdaptiveDoubleExponential m_JointFilter;

    XMVECTOR    m_vCentre;
    XMVECTOR    m_vRawSkeletonPosition[NUI_SKELETON_POSITION_COUNT];
    XMVECTOR    m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_COUNT];

    FLOAT       m_fAverageLeftHandLength;
    FLOAT       m_fAverageRightHandLength;
    FLOAT       m_fAverageShoulderLength;

    BOOL        m_bResetSkeletonParams;

};
   const FLOAT  ScreenReletiveCoordinateSystem::m_fSmoothingFactor = 0.99f;

    // This constant variable specifies the coordinate center X offset from the spine in fraction of shoulder width. 
    // For example, the center X is set to spine.X + 0.60 * shoulder witdh for right hand. This contstant is chosen by 
    // experiment and is meant to be the coordinates where a player naturally holds his/her toward the screen to point 
    // to the center of screen. 
    const FLOAT  ScreenReletiveCoordinateSystem::m_fCenterXOffsetToSpine = 0.60f;

    ScreenReletiveCoordinateSystem::ScreenReletiveCoordinateSystem( BOOL bApplyFilteringToHands ) :
        m_bApplyFilteringToHands ( bApplyFilteringToHands ),
        m_fAverageLeftHandLength( 0.0f ),
        m_fAverageRightHandLength( 0.0f ),
        m_fAverageShoulderLength( 0.0f ), 
        m_bResetSkeletonParams( TRUE )
    {
    }

    VOID ScreenReletiveCoordinateSystem::Update( const NUI_SKELETON_DATA* pSkeletonData )
    {
        XMemCpy( &m_vRawSkeletonPosition, pSkeletonData->SkeletonPositions, sizeof( XMVECTOR ) * NUI_SKELETON_POSITION_COUNT );

        m_JointFilter.Update( pSkeletonData, ( FLOAT )m_Timer.GetElapsedTime() );
        XMVECTOR* pvSkeletonPosition = m_JointFilter.GetFilteredJoints( );
        XMemCpy( &m_vFilteredSkeletonPosition, pvSkeletonPosition, sizeof( XMVECTOR ) * NUI_SKELETON_POSITION_COUNT );

        FLOAT fLeftHandLength  = XMVector3Length( m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_HAND_LEFT]  - m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_ELBOW_LEFT] ).x;
        FLOAT fRightHandLength = XMVector3Length( m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_HAND_RIGHT] - m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_ELBOW_RIGHT] ).x;
        FLOAT fShoulderLength  = XMVector3Length( m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_SHOULDER_RIGHT] - m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_SHOULDER_LEFT] ).x;

        if( m_bResetSkeletonParams ) 
        {
            m_fAverageLeftHandLength  = fLeftHandLength;
            m_fAverageRightHandLength = fRightHandLength;
            m_fAverageShoulderLength  = fShoulderLength;
            m_bResetSkeletonParams = FALSE; 
        }
        else
        {
            m_fAverageLeftHandLength  = Lerp( fLeftHandLength,  m_fAverageLeftHandLength,  m_fSmoothingFactor );
            m_fAverageRightHandLength = Lerp( fRightHandLength, m_fAverageRightHandLength, m_fSmoothingFactor );
            m_fAverageShoulderLength  = Lerp( fShoulderLength,  m_fAverageShoulderLength,  m_fSmoothingFactor );
        }
    }

    XMFLOAT2  ScreenReletiveCoordinateSystem::GetRightHandScreenReletive()
    {
        XMFLOAT2 fResult;
        XMVECTOR vCenter;

        vCenter.y = m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_SPINE].y;
        vCenter.x = m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_SPINE].x + m_fAverageShoulderLength * 0.60f;

        XMVECTOR vHand = m_bApplyFilteringToHands ? m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_HAND_RIGHT] : m_vRawSkeletonPosition[NUI_SKELETON_POSITION_HAND_RIGHT];

        XMStoreFloat2( &fResult, XMVectorScale( vHand - vCenter, 1/m_fAverageRightHandLength ) );

        fResult.x = (fResult.x + 1.0f ) * .50f;
        fResult.y = (-fResult.y + 1.0f ) * .50f;

        return fResult;
    }

    XMFLOAT2  ScreenReletiveCoordinateSystem::GetLeftHandScreenReletive()
    {
        XMFLOAT2 fResult;
        XMVECTOR vCenter;

        vCenter.y = m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_SPINE].y;
        vCenter.x = m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_SPINE].x - m_fAverageShoulderLength * 0.60f;

        XMVECTOR vHand = m_bApplyFilteringToHands ? m_vFilteredSkeletonPosition[NUI_SKELETON_POSITION_HAND_LEFT] : m_vRawSkeletonPosition[NUI_SKELETON_POSITION_HAND_LEFT];

        XMStoreFloat2( &fResult, XMVectorScale( vHand - vCenter, 1/m_fAverageLeftHandLength ) );

        fResult.x = (fResult.x + 1.0f ) * .50f;
        fResult.y = (-fResult.y + 1.0f ) * .50f;

        return fResult;
    }


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class WaveGestureSample : public ATG::Application
{
public: 

    WaveGestureSample();

private:
    
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    HRESULT RenderText();
    HRESULT RenderWaveUI();

    HRESULT DrawHandIcon( FLOAT fX, FLOAT fY, FLOAT fSize, BOOL bRightHand );
    HRESULT DrawProgressBar( FLOAT fX, FLOAT fY, FLOAT fSize, BOOL bFillRightToLeft, FLOAT fFillLevel );

    // General sample data
    ATG::Timer  m_Timer;
    ATG::Font   m_Font;
    ATG::Help   m_Help;
    BOOL        m_bDrawHelp;

    // NUI related member variables
    HANDLE      m_FrameEndEventHandle;
    HANDLE      m_ColorStreamHandle;
    NUI_SKELETON_FRAME  m_Skeleton;

    DWORD       m_dwLastPlayerWavedTrackingId; 
    FLOAT       m_fLastWaveTime; 

    DWORD       m_dwGestureOwnerTrackingID;
    FLOAT       m_fGestureOwnerProgress;
    INT         m_iLastPlayerWavedIndex;
    
    // variables used for drawing wave progress bar 
    static const FLOAT m_fSingleWaveUITime;
    DWORD   m_dwWaveUITrackingID;
    FLOAT   m_fWaveUIStartTime;
    FLOAT   m_fWaveUIProgress;
    BOOL    m_bFillLeftToRight;

    ATG::PackedResource m_Resource;

    ATG::NuiVisualization m_pip;
    ScreenReletiveCoordinateSystem m_CoordSystems;

    // member variables used for drawing progress bar and hand icon
    struct WaveProgressBarVetrex
    {
        XMFLOAT3 Pos;
        XMFLOAT2 UV;
    };

    LPDIRECT3DTEXTURE9 m_pHandTexture[2];
    LPDIRECT3DTEXTURE9 m_pProgressBarTexture[2];

    IDirect3DVertexShader9* m_pProgressBarVS;
    IDirect3DPixelShader9* m_pProgressBarPS;
    LPDIRECT3DVERTEXDECLARATION9 m_pVertexDeclaration;

};

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    WaveGestureSample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}

// Time each wave progress bar should take to fill, set at 0.5 seconds
const FLOAT WaveGestureSample::m_fSingleWaveUITime = 0.50f;

//-----------------------------------------------------------------------------
// Name: WaveGestureSample()
// Desc: Constructor
//-----------------------------------------------------------------------------
WaveGestureSample::WaveGestureSample( ) : ATG::Application() ,
    m_bDrawHelp( FALSE ), 
    m_FrameEndEventHandle( NULL ),
    m_ColorStreamHandle( NULL ),
    m_iLastPlayerWavedIndex( -1 ), 
    m_dwGestureOwnerTrackingID( NUI_SKELETON_INVALID_TRACKING_ID ),
    m_dwLastPlayerWavedTrackingId( 0 ),
    m_fLastWaveTime( 0.0f ),
    m_fGestureOwnerProgress( 0.0f ),
    m_CoordSystems( TRUE ), 
    m_dwWaveUITrackingID( 0 ), 
    m_fWaveUIStartTime( 0.0f ),
    m_fWaveUIProgress( 0.0f ), 
    m_bFillLeftToRight( TRUE )
{
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT WaveGestureSample::Initialize()
{

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    HRESULT hr = m_pip.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                                 NUI_INITIALIZE_FLAG_USES_COLOR | 
                                                 NUI_INITIALIZE_FLAG_USES_SKELETON,
                                                 NUI_IMAGE_RESOLUTION_640x480 );
    if( hr != ERROR_SUCCESS )
    {
        ATG::DebugSpew( "Failed to initialize visualization\n" );
        return E_FAIL;
    }

    // Initialize the camera and skeleton tracking
    // Create event which will be signaled when frame processing ends
    m_FrameEndEventHandle = CreateEvent( NULL,
                                         FALSE,  // auto-reset
                                         FALSE,  // create unsignaled
                                         "NuiFrameEndEvent" );
    if( m_FrameEndEventHandle == NULL)
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
                        NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                        NUI_INITIALIZE_FLAG_USES_COLOR,
                        NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    // Register frame end event with NUI
    hr = NuiSetFrameEndEvent( m_FrameEndEventHandle, 0 );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR,
                             NUI_IMAGE_RESOLUTION_640x480,
                             0, 
                             1, 
                             NULL, 
                             &m_ColorStreamHandle); 
    if( FAILED( hr ) ) 
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    // Enable Nui skeleton tracking and wave detection 
    hr = NuiSkeletonTrackingEnable( NULL, 0 );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
        return E_FAIL;
    }

    hr = NuiWaveSetEnabled( TRUE ); 
    if( FAILED( hr ) ) 
    {
        ATG::NuiPrintError( hr, "NuiWaveSetEnabled" );
        return E_FAIL;
    }

    // Create texture resources
    hr = m_Resource.Create( "game:\\Media\\Resource.xpr" );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }

    // Load the textures that will be used to visualize wave
    m_pHandTexture[0] = m_Resource.GetTexture("RightHand");
    m_pHandTexture[1] = m_Resource.GetTexture("LeftHand");
    m_pProgressBarTexture[0] = m_Resource.GetTexture("WaveProgressRight");
    m_pProgressBarTexture[1] = m_Resource.GetTexture("WaveProgressLeft");

    // Load the vertex and pixel shaders used for drawing wave progress bar
    hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\Shader.xvu", &m_pProgressBarVS ) ;
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't load vertex shader.\n" );
        return hr;
    }

    hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\Shader.xpu", &m_pProgressBarPS ) ;
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't load pixel shader.\n" );
        return hr;
    }

    static const D3DVERTEXELEMENT9 declUpdate[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    hr = m_pd3dDevice->CreateVertexDeclaration( declUpdate, &m_pVertexDeclaration );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "CreateVertexDeclaration has failed.\n" );
        return hr;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT WaveGestureSample::Update()
{
    const NUI_IMAGE_FRAME* pColorImageFrame;

    // Wait for frame processing to end
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_FrameEndEventHandle, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }

    // Update the color buffer streams.
    HRESULT hr = NuiImageStreamGetNextFrame( m_ColorStreamHandle, 0, &pColorImageFrame );
    if( SUCCEEDED( hr ) )
    {
        m_pip.SetColorTexture( pColorImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_ColorStreamHandle, pColorImageFrame );
    }
    else
    {
        ATG::NuiPrintError( hr, "NuiImageStreamGetNextFrame" );
    }

    // Get the next skeleton frame and pass it to visualizer
    hr = NuiSkeletonGetNextFrame( 0, &m_Skeleton );
    if( SUCCEEDED( hr ) )
        m_pip.SetSkeletons( &m_Skeleton );
    else
        ATG::NuiPrintError( hr, "NuiSkeletonGetNextFrame" );
    

    // Get the status of wave gesture for the gesture owner 
    hr = NuiWaveGetGestureOwnerProgress( &m_dwGestureOwnerTrackingID, &m_fGestureOwnerProgress );
    if( SUCCEEDED( hr ) )
    {
        // Check if a wave gesture has been completed, which is done by checking
        // if the progress level is >= 1.0f 
        if( m_fGestureOwnerProgress >= 1.0f ) 
        {
            // Record the time and player ID of wave gesture owner, which are to 
            // be used in the Render method to draw wave UI
            m_dwLastPlayerWavedTrackingId = m_dwGestureOwnerTrackingID;
            m_fLastWaveTime = ( FLOAT ) m_Timer.GetAppTime();
            
            m_iLastPlayerWavedIndex = -1; 
            for( DWORD i = 0; i < NUI_SKELETON_COUNT; ++i )
            {
                if( m_Skeleton.SkeletonData[i].dwTrackingID == m_dwGestureOwnerTrackingID)
                {
                    m_iLastPlayerWavedIndex = i;
                    break;
                }
            }
        }

    }
    else
    {
        m_dwGestureOwnerTrackingID = NUI_SKELETON_INVALID_TRACKING_ID;
        ATG::NuiPrintError( hr, "NuiWaveGetGestureOwnerProgress" );
    }

    // This sample tracks the hands of last player waved, and simply draws a hand icon at 
    // screen relative coordinates of the player left and right hands. 
    if( m_iLastPlayerWavedIndex != -1 )
    {
        // Check if the skeleton of last player waved is still being tracked, and then update 
        // the right hand screen relative coordinates
        if( m_Skeleton.SkeletonData[m_iLastPlayerWavedIndex].eTrackingState == NUI_SKELETON_TRACKED )
        {
            m_CoordSystems.Update( &m_Skeleton.SkeletonData[m_iLastPlayerWavedIndex] );
        }
        else
        {
            // The skeleton of last player waved is lost, so we stop tracking this player
            m_iLastPlayerWavedIndex = -1;
        }           
    }

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();
    
    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT WaveGestureSample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( D3DCOLOR_ARGB(255,0,0,255), D3DCOLOR_ARGB(255,0,0,0) );

    // Visualize the color stream and skeleton 
    const FLOAT fWidth = 512.0f;
    const FLOAT fHeight = 384.0f;
    const FLOAT fX = 64.0f;
    const FLOAT fY = 150.0f;
    m_pip.BeginRender();
    m_pip.RenderColorStream( fX, fY, fWidth, fHeight );
    m_pip.RenderSkeletons( fX, fY, fWidth, fHeight, FALSE, TRUE );
    m_pip.EndRender();

    RenderText();

    RenderWaveUI();

    // Draw hand icons at sreen relative coordinate of the player hands. 
    if( m_iLastPlayerWavedIndex != -1 )
    {
        UINT uWidth;
        UINT uHeight;
        ATG::GetVideoSettings( &uWidth, &uHeight );

        XMFLOAT2 fHandCoord = m_CoordSystems.GetRightHandScreenReletive();
        DrawHandIcon( fHandCoord.x * ( FLOAT ) uWidth, fHandCoord.y * (FLOAT) uHeight, 50.0f, TRUE );
        
        fHandCoord = m_CoordSystems.GetLeftHandScreenReletive();
        DrawHandIcon( fHandCoord.x * ( FLOAT ) uWidth, fHandCoord.y * (FLOAT) uHeight, 50.0f, FALSE );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderText()
// Desc: Renders text info regarding the current wave gesture
//--------------------------------------------------------------------------------------
HRESULT WaveGestureSample::RenderText()
{
    const D3DCOLOR TEXT_COLOR = D3DCOLOR_ARGB(255,255,255,255);
    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    WCHAR strBuffer[ 512 ];

    // Draw title text
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, TEXT_COLOR,  L"Wave Gesture Tracking Sample" );

    m_Font.SetScaleFactors( 1.0f, 1.0f );
    swprintf_s( strBuffer, L" Time: %.1f sec\n %s", m_Timer.GetAppTime(), m_Timer.GetFrameRate() );
    m_Font.DrawText( 0, 0, D3DCOLOR_ARGB( 255, 255, 255, 0 ), strBuffer, ATGFONT_RIGHT );

    FLOAT fLineXPosition = 490.0f;
    FLOAT fLineYPosition = 80.0f;
    FLOAT fNextLineYDelat = 30.0f;

    if( m_dwGestureOwnerTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
        swprintf_s( strBuffer, L" Skeleton number of wave gesture owner: %d \n Wave progress: %2.2f ", m_dwGestureOwnerTrackingID, m_fGestureOwnerProgress);
    else
        swprintf_s( strBuffer, L" Skeleton number of wave gesture owner: No Player is waving \n Wave progress: Not applicable");

    m_Font.DrawText( fLineXPosition, fLineYPosition, TEXT_COLOR, strBuffer, ATGFONT_LEFT );
    fLineYPosition += fNextLineYDelat;

    if( m_iLastPlayerWavedIndex != -1 )
    {
        swprintf_s( strBuffer, L"Skeleton %d with player index %d completed a wave gesture at %.2f second", 
                                 m_dwLastPlayerWavedTrackingId, 
                                 m_iLastPlayerWavedIndex, 
                                 m_fLastWaveTime );
        m_Font.DrawText( 0, 500, TEXT_COLOR, strBuffer, ATGFONT_LEFT );
    }


    m_Font.End();


    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderWaveUI()
// Desc: Renders a progress bar for the current wave gesture
//--------------------------------------------------------------------------------------
HRESULT WaveGestureSample::RenderWaveUI()
{
    FLOAT fX = 1150.0f;
    FLOAT fY = 200.0f;
    FLOAT fSize = 50.0f;

    // A full wave gesture consists of 4 wave moves, however the NuiWave starts returning wave 
    // progress from the 2nd wave. That is, the wave progress values reported by Nui during a wave 
    // gesture are 0.25, 0.50, 0.75. A progress equal to 1.00 means that a full wave gesture is
    // complete. As noted, these progress values are not continuous, and are not meant to be used 
    // as the fill level of wave progress bar UI. 

    // In order to visualize the wave progress, we start drawing a progress bar whenever the 
    // progress value changes (i.e., progress value hits .25, .50 and .75). Each progress bar 
    // takes 0.5 seconds to fill, and the direction of fill always starts from left to right. 
    

    if( m_dwGestureOwnerTrackingID != NUI_SKELETON_INVALID_TRACKING_ID)
    {
        // Reset the local variables used for wave progress bar if the player tracked by NuiWave has changed 
        if( m_dwGestureOwnerTrackingID != m_dwWaveUITrackingID )
        {
            m_dwWaveUITrackingID = m_dwGestureOwnerTrackingID;
            m_fWaveUIProgress = 0.0f;
            m_bFillLeftToRight = TRUE;
        }            

        // Start a new progress bar if the progress reported by NuiWave is changed
        if( ( m_fGestureOwnerProgress != m_fWaveUIProgress ) && ( m_fGestureOwnerProgress < 1.0f ) )
        {
            m_fWaveUIProgress = m_fGestureOwnerProgress;
            m_fWaveUIStartTime = ( FLOAT ) m_Timer.GetAppTime();
            m_bFillLeftToRight = !m_bFillLeftToRight;
        }
        
        // Calculate the progress bar fill level based on the time passed since wave started
        FLOAT fTimeSinceGestureStart = ( FLOAT ) m_Timer.GetAppTime() - m_fWaveUIStartTime;
        FLOAT fFillLevel = fTimeSinceGestureStart / m_fSingleWaveUITime;
        fFillLevel = ( fFillLevel > 1.0f ) ? 1.0f : fFillLevel;

        DrawProgressBar( fX, fY, fSize, m_bFillLeftToRight, fFillLevel );
        fY += 25.0f;

        DrawHandIcon( fX , fY, fSize, TRUE );
    }
    
    // check if player that NuiWave is tracking is changed, or if a full wave gesture is complete, 
    // and reset the local player tracking ID 
    if( ( m_dwGestureOwnerTrackingID == NUI_SKELETON_INVALID_TRACKING_ID ) || (m_fGestureOwnerProgress >= 1.0f ) )
    {
        m_dwWaveUITrackingID = 0;
    }

    return S_OK;
}
 
//--------------------------------------------------------------------------------------
// Name: DrawHandIcon()
// Desc: Renders a hand icon at the given position 
//--------------------------------------------------------------------------------------
HRESULT WaveGestureSample::DrawHandIcon( FLOAT fX, FLOAT fY, FLOAT fSize, BOOL bRightHand )
{
    D3DVIEWPORT9    saveVp;
    m_pd3dDevice->GetViewport( &saveVp );

    D3DVIEWPORT9    newVp = saveVp;

    newVp.X = ( DWORD ) fX;
    newVp.Y = ( DWORD ) fY;
    newVp.Width = ( DWORD ) fSize;
    newVp.Height = ( DWORD ) fSize;
    m_pd3dDevice->SetViewport( &newVp );
    ATG::DebugDraw::SetViewProjection( XMMatrixIdentity( ) );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
    ATG::DebugDraw::DrawTexturedQuad( XMFLOAT3( -1, -1, 0 ),
        XMFLOAT3(  1, -1, 0 ),
        XMFLOAT3( -1,  1, 0 ),
        XMFLOAT2( 1, -1 ),
        bRightHand ? m_pHandTexture[0] : m_pHandTexture[1] );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    m_pd3dDevice->SetViewport( &saveVp );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DrawProgressBar()
// Desc: Renders a progress bar at the given position 
//--------------------------------------------------------------------------------------
HRESULT WaveGestureSample::DrawProgressBar( FLOAT fX, FLOAT fY, FLOAT fSize, BOOL bFillRightToLeft, FLOAT fFillLevel )
{
    D3DVIEWPORT9    saveVp;
    m_pd3dDevice->GetViewport( &saveVp );

    D3DVIEWPORT9    newVp = saveVp;

    fFillLevel = min( fFillLevel, 1.0f ); 
    fFillLevel = max( fFillLevel, 0.0f );

    newVp.X = ( DWORD ) fX;
    newVp.Y = ( DWORD ) fY;
    newVp.Width = ( DWORD ) fSize;
    newVp.Height = ( DWORD ) fSize;
    newVp.MinZ = 0.0f;
    newVp.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &newVp );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    XMFLOAT4 ProgressBarParams =  XMFLOAT4( fFillLevel, bFillRightToLeft ? 1.0f : -1.0f, 0.0f, 0.0f);
    XMFLOAT4 ProgressEmptyColor =  XMFLOAT4( 1.0f, 1.0f, 1.0f, 1.0f);
    XMFLOAT4 ProgressFillColor =  XMFLOAT4( 0.0f, 0.0, 0.0f, 1.0f);

    m_pd3dDevice->SetVertexShader( m_pProgressBarVS );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDeclaration );
    m_pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT *) &ProgressEmptyColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, (FLOAT *) &ProgressFillColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 2, (FLOAT *) &ProgressBarParams, 1 );

    m_pd3dDevice->SetPixelShader( m_pProgressBarPS );
    m_pd3dDevice->SetTexture(0, bFillRightToLeft ? m_pProgressBarTexture[0] : m_pProgressBarTexture[1] );

    WaveProgressBarVetrex vCorners[4]; 
    vCorners[0].Pos = XMFLOAT3( -1.0f, -1.0f,  0.0f ); 
    vCorners[0].UV = XMFLOAT2( 0.0f, 1.0f );

    vCorners[1].Pos = XMFLOAT3( 1.0f, -1.0f,  0.0f ); 
    vCorners[1].UV = XMFLOAT2( 1.0f, 1.0f );

    vCorners[2].Pos = XMFLOAT3( 1.0f,  1.0f,  0.0f ); 
    vCorners[2].UV = XMFLOAT2( 1.0f, 0.0f );

    vCorners[3].Pos = XMFLOAT3( -1.0f,  1.0f,  0.0f ); 
    vCorners[3].UV = XMFLOAT2( 0.0f, 0.0f );

    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, vCorners, sizeof( WaveProgressBarVetrex ) );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetViewport( &saveVp );

    return S_OK;
}
