//--------------------------------------------------------------------------------------
// HandOpenClosed.cpp
//
// This sample demonstrates hand open closed detection using machine learning
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define NOMINMAX

#include <xtl.h>
#include <xnamath.h>
#include <xbdm.h>
#include <xstudio.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>
#include <AtgResource.h>
#include "CameraManager.h"

#include "HOCDetector/HOCDetector.h"

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_1, L"Record" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Pause" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_1, L"Switch mode" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle detector" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


// colours
static const DWORD  CLR_WHITE = ~0ul;
static const DWORD  CLR_RED = 0xffff0000;
static const DWORD  CLR_GREEN = 0xff00ff00;
static const DWORD  CLR_BLUE = 0xff0000ff;
static const DWORD  CLR_DARK_RED = 0xff800000;
static const DWORD  CLR_DARK_GREEN = 0xff008000;
static const DWORD  CLR_DARK_BLUE = 0xff000080;
static const DWORD  CLR_CYAN = 0xff00ffff;
static const DWORD  CLR_YELLOW = 0xffffff00;
static const DWORD  CLR_GRAY = 0xffA0A0A0;
static const DWORD  CLR_LEFT_HAND = CLR_YELLOW;
static const DWORD  CLR_RIGHT_HAND = CLR_CYAN;

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    //--------------------------------------------------------------------------------------
    // Different modes for this sample
    //--------------------------------------------------------------------------------------
    enum SampleMode
    {
        SM_DEBUG,
        SM_BOXES
    };

    //--------------------------------------------------------------------------------------
    // Classifier name
    //--------------------------------------------------------------------------------------
    enum ClassifierName
    {
        CI_HOC = 0,
        CI_THM,
        CI_NUM
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

        void    AddConfidence( UINT uHand, FLOAT fConfidence )
        {
            m_filter[ uHand ].FilterType1( fConfidence );
//            m_filter[ uHand ].FilterType0( fConfidence );
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

        FilteredResult m_results[ CI_NUM ];        // filtered classifier results

        PerPlayerRecord()
        {
            m_fPlayerSize = 0;
        }
    };

    //--------------------------------------------------------------------------------------
    // Name: DebugMode
    // Desc: Debug mode display. This displays a lot of debug information.
    //--------------------------------------------------------------------------------------
    struct DebugMode
    {
        BOOL            m_bDetectThumbup;
        XMFLOAT4        m_history[ 2 ][ 100 ];
        UINT            m_uHistoryTail[ 2 ];

        DebugMode()
        {
            m_uHistoryTail[ 0 ] = m_uHistoryTail[ 1 ] = 0;
            memset( m_history, 0, sizeof( m_history ) );
            m_bDetectThumbup = FALSE;
        }

        void    Update( Sample* pSample, const ATG::GAMEPAD* pGamepad );
        void    Render( Sample* pSample );
    };


    //--------------------------------------------------------------------------------------
    // Name: BoxesMode
    // Desc: Boxes mode demo. This shows how hand open closed works in a pointless mini game.
    //--------------------------------------------------------------------------------------
    struct BoxesMode
    {
        XMFLOAT3    m_cursorPos;
        XMFLOAT3    m_cursorDownOfs;
        XMFLOAT3    m_cursorPosLastClosed;
        XMFLOAT3    m_cursorDown;

        struct Box
        {
            XMFLOAT2    m_pos;
            XMFLOAT2    m_size;
            FLOAT       m_fAngle;
            DWORD       m_dwColor0;
            DWORD       m_dwColor1;
        };

        struct BoxTarget
        {
            XMFLOAT2    m_pos;
            FLOAT       m_fAngle;
            BOOL        m_bPlaced;
            BOOL        m_bOnTop;
        };

        Box         m_boxes[ 3 ];
        BoxTarget   m_boxesTargets[ 3 ];

        BOOL        m_bDragging;
        BOOL        m_bTurning;
        UINT        m_uHoverIndex;

        BoxesMode()
        {
            ZeroMemory( m_boxes, sizeof( m_boxes ) );
            ZeroMemory( m_boxesTargets, sizeof( m_boxesTargets ) );

            m_boxes[ 0 ].m_dwColor0 = CLR_DARK_BLUE;
            m_boxes[ 0 ].m_dwColor1 = CLR_BLUE;

            m_boxes[ 1 ].m_dwColor0 = CLR_DARK_RED;
            m_boxes[ 1 ].m_dwColor1 = CLR_RED;

            m_boxes[ 2 ].m_dwColor0 = CLR_DARK_GREEN;
            m_boxes[ 2 ].m_dwColor1 = CLR_GREEN;

            m_boxes[ 0 ].m_size.x = 128.f;
            m_boxes[ 0 ].m_size.y = 128;

            m_boxes[ 1 ].m_size.x = 128.f;
            m_boxes[ 1 ].m_size.y = 64.f;

            m_boxes[ 2 ].m_size.x = 64.f;
            m_boxes[ 2 ].m_size.y = 128.f;

            m_boxes[ 0 ].m_pos.x = 1280.f / 2.f;
            m_boxes[ 0 ].m_pos.y = 720.f / 2.f;

            m_boxes[ 1 ].m_pos.x = 500;
            m_boxes[ 1 ].m_pos.y = 200;

            m_boxes[ 2 ].m_pos.x = 200;
            m_boxes[ 2 ].m_pos.y = 500;

            m_bDragging = FALSE;
            m_bTurning = FALSE;
            m_uHoverIndex = ~0ul;

            m_cursorPos.x = m_cursorPos.y = 0;

            CreateBoxTargets();
        }

        void    Update( Sample* pSample, const ATG::GAMEPAD* pGamepad );
        void    Render( Sample* pSample );
        void    CreateBoxTargets();
    };

    //--------------------------------------------------------------------------------------
    // Name: DebugDraw
    // Desc: detector debugging output
    //--------------------------------------------------------------------------------------
    struct DebugDraw : IHOCDetectorDebugDraw
    {
        std::vector< DWORD >  quads;

        void    NewFrame()
        {
            quads.clear();
        }

        void    AddQuadInDepthImageSpace( INT x, INT y, INT sx, INT sy, DWORD clr )
        {
            quads.push_back( x );
            quads.push_back( y );
            quads.push_back( sx );
            quads.push_back( sy );
            quads.push_back( clr );
        }

        void    Render()
        {
            const UINT num = quads.size() / 5;
            const FLOAT scale = CameraManager::pipDrawWidth / 320.f;
            for( UINT i=0; i < num; ++i )
            {
                XMFLOAT2    p, s;

                p.x = quads[ i * 5 + 0 ] * scale + CameraManager::pipDrawX;
                p.y = quads[ i * 5 + 1 ] * scale + CameraManager::pipDrawY;

                s.x = quads[ i * 5 + 2 ] * scale;
                s.y = quads[ i * 5 + 3 ] * scale;

                ATG::DebugDraw::DrawScreenSpaceRect( p, s, 1, quads[ i * 5 + 4 ] );
            }
        }
    };

    SampleMode      m_sampleMode;
    BOOL            m_bPaused;
    BOOL            m_bLoaded;
    BOOL            m_bDrawHelp;
    BOOL            m_bTracked;
    ATG::Timer      m_Timer;
    ATG::Font       m_Font;
    ATG::Help       m_Help;

    PerPlayerRecord m_player;
    HOCDetector     m_classifiers[ CI_NUM ];     // [ 0 ] is trained so -1 = non-closed, 1 = closed
                                                 // [ 1 ] is trained so 1 = thumb up, -1 = thumb isn't up

    CameraManager   m_cameraManager;

    // XED file
    UINT            m_uRecordingNumber;
    CHAR            m_szXEDFilePath[ MAX_PATH ];
    XSTUDIO_FILE_HANDLE m_hXEDFile;

    static DebugDraw  ms_debugDraw;

    DebugMode   m_debugMode;
    BoxesMode m_boxesMode;

    BOOL    LoadDetectors();
    VOID    UpdateHandsConfidence();
public:
    Sample()
    {
        m_bLoaded = FALSE;
        m_uRecordingNumber = 0;
        m_hXEDFile = 0;
        m_bTracked = FALSE;
    }

    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
};


Sample::DebugDraw   Sample::ms_debugDraw;

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample* atgApp = new Sample;
    ATG::GetVideoSettings( &atgApp->m_d3dpp.BackBufferWidth, &atgApp->m_d3dpp.BackBufferHeight );
    atgApp->m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp->Run();
    delete atgApp;
}


//--------------------------------------------------------------------------------------
// Name: LoadDetectors
// Desc: Load HOC detectors
//--------------------------------------------------------------------------------------
BOOL Sample::LoadDetectors()
{
    // load detectors trained on the entire range
    if( FAILED( m_classifiers[ 0 ].Load( "GAME:\\Media\\opt.hoc" ) )/*   ||
        FAILED( m_classifiers[ 1 ].Load( "GAME:\\Media\\thm.hoc" ) )*/ )
    {
        return FALSE;
    }

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    DmMapDevkitDrive();

    HRESULT hr;
    m_bDrawHelp = FALSE;

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

    // Create the help
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    m_bPaused = FALSE;

    m_cameraManager.InitializeCamera( m_pd3dDevice );

    if( !m_bLoaded )
        m_bLoaded = LoadDetectors();

    HOCDetector::ms_debugDraw = &ms_debugDraw;

    m_sampleMode = SM_BOXES;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, "Update" );

    // Get the current gamepad state
    const ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        if( !m_hXEDFile )
        {
            sprintf_s( m_szXEDFilePath, "GAME:\\HOC_%d.XED", m_uRecordingNumber++ );

            HRESULT hr = XStudioCreateFile( m_szXEDFilePath,
                                            GENERIC_WRITE,
                                            CREATE_ALWAYS,
                                            XSTUDIO_STREAM_FLAG_NUICAM_DEPTH        |
                                            XSTUDIO_STREAM_FLAG_NUIAPI_SKELETON     |
                                            XSTUDIO_STREAM_FLAG_NUIAPI_PLAYER_INDEX |
                                            XSTUDIO_STREAM_FLAG_TITLE_DATA,
                                            &m_hXEDFile );

            if( SUCCEEDED( hr ) )
            {
                hr = XStudioMapStreams( XSTUDIO_STREAM_FLAG_NUICAM_DEPTH |
                                        XSTUDIO_STREAM_FLAG_NUIAPI_SKELETON |
                                        XSTUDIO_STREAM_FLAG_NUIAPI_PLAYER_INDEX |
                                        XSTUDIO_STREAM_FLAG_TITLE_DATA,
                                        m_hXEDFile );

                if( FAILED( hr ) )
                {
                    XStudioCloseFile( m_hXEDFile );
                    m_hXEDFile = 0;
                } else
                {
                    hr = XStudioStart( XSTUDIO_STREAM_FLAG_ALL );
                    if( FAILED( hr ) )
                    {
                        XStudioUnmapStreams( XSTUDIO_STREAM_FLAG_ALL, m_hXEDFile );
                        XStudioCloseFile( m_hXEDFile );
                        m_hXEDFile = 0;
                    }
                }
            }
        } else
        {
            XStudioStop( XSTUDIO_STREAM_FLAG_ALL );
            XStudioUnmapStreams( XSTUDIO_STREAM_FLAG_ALL, m_hXEDFile );
            XStudioCloseFile( m_hXEDFile );
            m_hXEDFile = 0;
        }
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bPaused = !m_bPaused;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        m_sampleMode = (m_sampleMode == SM_BOXES) ? SM_DEBUG : SM_BOXES;

    // update nui
    const FLOAT fElapsedTime = (FLOAT)m_Timer.GetElapsedTime();
    m_bTracked = m_cameraManager.CheckForNewSkeletonAndDepthMaps( fElapsedTime, m_bPaused );

    switch( m_sampleMode )
    {
    default:
    case SM_DEBUG:    m_debugMode.Update( this, pGamepad ); break;
    case SM_BOXES:    m_boxesMode.Update( this, pGamepad ); break;
    }

    PIXEndNamedEvent();

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: UpdateHandConfidence()
// Desc: Updates open/closed confidence for one or two hands
//--------------------------------------------------------------------------------------
VOID Sample::UpdateHandsConfidence()
{
    CONST NUI_SKELETON_DATA* __restrict pSkeleton = m_cameraManager.GetTrackedSkeleton();
    if( !pSkeleton )
        return;

    PIXBeginNamedEvent( 0, "Acquire and Detect Hand(s)" );

    CONST USHORT* __restrict pDepth320 = m_cameraManager.GetDepth320x240();

    // tilt corrected inside CameraManager
    CONST XMVECTOR* __restrict pPositions = pSkeleton->SkeletonPositions;

    // update player size with a low pass filter
    // do this once per frame per player
    FLOAT fSize;
    if( HOCDetector::GetPlayerSize( fSize, pSkeleton ) )
        m_player.m_fPlayerSize = 0.98f * m_player.m_fPlayerSize + 0.02f * fSize;

    // update one or both hands
    for( UINT uHand=0; uHand < 2; ++uHand )
    {
        // each frame set to non tracked
        for( UINT i=0; i < CI_NUM; ++i )
            m_player.m_results[ i ].m_bLastTracked[ uHand ] = FALSE;

        const BOOL bRightHand = 0 == uHand;

        const NUI_SKELETON_POSITION_INDEX hand  = bRightHand ? NUI_SKELETON_POSITION_HAND_RIGHT  : NUI_SKELETON_POSITION_HAND_LEFT;
        const NUI_SKELETON_POSITION_INDEX wrist = bRightHand ? NUI_SKELETON_POSITION_WRIST_RIGHT : NUI_SKELETON_POSITION_WRIST_LEFT;
        const NUI_SKELETON_POSITION_INDEX elbow = bRightHand ? NUI_SKELETON_POSITION_ELBOW_RIGHT : NUI_SKELETON_POSITION_ELBOW_LEFT;

        // we need ST to tell us where the hand is
        if( pSkeleton->eSkeletonPositionTrackingState[ hand ] == NUI_SKELETON_POSITION_TRACKED )
        {
            const BOOL bElbowTracked = pSkeleton->eSkeletonPositionTrackingState[ elbow ] == NUI_SKELETON_POSITION_TRACKED;
            const BOOL bWristTracked = pSkeleton->eSkeletonPositionTrackingState[ wrist ] == NUI_SKELETON_POSITION_TRACKED;
                
            if( HOCDetector::GetFrameData(  m_player.m_hocData[ uHand ],  // can have a temporary of this type instead, only keeping for display
                                            bElbowTracked, bWristTracked,
                                            pPositions[ elbow ],
                                            pPositions[ wrist ],
                                            pPositions[ hand ],
                                            pDepth320,
                                            m_player.m_fPlayerSize ) )
            {
                // now run our classifiers
                if( m_bLoaded )
                {
                    for( UINT i=0; i < CI_NUM; ++i )
                    {
                        m_player.m_results[ i ].m_bLastTracked[ uHand ] = TRUE;

                        PIXBeginNamedEvent( 0, "Detection" );
                        const FLOAT fDetectedValue = m_classifiers[ i ].Detect( m_player.m_hocData[ uHand ] );
                        PIXEndNamedEvent();

                        // filter confidence
                        m_player.m_results[ i ].AddConfidence( uHand, fDetectedValue );
                    }
                }
            }
        }
    }

    // write out the results for the first classifier for the right hand
    if( m_hXEDFile )
    {
        XStudioWriteTitleData( NULL, &m_player.m_results[ 0 ].m_filter[ 0 ].m_fConfidence, 4 );
    }

    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    PIXBeginNamedEvent( 0, "Render" );

    // Draw a gradient filled background black 
    ATG::RenderBackground( 0xff101025, 0xff101025 );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    // render mode
    switch( m_sampleMode )
    {
    default:
    case SM_DEBUG:    m_debugMode.Render( this ); break;
    case SM_BOXES:    m_boxesMode.Render( this ); break;
    }

    // Render the HUD
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0.0f, 0.0f, 0xffffffff, L"Hand Open/Closed" );
    if( m_bPaused )
        m_Font.DrawText( 300.0f, 0.0f, 0xffffffff, L"(PAUSED)" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0.0f, 0.0f, CLR_YELLOW, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

    if( m_hXEDFile )
    {
        WCHAR tmp[ 1024 ];
        swprintf_s( tmp, L"recording %S", m_szXEDFilePath );
        m_Font.DrawText( 550 - 128, 0, CLR_WHITE, tmp );
    } else
    {
        m_Font.DrawText( 550 - 128, 0, CLR_WHITE, L"NOT recording" );
    }
   
    m_Font.End();

    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );

    PIXEndNamedEvent();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}



//
// debug mode
//

void Sample::DebugMode::Update( Sample* pSample, const ATG::GAMEPAD* pGamepad )
{
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bDetectThumbup = !m_bDetectThumbup;

    if( pSample->m_bTracked )
    {
        ms_debugDraw.NewFrame();

        // do this once per frame per player
        pSample->UpdateHandsConfidence();
    }
}


void Sample::DebugMode::Render( Sample* pSample )
{
    PIXBeginNamedEvent( 0, "Draw HOC debug" );

    pSample->m_cameraManager.DisplayPIP();

    static const FLOAT  LEFT_HAND_LEFT_EDGE_TEXT = 128;
    static const FLOAT  RIGHT_HAND_LEFT_EDGE_TEXT = 980;
    static const UINT   HISTORY_Y = 300;
    static const UINT   CONTOUR_BOTTOM = 500;
    static const FLOAT  CONTOUR_HEIGHT = 100;
    static const UINT   HISTOGRAM_BOTTOM = 720 - 64;
    static const UINT   NORM_HISTOGRAM_SCALE = 500;
    static const UINT   INT_HISTOGRAM_SCALE = 100;
    static const UINT   HISTOGRAM_BAR_WIDTH = 10;
    static const FLOAT  VOXELS_PORT_SX = 1280;
    static const FLOAT  VOXELS_PORT_SY = 720;
    static const FLOAT  CONTOUR_WIDTH = _countof( pSample->m_player.m_hocData[ 0 ].m_computedData.m_normalizedHistogram[ 0 ] ) * HISTOGRAM_BAR_WIDTH;

    const FilteredResult& res = pSample->m_player.m_results[ !!m_bDetectThumbup ];

    for( UINT uHand=0; uHand < 2; ++uHand )
    {
        const FLOAT fLeftEdge = (uHand == 0) ? RIGHT_HAND_LEFT_EDGE_TEXT : LEFT_HAND_LEFT_EDGE_TEXT;

        const HOCDataViews& dv = pSample->m_player.m_hocData[ uHand ];

        // draw histograms
        {
            for( UINT i=0; i < _countof( dv.m_computedData.m_normalizedHistogram[ 0 ] ); ++i )
            {
                ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( fLeftEdge + i * HISTOGRAM_BAR_WIDTH,
                                                     (FLOAT)HISTOGRAM_BOTTOM ),
                                                     XMFLOAT2( (FLOAT)HISTOGRAM_BAR_WIDTH, -dv.m_computedData.m_normalizedHistogram[ 0 ][ i ] * NORM_HISTOGRAM_SCALE ),
                                                     1,
                                                     0xff00ff00 );
            }

            for( UINT i=0; i < _countof( dv.m_computedData.m_integralHistogram[ 0 ] ); ++i )
            {
                ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( fLeftEdge + i * HISTOGRAM_BAR_WIDTH,
                                                     (FLOAT)HISTOGRAM_BOTTOM ),
                                                     XMFLOAT2( (FLOAT)HISTOGRAM_BAR_WIDTH, -dv.m_computedData.m_integralHistogram[ 0 ][ i ] * INT_HISTOGRAM_SCALE ),
                                                     1,
                                                     0xffff0000 );
            }
        }

        // draw voxels
        {
            static const FLOAT TAN_FOV_PER_PIXEL_INV = ( (VOXELS_PORT_SX)/2.0f ) / tan( XMConvertToRadians( NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV * 0.5f ) );

            const FLOAT fCentreXOfs = (uHand == 0) ? 128.f : -128.f;

            const UINT num = pSample->m_player.m_hocData[ uHand ].m_transientData.m_points.size();
            for( UINT i=0; i < num; ++i )
            {
                XMFLOAT3   w = dv.m_transientData.m_points[ i ];

                w.x -= dv.m_transientData.m_vCentroid.x;
                w.y -= dv.m_transientData.m_vCentroid.y;

                XMFLOAT2    p;
                
                FLOAT   iz = 1.f / w.z;
                p.x = ( w.x * TAN_FOV_PER_PIXEL_INV ) * iz + VOXELS_PORT_SX/2.0f + fCentreXOfs;
                p.y = VOXELS_PORT_SY/2.0f - ( w.y * TAN_FOV_PER_PIXEL_INV ) * iz;

                ATG::DebugDraw::DrawScreenSpaceRect( p, XMFLOAT2( 4, 4 ), 1, i == dv.m_transientData.m_uIndexToFurthestVoxel[ 0 ] ? CLR_YELLOW : CLR_GRAY );
            }

            // centroid and palm centre
            {
                XMFLOAT2    p;

                FLOAT   iz = 1.f / dv.m_transientData.m_vCentroid.z;
                p.x = VOXELS_PORT_SX/2.0f + fCentreXOfs;
                p.y = VOXELS_PORT_SY/2.0f;
                ATG::DebugDraw::DrawScreenSpaceRect( p, XMFLOAT2( 4, 4 ), 1, CLR_RED );

                XMFLOAT3   w = dv.m_transientData.m_vPalmCentre;

                w.x -= dv.m_transientData.m_vCentroid.x;
                w.y -= dv.m_transientData.m_vCentroid.y;

                iz = 1.f / w.z;
                p.x = ( w.x * TAN_FOV_PER_PIXEL_INV ) * iz + VOXELS_PORT_SX/2.0f + fCentreXOfs;
                p.y = VOXELS_PORT_SY/2.0f - ( w.y * TAN_FOV_PER_PIXEL_INV ) * iz;

                ATG::DebugDraw::DrawScreenSpaceRect( p, XMFLOAT2( 4, 4 ), 1, CLR_GREEN );
            }
        }

        // draw contour in depth image and also as a separate graph
        if( !dv.m_transientData.m_points.empty() )
        {
            const HOCComputedTransientData& data = dv.m_transientData;

            // the contour graph
            const FLOAT   fGraphStepX = CONTOUR_WIDTH / (FLOAT)HOCComputedData::NUM_CONTOUR_POINTS;
            const FLOAT   fGraphScaleY = CONTOUR_HEIGHT;
            XMFLOAT2 contourGraph[ HOCComputedData::NUM_CONTOUR_POINTS ];
            for( UINT i=0; i < HOCComputedData::NUM_CONTOUR_POINTS; ++i )
            {
                contourGraph[ i ].x = i * fGraphStepX + fLeftEdge;
                contourGraph[ i ].y = CONTOUR_BOTTOM - (data.m_contourSmooth[ i ]) * fGraphScaleY;
            }

            // the contour on the picture in picture image
            const UINT fSizeX = data.m_depthImageBoundingBox[ 2 ];
            const UINT fSizeY = data.m_depthImageBoundingBox[ 3 ];

            XMFLOAT2 contourPoints[ HOCComputedData::NUM_CONTOUR_POINTS ];

            const FLOAT fAngleStep = 2 * XM_PI / (FLOAT)HOCComputedData::NUM_CONTOUR_POINTS;

            const FLOAT fCentreX = fSizeX/2.f + data.m_depthImageBoundingBox[ 0 ];
            const FLOAT fCentreY = fSizeY/2.f + data.m_depthImageBoundingBox[ 1 ];

            FLOAT   fAngle = 0;

            // from each point of the side of the square cast a ray to the centre
            for( UINT i=0; i < HOCComputedData::NUM_CONTOUR_POINTS; ++i )
            {
                const FLOAT fX = cosf( fAngle );
                const FLOAT fY = sinf( fAngle );

                const FLOAT fRadius = data.m_contour[ i ];

                const UINT  x0 = (UINT)(fCentreX + fRadius * fX);
                const UINT  y0 = (UINT)(fCentreY + fRadius * fY);

                contourPoints[ i ].x = x0 * 2.f + CameraManager::pipDrawX;
                contourPoints[ i ].y = y0 * 2.f + CameraManager::pipDrawY;
                    
                fAngle += fAngleStep;
            }

            ATG::DebugDraw::DrawScreenSpaceLineList( &contourPoints[ 0 ], HOCComputedData::NUM_CONTOUR_POINTS, CLR_WHITE, 1 );
            ATG::DebugDraw::DrawScreenSpaceLineList( &contourGraph[ 0 ], HOCComputedData::NUM_CONTOUR_POINTS, CLR_WHITE, 1 );
            ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( fLeftEdge, CONTOUR_BOTTOM - fGraphScaleY ), XMFLOAT2( (FLOAT)CONTOUR_WIDTH, fGraphScaleY ), 1, CLR_WHITE );
        }
    }

    // this is to illustrate interactions -- draw a tracer when the hand is closed
    const XMFLOAT3    vHandScreenPos[ 2 ] = { pSample->m_cameraManager.GetHandsPositions().m_rightHand,
                                              pSample->m_cameraManager.GetHandsPositions().m_leftHand };
    if( pSample->m_bTracked )
    {
        for( UINT uHand=0; uHand < 2; ++uHand )
        {
            const FLOAT fSz = (res.m_filter[ uHand ].m_fFilteredConfidence > 0) ? 16.f : 32.f;

            ATG::DebugDraw::DrawScreenSpaceRect( *(XMFLOAT2*)&vHandScreenPos[ uHand ],
                                                 XMFLOAT2( fSz, fSz ),
                                                 5,
                                                 (uHand == 0) ? CLR_RIGHT_HAND : CLR_LEFT_HAND );
        }
    }

    // store history of filtered result and render it
    for( UINT uHand=0; uHand < 2; ++uHand )
    {
        XMFLOAT4& h = m_history[ uHand ][ m_uHistoryTail[ uHand ] ];
        h.x = vHandScreenPos[ uHand ].x;
        h.y = vHandScreenPos[ uHand ].y;
        h.z = 2;
        h.w = res.m_filter[ uHand ].m_fFilteredConfidence;

        m_uHistoryTail[ uHand ] = (m_uHistoryTail[ uHand ] + 1) % _countof( m_history[ uHand ] );

        const FLOAT   fLeftEdge = (uHand == 0) ? RIGHT_HAND_LEFT_EDGE_TEXT : LEFT_HAND_LEFT_EDGE_TEXT;

        for( UINT i=0; i < _countof( m_history[ uHand ] ); ++i )
        {
            XMFLOAT2 org, sz;

            const XMFLOAT4& v = m_history[ uHand ][ (m_uHistoryTail[ uHand ] + i) % _countof( m_history[ uHand ] ) ];

            org.x = fLeftEdge + i;
            org.y = HISTORY_Y;
            sz.x = 1;
            sz.y = -v.w;    // 1 is up, -1 is down on the screen
            ATG::DebugDraw::DrawScreenSpaceRect( org, sz, 1, sz.y > 0 ? 0xffff0000 : 0xff00ff00 );

            if( v.w > 0 &&
                i < _countof( m_history[ uHand ] ) - 1 )
            {
                const XMFLOAT4& v1 = m_history[ uHand ][ (m_uHistoryTail[ uHand ] + i + 1) % _countof( m_history[ uHand ] ) ];

                ATG::DebugDraw::DrawScreenSpaceLine( *(XMFLOAT2*)&v, *(XMFLOAT2*)&v1, (uHand == 0) ? CLR_RIGHT_HAND : CLR_LEFT_HAND, v.z );
            }
        }
    }

    // text rendering
    pSample->m_Font.Begin();

    extern const WCHAR* pDebugText;
    if( pDebugText )
        pSample->m_Font.DrawText( RIGHT_HAND_LEFT_EDGE_TEXT - 128, 300, CLR_WHITE, pDebugText );

    for( UINT uHand=0; uHand < 2; ++uHand )
    {
        const FLOAT fLeftEdge = (uHand == 0) ? RIGHT_HAND_LEFT_EDGE_TEXT : LEFT_HAND_LEFT_EDGE_TEXT;

        WCHAR    tmp[ 1024 ];
        if( pSample->m_bLoaded )
        {
            if( res.m_bLastTracked[ uHand ] )
            {
                if( m_bDetectThumbup )
                {
                    swprintf_s( tmp, (res.m_filter[ uHand ].m_fFilteredConfidence > 0) ? L"THUMB UP" : L"NOT THUMB UP" );
                } else
                {
                    swprintf_s( tmp, (res.m_filter[ uHand ].m_fFilteredConfidence > 0) ? L"CLOSED HAND" : L"NOT CLOSED HAND" );
                }
            } else
            {
                swprintf_s( tmp, L"NOT TRACKED" );
            }
        } else
        {
            swprintf_s( tmp, L"DATABASE NOT LOADED" );
        }

        pSample->m_Font.DrawText( fLeftEdge - 128, 100, CLR_WHITE, tmp );
    }

    pSample->m_Font.End();

    // draw debug renderer
    ms_debugDraw.Render();

    PIXEndNamedEvent();
}



//
// boxes mode
//

void Sample::BoxesMode::Update( Sample* pSample, const ATG::GAMEPAD* pGamepad )
{
    // check the controller inputs
    if( pGamepad->sThumbRX >  pGamepad->RIGHT_THUMB_DEADZONE  ||
        pGamepad->sThumbRX < -pGamepad->RIGHT_THUMB_DEADZONE ||
        pGamepad->sThumbRY >  pGamepad->RIGHT_THUMB_DEADZONE  ||
        pGamepad->sThumbRY < -pGamepad->RIGHT_THUMB_DEADZONE  )
    {
        m_cursorPos.x += (pGamepad->sThumbRX / 32767.f) * 5.f;
        m_cursorPos.y -= (pGamepad->sThumbRY / 32767.f) * 5.f;
    }

    // check the nui inputs for cursor position update
    if( pSample->m_bTracked )
    {
        ms_debugDraw.NewFrame();

        // do this once per frame per player
        pSample->UpdateHandsConfidence();

        // only use one hand at a time for this mode
        // get the active hand's position and open/closed status
        const CameraManager::HandPositions& hands = pSample->m_cameraManager.GetHandsPositions();
        if( hands.m_bUseHands )
            m_cursorPos = hands.m_bUseLeftHand ? hands.m_leftHand : hands.m_rightHand;
    }

    // clamp cursor position to safe area
    if( m_cursorPos.x < 128.f )
        m_cursorPos.x = 128.f;
    if( m_cursorPos.y < 72.f )
        m_cursorPos.y = 72.f;
    if( m_cursorPos.x > 1152.f )
        m_cursorPos.x = 1152;
    if( m_cursorPos.y > 648.f )
        m_cursorPos.y = 648.f;

    // is cursor over the box?
    if( !m_bDragging && !m_bTurning )
    {
        m_uHoverIndex = ~0ul;

        for( UINT i=0; i < _countof( m_boxes ); ++i )
        {
            BOOL bHover = (m_cursorPos.x >= m_boxes[ i ].m_pos.x) &&
                          (m_cursorPos.x < (m_boxes[ i ].m_pos.x + m_boxes[ i ].m_size.x)) &&
                          (m_cursorPos.y >= m_boxes[ i ].m_pos.y) &&
                          (m_cursorPos.y < (m_boxes[ i ].m_pos.y + m_boxes[ i ].m_size.y));

            // don't allow to change placed boxes
            if( bHover  &&
                !m_boxesTargets[ i ].m_bPlaced )
            {
                m_uHoverIndex = i;
                break;
            }
        }
    }

    // grabbing and turning triggers -- controller check first, then nui check
    BOOL bGrabbing = pGamepad->bRightTrigger;
    BOOL bTurning = (pGamepad->sThumbLX > pGamepad->LEFT_THUMB_DEADZONE  ||
                    pGamepad->sThumbLX < -pGamepad->LEFT_THUMB_DEADZONE);

    // when using controller we don't have the ST overshoot on open hand so just
    // store the last position
    if( bGrabbing )
    {
        m_cursorPosLastClosed = m_cursorPos;
    }

    // nui check for grabbing and turning states
    if( pSample->m_bTracked )
    {
        const CameraManager::HandPositions& hands = pSample->m_cameraManager.GetHandsPositions();
        if( hands.m_bUseHands )
        {
            {
                // closed -- grab! opened -- release
                const FilteredResult& res = pSample->m_player.m_results[ Sample::CI_HOC ];
                bGrabbing = res.m_filter[ hands.m_bUseLeftHand ].m_fFilteredConfidence > 0;

                // remember the last position when the hand was closed for a smoother release
                if( res.m_filter[ hands.m_bUseLeftHand ].m_fConfidence > 0 )
                    m_cursorPosLastClosed = m_cursorPos;
            }

            {
                // thumb up starts turning
                const FilteredResult& res = pSample->m_player.m_results[ Sample::CI_THM ];
                bTurning = res.m_filter[ hands.m_bUseLeftHand ].m_fFilteredConfidence > 0;
            }
        }
    }

    // move the box if grabbed
    if( bGrabbing && !bTurning )
    {
        if( !m_bDragging &&
            m_uHoverIndex != ~0ul )
        {
            m_bDragging = TRUE;

            m_cursorDown = m_cursorPos;

            m_cursorPosLastClosed = m_cursorPos;

            m_cursorDownOfs.x = m_boxes[ m_uHoverIndex ].m_pos.x - m_cursorPos.x;
            m_cursorDownOfs.y = m_boxes[ m_uHoverIndex ].m_pos.y - m_cursorPos.y;
        }
    } else if( m_bDragging )
    {
        m_bDragging = FALSE;

        // ST moves hand joint up when you open the hand, so we only remember cursor end when
        // this frame's unfiltered confidence is "closed" to prevent it moving up when you open
        // the hand. this should probably be speed dependant.
        m_boxes[ m_uHoverIndex ].m_pos.x = (m_cursorPos.x + m_cursorPosLastClosed.x) * 0.5f + m_cursorDownOfs.x;
        m_boxes[ m_uHoverIndex ].m_pos.y = (m_cursorPos.y + m_cursorPosLastClosed.y) * 0.5f + m_cursorDownOfs.y;
    }

    // move the box if dragging
    if( m_bDragging )
    {
        m_boxes[ m_uHoverIndex ].m_pos.x = m_cursorPos.x + m_cursorDownOfs.x;
        m_boxes[ m_uHoverIndex ].m_pos.y = m_cursorPos.y + m_cursorDownOfs.y;
    }

    // turning using a thumb up
    if( m_uHoverIndex != ~0ul   &&
        !m_bDragging )
    {
        if( bTurning )
        {
            m_bTurning = TRUE;

            // controller input first for the turning
            m_boxes[ m_uHoverIndex ].m_fAngle = pGamepad->sThumbLX / 32767.f;

            // nui input for the turning
            if( pSample->m_bTracked )
            {
                const CameraManager::HandPositions& hands = pSample->m_cameraManager.GetHandsPositions();
                if( hands.m_bUseHands )
                {
                    const HOCDataViews& dv = pSample->m_player.m_hocData[ hands.m_bUseLeftHand ];
                    if( !dv.m_transientData.m_points.empty() )
                    {
                        const XMVECTOR vToFinger = XMLoadFloat3( &dv.m_transientData.m_points[ dv.m_transientData.m_uIndexToFurthestVoxel[ 0 ] ] ) -
                                                    XMLoadFloat3( &dv.m_transientData.m_vCentroid );

                        // run LPF on it as it can be very noisy
                        m_boxes[ m_uHoverIndex ].m_fAngle = m_boxes[ m_uHoverIndex ].m_fAngle * 0.80f +
                                                           XMVectorGetX( XMVector3Normalize( vToFinger ) ) * 0.2f;

                        // snap cursor to the centre, ST moves hand joint as you rotate the finger causing it to lose focus
                        m_cursorPos.x = m_boxes[ m_uHoverIndex ].m_pos.x + m_boxes[ m_uHoverIndex ].m_size.x * 0.5f;
                        m_cursorPos.y = m_boxes[ m_uHoverIndex ].m_pos.y + m_boxes[ m_uHoverIndex ].m_size.y * 0.5f;
                    }
                }
            }
        } else
        {
            m_bTurning = FALSE;
        }
    } else
    {
        m_bTurning = FALSE;
    }

    // check box placements
    {
        for( UINT i=0; i < _countof( m_boxesTargets ); ++i )
        {
            if( m_boxesTargets[ i ].m_bPlaced )
                continue;

            const FLOAT fdA = m_boxesTargets[ i ].m_fAngle - m_boxes[ i ].m_fAngle;
            const FLOAT fdX = m_boxesTargets[ i ].m_pos.x - m_boxes[ i ].m_pos.x;
            const FLOAT fdY = m_boxesTargets[ i ].m_pos.y - m_boxes[ i ].m_pos.y;

            const FLOAT fD2 = fdX * fdX + fdY * fdY;

            if( fD2 < 64.f  && fabsf( fdA ) < 0.5f )
            {
                if( !m_bDragging && !m_bTurning )
                {
                    m_boxesTargets[ i ].m_bPlaced = TRUE;

                    m_boxes[ i ].m_fAngle = m_boxesTargets[ i ].m_fAngle;
                    m_boxes[ i ].m_pos = m_boxesTargets[ i ].m_pos;
                } else
                {
                    m_boxesTargets[ i ].m_bOnTop = TRUE;
                }
            } else
            {
                m_boxesTargets[ i ].m_bOnTop = FALSE;
            }
        }
    }

    // regenerate boxes placements when all are done
    {
        UINT i;
        for( i = 0; i < _countof( m_boxesTargets ); ++i )
        {
            if( !m_boxesTargets[ i ].m_bPlaced )
                break;
        }

        if( i == _countof( m_boxesTargets ) )
        {
            CreateBoxTargets();
        }
    }
}


static
void DrawRotatedQuad( const XMFLOAT2& pos0, const XMFLOAT2& size, FLOAT fAngle, DWORD dwColor, FLOAT fExpand = 1.f )
{
    const FLOAT cs = cosf( fAngle );
    const FLOAT sn = sinf( fAngle );

    const FLOAT hx = size.x * 0.5f + fExpand;
    const FLOAT hy = size.y * 0.5f + fExpand;

    XMFLOAT2 pos = pos0;
    pos.y = 720.f - pos0.y;

    XMFLOAT3 v0, v1, v2;

    v0.x = -hx *  cs + -hy * sn + pos.x + hx - fExpand;
    v0.y = -hx * -sn + -hy * cs + pos.y - hy + fExpand;
    v0.z = 1;                           
                                        
    v1.x =  hx *  cs + -hy * sn + pos.x + hx - fExpand;
    v1.y =  hx * -sn + -hy * cs + pos.y - hy + fExpand;
    v1.z = 1;                           
                                        
    v2.x = -hx *  cs + hy * sn + pos.x  + hx - fExpand;
    v2.y = -hx * -sn + hy * cs + pos.y  - hy + fExpand;
    v2.z = 1; 

    ATG::DebugDraw::DrawQuad( v0, v1, v2, dwColor );
}



void Sample::BoxesMode::Render( Sample* pSample )
{
    PIXBeginNamedEvent( 0, "Draw bubbles" );

    // draw physics scene
    XMMATRIX    v = XMMatrixLookAtLH( XMVectorSet( 1280 / 2, 720 / 2, 0, 1 ), XMVectorSet( 1280 / 2, 720 / 2, 1, 1 ), XMVectorSet( 0, 1, 0, 0 ) );
    XMMATRIX    p = XMMatrixOrthographicLH( 1280, 720, 0, 10 );
    ATG::DebugDraw::SetViewProjection( XMMatrixMultiply( v, p ) );

    for( UINT i=0; i < _countof( m_boxesTargets ); ++i )
    {
        const DWORD dwClr = m_boxesTargets[ i ].m_bPlaced ? CLR_CYAN :                          // cyan if placed
                                        m_boxesTargets[ i ].m_bOnTop ? CLR_YELLOW :             // yellow if on top
                                                ((m_uHoverIndex == i) ? CLR_WHITE : CLR_GRAY);  // gray is inactive, white if active
        DrawRotatedQuad( m_boxesTargets[ i ].m_pos, m_boxes[ i ].m_size, m_boxesTargets[ i ].m_fAngle, dwClr, 6.f );
    }

    for( UINT i=0; i < _countof( m_boxes ); ++i )
    {
        if( m_uHoverIndex == i )
        {
            DrawRotatedQuad( m_boxes[ i ].m_pos, m_boxes[ i ].m_size, m_boxes[ i ].m_fAngle, CLR_WHITE, 4.f );
            DrawRotatedQuad( m_boxes[ i ].m_pos, m_boxes[ i ].m_size, m_boxes[ i ].m_fAngle, m_boxes[ i ].m_dwColor1 );
        } else
        {
            DrawRotatedQuad( m_boxes[ i ].m_pos, m_boxes[ i ].m_size, m_boxes[ i ].m_fAngle, m_boxes[ i ].m_dwColor0 );
        }
    }

    const CameraManager::HandPositions& hands = pSample->m_cameraManager.GetHandsPositions();

    // cursor
    {
        const FLOAT fSz = m_bDragging ? 16.f : 32.f;

        const XMFLOAT2 pos( m_cursorPos.x - fSz * 0.5f, m_cursorPos.y - fSz * 0.5f );

        if( pSample->m_bTracked )
        {
            ATG::DebugDraw::DrawScreenSpaceRect( pos, XMFLOAT2( fSz, fSz ), 5.f, hands.m_bUseLeftHand ? CLR_LEFT_HAND : CLR_RIGHT_HAND );
        } else
        {
            ATG::DebugDraw::DrawScreenSpaceRect( pos, XMFLOAT2( fSz, fSz ), 5.f, CLR_WHITE );
        }
    }

    pSample->m_Font.Begin();
    pSample->m_Font.DrawText( 512.0f, 20.0f, pSample->m_bTracked? 0xffffffff : 0xff808080, L"NUI" );
    pSample->m_Font.DrawText( 600, 20.0f, pSample->m_bTracked ? 0xffffffff : 0xff808080, hands.m_bUseHands ? (!hands.m_bUseLeftHand ? L"USING RIGHT HAND" : L"USING LEFT HAND") : L"PLAYER NOT INTERACTING" );
    pSample->m_Font.DrawText( 512, 40, m_bDragging ? 0xffffffff : 0xff808080, L"DRAGGING" );
//    pSample->m_Font.DrawText( 800, 40, m_bTurning? 0xffffffff : 0xff808080, L"TURNING" );
    pSample->m_Font.End();

    PIXEndNamedEvent();
}


void Sample::BoxesMode::CreateBoxTargets()
{
    for( UINT i=0; i < _countof( m_boxesTargets ); ++i )
    {
        m_boxesTargets[ i ].m_bPlaced = FALSE;
        m_boxesTargets[ i ].m_bOnTop = FALSE;

        XMFLOAT2    newPos;
        for( UINT j=0; j < 3; ++j )
        {
            newPos.x = ((FLOAT)rand() / RAND_MAX) * 700.f + 1280.f * 0.1f;
            newPos.y = ((FLOAT)rand() / RAND_MAX) * 450.f + 720.f * 0.1f;

            if( (newPos.x - m_boxes[ i ].m_pos.x) > 150.f &&
                (newPos.y - m_boxes[ i ].m_pos.y) > 150.f )
            {
                break;
            }
        }

        m_boxesTargets[ i ].m_pos = newPos;
    }
}
