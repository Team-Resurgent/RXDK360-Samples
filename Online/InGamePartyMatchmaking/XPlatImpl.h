//--------------------------------------------------------------------------------------
// XplatImpl.h
//
// Implementation file for integrating GFWL into a single codebase with the XBox 360
// version of a project
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "Xplat.h"

//
// CXPlat_Draw
//

CXPlat_Draw::CXPlat_Draw( Sample* pSample ) : 
    m_pSample( pSample )
{

}

BOOL CXPlat_Draw::Initialize()
{
#ifdef _XBOX
    m_pSample->m_bDrawHelp = FALSE;

    // Create the fonts
    if( FAILED( m_Font16.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        FatalError( "Could not find font media!\n" );
    }

    if( FAILED( m_Font12.Create( "game:\\Media\\Fonts\\Arial_12.xpr" ) ) )
    {
        FatalError( "Could not find font media!\n" );
    }

    // Create the help
    if( FAILED( m_pSample->m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        FatalError( "Could not find help media!\n" );
    }

    // Intialise the TrueSkill(TM) skill bar specific render code
    XGUI::Initialise( m_pSample->m_pd3dDevice );
    m_pSample->m_bUseAlphaBlending = FALSE;

    // Confine text drawing to the title safe area
    m_Font12.SetWindow( ATG::GetTitleSafeArea() );
    m_Font16.SetWindow( ATG::GetTitleSafeArea() );

    return TRUE;
#else if LIVE_ON_WINDOWS
    return TRUE;
#endif
}

    
VOID CXPlat_Draw::BeginDraw()
{
#ifdef _XBOX
    ATG::RenderBackground( m_pSample->COLOR_BACKGROUND1, m_pSample->COLOR_BACKGROUND2 );
    m_fCenterX = ( m_Font12.m_rcWindow.x2 - m_Font12.m_rcWindow.x1 ) / 2.0f;
    m_fCenterY = ( m_Font12.m_rcWindow.y2 - m_Font12.m_rcWindow.y1 ) / 2.0f;
    m_Font16.Begin();
#else if LIVE_ON_WINDOWS
    IDirect3DDevice9* pDevice = DXUTGetD3D9Device();
    if( FAILED( pDevice->BeginScene() ) )
    {
        return;
    }
    m_Background.RenderBackground( pDevice, m_pSample->COLOR_BACKGROUND1, m_pSample->COLOR_BACKGROUND2 );
    m_fCenterX = static_cast<FLOAT>( ( DXUTGetWindowWidth() ) / 2 );
    m_fCenterY = static_cast<FLOAT>( ( DXUTGetWindowHeight() ) / 2 );
#endif
}

VOID CXPlat_Draw::EndDraw()
{
#ifdef _XBOX
    m_pSample->m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    m_Font16.End();
#else if LIVE_ON_WINDOWS
    XLiveRender();
    IDirect3DDevice9* pDevice = DXUTGetD3D9Device();
    pDevice->EndScene();
#endif
}

VOID CXPlat_Draw::BeginFont( BOOL bUseLargeFont )
{
#ifdef _XBOX
    if( bUseLargeFont )
    {
        return m_Font16.Begin();
    }
    else
    {
        return m_Font12.Begin();
    }
#endif
}

VOID CXPlat_Draw::EndFont( BOOL bUseLargeFont )
{
#ifdef _XBOX
    if( bUseLargeFont )
    {
        return m_Font16.End();
    }
    else
    {
        return m_Font12.End();
    }
#endif
}

FLOAT CXPlat_Draw::GetXPosition() const
{
    return m_X;
}

FLOAT CXPlat_Draw::GetYPosition() const
{
    return m_Y;
}

FLOAT CXPlat_Draw::GetCenterXPosition() const
{
    return m_fCenterX;
}

FLOAT CXPlat_Draw::GetCenterYPosition() const
{
    return m_fCenterY;
}

FLOAT CXPlat_Draw::GetFontHeight( BOOL bUseLargeFont ) const
{
#ifdef _XBOX
    if( bUseLargeFont )
    {
        return m_Font16.GetFontHeight();
    }
    else
    {
        return m_Font12.GetFontHeight();
    }
#else if LIVE_ON_WINDOWS
    TEXTMETRIC tm;
    m_pSample->m_pFont->GetTextMetrics( &tm );
    return static_cast<FLOAT>( tm.tmHeight );
#endif
}

VOID CXPlat_Draw::SetXPosition( FLOAT x )
{
    m_X = x;
}

VOID CXPlat_Draw::SetYPosition( FLOAT y )
{
    m_Y = y;
}

VOID CXPlat_Draw::IncrementXPosition( FLOAT deltaX )
{
    m_X += deltaX;
}

VOID CXPlat_Draw::IncrementYPosition( FLOAT deltaY )
{
    m_Y += deltaY;
}

VOID CXPlat_Draw::DecrementXPosition( FLOAT deltaX )
{
    m_X -= deltaX;
}

VOID CXPlat_Draw::DecrementYPosition( FLOAT deltaY )
{
    m_Y -= deltaY;
}

#ifdef _XBOX
ATG::Font& CXPlat_Draw::GetLargeFont()
{

    return m_Font16;
}

ATG::Font& CXPlat_Draw::GetSmallFont()
{

    return m_Font12;
}
#endif


VOID CXPlat_Draw::SetScaleFactor( FLOAT factorX, FLOAT factorY )
{
#ifdef _XBOX
    m_Font12.SetScaleFactors( factorX, factorY );
#else if LIVE_ON_WINDOWS
#endif
}

FLOAT CXPlat_Draw::GetTextWidth( const WCHAR* str ) const
{
#ifdef _XBOX
    return m_Font12.GetTextWidth( str );
#else if LIVE_ON_WINDOWS
    RECT rc;
    m_pSample->m_pFont->DrawText( NULL, str, -1, &rc, DT_CALCRECT, 0 );
    return static_cast<FLOAT>( rc.right - rc.left );
#endif
}

FLOAT CXPlat_Draw::GetWindowWidth() const
{
#ifdef _XBOX
    return static_cast<FLOAT>( m_Font12.m_rcWindow.x2 );
#else if LIVE_ON_WINDOWS
    return static_cast<FLOAT>( DXUTGetWindowWidth() );
#endif
}

FLOAT CXPlat_Draw::GetWindowHeight() const
{
#ifdef _XBOX
    return static_cast<FLOAT>( m_Font12.m_rcWindow.y2 );
#else if LIVE_ON_WINDOWS
    return static_cast<FLOAT>( DXUTGetWindowHeight() );
#endif
}

VOID CXPlat_Draw::DrawText( FLOAT x, 
                            FLOAT y, 
                            D3DCOLOR col, 
                            const WCHAR* str, 
                            BOOL bUseLargeFont,
                            DWORD style, 
                            FLOAT len )
{
#ifdef _XBOX
    m_Font12.DrawText( x, y, col, str, style, len );
#else if LIVE_ON_WINDOWS
    m_pSample->DrawText( static_cast<UINT>( x ), 
                         static_cast<UINT>( y ), 
                         col, 
                         str, 
                         style | DT_NOCLIP, 
                         static_cast<INT>( len ) );
#endif
}

VOID CXPlat_Draw::DrawTextFromCurrentXY( D3DCOLOR col, const WCHAR* str, DWORD style, FLOAT len )
{
#ifdef _XBOX
    this->DrawText( m_X, m_Y, col, str, FALSE, style, len );
#else if LIVE_ON_WINDOWS
    this->DrawText( m_X, 
                    m_Y, 
                    col, 
                    str );
#endif
}

VOID CXPlat_Draw::DrawTextCentered( FLOAT y, D3DCOLOR col, const WCHAR* str )
{
    this->DrawText( m_fCenterX, y, col, str, FALSE, DRAW_CENTER_STYLE );
}

VOID CXPlat_Draw::DrawTextCenterScreen( D3DCOLOR col, const WCHAR* str )
{
    this->DrawText( m_fCenterX, m_fCenterY, col, str, FALSE, DRAW_CENTER_STYLE );
}

HRESULT CXPlat_Draw::CreateBackground()
{
#ifdef _XBOX
return S_OK;
#else if LIVE_ON_WINDOWS
return m_Background.Create( DXUTGetD3D9Device() );
#endif
}

VOID CXPlat_Draw::DestroyBackground()
{
#ifdef _XBOX
#else if LIVE_ON_WINDOWS
    m_Background.Destroy();
#endif
}

//
// CXPlat_UserInput
//
CXPlat_UserInput::CXPlat_UserInput( Sample* pSample ) :
    m_pSample( pSample )
{
}

BOOL CXPlat_UserInput::Update()
{
    #ifdef _XBOX
    m_gamepad   = &ATG::Input::m_DefaultGamepad;
    memcpy_s( m_gamepads, sizeof(ATG::GAMEPAD) * MAX_USER_COUNT, ATG::Input::m_Gamepads, sizeof(ATG::GAMEPAD) * MAX_USER_COUNT );
    
    if( ATG::SignIn::IsSystemUIShowing() )
    {
        return FALSE;
    }
    #else if LIVE_ON_WINDOWS
    if( m_pSample->m_CXPlat_Signin.IsSystemUIShowing() )
    {
        return FALSE;
    }
    #endif

    return TRUE;
}

VOID CXPlat_UserInput::MergeInputs( DWORD dwMask, DWORD* pdwActiveGamePadsMask )
{
#ifdef _XBOX
    m_gamepad = ATG::Input::GetMergedInput( dwMask, pdwActiveGamePadsMask );
#else if LIVE_ON_WINDOWS
#endif
}

BOOL CXPlat_UserInput::GotoNextMenuItem( DWORD dwControllerID )
{
    #ifdef _XBOX
    if( m_gamepads[ dwControllerID ].wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        return TRUE;
    }
    #else if LIVE_ON_WINDOWS
    if( m_pSample->WasKeyReleased( VK_DOWN ) )
    {
        return TRUE;
    }
    #endif

    return FALSE;
}

BOOL CXPlat_UserInput::GotoPreviousMenuItem( DWORD dwControllerID )
{
    #ifdef _XBOX
    if( m_gamepads[ dwControllerID ].wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {

        return TRUE;
    }
    #else if LIVE_ON_WINDOWS
    if( m_pSample->WasKeyReleased( VK_UP ) )
    {

        return TRUE;
    }
    #endif

    return FALSE;
}

BOOL CXPlat_UserInput::SelectCurrentMenuItem( DWORD dwControllerID )
{
    #ifdef _XBOX
    if( m_gamepads[ dwControllerID ].wPressedButtons & XINPUT_GAMEPAD_A ||
        m_gamepads[ dwControllerID ].wPressedButtons & XINPUT_GAMEPAD_START )
    {
        return TRUE;
    }
    #else if LIVE_ON_WINDOWS
    if( m_pSample->WasKeyReleased( VK_RETURN ) )
    {
        return TRUE;
    }
    #endif

    return FALSE;
}

BOOL CXPlat_UserInput::GotoNextMenuItemValue( DWORD dwControllerID )
{
    #ifdef _XBOX
    if( m_gamepads[ dwControllerID ].wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        return TRUE;
    }
    #else if LIVE_ON_WINDOWS
    if( m_pSample->WasKeyReleased( VK_RIGHT ) )
    {
        return TRUE;
    }
    #endif

    return FALSE;
}

BOOL CXPlat_UserInput::GotoPreviousMenuItemValue( DWORD dwControllerID )
{
    #ifdef _XBOX
    if( m_gamepads[ dwControllerID ].wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        return TRUE;
    }
    #else if LIVE_ON_WINDOWS
    if( m_pSample->WasKeyReleased( VK_LEFT ) )
    {
        return TRUE;
    }
    #endif

    return FALSE;
}

BOOL CXPlat_UserInput::ReturnToPreviousUIScreen( DWORD dwControllerID )
{
#ifdef _XBOX
    if( m_gamepads[ dwControllerID ].wPressedButtons & XINPUT_GAMEPAD_B )
    {
        return TRUE;
    }
#else if LIVE_ON_WINDOWS
    if( m_pSample->WasKeyReleased( VK_BACK ) )
    {
        return TRUE;
    }
#endif
    return FALSE;
}

BOOL CXPlat_UserInput::WasUserInputSelected( DWORD dwUserInput, DWORD dwControllerID )
{
#ifdef _XBOX
    if( m_gamepads[ dwControllerID ].wPressedButtons & dwUserInput )
    {
        return TRUE;
    }
#else if LIVE_ON_WINDOWS
    if( m_pSample->WasKeyReleased( (BYTE)dwUserInput ) || m_pSample->GetLastKeyReleased() == dwUserInput )
    {
        return TRUE;
    }
#endif
    return FALSE;
}

DWORD CXPlat_UserInput::GetLastUserInput( DWORD dwControllerID )
{
#ifdef _XBOX
    return m_gamepads[ dwControllerID ].wPressedButtons;
#else if LIVE_ON_WINDOWS
    DWORD dwLastKeyReleased = m_pSample->GetLastKeyReleased();
    m_pSample->ClearLastKeyReleased();
    return dwLastKeyReleased;
#endif
}

//
// CXPlat_Signin
//
CXPlat_Signin::CXPlat_Signin( Sample* pSample ) :
    m_pSample( pSample )
{
    m_bRequireOnlineUsers = FALSE;
}

VOID CXPlat_Signin::Initialize( 
                               DWORD dwMinUsers, 
                               DWORD dwMaxUsers,
                               BOOL  bRequireOnlineUsers,
                               DWORD dwSignInPanes )
{
    m_bRequireOnlineUsers = bRequireOnlineUsers;

#ifdef _XBOX
    ATG::SignIn::Initialize( dwMinUsers, 
                             dwMaxUsers, 
                             bRequireOnlineUsers, 
                             dwSignInPanes );
#endif
}

VOID CXPlat_Signin::Cleanup()
{
#ifdef _XBOX
#else if LIVE_ON_WINDOWS
    m_SignIn.Cleanup();
#endif
}

VOID CXPlat_Signin::Update()
{
#ifdef _XBOX
    m_dwUpdateFlags = ATG::SignIn::Update();
#else if LIVE_ON_WINDOWS
#endif
}

BOOL CXPlat_Signin::HasConnectionChanged()
{
#ifdef _XBOX
    return ( m_dwUpdateFlags & ATG::SignIn::CONNECTION_CHANGED );
#else if LIVE_ON_WINDOWS
    HRESULT hrLiveConnection = XONLINE_E_LOGON_NO_NETWORK_CONNECTION;
    return m_SignIn.HasConnectionChanged( &hrLiveConnection );
#endif
}

BOOL CXPlat_Signin::HaveSigninUsersChanged()
{
#ifdef _XBOX
    return ( m_dwUpdateFlags & ATG::SignIn::SIGNIN_USERS_CHANGED );
#else if LIVE_ON_WINDOWS
    return m_SignIn.HasSignedInUserChanged();
#endif
}

BOOL CXPlat_Signin::IsSystemUIShowing()
{
#ifdef _XBOX
    return ATG::SignIn::IsSystemUIShowing();
#else if LIVE_ON_WINDOWS
    return m_SignIn.IsSystemUIShowing();
#endif
}

VOID CXPlat_Signin::ShowSigninUI()
{
#ifdef _XBOX
    ATG::SignIn::ShowSignInUI();
#else if LIVE_ON_WINDOWS
    m_SignIn.ShowSignInUI( m_bRequireOnlineUsers );
#endif
}

DWORD CXPlat_Signin::GetSignedInUserMask()
{
#ifdef _XBOX
    return ATG::SignIn::GetSignedInUserMask();
#else if LIVE_ON_WINDOWS
    return 0x1; // only one user per machine supported
#endif
}

DWORD CXPlat_Signin::GetLowestSignedInControllerID()
{
#ifdef _XBOX
    return ATG::SignIn::GetSignedInUser();
#else if LIVE_ON_WINDOWS
    return 0; // only one user per machine supported
#endif
}

BOOL CXPlat_Signin::AreUsersSignedIn()
{
#ifdef _XBOX
    if( ATG::SignIn::AreUsersSignedIn() )
    {
        return TRUE;
    }
#else if LIVE_ON_WINDOWS
    if( m_SignIn.IsUserSignedInToLive() || m_SignIn.IsUserSignedInToLocally() )
    {
        return TRUE;
    }
#endif
    return FALSE;
}

BOOL CXPlat_Signin::AreUsersSignedInToLive()
{
#ifdef _XBOX
    return ( ATG::SignIn::GetOnlineUserMask() );
#else if LIVE_ON_WINDOWS
    return m_SignIn.IsUserSignedInToLive();
#endif
}

BOOL CXPlat_Signin::AreUsersSignedInLocally()
{
#ifdef _XBOX
#else if LIVE_ON_WINDOWS
#endif
}

//
// CXPlat_XOnline
//
CXPlat_XOnline::CXPlat_XOnline( Sample* pSample ) :
    m_pSample( pSample )
{
}

VOID CXPlat_XOnline::Startup()
{
#ifdef _XBOX
    // Start up Xbox Live functionality using default Secure Network Layer settings
    if( XOnlineStartup() != ERROR_SUCCESS )
    {
        FatalError( "Failed to start Xbox Live\n" );
    }
#else if LIVE_ON_WINDOWS
    if( FAILED( m_XLiveManager.Initialize() ) )
    {
        FatalError( "Failed to initialize GFWL!\n" );
    }
#endif
}

VOID CXPlat_XOnline::Cleanup()
{
#ifdef _XBOX
    XOnlineCleanup();
#else if LIVE_ON_WINDOWS
    m_XLiveManager.UnInitialize();
#endif
}

HRESULT CXPlat_XOnline::OnDeviceCreated()
{
#ifdef _XBOX
    return S_OK;
#else if LIVE_ON_WINDOWS
    return m_XLiveManager.OnCreateDevice( DXUTGetD3D9Device() );
#endif
}

HRESULT CXPlat_XOnline::OnDeviceReset()
{
#ifdef _XBOX
    return S_OK;
#else if LIVE_ON_WINDOWS
    D3DPRESENT_PARAMETERS d3dpp = DXUTGetD3D9PresentParameters();
    return m_XLiveManager.OnDeviceReset( &d3dpp );
#endif
}

HRESULT CXPlat_XOnline::OnDeviceDestroyed()
{
#ifdef _XBOX
    return S_OK;
#else if LIVE_ON_WINDOWS
    return m_XLiveManager.OnDeviceDestroyed();
#endif
}
