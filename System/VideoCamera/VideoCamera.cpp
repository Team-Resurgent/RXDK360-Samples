//--------------------------------------------------------------------------------------
// VideoCamera.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <xgraphics.h>
#include <xcam.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_2, L"Reset\nparameters" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Take snapshot" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change\nresolution" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change\ncamera parameters" },
    { ATG::HELP_LEFT_SHOULDER,ATG::HELP_PLACEMENT_1, L"Cycle zoom setting in zoom mode" },
    { ATG::HELP_RIGHT_SHOULDER,ATG::HELP_PLACEMENT_1, L"Cycle zoom setting in zoom mode" },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_1, L"Press to change framerate\nPan in zoom mode" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_1, L"Press to\nchange framerate" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"Change camera parameters\nin parameter setting mode" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[ 0 ] );


//--------------------------------------------------------------------------------------
// Definitions
//--------------------------------------------------------------------------------------
struct VIDEOFEEDVERTEX
{
    float   Position[ 3 ];
    float   TexCoords[ 2 ];
};

XCAMRESOLUTION g_FrameResolution[] =
{
    XCAMRESOLUTION_1280x960,
    XCAMRESOLUTION_960x720,
    XCAMRESOLUTION_640x480,
    XCAMRESOLUTION_352x288,
    XCAMRESOLUTION_320x240,
    XCAMRESOLUTION_176x144,
    XCAMRESOLUTION_160x120,
};
const INT           NUMBER_OF_RESOLUTION = sizeof( g_FrameResolution ) / sizeof( g_FrameResolution[0] );
const INT           DEFAULT_RESOLUTION_INDEX = 2;

XCAMFRAMERATE g_FrameRate[ NUMBER_OF_RESOLUTION ][ 8 ] =
{
    { XCAMFRAMERATE_FASTEST, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1,
        ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1 }, // 1280
    { XCAMFRAMERATE_FASTEST, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1,
        ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1 }, // 960
    { XCAMFRAMERATE_30, XCAMFRAMERATE_25, XCAMFRAMERATE_20, XCAMFRAMERATE_15, XCAMFRAMERATE_10,
        XCAMFRAMERATE_5, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1 },            // 640
    { XCAMFRAMERATE_30, XCAMFRAMERATE_25, XCAMFRAMERATE_20, XCAMFRAMERATE_15, XCAMFRAMERATE_10,
        XCAMFRAMERATE_5, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1 },            // 352
    { XCAMFRAMERATE_30, XCAMFRAMERATE_25, XCAMFRAMERATE_20, XCAMFRAMERATE_15, XCAMFRAMERATE_10,
        XCAMFRAMERATE_5, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1 },            // 320
    { XCAMFRAMERATE_30, XCAMFRAMERATE_25, XCAMFRAMERATE_20, XCAMFRAMERATE_15, XCAMFRAMERATE_10,
        XCAMFRAMERATE_5, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1 },            // 176
    { XCAMFRAMERATE_30, XCAMFRAMERATE_25, XCAMFRAMERATE_20, XCAMFRAMERATE_15, XCAMFRAMERATE_10,
        XCAMFRAMERATE_5, ( XCAMFRAMERATE )-1, ( XCAMFRAMERATE )-1 },            // 160
};
const INT           NUMBER_OF_RATE = sizeof( g_FrameRate[0] ) / sizeof( g_FrameRate[0][0] );

XCAMZOOMFACTOR g_ZoomTable[ NUMBER_OF_RESOLUTION ][ 3 ] =
{
    { XCAMZOOMFACTOR_1X, ( XCAMZOOMFACTOR )-1, ( XCAMZOOMFACTOR )-1 },  //1280
    { XCAMZOOMFACTOR_1X, ( XCAMZOOMFACTOR )-1, ( XCAMZOOMFACTOR )-1 },  //960
    { XCAMZOOMFACTOR_1X, XCAMZOOMFACTOR_2X, ( XCAMZOOMFACTOR )-1 },   //640
    { XCAMZOOMFACTOR_1X, XCAMZOOMFACTOR_2X, ( XCAMZOOMFACTOR )-1 },   //352
    { XCAMZOOMFACTOR_1X, XCAMZOOMFACTOR_2X, XCAMZOOMFACTOR_4X },    //320
    { XCAMZOOMFACTOR_1X, XCAMZOOMFACTOR_2X, XCAMZOOMFACTOR_4X },    //176
    { XCAMZOOMFACTOR_1X, XCAMZOOMFACTOR_2X, XCAMZOOMFACTOR_4X },    //160
};
const INT           NUM_ZOOMTABLE = sizeof( g_ZoomTable[0] ) / sizeof( g_ZoomTable[0][0] );

const WCHAR*        g_ResolutionStrings[NUMBER_OF_RESOLUTION] =
{
    L"1280x960",
    L"960x720",
    L"640x480",
    L"352x288",
    L"320x240",
    L"176x144",
    L"160x120",
};

const FLOAT         PAN_FACTOR = 1.0f;

const LONG          MAX_ZOOM_X_2X = 320;
const LONG          MAX_ZOOM_Y_2X = 240;
const LONG          MAX_ZOOM_X_4X = 480;
const LONG          MAX_ZOOM_Y_4X = 360;

// Number of buffers that are used for XCAM driver
// To keep the camera working at optimal speed, you will always want to have at least two
// read requests outstanding.  This way, as soon as one request has been completely filled,
// the camera driver can immediately move on to the next request without stalling the camera
// hardware to wait for you to submit another read request.
//
// In addition, we don't want to lock a texture and kick off another read into it immediately
// after using the texture to render on-screen.  This would cause the CPU to stall waiting for
// the GPU to finish using the texture.  So, this means that we want 4 total buffers, two of
// which can be queued up for read frames, one which is currently being used to render to
// the screen, and a fourth slot where the texture can sit "idle" after being used to render
// before sticking it back into the camera driver.
const INT           NUMBER_OF_BUFFERS = 4;
const INT           READ_FRAME_QUEUE_SIZE = 2;

struct CAMERA_PARAMETER
{
    XCAMCONFIGCONTROLID ControlID;
    WCHAR* strParamName;
    INT iMax;
    INT iMin;
    INT iDefault;
    INT iStep;
    INT iCurrent;
};

CAMERA_PARAMETER g_ParameterList[] =
{
    { XCAMCONFIGID_AE_MODE,         L"Auto Exposure   ",  1,   0,   1, 1,  1 },
    { XCAMCONFIGID_AE_PRIORITY,     L"AE priority     ",  1,   0,   0, 1,  1 },
    { XCAMCONFIGID_EXPOSURE_TIME,   L"Exposure time   ",2200,  6, 166,10,166 },
    { XCAMCONFIGID_BACKLIGHT_COMP,  L"Backlight comp. ",  1,   0,   0, 1,  0 },
    { XCAMCONFIGID_BRIGHTNESS,      L"Brightness      ",255,   0, 127, 1,127 },
    { XCAMCONFIGID_CONTRAST,        L"Contrast        ",255,   0,  32, 1, 32 },
    { XCAMCONFIGID_GAIN,            L"Gain            ",255,   0,   0, 1,  0 },
    { XCAMCONFIGID_FLICKER,         L"Flicker         ",  2,   0,   2,-1,  2 },
    { XCAMCONFIGID_HUE,             L"Hue             ",180,-180,   0, 1,  0 },
    { XCAMCONFIGID_SATURATION,      L"Saturation      ",255,   0,  32, 1, 32 },
    { XCAMCONFIGID_SHARPNESS,       L"Sharpness       ",255,   0,   0, 1,  0 },
    { XCAMCONFIGID_GAMMA,           L"Gamma           ",250, 150, 180, 1,180 },
    { XCAMCONFIGID_AUTO_WHITE_BAL,  L"Auto WB         ",  1,   0,   1, 1,  1 },
    { XCAMCONFIGID_WHITE_BALANCE,   L"White balance   ",6550,2770,4000,10,4000},
    { XCAMCONFIGID_LOWLIGHT,        L"Low light       ",  1,   0,   0, 1,  0 },
    { XCAMCONFIGID_LIGHTSOURCE,     L"Light Source    ",  2,   0,   0,-1,  0 },
    { XCAMCONFIGID_COMP_ROOM_LIGHT, L"Room light comp.",  2,   0,   0,-1,  0 },
    { XCAMCONFIGID_COMP_CORRECTED_LIGHTSOURCE, L"Corrected light source comp.",  3,   0,   0,-1,  0 },
};
const INT           NUM_PARAMETERLIST = sizeof( g_ParameterList ) / sizeof( g_ParameterList[0] );

const INT RESET_TABLE[NUM_PARAMETERLIST] =
{
    6, 13, 16, 2, 0, 1, 3, 4, 5, 7, 8, 9, 10, 11, 12, 14, 15, 17
};

const INT           PARAM_WB = 13;
const INT           PARAM_AUTOWB = 12;
const INT           PARAM_EXP = 2;
const INT           PARAM_GAIN = 6;
const INT           PARAM_AUTOEXP = 0;
const INT           PARAM_COMP_CORRECTED = 17;

typedef enum SAMPLEMODE
{
    SAMPLEMODE_SNAPSHOT     = 0,
    SAMPLEMODE_PARAMTEST,
    SAMPLEMODE_SELECTZOOM,
    SAMPLEMODE_FORCE_DWORD  = 0x7fffffff
};


//-------------------------------------------------------------------------------------
// Video and pixel shader definitions
//-------------------------------------------------------------------------------------
static const char*  g_strVideoShaderHLSL =
    " struct VS_OUT                                                              "
    " {                                                                          "
    "     float4 Position : POSITION;                                            "
    "     float2 TexCoord : TEXCOORD0;                                           "
    " };                                                                         "
    "                                                                            "
    " sampler VideoTexture : register(s0);                                       "
    "                                                                            "
    " VS_OUT VideoVertex( const float3 Position : POSITION,                      "
    "                    const float2 TexCoord : TEXCOORD0 )                     "
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
    " float4 VideoPixelYUV( VS_OUT Input ) : COLOR                               "
    " {                                                                          "
    "     float4 Tex = tex2D( VideoTexture, Input.TexCoord )                     "
    "                  - float4( 0.5f, 16.f / 256.f, 0.5f, 0.f );                "
    "     float4 Color = Tex.ggga * 1.164f;                                      "
    "     Color.r +=                  Tex.r * 1.596f;                            "
    "     Color.g += -Tex.b * 0.391f -Tex.r * 0.813f;                            "
    "     Color.b +=  Tex.b * 2.018f;                                            "
    "     return Color;                                                          "
    " }                                                                          ";

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Timer m_VideoTimer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    BOOL m_CaptureModeSet;
    BOOL m_bZoomNeedsToBeReset;
    IXCamVideo* m_pIXCamVideo;
    XCAMRESOLUTION m_CameraResolution;
    XCAMFRAMERATE m_CameraFramerate;

    BOOL m_bSelectingZoom;
    XCAMZOOMFACTOR m_ZoomSetting;
    BOOL m_bIsZoomed;
    LONG m_CameraCenterX;
    LONG m_CameraCenterY;
    FLOAT m_CameraCenterXOffset;
    FLOAT m_CameraCenterYOffset;

    FLOAT m_fPictureX1;
    FLOAT m_fPictureWidth;
    FLOAT m_fPictureY1;
    FLOAT m_fPictureHeight;

    XCAMRESOLUTION m_SnapshotResolution;

    XOVERLAPPED     m_ReadRequests[ NUMBER_OF_BUFFERS ];
    XOVERLAPPED m_SnapshotRequest;
    IDirect3DTexture9* m_pVideoTextures[ NUMBER_OF_BUFFERS ];
    D3DLOCKED_RECT  m_VideoLockedRects[ NUMBER_OF_BUFFERS ];
    BOOL            m_bReadSubmitted[ NUMBER_OF_BUFFERS ];
    IDirect3DTexture9* m_pSnapshotTexture;
    ULONG m_CurrentTexture;

    IDirect3DVertexBuffer9* m_pVideoVB;               // Buffer to hold vertices
    IDirect3DVertexBuffer9* m_pSnapshotVB;            // Buffer to hold vertices
    IDirect3DVertexDeclaration9* m_pVideoVertexDecl;       // Vertex format decl
    IDirect3DVertexShader9* m_pVideoVertexShader;     // Vertex Shader
    IDirect3DPixelShader9* m_pVideoPixelShaderYUY2;  // Pixel Shader for YUY2 image

    INT m_iCurZoom;                          // Zoom setting
    INT m_iCurrentFrameResolution;
    INT m_iCurrentFrameRate;
    INT m_iCurrentMode;
    INT m_iCurrentParameter;

    HRESULT         InitializeShaders();
    HRESULT         InitializeVertexBuffer();
    HRESULT         InitializeVideoTextures();
    HRESULT         InitializeSnapshotTexture();

    HRESULT         ChangeCaptureMode();
    VOID            ChangeZoomLevel( INT zoom );
    VOID            AdjustZoomPanPosition();
    HRESULT         DoZoomAndPan();

    void            UpdateVideoFeed();
    void            SubmitReadFrameHelper( DWORD index );

    void            TakeSnapshot();
    void            GetWidthHeight( XCAMRESOLUTION Resolution,
                                    LPDWORD pdwWidth,
                                    LPDWORD pdwHeight );

    VOID            ResetParameters( VOID );
    VOID            SetParameters( VOID );

    // Text scale factor
    FLOAT m_fTextScaleX;
    FLOAT m_fTextScaleY;

public:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;

    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize variables
    m_CaptureModeSet = FALSE;
    m_pIXCamVideo = NULL;
    m_iCurrentFrameResolution = DEFAULT_RESOLUTION_INDEX;
    m_iCurrentFrameRate = 0;
    m_iCurrentMode = SAMPLEMODE_SNAPSHOT;             // Snapshot mode
    m_iCurZoom = 0;
    m_iCurrentParameter = 0;

    m_bZoomNeedsToBeReset = TRUE;
    m_bSelectingZoom = FALSE;
    m_CameraCenterX = m_CameraCenterY = 0;
    m_CameraCenterXOffset = m_CameraCenterYOffset = 0.0f;
    m_ZoomSetting = XCAMZOOMFACTOR_1X;
    m_bIsZoomed = FALSE;

    m_CameraResolution = g_FrameResolution[m_iCurrentFrameResolution];
    m_SnapshotResolution = g_FrameResolution[m_iCurrentFrameResolution];
    m_CameraFramerate = g_FrameRate[ m_iCurrentFrameResolution ][ 0 ];

    m_bDrawHelp = FALSE;

    for( INT i = 0; i < NUMBER_OF_BUFFERS; ++i )
    {
        m_pVideoTextures[ i ] = NULL;
        m_bReadSubmitted[ i ] = FALSE;
    }
    m_pSnapshotTexture = NULL;

    // Initialize XOVERLAPPED structures with event objects
    // so that we can wait for their completions when needed
    ZeroMemory( &m_ReadRequests, sizeof( m_ReadRequests ) );
    ZeroMemory( &m_SnapshotRequest, sizeof( m_SnapshotRequest ) );
    for( INT i = 0; i < NUMBER_OF_BUFFERS; ++i )
        m_ReadRequests[ i ].hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    m_SnapshotRequest.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize shaders
    if( FAILED( InitializeShaders() ) )
        return E_FAIL;

    // Initialize simple shaders for UI
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Initialize a vertexbuffer
    if( FAILED( InitializeVertexBuffer() ) )
        return E_FAIL;

    // Initialize video textures
    if( FAILED( InitializeVideoTextures() ) )
        return E_FAIL;

    if( FAILED( InitializeSnapshotTexture() ) )
        return E_FAIL;

    m_fTextScaleX = 1280.f / 640.f;
    m_fTextScaleY = 720.f / 480.f;

    ResetParameters();

    if( ERROR_SUCCESS != XCamInitialize() )
        return E_FAIL;

    if( ERROR_SUCCESS != XCamCreateVideo( &m_pIXCamVideo ) )
    {
        XCamShutdown();
        return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Get the current camera state
    XCAMDEVICESTATE deviceState = XCamGetStatus();

    //  Work around for known camera driver bug: 
    //  In order to restore camera zoom and pan settings properly, developers must call 
    //  XCamSetView() when the camera state transitions from a non-initialized 
    //  state to an initialized state.
    if( deviceState != XCAMDEVICESTATE_INITIALIZED )
    {
        m_bZoomNeedsToBeReset = TRUE;
    }

        // If the camera device is active (not disconnected or initializing)
    else if( deviceState == XCAMDEVICESTATE_INITIALIZED )
    {
        //  Work around for known camera driver bug: 
        //  In order to restore camera zoom and pan settings properly, developers must call 
        //  XCamSetView() when the camera state transitions from a non-initialized 
        //  state to an initialized state.
        if( m_bZoomNeedsToBeReset )
        {
            DoZoomAndPan();
            m_bZoomNeedsToBeReset = false;
        }

        // If the capture mode has not yet been set for our IXCamVideo
        // interface, then set the capture mode now.
        if( !m_CaptureModeSet )
        {
            ChangeCaptureMode();
        }

        // The camera is active - update the video feed, and check for
        // button presses to switch the camera into a different mode
        UpdateVideoFeed();

        // Check for other button presses
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            m_iCurrentFrameResolution = ( m_iCurrentFrameResolution + 1 ) % NUMBER_OF_RESOLUTION;
            m_CameraResolution = g_FrameResolution[ m_iCurrentFrameResolution ];
            m_SnapshotResolution = g_FrameResolution[ m_iCurrentFrameResolution ];
            m_iCurrentFrameRate = 0;
            m_CameraFramerate = g_FrameRate[ m_iCurrentFrameResolution ][ 0 ];
            ChangeCaptureMode();
            ChangeZoomLevel( 0 );
            DoZoomAndPan();
        }
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_THUMB )
        {
            do
            {
                m_iCurrentFrameRate = ( m_iCurrentFrameRate - 1 + NUMBER_OF_RATE ) % NUMBER_OF_RATE;
            } while( g_FrameRate[ m_iCurrentFrameResolution ][ m_iCurrentFrameRate ] == -1 );
            m_CameraFramerate = g_FrameRate[ m_iCurrentFrameResolution ][ m_iCurrentFrameRate ];
            ChangeCaptureMode();
        }
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_THUMB )
        {
            do
            {
                m_iCurrentFrameRate = ( m_iCurrentFrameRate + 1 ) % NUMBER_OF_RATE;
            } while( g_FrameRate[ m_iCurrentFrameResolution ][ m_iCurrentFrameRate ] == -1 );
            m_CameraFramerate = g_FrameRate[ m_iCurrentFrameResolution ][ m_iCurrentFrameRate ];
            ChangeCaptureMode();
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            // Take a snapshot
            if( m_iCurrentMode == SAMPLEMODE_SNAPSHOT )
                TakeSnapshot();

            // Adjust zoom
            if( m_iCurrentMode == SAMPLEMODE_SELECTZOOM )
            {
                if( m_bIsZoomed )
                {
                    // Zoom out
                    ChangeZoomLevel( 0 );

                    DoZoomAndPan();
                }
                else if( !m_bSelectingZoom )
                {
                    if( m_ZoomSetting == XCAMZOOMFACTOR_2X ||
                        m_ZoomSetting == XCAMZOOMFACTOR_4X )
                    {
                        // User may now steer a rectangle around to choose the zoom region
                        m_bSelectingZoom = TRUE;
                    }
                }
                else if( m_bSelectingZoom )
                {
                    // Zoom to the new desired setting
                    m_bSelectingZoom = FALSE;

                    DoZoomAndPan();
                }
            }
        }


        if( m_iCurrentMode == SAMPLEMODE_PARAMTEST )
        {
            // Change a parameter in focus
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
            {
                m_iCurrentParameter = ( m_iCurrentParameter + 1 ) % NUM_PARAMETERLIST;
            }
            else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            {
                m_iCurrentParameter = ( m_iCurrentParameter - 1 + NUM_PARAMETERLIST ) % NUM_PARAMETERLIST;
            }
                // Adjust values only if allowed to
            else if( ( g_ParameterList[ PARAM_AUTOEXP ].iCurrent == 0 || ( m_iCurrentParameter != PARAM_EXP &&
                                                                           m_iCurrentParameter != PARAM_GAIN ) ) &&
                     ( g_ParameterList[ PARAM_AUTOWB ].iCurrent == 0 || ( m_iCurrentParameter != PARAM_WB ) ) )
            {
                // Increment a parameter value
                if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                {
                    if( g_ParameterList[ m_iCurrentParameter ].iStep > 0 )
                    {
                        g_ParameterList[ m_iCurrentParameter ].iCurrent =
                            min( g_ParameterList[ m_iCurrentParameter ].iCurrent +
                                 g_ParameterList[ m_iCurrentParameter ].iStep,
                                 g_ParameterList[ m_iCurrentParameter ].iMax );
                    }
                    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                    {
                        g_ParameterList[ m_iCurrentParameter ].iCurrent =
                            min( g_ParameterList[ m_iCurrentParameter ].iCurrent -
                                 g_ParameterList[ m_iCurrentParameter ].iStep,
                                 g_ParameterList[ m_iCurrentParameter ].iMax );
                    }
                    SetParameters();
                }
                    // Decrement a parameter value
                else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                {
                    if( g_ParameterList[ m_iCurrentParameter ].iStep > 0 )
                    {
                        g_ParameterList[ m_iCurrentParameter ].iCurrent =
                            max( g_ParameterList[ m_iCurrentParameter ].iCurrent -
                                 g_ParameterList[ m_iCurrentParameter ].iStep,
                                 g_ParameterList[ m_iCurrentParameter ].iMin );
                    }
                    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                    {
                        g_ParameterList[ m_iCurrentParameter ].iCurrent =
                            max( g_ParameterList[ m_iCurrentParameter ].iCurrent +
                                 g_ParameterList[ m_iCurrentParameter ].iStep,
                                 g_ParameterList[ m_iCurrentParameter ].iMin );
                    }
                    SetParameters();
                }
            }
        }

        // Cycle mode
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            if( m_iCurrentMode < SAMPLEMODE_SELECTZOOM )
                m_iCurrentMode++;
            else
                m_iCurrentMode = SAMPLEMODE_SNAPSHOT;
        }
        // Reset params
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        {
            ResetParameters();
        }

        if( m_iCurrentMode == SAMPLEMODE_SELECTZOOM )
        {
            // Change zoom settings
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
            {
                if( !m_bIsZoomed )
                    ChangeZoomLevel( 1 );
            }
            // Change zoom settings
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
            {
                if( !m_bIsZoomed )
                    ChangeZoomLevel( -1 );
            }

            // Update the offset that should be used in the next view change
            if( m_bSelectingZoom )
            {
                m_CameraCenterXOffset += pGamepad->fX1 * PAN_FACTOR;
                m_CameraCenterYOffset += pGamepad->fY1 * PAN_FACTOR;
                if( pGamepad->fX1 != 0.0f || pGamepad->fY1 != 0.0f )
                {
                    AdjustZoomPanPosition();
                }
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Render video texture
    m_pd3dDevice->SetPixelShader( m_pVideoPixelShaderYUY2 );

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetVertexShader( m_pVideoVertexShader );
    m_pd3dDevice->SetVertexDeclaration( m_pVideoVertexDecl );

    m_pd3dDevice->SetTexture( 0, m_pVideoTextures[ m_CurrentTexture ] );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    m_pd3dDevice->SetStreamSource( 0, m_pVideoVB, 0, sizeof( VIDEOFEEDVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Render a snapshot image
    if( m_iCurrentMode == SAMPLEMODE_SNAPSHOT )
    {
        m_pd3dDevice->SetPixelShader( m_pVideoPixelShaderYUY2 );

        m_pd3dDevice->SetTexture( 0, m_pSnapshotTexture );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );

        m_pd3dDevice->SetStreamSource( 0, m_pSnapshotVB, 0, sizeof( VIDEOFEEDVERTEX ) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    }

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetTexture( 0, NULL );
    m_Timer.MarkFrame();

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        WCHAR str[ 100 ];

        const WCHAR* strZoom[] =
        {
            L"1X", L"2X", L"4X"
        };
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"VideoCamera" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.SetScaleFactors( 0.6f, 0.8f );
        m_Font.DrawText( 20 * m_fTextScaleX, 30 * m_fTextScaleY, 0xffffffff, L"Video stream" );

        swprintf_s( str, L"%s %s\nFrame rate : %s",
                    g_ResolutionStrings[ m_iCurrentFrameResolution ],
                    L"D3DFMT_LE_LIN_YUY2",
                    m_VideoTimer.GetFrameRate() );
        if( g_FrameRate[ m_iCurrentFrameResolution ][ m_iCurrentFrameRate ] != XCAMFRAMERATE_FASTEST )
            swprintf_s( str, L"%s/%d.00 fps", str, g_FrameRate[ m_iCurrentFrameResolution ][ m_iCurrentFrameRate ] );
        m_Font.DrawText( 28 * m_fTextScaleX, 300 * m_fTextScaleY, 0xffffffff, str );

        if( m_iCurrentMode == SAMPLEMODE_SNAPSHOT )
        {
            m_Font.DrawText( 260 * m_fTextScaleX, 30 * m_fTextScaleY, 0xffffffff, L"Snapshot" );
            swprintf_s( str, L"%s %s",
                        g_ResolutionStrings[ m_iCurrentFrameResolution ],
                        L"D3DFMT_LE_LIN_YUY2" );
            m_Font.DrawText( 268 * m_fTextScaleX, 300 * m_fTextScaleY, 0xffffffff, str );
        }

        if( m_iCurrentMode == SAMPLEMODE_PARAMTEST )
        {
            FLOAT fX = 300 * m_fTextScaleX;
            FLOAT fY = 20 * m_fTextScaleY;
            for( INT i = 0; i < NUM_PARAMETERLIST; ++i )
            {
                if( i == m_iCurrentParameter )
                {
                    m_Font.DrawText( fX - 10.f, fY, 0xffffffff, L">" );
                }

                if( i == PARAM_AUTOEXP || i == PARAM_AUTOWB )
                {
                    swprintf_s( str, L"%s: %s", g_ParameterList[ i ].strParamName,
                                g_ParameterList[ i ].iCurrent ? L"TRUE":L"FALSE" );
                }
                else
                {
                    swprintf_s( str, L"%s: %d", g_ParameterList[ i ].strParamName, g_ParameterList[ i ].iCurrent );
                }

                // Draw the parameter in white if it is allowed to be modified, or gray if not
                DWORD fontColor = ( ( g_ParameterList[ PARAM_AUTOEXP ].iCurrent == 0 ||
                                      ( i != PARAM_EXP && i != PARAM_GAIN ) ) &&
                                    ( g_ParameterList[ PARAM_AUTOWB ].iCurrent == 0 ||
                                      ( i != PARAM_WB ) ) ) ? 0xffffffff : 0xff666666;

                m_Font.DrawText( fX, fY, fontColor, str );
                fY += 20;
            }
        }

        m_Font.DrawText( 28 * m_fTextScaleX, 340 * m_fTextScaleY, 0xffffffff, L"Camera Status: " );
        switch( XCamGetStatus() )
        {
            case XCAMDEVICESTATE_DISCONNECTED:
                m_Font.DrawText( 0xffffff00, L"Unplugged" );    break;
            case XCAMDEVICESTATE_CONNECTED:
                m_Font.DrawText( 0xffffff00, L"Plugged-in" );   break;
            case XCAMDEVICESTATE_INITIALIZED:
                m_Font.DrawText( 0xffffff00, L"Initialized" );  break;
            case XCAMDEVICESTATE_IN_ERROR:
                m_Font.DrawText( 0xffffff00, L"Error! (try reconnecting camera)" );
        }
        swprintf_s( str, L"Zoom : %s", strZoom[ m_iCurZoom ] );
        m_Font.DrawText( 28 * m_fTextScaleX, 356 * m_fTextScaleY, 0xffffffff, str );

        if( m_iCurrentMode == SAMPLEMODE_SELECTZOOM )
        {
            m_Font.DrawText( 260 * m_fTextScaleX, 30 * m_fTextScaleY, 0xffffffff, L"Select Zoom" );

            if( m_bIsZoomed )
            {
                m_Font.DrawText( 300 * m_fTextScaleX, 70 * m_fTextScaleY, 0xffffffff,
                    L"Press " GLYPH_A_BUTTON L" to zoom out." );
            }
            else if( !m_bSelectingZoom )
            {
                m_Font.DrawText( 300 * m_fTextScaleX, 130 * m_fTextScaleY, 0xffffffff,
                                 L"Press shoulder buttons to \nchoose zoom factor." );

                if( m_ZoomSetting == XCAMZOOMFACTOR_2X ||
                    m_ZoomSetting == XCAMZOOMFACTOR_4X )
                {
                    m_Font.DrawText( 300 * m_fTextScaleX, 70 * m_fTextScaleY, 0xffffffff,
                        L"Press " GLYPH_A_BUTTON L" to select zoom.\n" );
                }
                else
                {
                    m_Font.DrawText( 300 * m_fTextScaleX, 190 * m_fTextScaleY, 0xffffffff,
                                     L"Note that not all modes support zoom." );
                }
            }
            else if( m_bSelectingZoom )
            {
                m_Font.DrawText( 300 * m_fTextScaleX, 70 * m_fTextScaleY, 0xffffffff,
                    L"Press " GLYPH_A_BUTTON L" to zoom in." );
            }

            if( !m_bIsZoomed &&
                ( m_ZoomSetting == XCAMZOOMFACTOR_2X ||
                  m_ZoomSetting == XCAMZOOMFACTOR_4X ) )
            {
                // Draw the zoom selection rectangle
                D3DRECT Rect;
                D3DCOLOR Color = m_bSelectingZoom ? D3DCOLOR_XRGB( 255, 255, 255 ) : D3DCOLOR_XRGB( 128, 128, 128 );

                FLOAT fZoomX;
                FLOAT fZoomY;
                FLOAT fZoomSelectorWidth;
                FLOAT fZoomSelectorHeight;

                if( m_ZoomSetting == XCAMZOOMFACTOR_2X )
                {
                    fZoomX = ( FLOAT )MAX_ZOOM_X_2X;
                    fZoomY = ( FLOAT )MAX_ZOOM_Y_2X;
                    fZoomSelectorWidth = m_fPictureWidth / 2.0f;
                    fZoomSelectorHeight = m_fPictureHeight / 2.0f;
                }
                else
                {
                    // m_ZoomSetting == XCAMZOOMFACTOR_4X...

                    fZoomX = ( FLOAT )MAX_ZOOM_X_4X;
                    fZoomY = ( FLOAT )MAX_ZOOM_Y_4X;
                    fZoomSelectorWidth = m_fPictureWidth / 4.0f;
                    fZoomSelectorHeight = m_fPictureHeight / 4.0f;
                }

                Rect.x1 = ( LONG )( m_fPictureX1 + ( m_fPictureWidth - fZoomSelectorWidth -
                                                     ( ( FLOAT )( fZoomX + m_CameraCenterX ) / ( 2.0 * fZoomX ) ) *
                                                     ( m_fPictureWidth - fZoomSelectorWidth ) ) );
                Rect.x2 = Rect.x1 + ( LONG )fZoomSelectorWidth;
                Rect.y1 = ( LONG )( m_fPictureY1 +
                                    ( ( FLOAT )( fZoomY + m_CameraCenterY ) / ( 2.0 * fZoomY ) ) *
                                    ( m_fPictureHeight - fZoomSelectorHeight ) );
                Rect.y2 = Rect.y1 + ( LONG )fZoomSelectorHeight;

                ATG::DebugDraw::DrawScreenSpaceRect( Rect, 2.0f, Color );
            }
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ChangeCaptureMode()
// Desc: Performs a synchronous capture mode change on the IXCamVideo interface, 
//       reinitializes the video and snapshot textures, and then kicks off the video
//       feed by submitting a new read request for each of the textures.
//--------------------------------------------------------------------------------------
HRESULT Sample::ChangeCaptureMode()
{
    if( ERROR_SUCCESS != m_pIXCamVideo->SetCaptureMode( m_CameraResolution,
                                                        m_CameraFramerate,
                                                        NULL ) )
    {
        ATG::DebugSpew( "Failed to set capture mode of camera\n" );
        return S_FALSE;
    }

    m_CaptureModeSet = TRUE;

    if( FAILED( InitializeVideoTextures() ) )
        return E_FAIL;
    if( FAILED( InitializeSnapshotTexture() ) )
        return E_FAIL;


    for( INT i = 0; i < READ_FRAME_QUEUE_SIZE; ++i )
    {
        SubmitReadFrameHelper( ( m_CurrentTexture + 1 + i ) % NUMBER_OF_BUFFERS );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ChangeZoomLevel()
// Desc: Adjusts the desired zoom level, without actually changing zoom on the camera.
//--------------------------------------------------------------------------------------
VOID Sample::ChangeZoomLevel( INT zoom )
{
    if( zoom == 0 )
    {
        m_iCurZoom = zoom;

        // Reset pan position to center.
        m_CameraCenterX = m_CameraCenterY = 0;

        m_bIsZoomed = FALSE;

        m_bSelectingZoom = FALSE;
    }

    do
    {
        if( zoom == 1 ) m_iCurZoom = ( m_iCurZoom + 1 ) % NUM_ZOOMTABLE;
        else if( zoom == -1 ) m_iCurZoom = ( m_iCurZoom - 1 + NUM_ZOOMTABLE ) % NUM_ZOOMTABLE;
        m_ZoomSetting = g_ZoomTable[ m_iCurrentFrameResolution ][ m_iCurZoom ];
    } while( m_ZoomSetting == -1 );

}


//--------------------------------------------------------------------------------------
// Name: AdjustZoomPanPosition()
// Desc: Adjusts the pan position for the zoom, without actually changing zoom.
//--------------------------------------------------------------------------------------
VOID Sample::AdjustZoomPanPosition()
{
    if( m_CameraCenterXOffset > 1.0f || m_CameraCenterXOffset < -1.0f ||
        m_CameraCenterYOffset > 1.0f || m_CameraCenterYOffset < -1.0f )
    {
        m_CameraCenterX -= ( LONG )m_CameraCenterXOffset;
        m_CameraCenterY -= ( LONG )m_CameraCenterYOffset;

        m_CameraCenterXOffset = 0;
        m_CameraCenterYOffset = 0;
    }

    // Clamp the pan to valid values
    switch( m_ZoomSetting )
    {
        case XCAMZOOMFACTOR_1X:
            m_CameraCenterX = 0;
            m_CameraCenterY = 0;
            break;

        case XCAMZOOMFACTOR_2X:
            m_CameraCenterX = min( m_CameraCenterX, MAX_ZOOM_X_2X );
            m_CameraCenterX = max( m_CameraCenterX, -MAX_ZOOM_X_2X );
            m_CameraCenterY = min( m_CameraCenterY, MAX_ZOOM_Y_2X );
            m_CameraCenterY = max( m_CameraCenterY, -MAX_ZOOM_Y_2X );
            break;

        case XCAMZOOMFACTOR_4X:
            m_CameraCenterX = min( m_CameraCenterX, MAX_ZOOM_X_4X );
            m_CameraCenterX = max( m_CameraCenterX, -MAX_ZOOM_X_4X );
            m_CameraCenterY = min( m_CameraCenterY, MAX_ZOOM_Y_4X );
            m_CameraCenterY = max( m_CameraCenterY, -MAX_ZOOM_Y_4X );
    }
}


//--------------------------------------------------------------------------------------
// Name: DoZoomAndPan()
// Desc: Changes the zoom on the camera, setting the pan at the same time.
//--------------------------------------------------------------------------------------
HRESULT Sample::DoZoomAndPan()
{
    DWORD dw;

    // Mask off the 1's digit of the pan coordinates to force them to be even values 
    // (since the camera does not accept odd values for the coordinates).

    //  XCamSetView() takes a non-trivial time to process and should not be used to simulate smooth
    //  panning.  Instead, developers should use XCamSetView() only to set final zoom and pan
    //  values.  Smooth panning can be handled at the application level.
    dw = XCamSetView( m_ZoomSetting, m_CameraCenterX & ( ~1 ), m_CameraCenterY & ( ~1 ), NULL );
    if( m_CameraCenterX == 0 && m_CameraCenterY == 0 && dw != ERROR_SUCCESS )
    {
        ATG::DebugSpew( " ERROR: XCamSetView\n" );
        return E_FAIL;
    }

    if( m_ZoomSetting == XCAMZOOMFACTOR_1X )
        m_bIsZoomed = FALSE;
    else
        m_bIsZoomed = TRUE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateVideoFeed()
// Desc: Check whether video request has finished. If it has been finished, submit
//       for a new frame.
//--------------------------------------------------------------------------------------
VOID Sample::UpdateVideoFeed()
{
    ULONG lReadingTexture = ( m_CurrentTexture + 1 ) % NUMBER_OF_BUFFERS;
    PXOVERLAPPED lpOverlapped = &m_ReadRequests[ lReadingTexture ];

    if( XHasOverlappedIoCompleted( lpOverlapped ) )
    {
        if( ERROR_SUCCESS == XGetOverlappedExtendedError( lpOverlapped ) )
        {
            m_VideoTimer.MarkFrame();

            ULONG lNextReadTexture = ( lReadingTexture + READ_FRAME_QUEUE_SIZE ) % NUMBER_OF_BUFFERS;
            SubmitReadFrameHelper( lNextReadTexture );

            m_CurrentTexture = lReadingTexture;
            m_pVideoTextures[ m_CurrentTexture ]->UnlockRect( 0 );
            m_bReadSubmitted[ m_CurrentTexture ] = FALSE;
        }
        else
        {
            // Read failed. Submit a request again
            if( ERROR_IO_PENDING != m_pIXCamVideo->ReadFrame(
                &m_VideoLockedRects[ lReadingTexture ],
                &m_ReadRequests[ lReadingTexture ] ) )
            {
                ATG::DebugSpew( "XCamReadFrame failed\n" );
            }

            m_bReadSubmitted[ lReadingTexture ] = TRUE;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: SubmitReadFrameHelper()
// Desc: Helper function to lock the specified texture and then submit a read request
//       to the camera driver to fill in the texture with a new frame of video data.
//--------------------------------------------------------------------------------------
VOID Sample::SubmitReadFrameHelper( DWORD index )
{
    // Submit a read request for current texture 
    if( SUCCEEDED( m_pVideoTextures[ index ]->LockRect( 0, &m_VideoLockedRects[ index ], NULL, 0 ) ) )
    {
        if( ERROR_IO_PENDING == m_pIXCamVideo->ReadFrame( &m_VideoLockedRects[ index ], &m_ReadRequests[ index ] ) )
        {
            m_bReadSubmitted[ index ] = TRUE;
        }
        else
            ATG::DebugSpew( "XCamReadFrame failed\n" );
    }
    else
        ATG::DebugSpew( "ERROR: Could not Lock the texture!\n" );
}


//--------------------------------------------------------------------------------------
// Name: TakeSnapshot()
// Desc: Take a snapshot
//--------------------------------------------------------------------------------------
VOID Sample::TakeSnapshot()
{
    D3DLOCKED_RECT Locked;

    if( FAILED( m_pSnapshotTexture->LockRect( 0, &Locked, NULL, 0 ) ) )
    {
        ATG::DebugSpew( "ERROR: Could not Lock the texture!\n" );
        return;
    }

    if( ERROR_IO_PENDING != XCamSnapshot( m_SnapshotResolution,
                                          &Locked,
                                          &m_SnapshotRequest ) )
    {
        ATG::DebugSpew( "XCamSnapshot failed\n" );
    }
    else if( ERROR_SUCCESS != XGetOverlappedResult( &m_SnapshotRequest, NULL, TRUE ) )
    {
        ATG::DebugSpew( "XCamSnapshot failed\n" );
    }

    m_pSnapshotTexture->UnlockRect( 0 );

}


//--------------------------------------------------------------------------------------
// Name: InitializeVideoTextures()
// Desc: Recreates the m_pVideoTextures array of D3D textures using the current
//       resolution and pixel format settings.
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeVideoTextures()
{
    HRESULT hr;
    m_CurrentTexture = 0;

    for( INT i = 0; i < NUMBER_OF_BUFFERS; ++i )
    {
        // Release the old texture
        if( m_pVideoTextures[ i ] != NULL )
        {
            if( m_bReadSubmitted[ i ] )
            {
                // If the driver is currently transfering an image to the
                // texture, wait until it is finished. 
                XGetOverlappedResult( &m_ReadRequests[ i ], NULL, TRUE );
                m_pVideoTextures[ i ]->UnlockRect( 0 );
                m_bReadSubmitted[ i ] = FALSE;
            }
            m_pVideoTextures[ i ]->Release();
            m_pVideoTextures[ i ] = NULL;
        }

        // Retrieve the D3D texture size and format
        DWORD iWidth, iHeight;
        GetWidthHeight( m_CameraResolution,
                        &iWidth,
                        &iHeight );

        // Create the new texture using the current resolution and pixel format
        hr = m_pd3dDevice->CreateTexture( iWidth, iHeight, 1, 0,
                                          D3DFMT_LE_LIN_YUY2, D3DPOOL_MANAGED, &m_pVideoTextures[ i ], NULL );

        if( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed to create texture!\n" );
            return E_FAIL;
        }

        // Clear texture
        D3DLOCKED_RECT Locked;
        if( FAILED( m_pVideoTextures[ i ]->LockRect( 0, &Locked, NULL, 0 ) ) )
            return E_FAIL;

        // Fill the texture with black
        DWORD* lpBits = ( DWORD* )Locked.pBits;
        for( UINT j = 0; j < Locked.Pitch * iHeight / 4; ++j ) *lpBits++ = 0x00800080;

        m_pVideoTextures[ i ]->UnlockRect( 0 );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeSnapshotTexture()
// Desc: Creates the m_pSnapshotTexture D3D textures using the current
//       resolution and pixel format settings.
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeSnapshotTexture()
{
    HRESULT hr;

    if( m_pSnapshotTexture != NULL )
    {
        m_pSnapshotTexture->Release();
        m_pSnapshotTexture = NULL;
    }

    // Retrieve the D3D texture size and format
    DWORD iWidth, iHeight;

    GetWidthHeight( m_SnapshotResolution,
                    &iWidth,
                    &iHeight );

    // Create the new texture using the current resolution and pixel format
    hr = m_pd3dDevice->CreateTexture( iWidth, iHeight, 1, 0,
                                      D3DFMT_LE_LIN_YUY2, D3DPOOL_MANAGED, &m_pSnapshotTexture, NULL );

    if( FAILED( hr ) )
    {
        ATG::DebugSpew( "Failed to create texture!\n" );
        return E_FAIL;
    }

    // Clear texture
    D3DLOCKED_RECT Locked;
    if( FAILED( m_pSnapshotTexture->LockRect( 0, &Locked, NULL, 0 ) ) )
        return E_FAIL;

    // Fill the texture with black
    DWORD* lpBits = ( DWORD* )Locked.pBits;
    for( UINT j = 0; j < Locked.Pitch * iHeight / 4; ++j ) *lpBits++ = 0x00800080;

    m_pSnapshotTexture->UnlockRect( 0 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeShaders()
// Desc: Compile the shaders and create the vertex declaration used to render the video
//       textures on screen.
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeShaders()
{
    // Compile vertex shader.
    ID3DXBuffer* pVertexShaderCode;
    ID3DXBuffer* pVertexErrorMsg;
    HRESULT hr = D3DXCompileShader( g_strVideoShaderHLSL,
                                    ( UINT )strlen( g_strVideoShaderHLSL ),
                                    NULL,
                                    NULL,
                                    "VideoVertex",
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
                            "VideoPixelYUV",
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
                                     &m_pVideoPixelShaderYUY2 );

    // Define the vertex elements and
    // Create a vertex declaration from the element descriptions.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVideoVertexDecl );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeVertexBuffer()
// Desc: Create the vertx buffer used to render the video on screen
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeVertexBuffer()
{
    // Create the vertex buffer. Here we are allocating enough memory
    // (from the default pool) to hold all our 3 custom vertices. 
    if( FAILED( m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( VIDEOFEEDVERTEX ),
                                                  D3DUSAGE_WRITEONLY,
                                                  NULL,
                                                  D3DPOOL_MANAGED,
                                                  &m_pVideoVB,
                                                  NULL ) ) )
        return E_FAIL;

    if( FAILED( m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( VIDEOFEEDVERTEX ),
                                                  D3DUSAGE_WRITEONLY,
                                                  NULL,
                                                  D3DPOOL_MANAGED,
                                                  &m_pSnapshotVB,
                                                  NULL ) ) )
        return E_FAIL;

    FLOAT fWidth = ( FLOAT )m_d3dpp.BackBufferWidth;
    FLOAT fHeight = ( FLOAT )m_d3dpp.BackBufferHeight;

    FLOAT fPictureWidth = fWidth / 2.f * 0.8f;
    FLOAT fPictureHeight = fWidth * 3.f / 4.f / 2.f * 0.8f;

    XVIDEO_MODE VideoMode;
    ZeroMemory( &VideoMode, sizeof( VideoMode ) );
    XGetVideoMode( &VideoMode );

    if( ( VideoMode.fIsWideScreen ) && ( fWidth == 640.f ) )
        fPictureWidth /= 1.333f;
    FLOAT fPictureX = fWidth / 2.f - fPictureWidth - 10.f;
    FLOAT fPictureY = ( fHeight - fPictureHeight ) / 2.f - 30.f;

    // Now we fill the vertex buffer. To do this, we need to Lock() the VB to
    // gain access to the vertices. This mechanism is required because the
    // vertex buffer may still be in use by the GPU. This can happen if the
    // CPU gets ahead of the GPU. The GPU could still be rendering the previous
    // frame.
    VIDEOFEEDVERTEX g_Vertices[] =
    {
        //  We "reverse" the x coordinates, so that the preview mirrors the user.
        { fPictureX,                 fPictureY, 0,  1, 0 },
        { fPictureX + fPictureWidth, fPictureY, 0,  0, 0 },
        { fPictureX,                 fPictureY + fPictureHeight, 0,  1, 1 },
        { fPictureX + fPictureWidth, fPictureY + fPictureHeight, 0,  0, 1 },
    };

    VIDEOFEEDVERTEX* pVertices;
    if( FAILED( m_pVideoVB->Lock( 0, 0, ( void** )&pVertices, 0 ) ) )
        return E_FAIL;
    memcpy( pVertices, g_Vertices, 4 * sizeof( VIDEOFEEDVERTEX ) );
    m_pVideoVB->Unlock();

    // Store these coordinates for use in the zoom selection.
    m_fPictureX1 = fPictureX;
    m_fPictureWidth = fPictureWidth;
    m_fPictureY1 = fPictureY;
    m_fPictureHeight = fPictureHeight;

    fPictureX = fWidth / 2.f + 10.f;
    // Fill in the VB for snapshot texture
    VIDEOFEEDVERTEX g_SnapshotVertices[] =
    {
        //  Unlike the live video feed, we do not mirror the image.  This is so that the text can be read properly.
        { fPictureX,                 fPictureY, 0,  0, 0 },
        { fPictureX + fPictureWidth, fPictureY, 0,  1, 0 },
        { fPictureX,                 fPictureY + fPictureHeight, 0,  0, 1 },
        { fPictureX + fPictureWidth, fPictureY + fPictureHeight, 0,  1, 1 },
    };

    if( FAILED( m_pSnapshotVB->Lock( 0, 0, ( void** )&pVertices, 0 ) ) )
        return E_FAIL;
    memcpy( pVertices, g_SnapshotVertices, 4 * sizeof( VIDEOFEEDVERTEX ) );
    m_pSnapshotVB->Unlock();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GetWidthHeight()
// Helper function to get Width and Height from XCAMRESOLUTION value
//--------------------------------------------------------------------------------------
VOID Sample::GetWidthHeight( XCAMRESOLUTION Resolution,
                             LPDWORD pdwWidth,
                             LPDWORD pdwHeight )
{
    assert( pdwWidth != NULL );
    assert( pdwHeight != NULL );
    switch( Resolution )
    {
        case XCAMRESOLUTION_160x120:
            *pdwWidth = 160;  *pdwHeight = 120; break;
        case XCAMRESOLUTION_176x144:
            *pdwWidth = 176;  *pdwHeight = 144; break;
        case XCAMRESOLUTION_320x240:
            *pdwWidth = 320;  *pdwHeight = 240; break;
        case XCAMRESOLUTION_352x288:
            *pdwWidth = 352;  *pdwHeight = 288; break;
        case XCAMRESOLUTION_640x480:
            *pdwWidth = 640;  *pdwHeight = 480; break;
        case XCAMRESOLUTION_960x720:
            *pdwWidth = 960;  *pdwHeight = 720; break;
        case XCAMRESOLUTION_1280x960:
            *pdwWidth = 1280; *pdwHeight = 960; break;
    }
}


//--------------------------------------------------------------------------------------
// Name: ResetParameters()
// Desc: Reset camera configuration
//--------------------------------------------------------------------------------------
VOID Sample::ResetParameters( VOID )
{
    for( INT i = 0; i < NUM_PARAMETERLIST; ++i )
    {
        g_ParameterList[ i ].iCurrent = g_ParameterList[ i ].iDefault;
    }

    if( XCAMDEVICESTATE_INITIALIZED == XCamGetStatus() )
    {
        LONG lParameter = 0;
        for( INT i = 0; i < NUM_PARAMETERLIST; ++i )
        {
            lParameter = g_ParameterList[ RESET_TABLE[ i ] ].iCurrent;
            DWORD status = XCamSetConfig( g_ParameterList[ RESET_TABLE[ i ] ].ControlID,
                                          lParameter,
                                          NULL );
            if( status != ERROR_SUCCESS )
            {
                ATG::DebugSpew( "ERROR: Could not set parameters! %d\n", RESET_TABLE[ i ] );
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: SetParameters()
// Desc: Set camera configuration
//--------------------------------------------------------------------------------------
VOID Sample::SetParameters( VOID )
{
    // Change parameter setting
    LONG lParameter;

    lParameter = g_ParameterList[ m_iCurrentParameter ].iCurrent;

    // Adjust auto parameters
    if( m_iCurrentParameter == PARAM_AUTOEXP )
    {
        g_ParameterList[ PARAM_EXP ].iCurrent = g_ParameterList[ PARAM_EXP ].iDefault;
        g_ParameterList[ PARAM_GAIN ].iCurrent = g_ParameterList[ PARAM_GAIN ].iDefault;
    }
    if( m_iCurrentParameter == PARAM_AUTOWB )
    {
        g_ParameterList[ PARAM_WB ].iCurrent = g_ParameterList[ PARAM_WB ].iDefault;
    }

    // Apply a parameter change to the camera
    DWORD status = XCamSetConfig( g_ParameterList[ m_iCurrentParameter ].ControlID,
                                  lParameter,
                                  NULL );
    if( status != ERROR_SUCCESS )
    {
        ATG::DebugSpew( "ERROR: Could not set parameters!\n" );
    }
}
