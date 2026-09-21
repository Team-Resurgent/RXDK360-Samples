//--------------------------------------------------------------------------------------
// SimpleGestureDetection.cpp
//
// This sample demonstrates how the NuiGesture APIs can be used together with the output
// of the VisualGestureBuilder tool to simplify gesture detection. The VisualGestureBuilder
// generated two files, KickBoxing.h and KickBoxing.gbd. NuiGesture APIs are used to
// load the gesture database and retrieve the results of gesture detection. The sample
// uses two technologies, AdaBoostTrigger to determine when a gesture is detected, and
// PCAProgress to determine the progress of how far along in that gesture you are. The
// source content files to generate KickBoxing.gbd can be found on the portal.
//
// This sample can be compared to the KickBoxing sample. The only difference is that
// this sample uses the NuiGesture APIs.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSceneAll.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <AtgAudio.h>
#include <time.h>
#include <xaudio2.h>
#include <XStudio.h>
#include <deque>
#include "SkeletonTracking.h"
#include "Gestures\KickBoxing.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------

ATG::HELP_CALLOUT g_HelpCallouts[] = 
{
    { ATG::HELP_BACK_BUTTON,	ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,		ATG::HELP_PLACEMENT_1, L"Toggle data collection mode" },
    { ATG::HELP_B_BUTTON,		ATG::HELP_PLACEMENT_1, L"Toggle smoothing" },    
};
static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );


//--------------------------------------------------------------------------------------
// Defines and constatns used in this sample
//--------------------------------------------------------------------------------------

#define TOP_BACK_COLOR                  0xff00007f          // Background gradient colors
#define BOTTOM_BACK_COLOR               0xff000000
#define PUNCH_SOUND                     "game:\\Media\\Sounds\\PunchBag1.wav"
#define LOAD_PUNCH_SOUND                "game:\\Media\\Sounds\\PunchBag2.wav"
#define LOAD_PUNCH_VELOCITY_THRESHOLD   3.0f
#define CLOSEST_SKELETON                0                   // Index to use for closest skeleton

// Visualize 90 frames of data
static const UINT   g_nNumValuesToVisualize = 33;

// Specify how long the xed file recordings are in seconds.
static const UINT   g_nNumSecondsToRecord   = 30;


//--------------------------------------------------------------------------------------
// A struct to hold some data for each gesture we want to detect and joint data 
// needed to derive analog signals after the detection
//--------------------------------------------------------------------------------------

enum EGestureSide { LEFT, RIGHT };
struct GestureData
{
    CHAR                            m_szName[ 256 ];        // name of gesture, for display
    KickBoxing::AdaBoostTrigger*    m_pTrigger;             // Pointer to trigger gesture
    KickBoxing::PCAProgress*        m_pProgress;            // Pointer to progress gesture that gets enabled when the trigger fires
    EGestureSide                    m_Side;                 // left/right gesture
    NUI_SKELETON_POSITION_INDEX     m_JointIndex;           // joint data used to derive analog signal    
    UINT                            m_nNumDetected;         // Counting how many times a gesture have been detected   

    std::deque<KickBoxing::AdaBoostTrigger> m_TriggerData;  // Keep a ring buffer with results for debug display
    std::deque<KickBoxing::PCAProgress>     m_ProgressData; // Keep a ring buffer with results for debug display
};

// Debug data for trigger gestures
GestureData g_GestureData[] = { { "PunchLeft",  NULL, NULL, LEFT,  NUI_SKELETON_POSITION_WRIST_LEFT,  0 },
                                { "PunchRight", NULL, NULL, RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT, 0 },
                                { "KickLeft",   NULL, NULL, LEFT,  NUI_SKELETON_POSITION_ANKLE_LEFT,  0 },
                                { "KickRight",  NULL, NULL, RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT, 0 } };

const UINT NUM_GESTURES = ARRAYSIZE( g_GestureData );


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------

class Sample : public ATG::Application
{
private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    VOID SetActiveSkeleton();
    VOID UpdateAnimationFromGestureDetection( NUI_SKELETON_DATA* pCurrentSkeletonData, LARGE_INTEGER liCurTimeStamp,
                                              NUI_SKELETON_DATA* pPreviousSkeletonData, LARGE_INTEGER liPrevTimeStamp );
    VOID RenderScene( ATG::Scene* pScene, const BOOL bApplyModelMtx );
    VOID RenderConfidenceValues( const UINT uGestureIdx, const UINT x, const UINT y );
    VOID RecordData( NUI_SKELETON_DATA* pSkeletonData );
    VOID PlaySound( IXAudio2* pXaudio2, const char* szFilename );

    // Storing results of NuiGesture
    NUI_GESTURE_FRAME   m_GestureFrame;
    KickBoxing::FRAME   m_KickBoxingGestureFrame;
    
    // In data collection mode we record data to XED files
    BOOL                m_bRecordData;
    XSTUDIO_FILE_HANDLE m_hFile;
    INT                 m_nSecondsRecording;

    // General sample data
    ATG::Timer          m_Timer;
    ATG::Font           m_Font;
    ATG::Help           m_Help;
    BOOL                m_bDrawHelp;

    // Matrices to render the scene
    XMMATRIX            m_matWorld;
    XMMATRIX            m_matView;
    XMMATRIX            m_matProj;
    XMMATRIX            m_matWVP;

    // Rotation axis to animate punching bag around
    XMVECTOR            m_vAxis;

    // Shaders
    IDirect3DPixelShader9*          m_pPixelShader;
    IDirect3DVertexShader9*         m_pVertexShader;
    IDirect3DVertexDeclaration9*    m_pVertexDecl;

    // Scene objects
    ATG::Scene*             m_pPunchingBagScene;
    ATG::Scene*             m_pBoxingRingScene;

    // Audio objects for playing sounds
    IXAudio2*               m_pXAudio2;
    IXAudio2SourceVoice*    m_pSourceVoice;
    BYTE*                   m_pbWaveData;


    //--------------------------------------------------------------------------------------
    // A class that implements the dynamics of a simple pendulum
    //--------------------------------------------------------------------------------------

    class Pendulum
    {
    public:
        Pendulum()
        {
            m_fDisplacement = 0.0f;
            m_fVelocity = 0.0f;
            m_fAcceleration = 0.0f;
            m_fTheta = 0.0f;
            m_fMass = 9.8f;
            m_fLength = 1.0f;
            m_fRestitution = 0.995f;
        }

        VOID Update( const FLOAT fDeltaTimeInSeconds )
        {
            m_fDisplacement = m_fDisplacement - m_fVelocity * fDeltaTimeInSeconds;
            m_fAcceleration = m_fMass * m_fDisplacement / m_fLength;
            m_fVelocity = m_fRestitution * m_fVelocity + m_fAcceleration * fDeltaTimeInSeconds;
            m_fTheta = m_fDisplacement / m_fLength;
        }

        VOID SetDisplacement( const FLOAT fDisplacement ) { m_fDisplacement = fDisplacement; }

        FLOAT   m_fDisplacement;
        FLOAT   m_fVelocity;
        FLOAT   m_fAcceleration;
        FLOAT   m_fTheta;
        FLOAT   m_fMass;
        FLOAT   m_fLength;
        FLOAT   m_fRestitution;
    };
   
    // Use a simple pendulum to animate the punching bag
    Pendulum    m_pendulum;
};


//--------------------------------------------------------------------------------------
// Vertex shader
//--------------------------------------------------------------------------------------

const CHAR* m_strVertexShaderProgramScene =
    "float4x4 matWVP : register(c0);\n"
    "float4x4 g_matWorld : register(c4);\n"
    "\n"
    "struct VS_IN\n"
    "\n"
    "{\n"
    "    float3 ObjPos   : POSITION;\n"
    "    float3 Normal   : NORMAL;\n"
    "    float3 Binormal : BINORMAL;\n"
    "    float3 Tangent  : TANGENT;\n"
    "    float2 TexCoord : TEXCOORD0;\n"
    "};\n"
    "\n"
    "struct VS_OUT\n"
    "{\n"
    "    float4 ProjPos  : POSITION;\n"
    "    float3 WorldSpacePosition  : TEXCOORD0;\n"
    "    float3 Normal   : TEXCOORD1;\n"
    "    float3 Binormal : TEXCOORD2;\n"
    "    float3 Tangent  : TEXCOORD3;\n"
    "    float2 TexCoord : TEXCOORD4;\n"
    "};\n"
    "\n"
    "VS_OUT main( VS_IN In )\n"
    "{\n"
    "    VS_OUT Out;\n"
    "    Out.ProjPos = mul( matWVP, float4( In.ObjPos, 1 ) );\n"
    "    Out.WorldSpacePosition  = mul( g_matWorld, float4( In.ObjPos, 1 ) );\n"
    "    float3 vWorldSpaceNormal = normalize( mul( g_matWorld, float4( In.Normal, 0 ) ) );\n"
    "    Out.Normal = vWorldSpaceNormal;\n"
    "    Out.Binormal = In.Binormal;\n"
    "    Out.Tangent = In.Tangent;\n"
    "    Out.TexCoord = In.TexCoord;\n"
    "    return Out;\n"
    "}\n";


//-------------------------------------------------------------------------------------
// Pixel shader
//-------------------------------------------------------------------------------------

const CHAR* m_strPixelShaderProgramScene =
    "struct PS_IN\n"
    "{\n"
    "    float3 WorldSpacePosition  : TEXCOORD0;\n"
    "    float3 Normal   : TEXCOORD1;\n"
    "    float3 Binormal : TEXCOORD2;\n"
    "    float3 Tangent  : TEXCOORD3;\n"
    "    float2 TexCoord : TEXCOORD4;\n"
    "};\n"
    "\n"
    "sampler DiffuseTexture : register(s0);\n"
    "sampler NormalmapTexture : register(s1);\n"
    "\n"
    "float4 main( PS_IN In ) : COLOR\n"
    "{\n"
    "    float4 diffuseColor = tex2D( DiffuseTexture, In.TexCoord );\n"
    "    float2 vNormalMapXY = tex2D( NormalmapTexture, In.TexCoord ).xy;\n"
    "    vNormalMapXY        = vNormalMapXY * 2.0f - 1.0f;\n"
    "    float3 vSampledNormal = float3( vNormalMapXY.x, vNormalMapXY.y, saturate( 1 - dot( vNormalMapXY, vNormalMapXY ) ) );\n"
    "    float3 vWorldSpaceNormal = (In.Tangent * vSampledNormal.x) + (In.Binormal * vSampledNormal.y) + (In.Normal * vSampledNormal.z);\n"
    "    vWorldSpaceNormal = normalize( vWorldSpaceNormal );\n"
    "    float fDiffuseLightingI = max( 0.2, saturate( dot( vWorldSpaceNormal, normalize( float3( 1.0f, 1.0f, -1.0f ) ) ) ) );\n"
    "    float4 vColor = saturate( fDiffuseLightingI * diffuseColor );\n"
    "    return vColor;\n"
    "}\n";


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
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------

HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
	
    // Create the font
    RETURN_ON_FAIL( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the help
    RETURN_ON_FAIL( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) );

    // Initialize the camera and skeleton tracking
    RETURN_ON_FAIL( InitializeSkeletonTracking( m_pd3dDevice ) );

    // Load the gesture data file and enable all gestures
    RETURN_ON_FAIL( NuiGestureInitialize( "game:\\Media\\Gestures\\KickBoxing.gbd", KickBoxing::DIGEST, 0, NULL ) );
    RETURN_ON_FAIL( NuiGestureEnableGesture( KickBoxing::GESTURE_PUNCH_LEFT, TRUE ) );
    RETURN_ON_FAIL( NuiGestureEnableGesture( KickBoxing::GESTURE_PUNCH_RIGHT, TRUE ) );
    RETURN_ON_FAIL( NuiGestureEnableGesture( KickBoxing::GESTURE_KICK_LEFT, TRUE ) );
    RETURN_ON_FAIL( NuiGestureEnableGesture( KickBoxing::GESTURE_KICK_RIGHT, TRUE ) );
    RETURN_ON_FAIL( NuiGestureEnableGesture( KickBoxing::GESTURE_PUNCHPROGRESS, TRUE ) );
    RETURN_ON_FAIL( NuiGestureEnableGesture( KickBoxing::GESTURE_KICKPROGRESS, TRUE ) );

    // Initialize the NuiGesture frame that will receive data from NuiGesture APIs
    INITIALIZE_NUI_GESTURE_FRAME( &m_KickBoxingGestureFrame, &m_GestureFrame );

    // Setup data pointers to results in debug info
    g_GestureData[ 0 ].m_pTrigger  = &m_KickBoxingGestureFrame.Players[ CLOSEST_SKELETON ].Punch_Left;
    g_GestureData[ 1 ].m_pTrigger  = &m_KickBoxingGestureFrame.Players[ CLOSEST_SKELETON ].Punch_Right;
    g_GestureData[ 2 ].m_pTrigger  = &m_KickBoxingGestureFrame.Players[ CLOSEST_SKELETON ].Kick_Left;
    g_GestureData[ 3 ].m_pTrigger  = &m_KickBoxingGestureFrame.Players[ CLOSEST_SKELETON ].Kick_Right;
    
    // When a trigger fires, we want to enable one of the progress gestures
    g_GestureData[ 0 ].m_pProgress = &m_KickBoxingGestureFrame.Players[ CLOSEST_SKELETON ].PunchProgress;
    g_GestureData[ 1 ].m_pProgress = &m_KickBoxingGestureFrame.Players[ CLOSEST_SKELETON ].PunchProgress;
    g_GestureData[ 2 ].m_pProgress = &m_KickBoxingGestureFrame.Players[ CLOSEST_SKELETON ].KickProgress;
    g_GestureData[ 3 ].m_pProgress = &m_KickBoxingGestureFrame.Players[ CLOSEST_SKELETON ].KickProgress;

    // Initialize debug visualizing data
    for ( UINT i = 0; i < NUM_GESTURES; i++ )
    {
        g_GestureData[ i ].m_nNumDetected = 0;

        g_GestureData[ i ].m_TriggerData.resize( g_nNumValuesToVisualize );
        g_GestureData[ i ].m_ProgressData.resize( g_nNumValuesToVisualize );
        for ( UINT j = 0; j < g_nNumValuesToVisualize; j++ )
        {
            ZeroMemory( &g_GestureData[ i ].m_TriggerData[ j ], sizeof(KickBoxing::AdaBoostTrigger) );
            ZeroMemory( &g_GestureData[ i ].m_ProgressData[ j ], sizeof(KickBoxing::PCAProgress) );
        }
    }
    m_bRecordData = FALSE;
    m_hFile = 0;
    m_nSecondsRecording = g_nNumSecondsToRecord;

    // Buffers to hold compiled shaders and possible error messages
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Compile vertex shader.
    RETURN_ON_FAIL( D3DXCompileShader( m_strVertexShaderProgramScene, ( UINT )strlen( m_strVertexShaderProgramScene ),
                                         NULL, NULL, "main", "vs_3_0", D3DXSHADER_DEBUG, &pShaderCode, &pErrorMsg, NULL ) );

    // Create vertex shader.
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(), &m_pVertexShader );

    // Shader code is no longer required.
    pShaderCode->Release();
    pShaderCode = NULL;

    // Compile pixel shader.
    RETURN_ON_FAIL( D3DXCompileShader( m_strPixelShaderProgramScene, ( UINT )strlen( m_strPixelShaderProgramScene ),
                                        NULL, NULL, "main", "ps_3_0", D3DXSHADER_DEBUG, &pShaderCode, &pErrorMsg, NULL ) );

    // Create pixel shader.
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(), &m_pPixelShader );

    // Shader code no longer required.
    pShaderCode->Release();
    pShaderCode = NULL;

    // Define the vertex elements.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    // Create a vertex declaration from the element descriptions.
    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDecl );

    // Create and load scenes
    RETURN_ON_NULL( m_pPunchingBagScene = new ATG::Scene() );
    RETURN_ON_NULL( m_pBoxingRingScene = new ATG::Scene() );
    RETURN_ON_FAIL( ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\PunchingBag.xatg", m_pPunchingBagScene, NULL,
                                                        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL ) );
    RETURN_ON_FAIL( ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\BoxingRing.xatg", m_pBoxingRingScene, NULL,
                                                        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL ) );

    // Aspect ratio for HiDef displays is always 16x9 but our buffers may not be.
    FLOAT fAspectRatio = (FLOAT)m_d3dpp.BackBufferWidth / (FLOAT)m_d3dpp.BackBufferHeight;

    // Initialize matrices and vectors
    const FLOAT fFront  = 100.0f;
    const FLOAT fBack   = 5000.0f;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, fAspectRatio, fFront, fBack );
    m_matWorld = XMMatrixIdentity();
    m_vAxis = XMVectorSet( 1.0f, 0.0f, 0.0f, 0.0f );

    // Initialize XAudio2 and create mastering voice
    m_pXAudio2      = NULL;
    m_pSourceVoice  = NULL;
    m_pbWaveData    = NULL;
    UINT32 flags    = 0;
#ifdef _DEBUG
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    RETURN_ON_FAIL( XAudio2Create( &m_pXAudio2, flags ) );
    IXAudio2MasteringVoice* pMasteringVoice = NULL;
    RETURN_ON_FAIL( m_pXAudio2->CreateMasteringVoice( &pMasteringVoice ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------

HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, __FUNCTION__  );

    // Get the skeleton data and time stamp of the previous frame
    NUI_SKELETON_DATA previousSkeletonData;
    XMemCpy( &previousSkeletonData, GetClosestSkeletonData(), sizeof( NUI_SKELETON_DATA ) );
    LARGE_INTEGER liPrevTimeStamp = GetCurrentTimeStamp();

    // Update skeleton tracking with the current frame
    UpdateSkeletonTracking( m_pd3dDevice );
    SetActiveSkeleton();

    // Skeleton tracking is waiting for the EndFrameEvent. Gesture detection is included
    // in the EndFrameEvent, so we will get the lowest latency between ST and gestures 
    if ( SUCCEEDED( NuiGestureGetNextFrame( 0, &m_GestureFrame ) ) )
    {
        // Update the debug visualization ring buffer with confidence valaues
        for ( UINT i = 0; i < NUM_GESTURES; i++ )
        {
            BOOL bDetected      = FALSE;
            FLOAT fConfidence   = 0.0f;
            if ( g_GestureData[ i ].m_pTrigger->Valid )
            {
                bDetected   = g_GestureData[ i ].m_pTrigger->Value;
                fConfidence = g_GestureData[ i ].m_pTrigger->Confidence;
            }

            // Keep count of the number of times we get an instance of a gesture
            if ( g_GestureData[ i ].m_pTrigger->FirstFrameDetected )
            {
                g_GestureData[ i ].m_nNumDetected++;
            }

            // Copy the current trigger gesture results into the ringbuffer for visualization
            KickBoxing::AdaBoostTrigger triggerData;
            memcpy( &triggerData, g_GestureData[ i ].m_pTrigger, sizeof(KickBoxing::AdaBoostTrigger));
            g_GestureData[ i ].m_TriggerData.pop_front();
            g_GestureData[ i ].m_TriggerData.push_back( triggerData );

            // Copy the progress gesture results into the ringbuffer for visualization
            KickBoxing::PCAProgress progressData;
            if ( bDetected )
            {
                memcpy( &progressData, g_GestureData[ i ].m_pProgress, sizeof(KickBoxing::PCAProgress));
            }
            else
            {
                // If we didn't detect the gesture, simply zero out the progress results. Alternatively we
                // could have the progress gestures disabled and only enable them when we find a detection
                ZeroMemory( &progressData, sizeof(KickBoxing::PCAProgress) );
            }

            g_GestureData[ i ].m_ProgressData.pop_front();
            g_GestureData[ i ].m_ProgressData.push_back( progressData );
         
            // Add custom PIX events
            PIXAddNamedCounter( bDetected * 100.0f, "%s Detected", g_GestureData[ i ].m_szName );
            PIXAddNamedCounter( fConfidence * 100.0f, "%s Confidence", g_GestureData[ i ].m_szName );
            PIXAddNamedCounter( progressData.Value * 100.0f, "%s Progress", g_GestureData[ i ].m_szName );
        }
    }

    // Get the skeleton data and time stamp of the current frame
    NUI_SKELETON_DATA* pCurSkeletonData = GetClosestSkeletonData();
    LARGE_INTEGER liCurTimeStamp = GetCurrentTimeStamp();

    // Update the animation for the punching bag based on resutls from the gesture detection
    UpdateAnimationFromGestureDetection( pCurSkeletonData, liCurTimeStamp, &previousSkeletonData, liPrevTimeStamp );
    
    // Get input from all the gamepads
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Exit help screen if a player presses BACK
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Toggle between game mode and data collection mode
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bRecordData = !m_bRecordData;
    }

    // Toggle smoothing
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        SetSmoothingState( !GetSmoothingState() ); 
    }

    // Record data if we're in data collection mode
    RecordData( pCurSkeletonData );

    PIXEndNamedEvent();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetActiveSkeleton()
// Desc: Sets the active skeleton manually using the closest skeleton to the sensor
//--------------------------------------------------------------------------------------

VOID Sample::SetActiveSkeleton()
{
    // Intialize to no skeletons active
    DWORD dwTrackingIDs[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { 0, 0 };

    // Find the closest skeleton to the sensor
    DWORD dwTrackingID;
    if ( FindClosestSkeletonToSensor( &dwTrackingID ) )
    {
        dwTrackingIDs[ CLOSEST_SKELETON ] = dwTrackingID;
    }

    // Set the active skeletons for skeleton tracking and gesture detection
    NuiSkeletonSetTrackedSkeletons( dwTrackingIDs );
    NuiGestureSetActiveSkeletons( dwTrackingIDs );

}


//--------------------------------------------------------------------------------------
// Name: UpdateAnimationFromGestureDetection()
// Desc: Update the animation for the punching bag based on resutls from the gesture detection
//--------------------------------------------------------------------------------------

VOID Sample::UpdateAnimationFromGestureDetection( NUI_SKELETON_DATA* pCurrentSkeletonData, LARGE_INTEGER liCurTimeStamp,
                                                  NUI_SKELETON_DATA* pPreviousSkeletonData, LARGE_INTEGER liPrevTimeStamp )
{
    // We want to derive analog signals when detecting a gesture so that we can show
    // different results when the user is punching lightly vs. hard
    BOOL bDetected = FALSE;
    XMVECTOR vPrevJoint[ NUM_GESTURES ];
    XMVECTOR vCurJoint[ NUM_GESTURES ];
    FLOAT fJointVelocity[ NUM_GESTURES ];    
  
    UINT64 uCurTimeStamp    = liCurTimeStamp.QuadPart;
    UINT64 uPrevTimeStamp   = liPrevTimeStamp.QuadPart;

    // Use 33ms by default as time delta, but if skeleton is tracked, use the difference in time stamps
    FLOAT fDeltaTimeInSeconds = 0.033f;
    if ( pCurrentSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
    {
        // If the previous skeleton has not been tracked, simply copy the current frame's skeleton into it
        if ( pPreviousSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
        {
            XMemCpy( pPreviousSkeletonData, pCurrentSkeletonData, sizeof( NUI_SKELETON_DATA ) );
            uPrevTimeStamp = uCurTimeStamp;
        }

        // Get a valid time delta to calucate velocity
        fDeltaTimeInSeconds = ( uCurTimeStamp - uPrevTimeStamp ) * 0.001f;
        if ( fDeltaTimeInSeconds == 0.0f )
        {
            fDeltaTimeInSeconds = 0.033f;
        }

        FLOAT fDetectedJointVelocity = 0.0f;
        bDetected = FALSE;
        for ( UINT i = 0; i < NUM_GESTURES; i++ )
        {
            // Get the joints needed for calculate an analog signal for each gesture, in this case velocity
            // of the wrists and ankles.
            NUI_SKELETON_POSITION_INDEX jointIndex = g_GestureData[ i ].m_JointIndex;
            vPrevJoint[ i ]     = pPreviousSkeletonData->SkeletonPositions[ jointIndex ];
            vCurJoint[ i ]      = pCurrentSkeletonData->SkeletonPositions[ jointIndex ];
            fJointVelocity[ i ] = XMVectorGetX( XMVector3Length( vCurJoint[ i ] - vPrevJoint[ i ] ) ) / fDeltaTimeInSeconds;

            // Find out if any detection triggered and find the maximum velocity of joints from detected gestures
            if ( g_GestureData[ i ].m_pTrigger->Value == TRUE )
            {
                fDetectedJointVelocity = max( fDetectedJointVelocity, fJointVelocity[ i ] );
                bDetected |= g_GestureData[ i ].m_pTrigger->Value;
            }
        }       

        // If we found a new instance of the gesture, play a sound based on the velocity of the joint
        for ( UINT i = 0; i < NUM_GESTURES; i++ )
        {
            if ( g_GestureData[ i ].m_pTrigger->FirstFrameDetected )
            {
                PlaySound( m_pXAudio2, ( fDetectedJointVelocity > LOAD_PUNCH_VELOCITY_THRESHOLD ) ? LOAD_PUNCH_SOUND : PUNCH_SOUND );
            }
        }

        // If we found any triggered detection, we want adjust the displacement of the pendulum in correlation
        // to the punch/kick we did, i.e. if it's a slow punch then update with less displacement
        if ( bDetected )
        {
            static const FLOAT fMinDisplacement = 20.0f;
            static const FLOAT fMaxDisplacement = 100.0f;

            FLOAT fDisplacement = fDetectedJointVelocity * fDetectedJointVelocity * fDetectedJointVelocity;
            fDisplacement = max( fMinDisplacement, fDisplacement );
            fDisplacement = min( fMaxDisplacement, fDisplacement );
            FLOAT fCurrentDisplacement = -m_pendulum.m_fDisplacement;

            if ( fCurrentDisplacement == 0.0f )
            {
                // Just lerp between the current displacement and the new one
                fDisplacement = ( fDisplacement * 0.2f ) + ( fCurrentDisplacement * 0.8f );
            }
            else if ( m_pendulum.m_fVelocity > 0 )      // Swinging away from the player
            {                
                // Add extra displacement
                fDisplacement = ( ( fCurrentDisplacement + fDisplacement  ) * 0.05f ) + ( fCurrentDisplacement * 0.95f );
                m_pendulum.m_fVelocity += fDetectedJointVelocity; 
            }
            else if ( m_pendulum.m_fVelocity < 0 )      // Swinging toward the player
            {
                // Dampen the displacement
                fDisplacement = fCurrentDisplacement * 0.95f;
                m_pendulum.m_fVelocity += fDetectedJointVelocity; 
            }

            fDisplacement = min( fMinDisplacement, fDisplacement );
            m_pendulum.SetDisplacement( -fDisplacement );
        }
    }
    else
    {
        for ( UINT i = 0; i < NUM_GESTURES; i++ )
        {
            g_GestureData[ i ].m_nNumDetected = 0;
        }
    }    

    // Update the pendulum
    fDeltaTimeInSeconds = min( 0.066f, fDeltaTimeInSeconds );
    m_pendulum.Update( fDeltaTimeInSeconds );

    // Now update the rotation axis for the animation of the punching bag so that a left punch
    // makes the bag go to the right and visa versa
    if ( bDetected )
    {
        for ( UINT i = 0; i < NUM_GESTURES; i++ )
        {
            if ( g_GestureData[ i ].m_pTrigger->Value == TRUE )
            {
                switch( g_GestureData[ i ].m_Side )
                {
                case LEFT:
                    m_vAxis = ( XMVectorSet( 1.0f, 0.0f, -0.5f, 0.0f ) * 0.05f ) + ( m_vAxis * 0.95f );
                    break;

                case RIGHT:
                    m_vAxis = ( XMVectorSet( 1.0f, 0.0f, 0.5f, 0.0f ) * 0.05f ) + ( m_vAxis * 0.95f );
                    break;
                }
            }
        }
    }
    else
    {
        m_vAxis = ( XMVectorSet( 1.0f, 0.0f, 0.0f, 0.0f ) * 0.01f ) + ( m_vAxis * 0.99f );
    }

    // Update the world matrix from the rotation axis and pendulum rotation
    static const FLOAT fScaleTheta = 50.0f;
    m_matWorld = XMMatrixRotationAxis( m_vAxis, m_pendulum.m_fTheta / fScaleTheta );

    // Update the view matrix
    static const XMVECTOR vEyePt    = XMVectorSet( 0, 125.0f, -425.0f, 0.0f );
    static const XMVECTOR vLookatPt = XMVectorSet( 0.0f, 125.0f, 0.0f, 0.0f );
    static const XMVECTOR vUp       = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // Update the world*view*projection matrix
    m_matWVP = m_matWorld * m_matView * m_matProj;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------

HRESULT Sample::Render()
{
    PIXBeginNamedEvent( 0, __FUNCTION__  );

    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );

    // Set renderstates, shaders and constants
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetPixelShader( m_pPixelShader );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    for ( UINT i = 0; i < 3; i++ )
    {
        m_pd3dDevice->SetSamplerState( i, D3DSAMP_MAXANISOTROPY, 16 );
        m_pd3dDevice->SetSamplerState( i, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
        m_pd3dDevice->SetSamplerState( i, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
        m_pd3dDevice->SetSamplerState( i, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    }

    PIXBeginNamedEvent( 0, "PunchingBag" );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matWVP, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&m_matWorld, 4 );
    RenderScene( m_pPunchingBagScene, TRUE );
    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "BoxingRing" );
    static const FLOAT fBoxingRingScale = 1.5f;
    XMMATRIX matWorld = XMMatrixIdentity();
    m_matWVP = XMMatrixScaling( fBoxingRingScale, fBoxingRingScale, fBoxingRingScale ) * matWorld * m_matView * m_matProj;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matWVP, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matWorld, 4 );
    RenderScene( m_pBoxingRingScene, FALSE );
    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "UI"  );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if ( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Visualize the data streaming from the camera and the skeletons
        VisualizeSkeletonTracking( m_pd3dDevice );

        // Draw title text
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff,  L"Simple Gesture Detection" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // If we're recording data, show the name of the gesture we're requesting the player to do
        if ( m_bRecordData )
        {
            wchar_t szBuffer[ 256 ];
            m_Font.SetScaleFactors( 1.5f, 1.5f );
            swprintf_s( szBuffer, L"Recording... (%d)", m_nSecondsRecording );
            m_Font.DrawText( 0, 200, 0xffffff00, szBuffer, ATGFONT_RIGHT );
        }
        else
        {
            // Render gesture names, num times detected and confidence values
            FLOAT fTextPos = 100.0f;
            for ( UINT i = 0; i < NUM_GESTURES; i++ )
            {
                wchar_t szBuffer[ 256 ];
                MultiByteToWideChar( CP_ACP, 0, g_GestureData[ i ].m_szName, strlen( g_GestureData[ i ].m_szName ) + 1, szBuffer, sizeof( szBuffer) );
                swprintf_s( szBuffer, L"%s : %d", szBuffer, g_GestureData[ i ].m_nNumDetected );
                m_Font.DrawText( 0, fTextPos, 0xffffffff, szBuffer, ( g_GestureData[ i ].m_Side == LEFT ) ? ATGFONT_LEFT : ATGFONT_RIGHT );

                UINT uWidth = g_nNumValuesToVisualize * 4;
                UINT x = ( g_GestureData[ i ].m_Side == LEFT ) ? 125 : ( m_d3dpp.BackBufferWidth - 125 - uWidth );
                UINT y = (UINT)fTextPos + 25;
                RenderConfidenceValues( i, x, y );

                if ( g_GestureData[ i ].m_Side == RIGHT )
                {
                    fTextPos += 100.0f;
                }
            }
        }

        m_Font.End();
    }
    PIXEndNamedEvent(); // UI

    PIXEndNamedEvent(); // Render

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderScene()
// Desc: Renders scene geometry
//--------------------------------------------------------------------------------------

VOID Sample::RenderScene( ATG::Scene* pScene, const BOOL bApplyModelMtx )
{
    ATG::NameIndexedCollection::iterator i;
    for( i = pScene->GetInstanceList()->begin(); i != pScene->GetInstanceList()->end(); i++ )
    {
        // Select models from the object list.
        if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );

            if ( bApplyModelMtx )
            {
                ATG::Bound bound = pModel->GetLocalBound();
                ATG::Sphere sphere = bound.GetSphere();
                XMMATRIX matCenterModel = XMMatrixTranslation( -sphere.Center.x, -sphere.Center.y - sphere.Radius, -sphere.Center.z );
                XMMATRIX matTranslateModel = XMMatrixTranslation( 0, sphere.Center.y + sphere.Radius, 0 );
                XMMATRIX matWorld = matCenterModel * m_matWorld * matTranslateModel;
                m_matWVP = matWorld * m_matView * m_matProj;
                m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matWVP, 4 );
            }

            // Loop over mesh mappings.
            DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
            for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
            {
                ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                ATG::BaseMesh* pMesh = mm.pMesh;

                // Loop over mesh subsets.
                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {

                    ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

                    for( DWORD j = 0; j < pMaterial->GetRawParameterCount(); ++j )
                    {
                        // Retrieve diffuse and normal maps and set
                        ATG::MaterialParameter& param = pMaterial->GetRawParameter( j );
                        if( param.pValue != NULL )
                        {
                            ATG::Texture2D* pTex2D = ( ATG::Texture2D* )param.pValue;

                            m_pd3dDevice->SetTexture( j, pTex2D->GetD3DTexture() );
                        }
                    }

                    // Render the mesh subset.
                    pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderConfidenceValues()
// Desc: Debug rendering to show confidence values for detection
//--------------------------------------------------------------------------------------

VOID Sample::RenderConfidenceValues( const UINT uGestureIdx, const UINT x, const UINT y )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Save all render states
    IDirect3DStateBlock9* pStateBlock = NULL;
    m_pd3dDevice->CreateStateBlock( D3DSBT_ALL, &pStateBlock );
    pStateBlock->Capture();

    // Set renderstates
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    static const UINT uHeight  = 120;
    static const FLOAT fScale  = 2.0f;
    
    D3DRECT Rect;
    BOOL bDetected;
    D3DCOLOR color;

    // Render confidence of trigger
    for ( UINT i = 0; i < g_nNumValuesToVisualize; i++ )
    {
        Rect.x1 = x + ( i * 4 );
        Rect.x2 = x + ( i * 4 ) + 4;
        Rect.y1 = y;
        Rect.y2 = y + uHeight;
        FLOAT fConfidence = g_GestureData[ uGestureIdx ].m_TriggerData[ i ].Confidence;
        Rect.y1 = Rect.y2 - (LONG)( fConfidence * fScale * (FLOAT)( uHeight) );

        bDetected = g_GestureData[ uGestureIdx ].m_TriggerData[ i ].Value;
        color = bDetected ? D3DCOLOR_ARGB( 127, 0, 255, 0 ) : D3DCOLOR_ARGB( 127, 255, 0, 0 );

        // draw baseline confidence in white
        if ( fConfidence == 0.0f )
        {
            color = D3DCOLOR_ARGB( 127, 255, 255, 255 );
        }

        ATG::DebugDraw::DrawScreenSpaceRect( Rect, 2.0f, color );
    }

    // Render progress
    for ( UINT i = 0; i < g_nNumValuesToVisualize; i++ )
    {
        Rect.x1 = x + ( i * 4 );
        Rect.x2 = x + ( i * 4 ) + 4;
        Rect.y1 = y;
        Rect.y2 = y + uHeight;
        FLOAT fProgress = g_GestureData[ uGestureIdx ].m_ProgressData[ i ].Value;
        Rect.y1 = Rect.y2 - (LONG)( fProgress * (FLOAT)( uHeight) );

        color = D3DCOLOR_ARGB( 127, 127, 127, 0 );

        ATG::DebugDraw::DrawScreenSpaceRect( Rect, 2.0f, color );
    }

    // Restore previous render states
    pStateBlock->Apply();
    pStateBlock->Release();

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RecordData
// Desc: Plays a wave and blocks until the wave finishes playing
//--------------------------------------------------------------------------------------

VOID Sample::RecordData( NUI_SKELETON_DATA* pSkeletonData )
{
    static DWORD dwStart = GetTickCount();

    if ( m_bRecordData )
    {
        // Only start recording if the skeleton is tracked
        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
        {
            // If we don't have a file open yet, create a file and start the recording
            if ( m_hFile == 0 )
            {
                DWORD dwRecordingID = GetTickCount();
                CHAR szFileName[ MAX_PATH ];
                sprintf_s( szFileName, "GAME:\\Recording_%d.xed", dwRecordingID );

                // Create a Xed file on the Xbox
                HRESULT hResult = XStudioCreateFile( szFileName, GENERIC_WRITE, CREATE_ALWAYS,
                                                        XSTUDIO_STREAM_FLAG_NUICAM_DEPTH |
                                                        XSTUDIO_STREAM_FLAG_NUIAPI_SKELETON |
                                                        XSTUDIO_STREAM_FLAG_NUIAPI_PLAYER_INDEX,
                                                        &m_hFile );
                if ( FAILED( hResult ) )
                {
                    m_bRecordData = FALSE;
                }
                else
                {
                    // Tell XStudio wich streams to record into the file
                    hResult = XStudioMapStreams( XSTUDIO_STREAM_FLAG_NUICAM_DEPTH |
                                                 XSTUDIO_STREAM_FLAG_NUIAPI_SKELETON |
                                                 XSTUDIO_STREAM_FLAG_NUIAPI_PLAYER_INDEX,
                                                 m_hFile );

                    // Start recording
                    hResult = XStudioStart( XSTUDIO_STREAM_FLAG_ALL );

                    // start the countdown
                    dwStart = GetTickCount();
                    m_nSecondsRecording = g_nNumSecondsToRecord;
                }
            }
            else
            {
                // We already have a file, so just update the seconds to record
                DWORD dwStop = GetTickCount();
                if ( dwStop - dwStart > 1000 )
                {
                    m_nSecondsRecording--;
                    dwStart = GetTickCount();
                }

                // Stop the recording when the countdown has finished
                if ( m_nSecondsRecording <= 0 )
                {
                    m_nSecondsRecording = g_nNumSecondsToRecord;
                    m_bRecordData = FALSE;
                    XStudioStop( XSTUDIO_STREAM_FLAG_ALL );
                    XStudioUnmapStreams( XSTUDIO_STREAM_FLAG_ALL, m_hFile );
                    XStudioCloseFile( m_hFile );
                    m_hFile = 0;
                }
            }
        }
    }
    else
    {
        // If the user pressed "A" while recording, simply stop the recording manually
        if ( m_hFile )
        {
            // Stop recording and close the file
            XStudioStop( XSTUDIO_STREAM_FLAG_ALL );
            XStudioUnmapStreams( XSTUDIO_STREAM_FLAG_ALL, m_hFile );
            XStudioCloseFile( m_hFile );
            m_hFile = 0;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: PlaySound
// Desc: Plays a wave sound file
//--------------------------------------------------------------------------------------

VOID Sample::PlaySound( IXAudio2* pXaudio2, const char* szFilename )
{
    HRESULT hr = S_OK;

    // Read the wave file
    ATG::WaveFile WaveFile;
    if ( FAILED( hr = WaveFile.Open( szFilename ) ) )
    {
        ATG::FatalError( "Error %#X opening WAV file\n", hr );
    }

    // Read the format header
    WAVEFORMATEXTENSIBLE wfx = { 0 };
    if ( FAILED( hr = WaveFile.GetFormat( &wfx ) ) )
    {
        ATG::FatalError( "Error %#X reading WAV format\n", hr );
    }

    // Calculate how many bytes and samples are in the wave
    DWORD cbWaveSize = 0;
    WaveFile.GetDuration( &cbWaveSize );

    if ( m_pbWaveData )
    {
        SAFE_DELETE_ARRAY( m_pbWaveData );
        m_pbWaveData = NULL;
    }

    // Read the sample data into memory
    BYTE* m_pbWaveData = new BYTE[ cbWaveSize ];
    if ( FAILED( hr = WaveFile.ReadSample( 0, m_pbWaveData, cbWaveSize, &cbWaveSize ) ) )
    {
        ATG::FatalError( "Error %#X reading WAV data\n", hr );
    }

    // Play the wave using a new XAudio2SourceVoice
    if ( m_pSourceVoice )
    {
        m_pSourceVoice->DestroyVoice();
        m_pSourceVoice = NULL;
    }

    if( FAILED( hr = pXaudio2->CreateSourceVoice( &m_pSourceVoice, ( WAVEFORMATEX* )&wfx ) ) )
    {
        ATG::FatalError( "Error %#X creating source voice\n", hr );
    }

    // Submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = { 0 };
    buffer.pAudioData = m_pbWaveData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
    buffer.AudioBytes = cbWaveSize;

    if ( FAILED( hr = m_pSourceVoice->SubmitSourceBuffer( &buffer ) ) )
    {
        ATG::FatalError( "Error %#X submitting source buffer\n", hr );
    }

    // Set the sound a bit louder
    static const FLOAT fVolumeGain = 5.0f;
    m_pSourceVoice->SetVolume( fVolumeGain );

    // Play the sound
    m_pSourceVoice->Start( 0 );
}