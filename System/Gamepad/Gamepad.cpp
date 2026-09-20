//--------------------------------------------------------------------------------------
// Gamepad.cpp
//
// Tool to experiment with all things related to an Xbox 360 gamepad
// controller
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgMesh.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "GamepadMesh.h"


extern LPDIRECT3DDEVICE9    g_pd3dDevice;

static const XMFLOAT4       vFill( 0.0f, 0.0f, 0.0f, 0.25f );
static const XMFLOAT4       vEmpty( 0.0f, 0.0f, 0.0f, 0.0f );
static const XMFLOAT4       vSolidBlack( 0.0f, 0.0f, 0.0f, 1.0f );
static const XMFLOAT4       vSolidRed( 1.0f, 0.0f, 0.0f, 1.0f );
static const XMFLOAT4       vSolidGreen( 0.0f, 1.0f, 0.0f, 1.0f );
static const XMFLOAT4       vSolidGrey( 0.25f, 0.25f, 0.25f, 1.0f );
static const XMFLOAT4       vAlphaGrey( 0.5f, 0.5f, 0.5f, 0.5f );
static const XMFLOAT4       vSolidWhite( 1.0f, 1.0f, 1.0f, 1.0f );


//-----------------------------------------------------------------------------
// Name: class Sample
// Desc: Application class.
//-----------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // Valid app states
    enum APPSTATE
    {
        APPSTATE_CONTROLTEST=0,
        APPSTATE_VIBRATIONTEST,
        APPSTATE_DEADZONECALIBRATION,
        APPSTATE_BUTTONQUANTIZATION,
        APPSTATE_MEMORYUNITDETECTION,
        APPSTATE_MAX,
    };

    // General application members
    APPSTATE m_AppState;         // State of the app
    ATG::Timer m_Timer;            // Timer for the app
    ATG::PackedResource m_xprResource;      // Packed resources for the app
    ATG::Font m_Font16;           // 16-point font class
    ATG::Font m_Font12;           // 12-point font class

    // Active gamepad
    ATG::GAMEPAD* m_pGamepad;

    // Geometry
    GamepadMesh m_GamepadMesh;     // Geometry for the Japan gamepad

    // Vibration motor values
    FLOAT m_fLeftMotorSpeed;
    FLOAT m_fRightMotorSpeed;

    // Deadzone calibration page
    FLOAT m_fDeadZone;

    // Control quantization page
    BYTE* m_pQuantizedThumbStickValues;
    BYTE* m_pQuantizedButtonValues;

    LPDIRECT3DVERTEXDECLARATION9 m_pScreenspaceDecl;
    LPDIRECT3DVERTEXDECLARATION9 m_pScreenspaceTexcoordDecl;

    LPDIRECT3DVERTEXSHADER9 m_pScreenspaceVS;
    LPDIRECT3DVERTEXSHADER9 m_pScreenspaceTexcoordVS;
    LPDIRECT3DPIXELSHADER9 m_pConstantColorPS;
    LPDIRECT3DPIXELSHADER9 m_pTexturePS;

    // Internal members
    VOID            ShowTexture( LPDIRECT3DTEXTURE9 pTexture );
    HRESULT         DrawBox( LONG x1, LONG y1, LONG x2, LONG y2,
                             const XMFLOAT4& vFillColor,
                             const XMFLOAT4& vwOutlineColor );

    HRESULT         RenderControlTestPage();
    HRESULT         RenderControlTestPageUI();

    HRESULT         RenderVibrationTestPage();
    HRESULT         RenderVibrationTestPageUI();

    HRESULT         RenderDeadZoneCalibrationPage();
    HRESULT         RenderDeadZoneCalibrationPageUI();

    HRESULT         RenderButtonQuantizationPage();
    HRESULT         RenderButtonQuantizationPageUI();

private:
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
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_AppState = APPSTATE_CONTROLTEST;
    m_fDeadZone = 0.24f;  // Set default deadzone to 24%
    m_fLeftMotorSpeed = 0.0f;
    m_fRightMotorSpeed = 0.0f;

    // Quantized control values
    m_pQuantizedThumbStickValues = new BYTE[256];
    ZeroMemory( m_pQuantizedThumbStickValues, 256 );

    m_pQuantizedButtonValues = new BYTE[256];
    ZeroMemory( m_pQuantizedButtonValues, 256 );

    // Create the fonts
    if( FAILED( m_Font16.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( m_Font12.Create( "game:\\Media\\Fonts\\Arial_12.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font12.SetWindow( ATG::GetTitleSafeArea() );
    m_Font16.SetWindow( ATG::GetTitleSafeArea() );

    // Create the resources
    if( FAILED( m_xprResource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Load the gamepad mesh
    if( FAILED( m_GamepadMesh.Create( "game:\\Media\\Meshes\\Gamepad.xbg", &m_xprResource ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Load shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\Screenspace.xvu", &m_pScreenspaceVS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ScreenspaceTexcoord.xvu",
                                       &m_pScreenspaceTexcoordVS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ConstantColor.xpu", &m_pConstantColorPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Texture.xpu", &m_pTexturePS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    static const D3DVERTEXELEMENT9 elems1[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END(),
    };
    if( FAILED( m_pd3dDevice->CreateVertexDeclaration( elems1, &m_pScreenspaceDecl ) ) )
        return E_FAIL;

    static const D3DVERTEXELEMENT9 elems2[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END(),
    };
    if( FAILED( m_pd3dDevice->CreateVertexDeclaration( elems2, &m_pScreenspaceTexcoordDecl ) ) )
        return E_FAIL;

    // Misc render states
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    //    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE,   TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Set up view matrix
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -250.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_GamepadMesh.m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );

    // Set up proj matrix
    m_GamepadMesh.m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, 4.0f / 3.0f, 1.0f, 1000.0f );

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

    // Get the current gamepad status
    m_pGamepad = ATG::Input::GetMergedInput();

    // Move to next app state when user presses BACK and START together
    BOOL bStartAndBackButtonsPushed = FALSE;

    if( ( m_pGamepad->wButtons & XINPUT_GAMEPAD_START ) &&
        ( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK ) )
        bStartAndBackButtonsPushed = TRUE;

    if( ( m_pGamepad->wButtons & XINPUT_GAMEPAD_BACK ) &&
        ( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_START ) )
        bStartAndBackButtonsPushed = TRUE;

    static XMMATRIX g_matWorld( 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 );

    if( bStartAndBackButtonsPushed )
    {
        switch( m_AppState )
        {
            case APPSTATE_CONTROLTEST:
                m_AppState = APPSTATE_VIBRATIONTEST; break;
            case APPSTATE_VIBRATIONTEST:
                m_AppState = APPSTATE_DEADZONECALIBRATION; break;
            case APPSTATE_DEADZONECALIBRATION:
                m_AppState = APPSTATE_BUTTONQUANTIZATION; break;
            case APPSTATE_BUTTONQUANTIZATION:
                m_AppState = APPSTATE_CONTROLTEST; break;
        }

        g_matWorld = XMMatrixIdentity();
        m_fLeftMotorSpeed = 0.0f;
        m_fRightMotorSpeed = 0.0f;
    }

    // Handle the control test page
    if( m_AppState == APPSTATE_CONTROLTEST )
    {
        // Perform object rotation
        XMMATRIX matRotate1, matRotate2;
        static FLOAT fXRotate = 0.0f;
        static FLOAT fYRotate = -XM_PI / 2;
        fXRotate -= m_pGamepad->fX1 * fElapsedTime * XM_PI * 0.5f;
        fYRotate += m_pGamepad->fY1 * fElapsedTime * XM_PI * 0.5f;
        matRotate1 = XMMatrixRotationRollPitchYaw( fYRotate, 0.0f, 0.0f );
        matRotate2 = XMMatrixRotationRollPitchYaw( 0.0f, fXRotate, 0.0f );
        g_matWorld = XMMatrixMultiply( matRotate1, matRotate2 );
        m_GamepadMesh.m_matWorld = g_matWorld;

        m_GamepadMesh.Update( m_pGamepad );
    }

    // Handle the vibration test page
    if( m_AppState == APPSTATE_VIBRATIONTEST )
    {
        m_fLeftMotorSpeed = m_pGamepad->bLeftTrigger / 255.0f;
        m_fRightMotorSpeed = m_pGamepad->bRightTrigger / 255.0f;
    }
    else
    {
        // Make sure the motors will be turned off for all other pages
        m_fLeftMotorSpeed = 0.0f;
        m_fRightMotorSpeed = 0.0f;
    }

    // Handle the deadzone calibration page
    if( m_AppState == APPSTATE_DEADZONECALIBRATION )
    {
        // Adjust the deadzone
        if( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            m_fDeadZone += 0.01f;
        if( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
            m_fDeadZone -= 0.01f;
        m_fDeadZone = min( 1.0f, max( 0.0f, m_fDeadZone ) );

        // Override the gamepad's deadzone pseudo-constants
        m_pGamepad->LEFT_THUMB_DEADZONE = ( SHORT )( m_fDeadZone * FLOAT( 0x7FFF ) );
        m_pGamepad->RIGHT_THUMB_DEADZONE = ( SHORT )( m_fDeadZone * FLOAT( 0x7FFF ) );
    }

    // Handle the button quantization page
    if( m_AppState == APPSTATE_BUTTONQUANTIZATION )
    {
        // Reset the recorded values
        if( m_pGamepad->wButtons & XINPUT_GAMEPAD_START )
        {
            ZeroMemory( m_pQuantizedThumbStickValues, 256 );
            ZeroMemory( m_pQuantizedButtonValues, 256 );
        }

        // Record quantized thumbstick values
        m_pQuantizedThumbStickValues[ ( 32768 + m_pGamepad->sThumbLX ) >> 8 ] |= 0x01;
        m_pQuantizedThumbStickValues[ ( 32768 + m_pGamepad->sThumbLY ) >> 8 ] |= 0x02;
        m_pQuantizedThumbStickValues[ ( 32768 + m_pGamepad->sThumbRX ) >> 8 ] |= 0x04;
        m_pQuantizedThumbStickValues[ ( 32768 + m_pGamepad->sThumbRY ) >> 8 ] |= 0x08;

        // Record quantized button values
        m_pQuantizedButtonValues[ m_pGamepad->bLeftTrigger  ] |= 0x01;
        m_pQuantizedButtonValues[ m_pGamepad->bRightTrigger ] |= 0x02;
    }

    // Set the vibration motors
    {
        // Only alter the motor values if they changed
        WORD wLeftMotorSpeed = WORD( m_fLeftMotorSpeed * 65535.0f );
        WORD wRightMotorSpeed = WORD( m_fRightMotorSpeed * 65535.0f );

        static XINPUT_VIBRATION Vibration = {0};
        if( Vibration.wLeftMotorSpeed != wLeftMotorSpeed ||
            Vibration.wRightMotorSpeed != wRightMotorSpeed )
        {
            Vibration.wLeftMotorSpeed = wLeftMotorSpeed;
            Vibration.wRightMotorSpeed = wRightMotorSpeed;
            XInputSetState( m_pGamepad->dwUserIndex, &Vibration );

            // To be safe, in case the active gamepad for the sample changes,
            // turn off rumble on other controllers
            for( DWORD i = 0; i < XUSER_MAX_COUNT; i++ )
            {
                if( i != m_pGamepad->dwUserIndex )
                {
                    XINPUT_VIBRATION VibrationOff = {0};
                    XInputSetState( i, &VibrationOff );
                }
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawBox()
// Desc: Graphics helper function
//--------------------------------------------------------------------------------------
HRESULT Sample::DrawBox( LONG x1, LONG y1, LONG x2, LONG y2,
                         const XMFLOAT4& vFillColor,
                         const XMFLOAT4& vOutlineColor )
{
    XMFLOAT4 v[5];
    v[0] = XMFLOAT4( x1 - 0.5f, y1 - 0.5f, 0, 1 );
    v[1] = XMFLOAT4( x2 - 0.5f, y1 - 0.5f, 0, 1 );
    v[2] = XMFLOAT4( x2 - 0.5f, y2 - 0.5f, 0, 1 );
    v[3] = XMFLOAT4( x1 - 0.5f, y2 - 0.5f, 0, 1 );
    v[4] = XMFLOAT4( x1 - 0.5f, y1 - 0.5f, 0, 1 );

    // Set Pixel shader and renderstate
    m_pd3dDevice->SetPixelShader( m_pConstantColorPS );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    // Set Vertex shader and declaration
    m_pd3dDevice->SetVertexShader( m_pScreenspaceVS );
    m_pd3dDevice->SetVertexDeclaration( m_pScreenspaceDecl );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );

    // Render the box
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vFillColor, 1 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, v, sizeof( v[0] ) );

    // Render the lines
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vOutlineColor, 1 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, 4, v, sizeof( v[0] ) );

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderControlTestPage()
// Desc: Renders the page for testing the controls
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderControlTestPage()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Render the gamepad
    m_GamepadMesh.RenderGamepad();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderControlTestPageUI()
// Desc: Renders the page for testing the controls
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderControlTestPageUI()
{
    D3DRECT rc = m_Font16.m_rcWindow;

    // Draw header
    DrawBox( rc.x1, rc.y1, rc.x2, rc.y1 + 43, vFill, vSolidBlack );

    m_Font16.Begin();
    m_Font16.DrawText( 5, 0, 0xffffffff, L"Gamepad Tool", ATGFONT_LEFT );
    m_Font16.DrawText( -5, 0, 0xffffffff, L"Control Test Page", ATGFONT_RIGHT );

    // Display text indicating controller type
    WCHAR strControllerType[64];
    switch( ATG::Input::m_Gamepads[0].caps.SubType )
    {
        case XINPUT_DEVSUBTYPE_GAMEPAD:
            swprintf_s( strControllerType, L"Gamepad" ); break;
        case XINPUT_DEVSUBTYPE_WHEEL:
            swprintf_s( strControllerType, L"Wheel" ); break;
        case XINPUT_DEVSUBTYPE_ARCADE_STICK:
            swprintf_s( strControllerType, L"Arcade Stick" ); break;
        case XINPUT_DEVSUBTYPE_FLIGHT_STICK:
            swprintf_s( strControllerType, L"Flight Stick" ); break;
        case XINPUT_DEVSUBTYPE_DANCEPAD:
            swprintf_s( strControllerType, L"Dance Pad" ); break;
        case XINPUT_DEVSUBTYPE_GUITAR:
            swprintf_s( strControllerType, L"Standard Guitar" ); break;
        case XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE:
            swprintf_s( strControllerType, L"Alternate Guitar" ); break;
        case XINPUT_DEVSUBTYPE_DRUM_KIT:
            swprintf_s( strControllerType, L"Drum Kit" ); break;
        case 0x09:
            swprintf_s( strControllerType, L"Stage Kit" ); break;
        default:
            swprintf_s( strControllerType, L"Subtype 0x%0.2X", ATG::Input::m_Gamepads[0].caps.SubType ); break;
    }

    // Also display controller capabilities
    if( ATG::Input::m_Gamepads[0].caps.Flags )
    {
        BOOL bFirstCapsType = FALSE;

        wcscat_s( strControllerType, L" (" );

        if( ATG::Input::m_Gamepads[0].caps.Flags & XINPUT_CAPS_FFB_SUPPORTED )
        {
            wcscat_s( strControllerType, L"FFB" );
            bFirstCapsType = TRUE;
        }

        if( ATG::Input::m_Gamepads[0].caps.Flags & XINPUT_CAPS_WIRELESS )
        {
            if( bFirstCapsType )
            {
                wcscat_s( strControllerType, L" " );
            }

            wcscat_s( strControllerType, L"Wireless" );
            bFirstCapsType = TRUE;
        }

        if( ATG::Input::m_Gamepads[0].caps.Flags & XINPUT_CAPS_VOICE_SUPPORTED )
        {
            if( bFirstCapsType )
            {
                wcscat_s( strControllerType, L" " );
            }

            wcscat_s( strControllerType, L"Voice" );
            bFirstCapsType = TRUE;
        }

        if( ATG::Input::m_Gamepads[0].caps.Flags & XINPUT_CAPS_PMD_SUPPORTED )
        {
            if( bFirstCapsType )
            {
                wcscat_s( strControllerType, L" " );
            }

            wcscat_s( strControllerType, L"PMD" );
            bFirstCapsType = TRUE;
        }

        wcscat_s( strControllerType, L")" );
    }

    m_Font16.DrawText( ( rc.x2 - rc.x1 ) / 2.0f, -20, 0xffffffff, strControllerType, ATGFONT_CENTER_X );
    m_Font16.End();

    // Draw options
    m_Font12.Begin();

    m_Font12.DrawText( 0, 25, 0xff808080, L"Press START and BACK for next page", ATGFONT_RIGHT );

    WCHAR strBuffer[40];

    swprintf_s( strBuffer, L"LeftTrigger = %d", m_pGamepad->bLeftTrigger );
    m_Font12.DrawText( 76, 45, m_pGamepad->bLeftTrigger ? 0xffffff00: 0x80ffffff, strBuffer );
    m_Font12.DrawText( 76, 66, g_ControlActive[CONTROL_LEFT_SHOULDER] ? 0xffffff00: 0x80ffffff, L"Left Shoulder" );

    swprintf_s( strBuffer, L"RightTrigger = %d", m_pGamepad->bRightTrigger );
    m_Font12.DrawText( -176, 45, m_pGamepad->bRightTrigger ? 0xffffff00: 0x80ffffff, strBuffer );
    m_Font12.DrawText( -176, 66, g_ControlActive[CONTROL_RIGHT_SHOULDER] ? 0xffffff00: 0x80ffffff, L"Right Shoulder" );

    swprintf_s( strBuffer, L"LeftStick.x = %d", m_pGamepad->sThumbLX );
    m_Font12.DrawText( 0, 84, m_pGamepad->fX1 != 0.0f ? 0xffffff00: 0x80ffffff, strBuffer );
    swprintf_s( strBuffer, L"LeftStick.y = %d", m_pGamepad->sThumbLY );
    m_Font12.DrawText( 0, 102, m_pGamepad->fY1 != 0.0f ? 0xffffff00: 0x80ffffff, strBuffer );
    swprintf_s( strBuffer, L"LeftThumb" );
    m_Font12.DrawText( 0, 120, m_pGamepad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB ? 0xffffff00: 0x80ffffff, strBuffer );

    m_Font12.DrawText( -86, 84, g_ControlActive[CONTROL_Y_BUTTON] ? 0xffffff00: 0x80ffffff, L"Y", ATGFONT_CENTER_X );
    m_Font12.DrawText( -106, 102, g_ControlActive[CONTROL_X_BUTTON] ? 0xffffff00: 0x80ffffff, L"X", ATGFONT_CENTER_X );
    m_Font12.DrawText( -66, 102, g_ControlActive[CONTROL_B_BUTTON] ? 0xffffff00: 0x80ffffff, L"B", ATGFONT_CENTER_X );
    m_Font12.DrawText( -86, 120, g_ControlActive[CONTROL_A_BUTTON] ? 0xffffff00: 0x80ffffff, L"A", ATGFONT_CENTER_X );

    m_Font12.DrawText( 46, -54, m_pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP    ? 0xffffff00: 0x80ffffff, L"Up",
                       ATGFONT_CENTER_X );
    m_Font12.DrawText( 21, -36, m_pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT  ? 0xffffff00: 0x80ffffff, L"Left",
                       ATGFONT_CENTER_X );
    m_Font12.DrawText( 71, -36, m_pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ? 0xffffff00: 0x80ffffff, L"Right",
                       ATGFONT_CENTER_X );
    m_Font12.DrawText( 46, -18, m_pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN  ? 0xffffff00: 0x80ffffff, L"Down",
                       ATGFONT_CENTER_X );

    FLOAT fCenterX = ( rc.x2 - rc.x1 ) / 2.0f;
    m_Font12.DrawText( fCenterX, -45, g_ControlActive[CONTROL_BACK_BUTTON]  ? 0xffffff00: 0x80ffffff, L"Back ",
                       ATGFONT_RIGHT );
    m_Font12.DrawText( fCenterX, -45, g_ControlActive[CONTROL_START_BUTTON] ? 0xffffff00: 0x80ffffff, L" Start",
                       ATGFONT_LEFT );

    swprintf_s( strBuffer, L"RightStick.x = %d", m_pGamepad->sThumbRX );
    m_Font12.DrawText( -146, -54, m_pGamepad->fX2 != 0.0f ? 0xffffff00: 0x80ffffff, strBuffer );
    swprintf_s( strBuffer, L"RightStick.y = %d", m_pGamepad->sThumbRY );
    m_Font12.DrawText( -146, -36, m_pGamepad->fY2 != 0.0f ? 0xffffff00: 0x80ffffff, strBuffer );
    swprintf_s( strBuffer, L"RightThumb" );
    m_Font12.DrawText( -146, -18, m_pGamepad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB ? 0xffffff00: 0x80ffffff,
                       strBuffer );

    m_Font12.End();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderVibrationTestPage()
// Desc: Renders the page for testing the gamepad vibration motors.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderVibrationTestPage()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderVibrationTestPageUI()
// Desc: Renders the page for testing the gamepad vibration motors.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderVibrationTestPageUI()
{
    D3DRECT rc = m_Font16.m_rcWindow;

    // Draw header
    DrawBox( rc.x1, rc.y1, rc.x2, rc.y1 + 43, vFill, vSolidBlack );
    m_Font16.DrawText( 5, 0, 0xffffffff, L"Gamepad Tool", ATGFONT_LEFT );
    m_Font16.DrawText( -5, 0, 0xffffffff, L"Vibration Test Page", ATGFONT_RIGHT );
    m_Font12.DrawText( -5, 25, 0xff808080, L"Press START and BACK for next page", ATGFONT_RIGHT );

    // Draw instructions
    DrawBox( rc.x2 - 200, rc.y1 + 50, rc.x2, rc.y2, vFill, vSolidBlack );
    m_Font12.DrawText( -200, 50, 0xff808080, L"Use the left and right\n"
                       L"triggers to test the\n"
                       L"vibration function of\n"
                       L"gamepad motors.\n" );

    // Draw outside box
    DrawBox( rc.x1, rc.y1 + 50, rc.x2 - 200, rc.y2, vEmpty, vSolidBlack );

    // Draw the analog gauges
    m_pd3dDevice->SetTexture( 0, m_xprResource.GetTexture( "AnalogGauge" ) );

    m_pd3dDevice->SetPixelShader( m_pTexturePS );
    m_pd3dDevice->SetVertexShader( m_pScreenspaceTexcoordVS );
    m_pd3dDevice->SetVertexDeclaration( m_pScreenspaceTexcoordDecl );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    struct VERTEX
    {
        XMFLOAT4 p;
        FLOAT tu, tv;
    };
    VERTEX v[4];
    v[0].p = XMFLOAT4( ( FLOAT )( rc.x1 ), ( FLOAT )( rc.y1 + 50 ), 0, 1 );  v[0].tu = 0; v[0].tv = 0;
    v[1].p = XMFLOAT4( ( FLOAT )( rc.x1 + 256 ), ( FLOAT )( rc.y1 + 50 ), 0, 1 );  v[1].tu = 1; v[1].tv = 0;
    v[2].p = XMFLOAT4( ( FLOAT )( rc.x1 + 256 ), ( FLOAT )( rc.y1 + 50 + 128 ), 0, 1 );  v[2].tu = 1; v[2].tv = 1;
    v[3].p = XMFLOAT4( ( FLOAT )( rc.x1 ), ( FLOAT )( rc.y1 + 50 + 128 ), 0, 1 );  v[3].tu = 0; v[3].tv = 1;
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, v, sizeof( v[0] ) );
    v[0].p = XMFLOAT4( ( FLOAT )( rc.x1 ), ( FLOAT )( rc.y1 + 50 + 140 ), 0, 1 );  v[0].tu = 0; v[0].tv = 0;
    v[1].p = XMFLOAT4( ( FLOAT )( rc.x1 + 256 ), ( FLOAT )( rc.y1 + 50 + 140 ), 0, 1 );  v[1].tu = 1; v[1].tv = 0;
    v[2].p = XMFLOAT4( ( FLOAT )( rc.x1 + 256 ), ( FLOAT )( rc.y1 + 50 + 140 + 128 ), 0, 1 );  v[2].tu = 1;
    v[2].tv = 1;
    v[3].p = XMFLOAT4( ( FLOAT )( rc.x1 ), ( FLOAT )( rc.y1 + 50 + 140 + 128 ), 0, 1 );  v[3].tu = 0; v[3].tv = 1;
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, v, sizeof( v[0] ) );

    // Draw the gauge needles
    FLOAT fLeftAngle = 0.85f * ( 2 * m_fLeftMotorSpeed - 1 );
    FLOAT fRightAngle = 0.85f * ( 2 * m_fRightMotorSpeed - 1 );
    v[0].p.x = rc.x1 + 100 + 92 * sinf( fLeftAngle );   v[0].p.y = rc.y1 + 50 + 111 - 92 * cosf( fLeftAngle );
    v[1].p.x = rc.x1 + 100 + 48 * sinf( fLeftAngle );   v[1].p.y = rc.y1 + 50 + 111 - 48 * cosf( fLeftAngle );
    v[2].p.x = rc.x1 + 100 + 92 * sinf( fRightAngle );  v[2].p.y = rc.y1 + 50 + 140 + 111 - 92 * cosf( fRightAngle );
    v[3].p.x = rc.x1 + 100 + 48 * sinf( fRightAngle );  v[3].p.y = rc.y1 + 50 + 140 + 111 - 48 * cosf( fRightAngle );

    m_pd3dDevice->SetPixelShader( m_pConstantColorPS );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vSolidWhite, 1 );

    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 2, v, sizeof( v[0] ) );

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

    // Draw the motor text
    WCHAR strBuffer[80];
    swprintf_s( strBuffer, L"%d%%", ( WORD )( 100 * m_fLeftMotorSpeed ) );
    m_Font16.DrawText( 0, 50 + 120 - 20, 0xffffffff, L"Left Motor: " );
    m_Font16.DrawText( 0xffffff00, strBuffer );

    swprintf_s( strBuffer, L"%d%%", ( WORD )( 100 * m_fRightMotorSpeed ) );
    m_Font16.DrawText( 0, 50 + 140 + 120 - 20, 0xffffffff, L"Right Motor: " );
    m_Font16.DrawText( 0xffffff00, strBuffer );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderDeadZoneCalibrationPage()
// Desc: Renders the page for calibrating the thumbsticks' deadzone.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderDeadZoneCalibrationPage()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderDeadZoneCalibrationPageUI()
// Desc: Renders the page for calibrating the thumbsticks' deadzone.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderDeadZoneCalibrationPageUI()
{
    D3DRECT rc = m_Font16.m_rcWindow;

    // Draw header
    DrawBox( rc.x1, rc.y1, rc.x2, rc.y1 + 43, vFill, vSolidBlack );
    m_Font16.DrawText( 5, 0, 0xffffffff, L"Gamepad Tool", ATGFONT_LEFT );
    m_Font16.DrawText( -5, 0, 0xffffffff, L"Deadzone Calibration Page", ATGFONT_RIGHT );
    m_Font12.DrawText( -5, 25, 0xff808080, L"Press START and BACK for next page", ATGFONT_RIGHT );

    // Draw instructions
    DrawBox( rc.x2 - 200, rc.y1 + 50, rc.x2, rc.y2, vFill, vSolidBlack );
    m_Font12.DrawText( -200, 50, 0xff808080, L"Move thumbsticks to test\n"
                       L"sensitivity around the\n"
                       L"the deadzone (defined by\n"
                       L"the white box). Let them\n"
                       L"snap back and notice how\n"
                       L"the controls adapt over\n"
                       L"time allowing for a smaller\n"
                       L"deadzone.\n\n"
                       L"Use DPad to control the\n"
                       L"deadzone size.\n\n" );

    WCHAR strBuffer[80];
    swprintf_s( strBuffer, L"Deadzone: %ld (%ld%%)", ( LONG )( 32768 * m_fDeadZone ), ( LONG )( 100 * m_fDeadZone ) );
    m_Font12.DrawText( 0xffffffff, strBuffer );

    LONG dwBoxRadius = min( ( rc.x2 - 210 ) - rc.x1, rc.y2 - ( rc.y1 + 50 ) ) / 2;
    LONG dwBoxCenterX = rc.x1 + dwBoxRadius;
    LONG dwBoxCenterY = rc.y1 + 50 + dwBoxRadius;
    LONG dwDeadZone = ( LONG )( m_fDeadZone * dwBoxRadius );

    // Draw outside box
    DrawBox( dwBoxCenterX - dwBoxRadius, dwBoxCenterY - dwBoxRadius,
             dwBoxCenterX + dwBoxRadius, dwBoxCenterY + dwBoxRadius,
             vEmpty, vSolidBlack );

    // Draw inside box
    DrawBox( dwBoxCenterX - dwDeadZone, dwBoxCenterY - dwDeadZone,
             dwBoxCenterX + dwDeadZone, dwBoxCenterY + dwDeadZone,
             vEmpty, vSolidWhite );

    // Draw left thumb stick in red
    LONG dwThumbLX = ( LONG )( dwBoxCenterX + dwBoxRadius * ( m_pGamepad->sThumbLX + 0.5f ) / 32767.5f );
    LONG dwThumbLY = ( LONG )( dwBoxCenterY - dwBoxRadius * ( m_pGamepad->sThumbLY + 0.5f ) / 32767.5f );
    DrawBox( dwThumbLX - 1, dwThumbLY - 8, dwThumbLX + 1, dwThumbLY + 8, vSolidRed, vSolidRed );
    DrawBox( dwThumbLX - 8, dwThumbLY - 1, dwThumbLX + 8, dwThumbLY + 1, vSolidRed, vSolidRed );

    // Draw right thumb stick in green
    LONG dwThumbRX = ( LONG )( dwBoxCenterX + dwBoxRadius * ( m_pGamepad->sThumbRX + 0.5f ) / 32767.5f );
    LONG dwThumbRY = ( LONG )( dwBoxCenterY - dwBoxRadius * ( m_pGamepad->sThumbRY + 0.5f ) / 32767.5f );
    DrawBox( dwThumbRX - 1, dwThumbRY - 8, dwThumbRX + 1, dwThumbRY + 8, vSolidGreen, vSolidGreen );
    DrawBox( dwThumbRX - 8, dwThumbRY - 1, dwThumbRX + 8, dwThumbRY + 1, vSolidGreen, vSolidGreen );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderButtonQuantizationPage()
// Desc: Renders the page for testing the quantization of the controls
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderButtonQuantizationPage()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderButtonQuantizationPageUI()
// Desc: Renders the page for testing the quantization of the controls
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderButtonQuantizationPageUI()
{
    D3DRECT rc = m_Font16.m_rcWindow;

    // Draw header
    DrawBox( rc.x1, rc.y1, rc.x2, rc.y1 + 43, vFill, vSolidBlack );
    m_Font16.DrawText( 5, 0, 0xffffffff, L"Gamepad Tool", ATGFONT_LEFT );
    m_Font16.DrawText( -5, 0, 0xffffffff, L"Control Quantization Page", ATGFONT_RIGHT );
    m_Font12.DrawText( -5, 25, 0xff808080, L"Press START and BACK for next page", ATGFONT_RIGHT );

    // Draw instructions
    DrawBox( rc.x2 - 200, rc.y1 + 50, rc.x2, rc.y2, vFill, vSolidBlack );
    m_Font12.DrawText( -200, 50, 0xff808080, L"Analog controls have 8\n"
                       L"bits of precision. Squeeze\n"
                       L"the analog triggers and\n"
                       L"move the thumbsticks to\n"
                       L"see what values are\n"
                       L"quantized. Hit the START\n"
                       L"button to reset the values." );

    // Draw outside box
    DrawBox( rc.x1, rc.y1 + 50, rc.x2 - 210, rc.y2, vEmpty, vSolidBlack );

    // Display quantized trigger values
    DWORD adwQuantizationCount[6] = { 0, 0, 0, 0, 0, 0 };
    LONG gap = ( LONG )( ( ( rc.x2 - 210 ) - rc.x1 ) / 8 );
    LONG x0 = ( LONG )( rc.x1 );
    LONG y0 = ( LONG )( rc.y1 + 50 + 25 );
    LONG y1 = ( LONG )( rc.y2 - 25 );

    m_Font12.DrawText( ( FLOAT )( x0 + 1.5f * gap - rc.x1 ), ( FLOAT )( y0 - 25 - rc.y1 ), 0xffffffff, L"Triggers",
                       ATGFONT_CENTER_X );

    for( DWORD x = 0; x < 2; x++ )
    {
        x0 += gap;

        // Draw gray bar
        DrawBox( x0 - 2, y0, x0 + 2, y1, vSolidGrey, vSolidGrey );

        for( DWORD y = 0; y < 256; y++ )
        {
            // Highlight recorded values
            if( m_pQuantizedButtonValues[y] & ( 1 << x ) )
            {
                LONG val = ( LONG )( ( y1 - y0 ) * y / 256.0f );
                DrawBox( x0 - 4, y1 - val, x0 + 4, y1 - val, vSolidWhite, vSolidWhite );
                adwQuantizationCount[x]++;
            }

            // Highlight the current value
            LONG val = ( x == 0 ) ? m_pGamepad->bLeftTrigger : m_pGamepad->bRightTrigger;
            val = ( LONG )( ( y1 - y0 ) * val / 256.0f );
            DrawBox( x0 - 6, y1 - val - 1, x0 + 6, y1 - val + 1, vSolidRed, vSolidRed );
        }

        // Draw label
        m_Font12.DrawText( ( FLOAT )( x0 - rc.x1 ), ( FLOAT )( y1 - rc.y1 ), 0xffffffff, L"LT\0RT\0" + 3 * x,
                           ATGFONT_CENTER_X );
    }
    x0 += gap;

    // Display quantized thumbstick values
    m_Font12.DrawText( ( FLOAT )( x0 + 2.5f * gap - rc.x1 ), ( FLOAT )( y0 - 25 - rc.y1 ), 0xffffffff, L"Thumbsticks",
                       ATGFONT_CENTER_X );

    for( DWORD x = 0; x < 4; x++ )
    {
        x0 += gap;

        // Draw gray bar
        DrawBox( x0 - 2, y0, x0 + 2, y1, vSolidGrey, vSolidGrey );

        for( DWORD y = 0; y < 256; y++ )
        {
            // Highlight recorded values
            if( m_pQuantizedThumbStickValues[y] & ( 1 << x ) )
            {
                LONG val = ( LONG )( ( y1 - y0 ) * y / 256.0f );
                DrawBox( x0 - 4, y1 - val, x0 + 4, y1 - val, vSolidWhite, vSolidWhite );
                adwQuantizationCount[x + 2]++;
            }

            // Highlight the current value
            LONG val = 0;
            if( x == 0 ) val = BYTE( ( 32768 + m_pGamepad->sThumbLX ) >> 8 );
            if( x == 1 ) val = BYTE( ( 32768 + m_pGamepad->sThumbLY ) >> 8 );
            if( x == 2 ) val = BYTE( ( 32768 + m_pGamepad->sThumbRX ) >> 8 );
            if( x == 3 ) val = BYTE( ( 32768 + m_pGamepad->sThumbRY ) >> 8 );
            val = ( LONG )( ( y1 - y0 ) * val / 256.0f );
            DrawBox( x0 - 6, y1 - val - 1, x0 + 6, y1 - val + 1, vSolidRed, vSolidRed );
        }

        // Draw label
        m_Font12.DrawText( ( FLOAT )( x0 - rc.x1 ), ( FLOAT )( y1 - rc.y1 ), 0xffffffff, L"LX\0LY\0RX\0RY\0" + 3 * x,
                           ATGFONT_CENTER_X );
    }

    // Show the number of unique values each analog control has hit
    WCHAR strBuffer[80];
    m_Font12.DrawText( -200, 260 - 48, 0xff808080, L"Count of unique values:" );

    m_Font12.DrawText( -150, 280 - 48, 0xff808080, L"LT\nRT\nLX\nLY\nRX\nRY\n" );
    swprintf_s( strBuffer, L"%ld\n%ld\n%ld\n%ld\n%ld\n%ld\n",
                adwQuantizationCount[0], adwQuantizationCount[1],
                adwQuantizationCount[2], adwQuantizationCount[3],
                adwQuantizationCount[4], adwQuantizationCount[5] );
    m_Font12.DrawText( -100, 280 - 48, 0xffffffff, strBuffer );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    switch( m_AppState )
    {
        case APPSTATE_CONTROLTEST:
            RenderControlTestPage();
            RenderControlTestPageUI();
            break;
        case APPSTATE_VIBRATIONTEST:
            RenderVibrationTestPage();
            RenderVibrationTestPageUI();
            break;
        case APPSTATE_DEADZONECALIBRATION:
            RenderDeadZoneCalibrationPage();
            RenderDeadZoneCalibrationPageUI();
            break;
        case APPSTATE_BUTTONQUANTIZATION:
            RenderButtonQuantizationPage();
            RenderButtonQuantizationPageUI();
            break;
    }

    // Show the frame on the primary surface.
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    // Record the frame time
    m_Timer.MarkFrame();

    return S_OK;
}

