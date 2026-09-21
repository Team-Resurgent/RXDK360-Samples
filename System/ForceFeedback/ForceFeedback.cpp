//--------------------------------------------------------------------------------------
// ForceFeedback.cpp
//
// Sample that demonstrates the force feedback APIs in conjunction with the Xbox 360
// driving wheel. Without the driving wheel, only rumble capabilities are demonstrated.
// The sample consists of driving on a road with different roughness areas.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <xffb.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgHelp.h>


//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------

// Structure to hold vertex data
struct ROADVERTEX
{
    FLOAT   Position[3];
    FLOAT   Texture[2];
};

// Road related constants
const DWORD         NUM_ROAD_PRIMITIVES = 3000;  // must be divisible by 3
const FLOAT         ROAD_SEGMENT_LENGTH = 50.0f;

// Number of force feedback effects
const DWORD         NUM_FF_EFFECTS = 3;

const DWORD         INVALID_CONTROLLER = 0xffffffff;

//--------------------------------------------------------------------------------------
// Vertex shader for the road
//--------------------------------------------------------------------------------------
const CHAR*         g_strVertexShaderProgram =
    " float4x4 matWVP : register(c0);              \n"
    "                                              \n"
    " struct VS_IN                                 \n"
    "                                              \n"
    " {                                            \n"
    "     float4 ObjPos   : POSITION;              \n"
    "     float2 Texture  : TEXCOORD0;             \n"
    " };                                           \n"
    "                                              \n"
    " struct VS_OUT                                \n"
    " {                                            \n"
    "     float4 ProjPos  : POSITION;              \n"
    "     float2 Texture  : TEXCOORD0;             \n"
    " };                                           \n"
    "                                              \n"
    " VS_OUT main( VS_IN In )                      \n"
    " {                                            \n"
    "     VS_OUT Out;                              \n"
    "     Out.ProjPos = mul( matWVP, In.ObjPos );  \n"
    "     Out.Texture = In.Texture;                \n"
    "     return Out;                              \n"
    " }                                            \n";


//-------------------------------------------------------------------------------------
// Pixel shader
//-------------------------------------------------------------------------------------
const CHAR*         g_strPixelShaderProgram =
    " struct PS_IN                                     \n"
    " {                                                \n"
    "     float2 Texture : TEXCOORD0;                  \n"
    " };                                               \n"
    "                                                  \n"
    " sampler TextureSampler0 : register(s0);          \n"
    "                                                  \n"
    " float4 main( PS_IN In ) : COLOR                  \n"
    " {                                                \n"
    "     return tex2D( TextureSampler0, In.Texture ); \n"
    " }                                                \n";


//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Start race" },
    { ATG::HELP_LEFT_STICK, ATG::HELP_PLACEMENT_1, L"Steer" },
    { ATG::HELP_LEFT_TRIGGER, ATG::HELP_PLACEMENT_1, L"Brake" },
    { ATG::HELP_RIGHT_TRIGGER, ATG::HELP_PLACEMENT_1, L"Accelerate" },
    { ATG::HELP_DPAD, ATG::HELP_PLACEMENT_1, L"Steering sensitivity" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;    // Timer
    ATG::Font m_Font;     // Font for drawing text
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    ATG::PackedResource m_Resource; // Bundled textures in a packed resource

    // Transform matrices
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMVECTOR m_vEyePt;

    // Road object
    LPDIRECT3DTEXTURE9 m_pRoadTexture;
    LPDIRECT3DVERTEXBUFFER9 m_pRoadVB;
    LPDIRECT3DVERTEXDECLARATION9 m_pVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pPixelShader;

    XMVECTOR            m_vRoadVector[NUM_ROAD_PRIMITIVES/3];

    // Controller/Wheel
    DWORD m_dwControllerIndex;
    BOOL m_bFFController; // Indicates if a force feedback controller is present
    XINPUT_VIBRATION m_RumbleEffect;
    XINPUT_FF_EFFECT    m_FFEffect[NUM_FF_EFFECTS];
    DWORD m_dwLastEffectTime;
    FLOAT m_fSteeringSensitivity;
    DWORD m_dwLastMotorGain;

    // Vehicle speed & time
    FLOAT m_fCurrentSpeed;
    FLOAT m_fTotalTime;

    XOVERLAPPED m_OverlappedRumble; // Separate overlapped structures for rumble and 
    XOVERLAPPED m_OverlappedEffect; // effect async calls

    DWORD               InitializeForceFeedback();
    VOID                StartForceFeedbackEffect( DWORD dwEffectIndex );
    VOID                BuildRoad();


public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_dwControllerIndex = INVALID_CONTROLLER;
    m_fCurrentSpeed = 0.0f;
    m_fTotalTime = 0.0f;
    m_bFFController = FALSE;
    m_fSteeringSensitivity = 0.2f;
    m_dwLastMotorGain = 0;

    HRESULT hr;

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

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }

    m_pRoadTexture = m_Resource.GetTexture( "RoadTexture" );

    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Compile vertex shader
    hr = D3DXCompileShader( g_strVertexShaderProgram, ( UINT )strlen( g_strVertexShaderProgram ),
                            NULL, NULL, "main", "vs.3.0", 0,
                            &pShaderCode, &pErrorMsg, NULL );
    if( FAILED( hr ) )
    {
        ATG::FatalError( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
    }

    // Create vertex shader
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                      &m_pVertexShader );

    // Shader code is no longer required
    pShaderCode->Release();

    // Compile pixel shader
    hr = D3DXCompileShader( g_strPixelShaderProgram, ( UINT )strlen( g_strPixelShaderProgram ),
                            NULL, NULL, "main", "ps.3.0", 0,
                            &pShaderCode, &pErrorMsg, NULL );
    if( FAILED( hr ) )
    {
        ATG::FatalError( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
    }

    // Create pixel shader
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                     &m_pPixelShader );

    // Shader code no longer required
    pShaderCode->Release();

    // Define the vertex elements
    static const D3DVERTEXELEMENT9 VertexElements[3] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    // Create vertex shader decl
    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDeclaration ) ) )
    {
        ATG_PrintError( "Couldn't create vertex declaration.\n" );
        return hr;
    }

    // Create vertex buffer
    if( FAILED( hr = m_pd3dDevice->CreateVertexBuffer( NUM_ROAD_PRIMITIVES * 4 * sizeof( ROADVERTEX ),
                                                       D3DUSAGE_WRITEONLY,
                                                       0, 0, &m_pRoadVB, NULL ) ) )
    {
        ATG_PrintError( "Couldn't create vertex buffer.\n" );
        return hr;
    }

    // Create road vertex buffer
    BuildRoad();

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    m_matWorld = XMMatrixIdentity();
    m_vEyePt = XMVectorSet( 0.0f, 3.0f, 5.0f, 0.0f );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 1.0f, 10000.0f );

    // Setup force feedback effects
    ZeroMemory( m_FFEffect, NUM_FF_EFFECTS * sizeof( XINPUT_FF_EFFECT ) );

    // This effect is for on road driving
    m_FFEffect[0].EffectIndex = 1;          // Index must be greater than 0
    m_FFEffect[0].EffectType = XINPUT_FFEFFECT_CONDITION_FRICTION;
    m_FFEffect[0].Axes = XINPUT_FFAXIS_X;   // The wheel has only an X axis
    m_FFEffect[0].Gain = 255;               // Maximum gain
    m_FFEffect[0].Duration = 100;           // 100 ms
    m_FFEffect[0].StartDelay = 0;
    m_FFEffect[0].LoopCount = 1;
    m_FFEffect[0].ConditionForce[0].CenterOffset = 0;
    m_FFEffect[0].ConditionForce[0].PositiveCoefficient = 60;
    m_FFEffect[0].ConditionForce[0].NegativeCoefficient = 60;
    m_FFEffect[0].ConditionForce[0].PositiveSaturation = 255;
    m_FFEffect[0].ConditionForce[0].NegativeSaturation = 255;

    // This effect is for on grass driving
    m_FFEffect[1].EffectIndex = 2;          // Index must be greater than 0
    m_FFEffect[1].EffectType = XINPUT_FFEFFECT_RAMP;
    m_FFEffect[1].Axes = XINPUT_FFAXIS_X;   // The wheel has only an X axis
    m_FFEffect[1].Gain = 255;
    m_FFEffect[1].Duration = 100;
    m_FFEffect[1].StartDelay = 0;
    m_FFEffect[1].LoopCount = 1;
    m_FFEffect[1].RampForce.Start = 70;     // Define the edges of the ramp, these values range from
    m_FFEffect[1].RampForce.End = -70;      // -128 to 127 (signed byte)

    // This effect is for off road driving
    m_FFEffect[2].EffectIndex = 3;          // Index must be greater than 0
    m_FFEffect[2].EffectType = XINPUT_FFEFFECT_RAMP;
    m_FFEffect[2].Axes = XINPUT_FFAXIS_X;   // The wheel has only an X axis
    m_FFEffect[2].Gain = 255;
    m_FFEffect[2].Duration = 100;
    m_FFEffect[2].StartDelay = 0;
    m_FFEffect[2].LoopCount = 1;
    m_FFEffect[2].RampForce.Start = 120;    // Define the edges of the ramp, these values range from      
    m_FFEffect[2].RampForce.End = -120;     // -128 to 127 (signed byte)

    // Setup overlapped structures
    ZeroMemory( &m_OverlappedRumble, sizeof( XOVERLAPPED ) );
    m_OverlappedRumble.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    if( m_OverlappedRumble.hEvent == NULL )
        return E_FAIL;

    ZeroMemory( &m_OverlappedEffect, sizeof( XOVERLAPPED ) );
    m_OverlappedEffect.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    if( m_OverlappedEffect.hEvent == NULL )
        return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the elapsed time
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Set the view angle
    static FLOAT fYaw = 0.0f;

    if( m_dwControllerIndex != INVALID_CONTROLLER )
    {
        m_fTotalTime += fElapsedTime;

        // Calculate distance from road center (this is not an optimal algorithm)
        FLOAT fMinDistance = ROAD_SEGMENT_LENGTH * 10.0f;
        for( DWORD i = 0; i < ( ( NUM_ROAD_PRIMITIVES / 3 ) - 1 ); i++ )
        {
            XMVECTOR vLine = m_vRoadVector[i + 1] - m_vRoadVector[i];
            XMVECTOR vPoint = m_vEyePt - m_vRoadVector[i];
            FLOAT fDp1 = vLine.x * vPoint.x + vLine.z * vPoint.z;
            FLOAT fDp2 = vLine.x * vLine.x + vLine.z * vLine.z;

            if( fDp1 > 0.0f && fDp1 < fDp2 && fDp2 != 0.0f )
            {
                XMVECTOR vDist = XMVectorSet( fDp1 / fDp2, 0, fDp1 / fDp2, 0 );
                XMVECTOR vPt = m_vRoadVector[i] + vDist * vLine;
                FLOAT fDistance = XMVector3Length( vPt - m_vEyePt ).x;
                if( fDistance < fMinDistance )
                {
                    fMinDistance = fDistance;
                }
            }
        }

        // Apply different rumble settings and force feedback effects based on car's
        // distance from road center
        FLOAT fSpeedFactor = 0.0004f;
        FLOAT fMaxSpeed = 0.3f;
        DWORD dwEffectIndex = 0;

        // On the road
        if( fMinDistance < ( ROAD_SEGMENT_LENGTH / 2 ) )
        {
            m_fCurrentSpeed -= fElapsedTime * 0.03f; // small amount of drag
            dwEffectIndex = 0;
            fMaxSpeed = 0.3f;
        }
            // On the grass
        else if( fMinDistance >= ( ROAD_SEGMENT_LENGTH / 2 ) && fMinDistance < ROAD_SEGMENT_LENGTH )
        {
            m_fCurrentSpeed -= fElapsedTime * 0.09f; // more drag on grass
            fMaxSpeed = 0.05f;
            if( m_fCurrentSpeed > fMaxSpeed )
                fSpeedFactor = 0.0f;

            dwEffectIndex = 1;
        }
            // Off road
        else
        {
            m_fCurrentSpeed -= fElapsedTime * 0.09f; // more drag off road
            fMaxSpeed = 0.025f;
            if( m_fCurrentSpeed > fMaxSpeed )
                fSpeedFactor = 0.0f;

            dwEffectIndex = 2;
        }

        m_fCurrentSpeed -= fElapsedTime * 0.0004f * ( FLOAT )ATG::Input::m_Gamepads[m_dwControllerIndex].bLeftTrigger;
        m_fCurrentSpeed += fElapsedTime * fSpeedFactor * ( FLOAT )
            ATG::Input::m_Gamepads[m_dwControllerIndex].bRightTrigger;

        if( m_fCurrentSpeed < 0.0f )
            m_fCurrentSpeed = 0.0f;
        if( m_fCurrentSpeed > 0.25f )
            m_fCurrentSpeed = 0.25f;

        // Set rumble effect from vehicle speed
        m_RumbleEffect.wLeftMotorSpeed = ( WORD )( m_fCurrentSpeed * 32768.0f / fMaxSpeed );
        m_RumbleEffect.wRightMotorSpeed = 5000 + ( WORD )( dwEffectIndex * 20000.0f * m_fCurrentSpeed / fMaxSpeed );

        // Start rumble & force feedback effects - limiting the update frequency to the effect frequency
        if( ( GetTickCount() - m_dwLastEffectTime ) > 100 )
        {
            // Set device gain based on speed (set only if gain has changed)
            if( m_bFFController )
            {
                DWORD dwMotorGain = ( DWORD )( 31 * m_fCurrentSpeed / fMaxSpeed );
                if( dwMotorGain > 31 )
                    dwMotorGain = 31;

                if( dwMotorGain != m_dwLastMotorGain )
                {
                    XInputFFSetDeviceGain( m_dwControllerIndex, dwMotorGain, NULL );
                }

                m_dwLastMotorGain = dwMotorGain;
            }

            // Set rumble and FF effect
            StartForceFeedbackEffect( dwEffectIndex );
            m_dwLastEffectTime = GetTickCount();
        }

        fYaw += ATG::Input::m_Gamepads[m_dwControllerIndex].fX1 * m_fCurrentSpeed * m_fSteeringSensitivity * 0.1f;

        // Adjust steering sensitivity
        if( ATG::Input::m_Gamepads[m_dwControllerIndex].wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            m_fSteeringSensitivity -= 0.01f;
            if( m_fSteeringSensitivity < 0.01f )
                m_fSteeringSensitivity = 0.01f;
        }

        if( ATG::Input::m_Gamepads[m_dwControllerIndex].wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        {
            m_fSteeringSensitivity += 0.01f;
            if( m_fSteeringSensitivity > 1.0f )
                m_fSteeringSensitivity = 1.0f;
        }
    }
    else
    {
        // Select controller and initialize force feedback
        for( DWORD i = 0; i < XUSER_MAX_COUNT; i++ )
        {
            if( ATG::Input::m_Gamepads[i].wPressedButtons & XINPUT_GAMEPAD_A )
            {
                m_dwControllerIndex = i;
                if( InitializeForceFeedback() != ERROR_SUCCESS )
                {
                    ATG::DebugSpew( "Force feedback could not be initialized. Is wheel connected?\n" );
                }
                break;
            }
        }
    }

    XMVECTOR vLookatDir = {0};
    vLookatDir.x = sinf( fYaw );
    vLookatDir.y = 0;
    vLookatDir.z = cosf( fYaw );

    m_vEyePt += vLookatDir * m_fCurrentSpeed;
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( m_vEyePt, m_vEyePt + vLookatDir, vUpVec );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff3f6385, 0xffcebdad );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    // Initialize default device states at the start of the frame
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXANISOTROPY, 16 );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );

    // Set shader constants
    XMMATRIX matWVP = m_matWorld * m_matView * m_matProj;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

    // Render the road
    m_pd3dDevice->SetPixelShader( m_pPixelShader );
    m_pd3dDevice->SetTexture( 0, m_pRoadTexture );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDeclaration );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetStreamSource( 0, m_pRoadVB, 0, sizeof( ROADVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, NUM_ROAD_PRIMITIVES );

    // Render the HUD
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, 0xffffffff, L"ForceFeedback" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

    FLOAT fCenterX = ( FLOAT )( ATG::GetTitleSafeArea().x2 - ATG::GetTitleSafeArea().x1 ) / 2;

    if( m_dwControllerIndex == INVALID_CONTROLLER )
    {
        m_Font.DrawText( fCenterX, 100, 0xffffffff, L"Press " GLYPH_A_BUTTON L" to start.", ATGFONT_CENTER_X );
    }
    else
    {
        WCHAR strRender[1024];

        swprintf_s( strRender, L"Sensitivity %.2f", m_fSteeringSensitivity );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 40, 0xffffff00, strRender );

        swprintf_s( strRender, L"%d MPH", ( INT )( m_fCurrentSpeed * 500.0f ) );
        m_Font.SetScaleFactors( 2.0f, 2.0f );
        m_Font.DrawText( fCenterX, 0, 0xffffff00, strRender, ATGFONT_CENTER_X );

        swprintf_s( strRender, L"Time: %.1f s", m_fTotalTime );
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( fCenterX, 50, 0xffffff00, strRender, ATGFONT_CENTER_X );
    }

    m_Font.End();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeForceFeedback()
// Desc: Initialize rumble and force feedback (if supported) on currently selected 
// controller device. Upload force feedback effects to the device.
//--------------------------------------------------------------------------------------
DWORD Sample::InitializeForceFeedback()
{
    DWORD dwErr;

    XINPUT_CAPABILITIES Capabilities;

    dwErr = XInputGetCapabilities( m_dwControllerIndex, XINPUT_FLAG_GAMEPAD, &Capabilities );

    if( dwErr != ERROR_SUCCESS )
        return dwErr;

    // Check for a force feedback wheel. This sample specifically uses force feedback on the X axis
    // of a wheel.
    if( !( Capabilities.Flags & XINPUT_CAPS_FFB_SUPPORTED && Capabilities.SubType & XINPUT_DEVSUBTYPE_WHEEL ) )
        return ERROR_NOT_SUPPORTED;

    // Check for force feedback power state. Power must be applied for force feedback to work,
    // otherwise FF APIs will return an error. Rumble will work however without power.
    XINPUT_FF_DEVICE_INFO DeviceInfo;
    dwErr = XInputFFGetDeviceInfo( m_dwControllerIndex, &DeviceInfo );

    if( dwErr != ERROR_SUCCESS )
        return dwErr;

    if( !( DeviceInfo.DeviceState & XINPUT_FFFLAG_POWER_STATE ) )
        return ERROR_NOT_SUPPORTED;

    m_bFFController = TRUE;

    dwErr = XInputFFResetDevice( m_dwControllerIndex, NULL );

    if( dwErr != ERROR_SUCCESS )
        return dwErr;

    // Set device gain to maximum
    dwErr = XInputFFSetDeviceGain( m_dwControllerIndex, 31, NULL );

    if( dwErr != ERROR_SUCCESS )
        return dwErr;

    // Enable motors
    dwErr = XInputFFEnableMotors( m_dwControllerIndex, TRUE, NULL );

    if( dwErr != ERROR_SUCCESS )
        return dwErr;

    // Add all effects into the memory of the device. To update effects at a later time,
    // use XInputFFUpdateEffect
    dwErr = XInputFFSetEffect( m_dwControllerIndex, m_FFEffect, NUM_FF_EFFECTS, NULL );

    if( dwErr != ERROR_SUCCESS )
        return dwErr;

    m_dwLastEffectTime = GetTickCount();

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: StartForceFeedbackEffect
// Desc: Start executing a rumble and force feedback effect on the device.
//--------------------------------------------------------------------------------------
VOID Sample::StartForceFeedbackEffect( DWORD dwEffectIndex )
{
    if( !m_bFFController )
        return;

    DWORD dwErr;

    // Set rumble effect first
    if( XHasOverlappedIoCompleted( &m_OverlappedRumble ) )
    {
        dwErr = XInputFFSetRumble( m_dwControllerIndex, &m_RumbleEffect, &m_OverlappedRumble );

        // ERROR_NOT_READY may occur when the Guide is showing
        assert( dwErr == ERROR_IO_PENDING || dwErr == ERROR_NOT_READY );
    }

    // Set force feedback effect by starting current effect and stopping all other effects
    // The default effect is not stopped as XINPUT_FFOPERATION_NOCHANGE is passed in for
    // the dwNativeOperation argument
    if( XHasOverlappedIoCompleted( &m_OverlappedEffect ) )
    {
        XINPUT_FF_EFFECT_OPERATION EffectOperation;

        EffectOperation.EffectIndex = dwEffectIndex + 1;
        EffectOperation.Operation = XINPUT_FFOPERATION_SOLO;

        dwErr = XInputFFEffectOperation( m_dwControllerIndex,
                                         &EffectOperation,               // List of effects
                                         1,                              // Number of effects
                                         XINPUT_FFOPERATION_NOCHANGE,    // Do not change the default effect
                                         &m_OverlappedEffect );

        // ERROR_NOT_READY may occur when the Guide is showing
        assert( dwErr == ERROR_IO_PENDING || dwErr == ERROR_NOT_READY );
    }

}


//--------------------------------------------------------------------------------------
// Name: BuildRoad()
// Desc: Create road vertex buffer from randomly oriented road segments.
//--------------------------------------------------------------------------------------
VOID Sample::BuildRoad()
{
    srand( GetTickCount() );

    // Each road segments has 3 quads, the left and right large quads and the center
    // road quad
    XMVECTOR vPos = {0};
    FLOAT fDirection = 0;
    FLOAT fDirectionRate = 0.0f;
    ROADVERTEX* pVertices;
    m_pRoadVB->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    for( DWORD i = 0; i < NUM_ROAD_PRIMITIVES; i += 3 )
    {
        DWORD dwVertexIndexLeft = i * 4;
        DWORD dwVertexIndexMiddle = ( i + 1 ) * 4;
        DWORD dwVertexIndexRight = ( i + 2 ) * 4;

        // Calculate vector along road directions
        XMVECTOR vNextPos = {0};

        vNextPos.x = vPos.x + ROAD_SEGMENT_LENGTH * sin( fDirection );
        vNextPos.z = vPos.z + ROAD_SEGMENT_LENGTH * cos( fDirection );

        XMVECTOR vNormal;
        vNormal.x = -( vNextPos.z - vPos.z );
        vNormal.z = ( vNextPos.x - vPos.x );
        FLOAT fLength = sqrt( vNormal.x * vNormal.x + vNormal.z * vNormal.z );
        vNormal.x /= fLength;
        vNormal.z /= fLength;

        m_vRoadVector[i / 3] = vPos;

        if( i != 0 )
        {
            // Copy vertices from last segment
            DWORD dwLastIndexLeft = ( i - 3 ) * 4;
            DWORD dwLastIndexMiddle = ( i - 2 ) * 4;
            DWORD dwLastIndexRight = ( i - 1 ) * 4;

            // Texture coordinates for the left and right quads are sampled from the main
            // texture's grass region
            pVertices[dwVertexIndexLeft] = pVertices[dwLastIndexLeft + 3];
            pVertices[dwVertexIndexLeft].Texture[0] = 0.0f;
            pVertices[dwVertexIndexLeft].Texture[1] = 0.04f;
            pVertices[dwVertexIndexLeft + 1] = pVertices[dwLastIndexLeft + 2];
            pVertices[dwVertexIndexLeft + 1].Texture[0] = 0.2f;
            pVertices[dwVertexIndexLeft + 1].Texture[1] = 0.04f;

            // Texture coordinates for the middle (road) quad, uses the full road texture
            pVertices[dwVertexIndexMiddle] = pVertices[dwLastIndexMiddle + 3];
            pVertices[dwVertexIndexMiddle].Texture[0] = 0.0f;
            pVertices[dwVertexIndexMiddle].Texture[1] = 1.0f;
            pVertices[dwVertexIndexMiddle + 1] = pVertices[dwLastIndexMiddle + 2];
            pVertices[dwVertexIndexMiddle + 1].Texture[0] = 1.0f;
            pVertices[dwVertexIndexMiddle + 1].Texture[1] = 1.0f;

            pVertices[dwVertexIndexRight] = pVertices[dwLastIndexRight + 3];
            pVertices[dwVertexIndexRight].Texture[0] = 0.0f;
            pVertices[dwVertexIndexRight].Texture[1] = 0.04f;
            pVertices[dwVertexIndexRight + 1] = pVertices[dwLastIndexRight + 2];
            pVertices[dwVertexIndexRight + 1].Texture[0] = 0.2f;
            pVertices[dwVertexIndexRight + 1].Texture[1] = 0.04f;
        }
        else
        {
            // First segment, generate first row of vertices
            pVertices[dwVertexIndexLeft].Position[0] = vPos.x - ( 10.0f * ROAD_SEGMENT_LENGTH * vNormal.x );
            pVertices[dwVertexIndexLeft].Position[1] = 0;
            pVertices[dwVertexIndexLeft].Position[2] = vPos.z - ( 10.0f * ROAD_SEGMENT_LENGTH * vNormal.z );
            pVertices[dwVertexIndexLeft].Texture[0] = 0.0f;
            pVertices[dwVertexIndexLeft].Texture[1] = 0.04f;
            pVertices[dwVertexIndexLeft + 1].Position[0] = vPos.x - ( ROAD_SEGMENT_LENGTH * vNormal.x );
            pVertices[dwVertexIndexLeft + 1].Position[1] = 0;
            pVertices[dwVertexIndexLeft + 1].Position[2] = vPos.z - ( ROAD_SEGMENT_LENGTH * vNormal.z );
            pVertices[dwVertexIndexLeft + 1].Texture[0] = 0.2f;
            pVertices[dwVertexIndexLeft + 1].Texture[1] = 0.04f;

            pVertices[dwVertexIndexMiddle].Position[0] = vPos.x - ( ROAD_SEGMENT_LENGTH * vNormal.x );
            pVertices[dwVertexIndexMiddle].Position[1] = 0;
            pVertices[dwVertexIndexMiddle].Position[2] = vPos.z - ( ROAD_SEGMENT_LENGTH * vNormal.z );
            pVertices[dwVertexIndexMiddle].Texture[0] = 0.0f;
            pVertices[dwVertexIndexMiddle].Texture[1] = 1.0f;
            pVertices[dwVertexIndexMiddle + 1].Position[0] = vPos.x + ( ROAD_SEGMENT_LENGTH * vNormal.x );
            pVertices[dwVertexIndexMiddle + 1].Position[1] = 0;
            pVertices[dwVertexIndexMiddle + 1].Position[2] = vPos.z + ( ROAD_SEGMENT_LENGTH * vNormal.z );
            pVertices[dwVertexIndexMiddle + 1].Texture[0] = 1.0f;
            pVertices[dwVertexIndexMiddle + 1].Texture[1] = 1.0f;

            pVertices[dwVertexIndexRight].Position[0] = vPos.x + ( ROAD_SEGMENT_LENGTH * vNormal.x );
            pVertices[dwVertexIndexRight].Position[1] = 0;
            pVertices[dwVertexIndexRight].Position[2] = vPos.z + ( ROAD_SEGMENT_LENGTH * vNormal.z );
            pVertices[dwVertexIndexRight].Texture[0] = 0.0f;
            pVertices[dwVertexIndexRight].Texture[1] = 0.04f;
            pVertices[dwVertexIndexRight + 1].Position[0] = vPos.x + ( 10.0f * ROAD_SEGMENT_LENGTH * vNormal.x );
            pVertices[dwVertexIndexRight + 1].Position[1] = 0;
            pVertices[dwVertexIndexRight + 1].Position[2] = vPos.z + ( 10.0f * ROAD_SEGMENT_LENGTH * vNormal.z );
            pVertices[dwVertexIndexRight + 1].Texture[0] = 0.2f;
            pVertices[dwVertexIndexRight + 1].Texture[1] = 0.04f;
        }

        // Add vertices to the last row of the segment
        pVertices[dwVertexIndexLeft + 2].Position[0] = vNextPos.x - ( ROAD_SEGMENT_LENGTH * vNormal.x );
        pVertices[dwVertexIndexLeft + 2].Position[1] = 0;
        pVertices[dwVertexIndexLeft + 2].Position[2] = vNextPos.z - ( ROAD_SEGMENT_LENGTH * vNormal.z );
        pVertices[dwVertexIndexLeft + 2].Texture[0] = 0.2f;
        pVertices[dwVertexIndexLeft + 2].Texture[1] = 0.0f;
        pVertices[dwVertexIndexLeft + 3].Position[0] = vNextPos.x - ( 10.0f * ROAD_SEGMENT_LENGTH * vNormal.x );
        pVertices[dwVertexIndexLeft + 3].Position[1] = 0;
        pVertices[dwVertexIndexLeft + 3].Position[2] = vNextPos.z - ( 10.0f * ROAD_SEGMENT_LENGTH * vNormal.z );
        pVertices[dwVertexIndexLeft + 3].Texture[0] = 0.0f;
        pVertices[dwVertexIndexLeft + 3].Texture[1] = 0.0f;

        pVertices[dwVertexIndexMiddle + 2].Position[0] = vNextPos.x + ( ROAD_SEGMENT_LENGTH * vNormal.x );
        pVertices[dwVertexIndexMiddle + 2].Position[1] = 0;
        pVertices[dwVertexIndexMiddle + 2].Position[2] = vNextPos.z + ( ROAD_SEGMENT_LENGTH * vNormal.z );
        pVertices[dwVertexIndexMiddle + 2].Texture[0] = 1.0f;
        pVertices[dwVertexIndexMiddle + 2].Texture[1] = 0.0f;
        pVertices[dwVertexIndexMiddle + 3].Position[0] = vNextPos.x - ( ROAD_SEGMENT_LENGTH * vNormal.x );
        pVertices[dwVertexIndexMiddle + 3].Position[1] = 0;
        pVertices[dwVertexIndexMiddle + 3].Position[2] = vNextPos.z - ( ROAD_SEGMENT_LENGTH * vNormal.z );
        pVertices[dwVertexIndexMiddle + 3].Texture[0] = 0.0f;
        pVertices[dwVertexIndexMiddle + 3].Texture[1] = 0.0f;

        pVertices[dwVertexIndexRight + 2].Position[0] = vNextPos.x + ( 10.0f * ROAD_SEGMENT_LENGTH * vNormal.x );
        pVertices[dwVertexIndexRight + 2].Position[1] = 0;
        pVertices[dwVertexIndexRight + 2].Position[2] = vNextPos.z + ( 10.0f * ROAD_SEGMENT_LENGTH * vNormal.z );
        pVertices[dwVertexIndexRight + 2].Texture[0] = 0.2f;
        pVertices[dwVertexIndexRight + 2].Texture[1] = 0.0f;
        pVertices[dwVertexIndexRight + 3].Position[0] = vNextPos.x + ( ROAD_SEGMENT_LENGTH * vNormal.x );
        pVertices[dwVertexIndexRight + 3].Position[1] = 0;
        pVertices[dwVertexIndexRight + 3].Position[2] = vNextPos.z + ( ROAD_SEGMENT_LENGTH * vNormal.z );
        pVertices[dwVertexIndexRight + 3].Texture[0] = 0.0f;
        pVertices[dwVertexIndexRight + 3].Texture[1] = 0.0f;

        vPos = vNextPos;

        // Randomly change the curvature of the road
        DWORD dwRand = rand() % 10;
        if( dwRand == 0 )
            fDirectionRate = -0.2f;
        if( dwRand == 1 )
            fDirectionRate = 0.2f;
        if( dwRand == 2 )
            fDirectionRate = -0.1f;
        if( dwRand == 3 )
            fDirectionRate = 0.1f;
        if( dwRand > 8 )
            fDirectionRate = 0.0f;

        if( fDirection < -( XM_PI / 2 ) )
            fDirection = -( XM_PI / 2 );

        if( fDirection > ( XM_PI / 2 ) )
            fDirection = ( XM_PI / 2 );

        fDirection += fDirectionRate;
    }
    m_pRoadVB->Unlock();
}
