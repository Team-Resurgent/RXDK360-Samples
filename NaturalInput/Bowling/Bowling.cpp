//--------------------------------------------------------------------------------------
// Bowling.cpp
//
// Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xnamath.h>
#include <nuiapi.h>

#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgNuiVisualization.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <AtgNuiCommon.h>

#include "BasicBowlingFilter.h"


// Colors used in the sample
const D3DCOLOR D3DCOLOR_LIGHT_BLUE = D3DCOLOR_XRGB( 0, 255, 255);
const D3DCOLOR D3DCOLOR_GREEN      = D3DCOLOR_XRGB( 0, 255, 0);
const D3DCOLOR D3DCOLOR_RED        = D3DCOLOR_XRGB( 255, 0, 0 );
const D3DCOLOR D3DCOLOR_WHITE      = D3DCOLOR_XRGB( 255, 255, 255 );
const D3DCOLOR D3DCOLOR_YELLOW     = D3DCOLOR_XRGB( 255, 255, 0 );

// Colors used to display different parts of a skeleton
const D3DCOLOR D3DCOLOR_INFERED_BONE     = D3DCOLOR_RED;
const D3DCOLOR D3DCOLOR_TRACKED_BONE     = D3DCOLOR_GREEN;
const D3DCOLOR D3DCOLOR_HIGHLIGHTED_BONE = D3DCOLOR_WHITE;

// Thickeness of the line used for drawing the skeleton in playback mode.
static const FLOAT LINE_WIDTH = 3.0f;

// Defines the state a players is in
enum BOWLING_STATE
{
    BOWLING_STATE_NOT_READY = 0,   // Waiting for the player to assume the ready position.
    BOWLING_STATE_READY,           // The gesture hasn't been confirmed as a bowling gesture yet.
    BOWLING_STATE_INITIATING,      // Arm is in front of the player and it looks like he is intent on bowling
    BOWLING_STATE_FRONT_SWING,
    BOWLING_STATE_BACK_SWING,      // Arm is behind the player 
    BOWLING_STATE_FORWARD_SWING,   // Arm has reach its maximum backward position and is returning to the front.
    BOWLING_STATE_SWING_COMPLETED, // Completion detected, release the ball!
    BOWLING_STATE_FOLLOW_THROUGH,  // Move is completed, keep the filter active some more for blending.
    BOWLING_STATE_MAX
};


// Rect used for displaying the color stream
typedef struct
{
    FLOAT fX;
    FLOAT fY;
    FLOAT fWidth;
    FLOAT fHeight;
} Rect;


// For displaying a color image of the player
struct VideoFeedVertex
{
    FLOAT vPosition[ 3 ];
    FLOAT vTexCoords[ 2 ];
};


// RGB shader used to display the color stream during the recording phase.
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
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] = 
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Single Step Mode" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_1, L"Record New Bowling Gesture" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_1, L"Change Playback Speed" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Exit Single Step Mode" },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_1, L"Rotate camera" },
};
static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE(g_HelpCallouts);


//--------------------------------------------------------------------------------------
// Name: DrawLineSegment
// Desc: Forward calls to ATG::DebugDraw::DrawLineSegment which takes XMFLOAT3 as 
//       parameters instead of an XMVECTOR.
//--------------------------------------------------------------------------------------
inline void DrawLineSegment( XMVECTOR vOrigin, XMVECTOR vEnd, D3DCOLOR Color )
{     
    XMFLOAT3 vOrigin3;
    XMStoreFloat3( &vOrigin3, vOrigin );
    XMFLOAT3 vEnd3;
    XMStoreFloat3( &vEnd3, vEnd );
    ATG::DebugDraw::DrawLineSegment( vOrigin3, vEnd3, Color );
}


//--------------------------------------------------------------------------------------
// Name: GetBowlingStateText
// Desc: Returns a text string describing a bowling state
//--------------------------------------------------------------------------------------
const WCHAR* GetBowlingStateText( BOWLING_STATE eBowlingState )
{
    switch( eBowlingState )
    {
        case BOWLING_STATE_NOT_READY:
            return L"Not Bowling";

        case BOWLING_STATE_READY:
            return L"Ready";

        case BOWLING_STATE_INITIATING:
            return L"Expecting a bowling gesture";

        case BOWLING_STATE_FRONT_SWING:
            return L"Bowling gesture detected";

        case BOWLING_STATE_BACK_SWING:
            return L"Swinging back";

        case BOWLING_STATE_FORWARD_SWING:
            return L"Swinging forward";

        case BOWLING_STATE_SWING_COMPLETED:
            return L"Throw Detected";

        case BOWLING_STATE_FOLLOW_THROUGH:
            return L"Following through";
        default:
            assert( false );
            return L"Unknow state";
    }
}


//--------------------------------------------------------------------------------------
// Name: GetThrowResult
// Desc: Returns a random text string indicating the result of the player's perfomance.
//--------------------------------------------------------------------------------------
const WCHAR* GetThrowResult()
{
    switch( rand() % 16 )
    {
        case 0:
            return L"Baby Split";

        case 1:
            return L"Big Five";

        case 2:
            return L"Blow";

        case 3:
            return L"Bucket";

        case 4:
            return L"Cincinnati";

        case 5:
            return L"Fit Split";

        case 6:
            return L"Golden Gate";

        case 7:
            return L"Gutter Ball";

        case 8:
            return L"Miss";

        case 9:
            return L"Quick Eight";

        case 10:
            return L"Railroad";

        case 11:
            return L"Rap";

        case 12:
            return L"Ringing Ten-Burner";

        case 13:
            return L"Sour Apple";

        case 14:
            return L"Strike";

        case 15:
            return L"Tumbler";

        default:
            assert( false );
            return L"Strike";
    }
}


// Keep the last gesture in memory for playback purposes
const static DWORD SKELETON_HISTORY_MAX = 30 * 3;  // Keep about 3 seconds worth of history (at 30 skeleton frames per second)
static NUI_SKELETON_FRAME g_RawSkeletonHistory[ SKELETON_HISTORY_MAX ];  // Ring buffer that hold unfiltered skeletons, used for comparision


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // General class operations
    ATG::Timer          m_Timer;
    ATG::Font           m_Font;
    ATG::Help           m_Help;
    BOOL                m_bDrawHelp;

    // Recording and Playback mode data
    BOOL   m_bIsRecording;         // Set to TRUE when the sample is in recording mode.
    FLOAT  m_fOutcomeDisplayTimer;  // Time (in seconds) left for displaying the result of the player's throw. 
    FLOAT  m_fPlaybackSpeed;
    BOOL   m_bIsStepModeOn;
    BOOL   m_bIsReadyForNextFrame;
    DWORD  m_dwSkeletonHistoryCount;        // Number of skeletons kept in history ring buffers
    DWORD  m_dwSkeletonHistoryCurrentIndex; // Current read possition when playing back recorded skeletons

    // Player data
    BOOL          m_bIsPlayerReadyToPlay;
    BOOL          m_bIsLeftHanded;         // TRUE if the player is left handed.
    BOWLING_STATE m_eBowlingState;
    FLOAT         m_fLastArmAngle;

    // Bowling Filter and filtered skeleton frame
    BasicBowlingFilter m_BasicBowlingFilter;
    NUI_SKELETON_FRAME m_FilteredSkeletonFrame;
    DWORD              m_dwTrackedSkeletonIndex;
    BOOL               m_bIsUsingInferredArm;

    // Depth and color streams are used during the recording phase.
    HANDLE                       m_hDepthStream;
    HANDLE                       m_hColorStream;

    IDirect3DVertexShader9*      m_pVideoVertexShader;
    IDirect3DPixelShader9*       m_pVideoPixelShaderRGB;
    IDirect3DVertexDeclaration9* m_pVideoVertexDecl;
    IDirect3DTexture9*           m_pDepthTexture;
    IDirect3DTexture9*           m_pColorTexture;

    Rect                         m_ColorWindow;

    INT                          m_DepthWidth;
    INT                          m_DepthHeight;
    INT                          m_ColorWidth;
    INT                          m_ColorHeight;


    // Camera control for playback mode.
    XMVECTOR m_vCameraPosition;
    XMVECTOR m_vUp;

private:

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    VOID EnterRecordingMode();  
    VOID ProcessGameController( FLOAT fElapsedTime );
    VOID RetrieveVideoStreams();
    BOOL RetrieveNextSkeletonFrame();
    FLOAT GetElapsedTime( DWORD dwFrameIndex ) const;
    DWORD GetPreviousFrameIndex( DWORD dwFrameIndex ) const;

    // Recorded mode only member functions
    HRESULT InitializeDepthAndColorRendering();
    VOID UpdateDepthTexture( IDirect3DTexture9* pSourceImage );
    VOID UpdateColorTexture( IDirect3DTexture9* pSourceImage );
    VOID SubmitVertexData();
    const WCHAR* GetPlayerMessage() const;

    // Playback only member functions
    VOID RenderSkeletonAndHighlightDifferenceInArm( const NUI_SKELETON_DATA* pSkeletonData, 
                                                    const NUI_SKELETON_DATA* pReferenceSkeleton, BOOL bIsLeftHanded );
    const WCHAR* GetPlaybackModeText() const;

};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;

    // Recording and Playback mode data
    m_bIsRecording         = TRUE;
    m_fOutcomeDisplayTimer = 0.0f; 
    m_fPlaybackSpeed       = 0.5f;
    m_bIsStepModeOn        = FALSE;
    m_bIsReadyForNextFrame = FALSE;

    // Initialize the ring buffer 
    m_dwSkeletonHistoryCurrentIndex = 0;
    m_dwSkeletonHistoryCount        = 0;

    // Player data
    m_bIsPlayerReadyToPlay = FALSE;
    m_bIsLeftHanded        = FALSE;
    m_eBowlingState        = BOWLING_STATE_NOT_READY;   
    m_fLastArmAngle        = 0.0f;

    m_dwTrackedSkeletonIndex = 0;
    m_bIsUsingInferredArm    = FALSE;

    HRESULT hr;

    // Initializes the Natural Input system.
    if( FAILED( hr = NuiInitialize(  NUI_INITIALIZE_FLAG_USES_COLOR |
                                     NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                     NUI_INITIALIZE_FLAG_USES_SKELETON, 
                                     NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD ) ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    InitializeDepthAndColorRendering();

    // Enable skeletal tracking
    if( FAILED ( hr = NuiSkeletonTrackingEnable( NULL, 0 ) ) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
        return E_FAIL;
    }

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Simple shader are required for NuiVisualization.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    m_vCameraPosition = XMVectorSet( 0.0f, 1.0f, -2.0f, 0.0f );
    m_vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: InitializeDepthAndColorRendering()
// Desc: Open the videos streams and create resources needed to procees and display them.
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeDepthAndColorRendering()
{
    m_pDepthTexture = NULL;
    m_pColorTexture = NULL;

    m_DepthWidth = 320;
    m_DepthHeight = 240;

    m_ColorWidth = 640;
    m_ColorHeight = 480;

    HRESULT hr;
    if( FAILED ( hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR_IN_DEPTH_SPACE,
                                     NUI_IMAGE_RESOLUTION_640x480,
                                     0,
                                     1, 
                                     NULL,
                                     &m_hColorStream ) ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    if( FAILED ( hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
                                     NUI_IMAGE_RESOLUTION_320x240,
                                     0,
                                     1,     // This indicates how many times we can call NuiImageStreamGetNextFrame without releasing the frame
                                     NULL,
                                     &m_hDepthStream ) ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    // Compile vertex shader.
    ID3DXBuffer* pVertexShaderCode;
    ID3DXBuffer* pVertexErrorMsg;
    hr = D3DXCompileShader( g_strVideoShaderHLSL,
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
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pVertexShaderCode->GetBufferPointer(),
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
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pPixelShaderCode->GetBufferPointer(),
                                     &m_pVideoPixelShaderRGB );

    // Define the vertex elements and
    // Create a vertex declaration from the element descriptions.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVideoVertexDecl );

    // Create textures for displaying color and depth streams
    if( FAILED( m_pd3dDevice->CreateTexture( m_DepthWidth, m_DepthHeight, 0, 0, D3DFMT_LIN_X8R8G8B8, 0, &m_pDepthTexture, NULL ) ) )
        return E_FAIL;

    if( FAILED( m_pd3dDevice->CreateTexture( m_ColorWidth, m_ColorHeight, 0, 0, ATG::GetAs16SRGBFormat( D3DFMT_LIN_A8R8G8B8 ), 0, &m_pColorTexture, NULL ) ) )
        return E_FAIL;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Sample::EnterRecordingMode()
// Desc: Resets internal states and returns the sample to the recording screen
//-----------------------------------------------------------------------------
VOID Sample::EnterRecordingMode()
{
    m_bIsPlayerReadyToPlay = FALSE;
    m_dwSkeletonHistoryCount = 0;
    m_bIsRecording = TRUE;
}


//-----------------------------------------------------------------------------
// Name: DetectIntentToPlay()
// Desc: Returns whether the player has signalled his intent to play by holding 
//       one of his hands above his shoulder for a few seconds. 
//-----------------------------------------------------------------------------
INT DetectIntentToPlay( const NUI_SKELETON_FRAME* pSkeletonFrame, DWORD dwSkeletonIndex )
{
    assert( pSkeletonFrame != NULL );
    assert( dwSkeletonIndex < NUI_SKELETON_COUNT );

    static DWORD HoldingSince = 0;

    if( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].eTrackingState != NUI_SKELETON_TRACKED )
    {
        HoldingSince = pSkeletonFrame->liTimeStamp.LowPart;
        return 0;
    }

    if( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_WRIST_LEFT ] != NUI_SKELETON_POSITION_NOT_TRACKED     &&
        pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_CENTER ] != NUI_SKELETON_POSITION_NOT_TRACKED &&
        XMVectorGetY( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_WRIST_LEFT ] ) > XMVectorGetY( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ] ) &&
        ( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_WRIST_RIGHT ] == NUI_SKELETON_POSITION_NOT_TRACKED  ||
          XMVectorGetY( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_WRIST_RIGHT ] ) < XMVectorGetY( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ] ) ) )
    {
        return -1;
    }

    if( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_WRIST_RIGHT ] != NUI_SKELETON_POSITION_NOT_TRACKED     &&
        pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_CENTER ] != NUI_SKELETON_POSITION_NOT_TRACKED &&
        XMVectorGetY( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_WRIST_RIGHT ] ) > XMVectorGetY( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ] ) &&
        ( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_WRIST_LEFT ] == NUI_SKELETON_POSITION_NOT_TRACKED  ||
          XMVectorGetY( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_WRIST_LEFT ] ) < XMVectorGetY( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ] ) ) )
    {
        return 1;
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT fFrameDelta = ( FLOAT )m_Timer.GetElapsedTime();

    ProcessGameController( fFrameDelta );

    if( m_bIsRecording || m_fOutcomeDisplayTimer > 0.0f )
    {
        RetrieveVideoStreams();
    }

    if( m_fOutcomeDisplayTimer > 0.0f )
    {
        m_fOutcomeDisplayTimer -= fFrameDelta;
        if( !m_bIsRecording )
        {
            return S_OK;
        }
    }

    if( !RetrieveNextSkeletonFrame() )
        return S_OK;

    if( g_RawSkeletonHistory[ m_dwSkeletonHistoryCurrentIndex ].SkeletonData[ m_dwTrackedSkeletonIndex ].eTrackingState != NUI_SKELETON_TRACKED )
    {
        // Since we don't have a lock on the skeleton currently being used by the sample, try switching 
        // to the first tracked skeleton we can find. If none is tracked, then leave the current 
        // index as is and try again next frame.
        for( UINT i = 0; i < NUI_SKELETON_COUNT; ++ i )
        {
            if( g_RawSkeletonHistory[ m_dwSkeletonHistoryCurrentIndex ].SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
            {
                m_dwTrackedSkeletonIndex = i;
                EnterRecordingMode();             
                break;
            }
        }

        return S_OK;
    }

    XMemCpy( &m_FilteredSkeletonFrame, &g_RawSkeletonHistory[ m_dwSkeletonHistoryCurrentIndex ], sizeof( NUI_SKELETON_FRAME ) );
    if( m_bIsRecording && !m_bIsPlayerReadyToPlay )
    {
        INT IntentToPlay = DetectIntentToPlay( &m_FilteredSkeletonFrame, m_dwTrackedSkeletonIndex );
        if( IntentToPlay == -1 )
        {
            m_bIsLeftHanded = TRUE;
            m_bIsPlayerReadyToPlay = TRUE;
            m_BasicBowlingFilter.Initialize( m_bIsLeftHanded );
            m_eBowlingState = BOWLING_STATE_NOT_READY;
        }
        else if( IntentToPlay == 1 )
        {
            m_bIsLeftHanded = FALSE;
            m_bIsPlayerReadyToPlay = TRUE;
            m_BasicBowlingFilter.Initialize( m_bIsLeftHanded );
            m_eBowlingState = BOWLING_STATE_NOT_READY;
        }
    }
    else
    {
        FLOAT fDeltaTime = GetElapsedTime( m_dwSkeletonHistoryCurrentIndex );
        m_bIsUsingInferredArm = m_BasicBowlingFilter.FilterSkeleton( &g_RawSkeletonHistory[ m_dwSkeletonHistoryCurrentIndex ].SkeletonData[ m_dwTrackedSkeletonIndex ], &m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ], fDeltaTime );
        FLOAT fCurrentAngle = ComputeWristPosition( &m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ], m_bIsLeftHanded );
        switch( m_eBowlingState )
        {
            case BOWLING_STATE_NOT_READY:
            {
                // The player is at the ready position when both his or her wrists are tracked and lower the hips.
                if( m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED  &&
                    m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_WRIST_RIGHT ] != NUI_SKELETON_POSITION_NOT_TRACKED  &&
                    m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_RIGHT ] != NUI_SKELETON_POSITION_NOT_TRACKED    &&
                    m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_WRIST_LEFT ] != NUI_SKELETON_POSITION_NOT_TRACKED   &&
                    m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ].eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_LEFT ] != NUI_SKELETON_POSITION_NOT_TRACKED     &&
                    m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_WRIST_RIGHT ].y < m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_HIP_RIGHT ].y &&
                    m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_WRIST_LEFT ].y < m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_HIP_LEFT ].y      )
                {
                    if( m_bIsRecording )
                    {
                        m_dwSkeletonHistoryCount = 0;
                    }
                    m_eBowlingState = BOWLING_STATE_READY;
                }
                break;
            }

            case BOWLING_STATE_READY:
            {
                if( fCurrentAngle < m_fLastArmAngle && fCurrentAngle < 2.0f * XM_PI / 8.0f )
                {
                    m_BasicBowlingFilter.Go();
                    m_eBowlingState = BOWLING_STATE_INITIATING;
                }
                break;
            }

            case BOWLING_STATE_INITIATING:
            {
                if( fCurrentAngle < XM_PI / 2.0f && fCurrentAngle > m_fLastArmAngle )
                {
                    m_eBowlingState = BOWLING_STATE_FRONT_SWING;
                }
                break;
            }

            case BOWLING_STATE_FRONT_SWING:
            {
                if( fCurrentAngle <= m_fLastArmAngle )
                {
                    m_eBowlingState = BOWLING_STATE_INITIATING;
                }
                else if( fCurrentAngle > XM_PI / 2.0f )
                {
                    m_eBowlingState = BOWLING_STATE_BACK_SWING;
                }
                break;
            }

            case BOWLING_STATE_BACK_SWING:
            {
                if( fCurrentAngle < XM_PI / 2.0f )
                {
                    m_BasicBowlingFilter.Stop();
                    m_eBowlingState = BOWLING_STATE_NOT_READY;
                }
                else if( fCurrentAngle <= m_fLastArmAngle )
                {
                    m_eBowlingState = BOWLING_STATE_FORWARD_SWING;
                }
                break;
            }

            case BOWLING_STATE_FORWARD_SWING:
            {
                if( fCurrentAngle >= m_fLastArmAngle )
                {
                    m_BasicBowlingFilter.Stop();
                    m_eBowlingState = BOWLING_STATE_NOT_READY;
                }
                else if( fCurrentAngle < XM_PI / 2.0f )
                {
                    m_eBowlingState = BOWLING_STATE_SWING_COMPLETED;              
                }
                break;
            }

            case BOWLING_STATE_SWING_COMPLETED:
            {
                m_eBowlingState = BOWLING_STATE_FOLLOW_THROUGH;
                if( m_bIsRecording )
                {
                    m_fOutcomeDisplayTimer = 3.0f;
                }
                break;
            }

            case BOWLING_STATE_FOLLOW_THROUGH:
            {
                if( fCurrentAngle >= m_fLastArmAngle || fCurrentAngle > XM_PI / 2.0f )
                {
                    m_BasicBowlingFilter.Stop();
                    m_eBowlingState = BOWLING_STATE_NOT_READY;
                    if( m_bIsRecording )
                    {
                        m_bIsRecording = FALSE;
                    }
                }
                break;
            }

            default:
            {
                assert( false );
                break;
            }
        }

        m_fLastArmAngle = fCurrentAngle;
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: Sample::ProcessGameController
// Desc: Process the input from the game controller
//-----------------------------------------------------------------------------
VOID Sample::ProcessGameController( FLOAT fElapsedTime )
{
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( !m_bIsRecording )
    {
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            if( m_bIsStepModeOn )
            {
                m_bIsReadyForNextFrame = TRUE;
            }
            else
            {
                m_bIsStepModeOn         = TRUE;
                m_bIsReadyForNextFrame  = FALSE;
            }
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            m_bIsStepModeOn = FALSE;
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            EnterRecordingMode();
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        {
            m_fPlaybackSpeed /= 2;
            if( m_fPlaybackSpeed < 0.25 )
                m_fPlaybackSpeed = 1.0f;
        }

        // Set the view matrix form the left and right sticks
        static FLOAT fTheta = -0.05f * XM_PI;
        static FLOAT fPhi   = +0.0f * XM_PI;

        fPhi   += pGamepad->fX2 * fElapsedTime * 0.3f * XM_PI;
        fTheta += pGamepad->fY2 * fElapsedTime * 0.3f * XM_PI;

        XMVECTOR vLookatDir;
        vLookatDir.x = cosf( fTheta ) * sinf( fPhi );
        vLookatDir.y = sinf( fTheta );
        vLookatDir.z = cosf( fTheta ) * cosf( fPhi );

        XMVECTOR vCrossDir;
        vCrossDir.x = cosf( fPhi );
        vCrossDir.y = 0.0;
        vCrossDir.z = -sinf( fPhi );

        m_vCameraPosition += vLookatDir * pGamepad->fY1 * fElapsedTime * 1.3f;
        m_vCameraPosition += vCrossDir * pGamepad->fX1 * fElapsedTime * 1.3f;
        XMMATRIX matView = XMMatrixLookAtLH( m_vCameraPosition, m_vCameraPosition + vLookatDir, m_vUp );

        // Set up projection matrix
        UINT uWidth, uHeight;
        ATG::GetVideoSettings( &uWidth, &uHeight );
        const FLOAT fZNear = 0.1f;
        const FLOAT fZFar = 10.0f;
        FLOAT fAspectRatio = (FLOAT)uWidth / (FLOAT)uHeight;
        XMMATRIX matProj = XMMatrixPerspectiveFovLH( XM_PI / 2.5f, fAspectRatio, fZNear, fZFar );

        // Set the world view projection matrix into the debug draw system.
        ATG::DebugDraw::SetViewProjection( matView * matProj );
    }
}


//-----------------------------------------------------------------------------
// Name: Sample::RetrieveVideoStreams
// Desc: Retrive the latest depth and color streams, if available.
//-----------------------------------------------------------------------------
VOID Sample::RetrieveVideoStreams()
{
    const NUI_IMAGE_FRAME* pImageFrame;    

    if( SUCCEEDED( NuiImageStreamGetNextFrame( m_hDepthStream, NUI_CAMERA_TIMEOUT_DEFAULT, &pImageFrame ) ) )
    {
        UpdateDepthTexture( pImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hDepthStream, pImageFrame );
    }

    if( SUCCEEDED( NuiImageStreamGetNextFrame( m_hColorStream, NUI_CAMERA_TIMEOUT_DEFAULT, &pImageFrame ) ) )
    {
        UpdateColorTexture( pImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hColorStream, pImageFrame );
    }
}


//-----------------------------------------------------------------------------
// Name: Sample::RetrieveNextSkeletonFrame
// Desc: Determine the next skeleton frame the sample should process, if any. 
//       Depending on the sample internal states the function may return a
//       skeleton from the skeleton pipeline, if available or from the internal 
//       record set.
//-----------------------------------------------------------------------------
BOOL Sample::RetrieveNextSkeletonFrame()
{
    if( m_bIsRecording )
    {
        if( FAILED( NuiSkeletonGetNextFrame( NUI_CAMERA_TIMEOUT_DEFAULT, &g_RawSkeletonHistory[ m_dwSkeletonHistoryCount % SKELETON_HISTORY_MAX ] ) ) )
        {
            return FALSE;
        }

        NuiTransformSmooth( &g_RawSkeletonHistory[ m_dwSkeletonHistoryCount % SKELETON_HISTORY_MAX ], NULL );

        m_dwSkeletonHistoryCurrentIndex = m_dwSkeletonHistoryCount % SKELETON_HISTORY_MAX;
	    ++ m_dwSkeletonHistoryCount;
    }
    else
    {
        static DOUBLE fLastFrameTime = 0;
        DOUBLE fTime = m_Timer.GetAppTime();

        if( m_bIsStepModeOn )
        {
             if( !m_bIsReadyForNextFrame )
                 return FALSE;
        }
        else
        {
            const FLOAT THIRTY_THREE_MS = 33.0f / 1000;
            if( fTime - fLastFrameTime < ( THIRTY_THREE_MS ) * ( 1 / m_fPlaybackSpeed ) )
            {
                return FALSE;
            }
        }

        m_dwSkeletonHistoryCurrentIndex = ( m_dwSkeletonHistoryCurrentIndex + 1 ) % SKELETON_HISTORY_MAX;
        if( m_dwSkeletonHistoryCurrentIndex == m_dwSkeletonHistoryCount % SKELETON_HISTORY_MAX )
        {
            if( m_dwSkeletonHistoryCurrentIndex == m_dwSkeletonHistoryCount )
            {
                m_dwSkeletonHistoryCurrentIndex = 0;
            }

            m_BasicBowlingFilter.ResetFilterState();
        }

        fLastFrameTime = fTime;
        m_bIsReadyForNextFrame = FALSE;
    }

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: Sample::UpdateDepthTexture
// Desc: Make a copy of a depth texture for future use.
//-----------------------------------------------------------------------------
VOID Sample::UpdateDepthTexture( IDirect3DTexture9* pSourceImage )
{
    D3DLOCKED_RECT Locked;
    m_pDepthTexture->LockRect( 0, &Locked, NULL, 0 );
        
    D3DLOCKED_RECT LockedSrc;
    pSourceImage->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );
        
    const DWORD* pBitsSrc = (const DWORD*)LockedSrc.pBits;
    DWORD* pBitsDst = (DWORD*)Locked.pBits;
    for( INT j = 0; j < m_DepthHeight; ++ j )
    {
        for( INT i = 0; i < m_DepthWidth / 2; ++ i )
        {
            *( pBitsDst + i ) = *( pBitsSrc + i );
        }

        pBitsDst += Locked.Pitch / sizeof( DWORD );
        pBitsSrc += LockedSrc.Pitch / sizeof( DWORD );
    }

    pSourceImage->UnlockRect( 0 );
    m_pDepthTexture->UnlockRect( 0 );
}


//-----------------------------------------------------------------------------
// Name: Sample::UpdateColorTexture
// Desc: Update the color texture displayed, filter out anything that isn't the 
//       current player.
//-----------------------------------------------------------------------------
VOID Sample::UpdateColorTexture( IDirect3DTexture9* pSourceImage )
{
    D3DLOCKED_RECT Locked;
    m_pColorTexture->LockRect( 0, &Locked, NULL, 0 );
        
    D3DLOCKED_RECT LockedSrc;
    pSourceImage->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );
        
    D3DLOCKED_RECT LockedDepth;
    m_pDepthTexture->LockRect( 0, &LockedDepth, NULL, D3DLOCK_READONLY );

    DWORD* pBitsDst = ( DWORD* )Locked.pBits;
    const DWORD* pBitsSrc = ( const DWORD* )LockedSrc.pBits;
    const USHORT* pBitsDepth = ( const USHORT* )LockedDepth.pBits;
    for( INT j = 0; j < m_ColorHeight; ++ j )
    {
        for( INT i = 0; i < m_ColorWidth; ++ i )
        {
            // Only show the pixels which are player data, all other pixels are marked black
            if( ( pBitsDepth[ i / 2 ] & 7 ) == (USHORT) m_dwTrackedSkeletonIndex + 1 )
            {
                *( pBitsDst + i ) = *( pBitsSrc + i );
            }
            else
            {
                *( pBitsDst + i ) = 0x00000000;
            }
        }
        
        if( j % 2 )
        {
            pBitsDepth += LockedDepth.Pitch / sizeof( USHORT );
        }
        pBitsSrc += LockedSrc.Pitch / sizeof( DWORD );
        pBitsDst += Locked.Pitch / sizeof( DWORD );
    }

    m_pDepthTexture->UnlockRect( 0 );
    pSourceImage->UnlockRect( 0 );
    m_pColorTexture->UnlockRect( 0 );
}


//--------------------------------------------------------------------------------------
// Name: Sample::GetElapsedTime()
// Desc: Returns the elapsed time between the frame specified and the preceeding one.
//--------------------------------------------------------------------------------------
FLOAT Sample::GetElapsedTime( DWORD dwFrameIndex ) const
{
    FLOAT fDeltaTime;

    if( m_dwSkeletonHistoryCount > 1 )
    {
        fDeltaTime = ( g_RawSkeletonHistory[ m_dwSkeletonHistoryCurrentIndex ].liTimeStamp.LowPart -  g_RawSkeletonHistory[ GetPreviousFrameIndex( m_dwSkeletonHistoryCurrentIndex ) ].liTimeStamp.LowPart ) / 1000.0f;
    }
    else
    {
        fDeltaTime = 0.033f;
    }

    return fDeltaTime;
}


//--------------------------------------------------------------------------------------
// Name: Sample::GetPreviousFrameIndex()
// Desc: For a circular buffer, computes the index that comes before the one specified.
//--------------------------------------------------------------------------------------
DWORD Sample::GetPreviousFrameIndex( DWORD dwFrameIndex ) const
{
    assert( m_dwSkeletonHistoryCount > 1 );

    if( dwFrameIndex == 0 )
    {
        if( m_dwSkeletonHistoryCount < SKELETON_HISTORY_MAX )
        {
            dwFrameIndex = m_dwSkeletonHistoryCount - 1;
        }
        else
        {
            dwFrameIndex = SKELETON_HISTORY_MAX - 1;
        }
    }
    else
    {
        -- dwFrameIndex;
    }

    return dwFrameIndex;
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        if( m_bIsRecording || m_fOutcomeDisplayTimer > 0.0f )
        {
            // Set default states
            m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
            m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
            m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
            m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 0x08 );
            m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
            m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
            m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
            m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
            m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

            // Render the color video stream
            m_pd3dDevice->SetVertexShader( m_pVideoVertexShader );
            m_pd3dDevice->SetPixelShader( m_pVideoPixelShaderRGB );

            m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
            m_pd3dDevice->SetVertexDeclaration( m_pVideoVertexDecl );            
            m_pd3dDevice->SetTexture( 0, m_pColorTexture );

            SubmitVertexData();
            m_pd3dDevice->SetTexture( 0, NULL );

            D3DRECT rcWindow;
            m_Font.GetWindow( rcWindow );

            m_Font.Begin();
            m_Font.SetScaleFactors( 1.5f, 1.5f );
            m_Font.DrawText( ( rcWindow.x2 - rcWindow.x1 ) / 2.0f, ( rcWindow.y2 - rcWindow.y1 ) / 2.0f, 
                             0xffffff00, GetPlayerMessage(), ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
            m_Font.End();
        }
        else
        {
            m_pd3dDevice->SetRenderState( D3DRS_LINEWIDTH, *( ( DWORD* ) &LINE_WIDTH ) );

            if( m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED &&
                g_RawSkeletonHistory[ m_dwSkeletonHistoryCurrentIndex ].SkeletonData[ m_dwTrackedSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
            {
                RenderSkeletonAndHighlightDifferenceInArm( &m_FilteredSkeletonFrame.SkeletonData[ m_dwTrackedSkeletonIndex ], &g_RawSkeletonHistory[ m_dwSkeletonHistoryCurrentIndex ].SkeletonData[ m_dwTrackedSkeletonIndex ], m_bIsLeftHanded );
            }

            m_Font.Begin();

            WCHAR wszMessage[ 256 ];
    
            wsprintfW( wszMessage, L"State: %s", GetBowlingStateText( m_eBowlingState ) );
            m_Font.DrawText( 0, 30, 0xffffffff, wszMessage );
            wsprintfW( wszMessage, L"Frame Number: %d", g_RawSkeletonHistory[ m_dwSkeletonHistoryCurrentIndex ].dwFrameNumber );
            m_Font.DrawText( 0, 50, 0xffffffff, wszMessage );
            wsprintfW( wszMessage, L"Elapsed Time: %0.3f", GetElapsedTime( m_dwSkeletonHistoryCurrentIndex ) );
            m_Font.DrawText( 0, 70, 0xffffffff, wszMessage );
            wsprintfW( wszMessage, L"Angle: %0.4f    (%0.1f)", m_fLastArmAngle, m_fLastArmAngle * 180 / XM_PI );
            m_Font.DrawText( 0, 90, 0xffffffff, wszMessage );
            wsprintfW( wszMessage, L"Playback Mode: %s", GetPlaybackModeText() );
            m_Font.DrawText( 0, 110, 0xffffffff, wszMessage );

            m_Font.DrawText( 0, 150, 0xffffffff, L"Internal Filter States" );
            FLOAT fY = m_BasicBowlingFilter.DebugOutput( &m_Font, 0, 170, 0xffffffff );

            if( m_bIsUsingInferredArm )
                m_Font.DrawText( 0, fY, 0xffffffff, L"Inferred Arm Returned" );

            m_Font.End();
        }

        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff,  L"Bowling" );
     
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderSkeletonAndHighlightDifferenceInArm()
// Desc: Render a skeleton in a way that highlight a specific arm to indicate if the 
//       bones making the arm are tracked, inferred or have been modified in respect to 
//       a reference skeleton.
//--------------------------------------------------------------------------------------
VOID Sample::RenderSkeletonAndHighlightDifferenceInArm( const NUI_SKELETON_DATA* pSkeletonData, const NUI_SKELETON_DATA* pReferenceSkeleton, BOOL bIsLeftHanded )
{
    assert( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED );

    for( UINT i = 0; i < ATG::g_uSkeletonBoneCount; ++ i )
    {
        if( pReferenceSkeleton->eSkeletonPositionTrackingState[ ATG::g_SkeletonBoneList[ i ].StartJoint ] == NUI_SKELETON_POSITION_NOT_TRACKED ||
            pReferenceSkeleton->eSkeletonPositionTrackingState[ ATG::g_SkeletonBoneList[ i ].EndJoint ]   == NUI_SKELETON_POSITION_NOT_TRACKED    )
        {
            continue;
        }

        D3DCOLOR BoneColor;
        if( pReferenceSkeleton->eSkeletonPositionTrackingState[ ATG::g_SkeletonBoneList[ i ].StartJoint ] == NUI_SKELETON_POSITION_INFERRED ||
            pReferenceSkeleton->eSkeletonPositionTrackingState[ ATG::g_SkeletonBoneList[ i ].EndJoint ]   == NUI_SKELETON_POSITION_INFERRED    )
        {
            BoneColor = D3DCOLOR_INFERED_BONE;
        }
        else
        {
            BoneColor = D3DCOLOR_TRACKED_BONE;
        }

        DrawLineSegment( pReferenceSkeleton->SkeletonPositions[ ATG::g_SkeletonBoneList[ i ].StartJoint ], pReferenceSkeleton->SkeletonPositions[ ATG::g_SkeletonBoneList[ i ].EndJoint ], BoneColor ); 

        if( m_bIsUsingInferredArm )
        {
            if( ATG::g_SkeletonBoneList[ i ].StartJoint == GetElbowSkeletonPositionIndex( bIsLeftHanded ) ||
                ATG::g_SkeletonBoneList[ i ].EndJoint   == GetElbowSkeletonPositionIndex( bIsLeftHanded ) ||
                ATG::g_SkeletonBoneList[ i ].StartJoint == GetWristSkeletonPositionIndex( bIsLeftHanded ) ||
                ATG::g_SkeletonBoneList[ i ].EndJoint   == GetWristSkeletonPositionIndex( bIsLeftHanded ) ||
                ATG::g_SkeletonBoneList[ i ].StartJoint == GetHandSkeletonPositionIndex( bIsLeftHanded )  ||
                ATG::g_SkeletonBoneList[ i ].EndJoint   == GetHandSkeletonPositionIndex( bIsLeftHanded )     )
            {
#if 0
                Should be if not not tracked...
                if( pSkeletonData->eSkeletonPositionTrackingState[ ATG::g_SkeletonBoneList[ i ].StartJoint ] == NUI_SKELETON_POSITION_TRACKED &&
                    pSkeletonData->eSkeletonPositionTrackingState[ ATG::g_SkeletonBoneList[ i ].EndJoint ]   == NUI_SKELETON_POSITION_TRACKED    )
#endif
                    // Silencing a CodeAnalysis mode false positive. 
                    assert( ATG::g_SkeletonBoneList[ i ].StartJoint < NUI_SKELETON_POSITION_COUNT && 
                            ATG::g_SkeletonBoneList[ i ].EndJoint < NUI_SKELETON_POSITION_COUNT      );
                    DrawLineSegment( pSkeletonData->SkeletonPositions[ ATG::g_SkeletonBoneList[ i ].StartJoint ], pSkeletonData->SkeletonPositions[ ATG::g_SkeletonBoneList[ i ].EndJoint ], D3DCOLOR_HIGHLIGHTED_BONE ); 
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: SubmitVertexData()
// Desc: Create the vertices used to render the video on screen, and feed them inline
//       to the D3D command buffer.
//--------------------------------------------------------------------------------------
VOID Sample::SubmitVertexData()
{
    UINT uWidth;
    UINT uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );

    XVIDEO_MODE VideoMode;
    ZeroMemory( &VideoMode, sizeof( VideoMode ) );
    XGetVideoMode( &VideoMode );

    m_ColorWindow.fHeight = ( FLOAT )uHeight;
    m_ColorWindow.fWidth = m_ColorWindow.fHeight * 4.f / 3.f;

    m_ColorWindow.fX = ( uWidth - m_ColorWindow.fWidth ) / 2.0f;
    m_ColorWindow.fY = 0.0f;

    // Now we fill the vertex buffer. 
    VideoFeedVertex g_SnapshotVertices[] =
    {
        { m_ColorWindow.fX,                                                         
          m_ColorWindow.fY, 0,  0, 0 },
        { m_ColorWindow.fX + m_ColorWindow.fWidth,               
          m_ColorWindow.fY, 0,  1, 0 },
        { m_ColorWindow.fX,                                                         
          m_ColorWindow.fY + m_ColorWindow.fHeight, 0,  0, 1 },
    };

    VideoFeedVertex* pVertices;

    m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( *g_SnapshotVertices ), &(VOID*&)pVertices );
    memcpy( pVertices, g_SnapshotVertices, sizeof( g_SnapshotVertices ) );
    m_pd3dDevice->EndVertices();
}


//--------------------------------------------------------------------------------------
// Name: Sample::GetPlaybackModeText
// Desc: Returns a text string describing the playback mode / speed currently active.
//--------------------------------------------------------------------------------------
const WCHAR* Sample::GetPlaybackModeText() const
{
    if( m_bIsStepModeOn )
    {
        return L"Frame by frame";
    }
    else if( m_fPlaybackSpeed == 1.0f ) 
    {
        return L"Full Speed";
    }
    else if( m_fPlaybackSpeed == 0.5f )
    {
        return L"1/2 Speed";
    }
    else if( m_fPlaybackSpeed == 0.25f )
    {
        return L"1/4 Speed";
    }

    assert( false );
    return L"Full Speed";
}

//--------------------------------------------------------------------------------------
// Name: Sample::GetPlayerMessage()
// Desc: Returns a message to help guide the player while performing the gesture
//--------------------------------------------------------------------------------------
const WCHAR* Sample::GetPlayerMessage() const
{
    static const WCHAR* pwszThrowResult = L"";

    if( m_bIsPlayerReadyToPlay )
    {
        if( m_eBowlingState == BOWLING_STATE_NOT_READY )
        {
            if( m_fOutcomeDisplayTimer > 0.0f )
            {
                return pwszThrowResult;
            }
            else
            {
                return L"Get Ready...";
            }
        }
        else if( m_eBowlingState == BOWLING_STATE_READY )
        {
            return L"And... Bowl...";
        }
        else if( m_eBowlingState == BOWLING_STATE_SWING_COMPLETED )
        {
            pwszThrowResult = GetThrowResult();
            return pwszThrowResult;
        }
        else if( m_eBowlingState == BOWLING_STATE_FOLLOW_THROUGH )
        {
            return pwszThrowResult;
        }
        else
        {
            return L"";
        }
    }
    else
    {
        return L"Raise your left or right hand to begin.";
    }
}
