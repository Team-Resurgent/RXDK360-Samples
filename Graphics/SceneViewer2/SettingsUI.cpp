//--------------------------------------------------------------------------------------
// SettingsUI.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <assert.h>
#include "SettingsUI.h"
#include <AtgDebugDraw.h>

const FLOAT g_fActivationTransitionTime = 0.15f;
const FLOAT g_fDebounceTime = 0.25f;

const FLOAT g_fTitleFontSize = 1.0f;
const FLOAT g_fTitleSpacing = 60.0f;

const FLOAT g_fSettingsFontSize = 1.1f;
const FLOAT g_fSettingsSpacing = 30.0f;

VOID SettingsPanel::Initialize( D3DDevice* pd3dDevice )
{
    m_pd3dDevice = pd3dDevice;

    m_MenuBarFont.Create( "game:\\Media\\Fonts\\SegoeUI_24.xpr" );
    m_MenuBarFont.SetWindow( 0, 0, 1280, 720 );
    m_SettingsFont.Create( "game:\\Media\\Fonts\\SegoeUI_16_Outline.xpr" );
    m_SettingsFont.SetWindow( 0, 0, 1280, 720 );
    m_MenuArtwork.Create( "game:\\Media\\SettingsUI.xpr" );
    m_pMenuBandTexture = m_MenuArtwork.GetTexture( "SV2_menubar" );
    m_pGroupBoxTexture = m_MenuArtwork.GetTexture( "SV2_box" );
    m_pGroupHelpBoxTexture = m_MenuArtwork.GetTexture( "SV2_help_box" );

    m_ActivationState = AS_DISABLED;
    m_fActivationTime = 0;
    m_fDebounceTime = 0;
    m_fTransitionPercent = 0.0f;
    m_dwCurrentGroupIndex = 0;
    m_dwCurrentSettingIndex = ( DWORD )-1;
}

DWORD GetPreviousSettingIndex( SettingsGroup* pGroup, DWORD dwCurrentIndex )
{
    if( dwCurrentIndex == ( DWORD )-1 || dwCurrentIndex == 0 )
        return ( DWORD )-1;
    --dwCurrentIndex;
    while( pGroup->m_Settings[dwCurrentIndex].Type == ST_SEPARATOR )
    {
        if( dwCurrentIndex == 0 )
            return ( DWORD )-1;
        --dwCurrentIndex;
    }
    return dwCurrentIndex;
}

DWORD GetNextSettingIndex( SettingsGroup* pGroup, DWORD dwCurrentIndex )
{
    DWORD dwLastIndex = ( DWORD )pGroup->m_Settings.size() - 1;
    if( dwLastIndex == ( DWORD )-1 )
        return ( DWORD )-1;
    if( dwCurrentIndex >= dwLastIndex )
        return dwLastIndex;
    DWORD dwSavedIndex = dwCurrentIndex;
    ++dwCurrentIndex;
    while( pGroup->m_Settings[dwCurrentIndex].Type == ST_SEPARATOR )
    {
        if( dwCurrentIndex == dwLastIndex )
            return dwSavedIndex;
        ++dwCurrentIndex;
    }
    return dwCurrentIndex;
}

DWORD SettingsPanel::Update( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, FLOAT fAppTime )
{
    m_fDeltaTime = fDeltaTime;
    m_fDebounceTime -= fDeltaTime;

    // Transition into visible or disabled after the transition time has elapsed.
    if( m_ActivationState == AS_STARTUP )
    {
        if( fAppTime > ( m_fActivationTime + g_fActivationTransitionTime ) )
        {
            m_ActivationState = AS_VISIBLE;
        }
    }
    else if( m_ActivationState == AS_SHUTDOWN )
    {
        if( fAppTime > ( m_fActivationTime + g_fActivationTransitionTime ) )
        {
            m_ActivationState = AS_DISABLED;
        }
    }

    switch( m_ActivationState )
    {
        case AS_STARTUP:
            m_fTransitionPercent = ( fAppTime - m_fActivationTime ) / g_fActivationTransitionTime;
            m_fTransitionPercent *= m_fTransitionPercent;
            break;
        case AS_SHUTDOWN:
            m_fTransitionPercent = ( fAppTime - m_fActivationTime ) / g_fActivationTransitionTime;
            m_fTransitionPercent *= m_fTransitionPercent;
            m_fTransitionPercent = 1.0f - m_fTransitionPercent;
            break;
        default:
            m_fTransitionPercent = 1.0f;
    }

    if( pGamepad == NULL )
        return 0;

    // Check for Start button when panel is disabled.
    if( m_ActivationState == AS_DISABLED )
    {
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        {
            m_ActivationState = AS_STARTUP;
            m_fActivationTime = fAppTime;
            m_fTransitionPercent = 0.0f;
        }
        return 0;
    }

    // When the panel is enabled and the Start/Back/B button is pressed, close the panel.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START ||
        pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK ||
        pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        if( m_ActivationState != AS_STARTUP )
            m_fActivationTime = fAppTime;
        m_ActivationState = AS_SHUTDOWN;
        m_fTransitionPercent = 1.0f;
        return 0;
    }

    if( m_ActivationState != AS_VISIBLE )
        return 0;

    m_dwReturnValue = 0;

    DWORD dwGroupCount = ( DWORD )m_Groups.size();
    SettingsEntry* pCurrentSetting = GetCurrentSetting();
    if( pCurrentSetting == NULL )
    {
        if( dwGroupCount > 0 && pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            m_dwCurrentGroupIndex = ( m_dwCurrentGroupIndex + 1 ) % dwGroupCount;
        }
        else if( dwGroupCount > 0 && pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        {
            m_dwCurrentGroupIndex = ( m_dwCurrentGroupIndex + dwGroupCount - 1 ) % dwGroupCount;
        }
        else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            SettingsGroup* pGroup = GetGroup( m_dwCurrentGroupIndex );
            if( pGroup != NULL && pGroup->m_Settings.size() > 0 )
                m_dwCurrentSettingIndex = 0;
        }
    }

    if( dwGroupCount > 0 && pGamepad->bPressedRightTrigger )
    {
        m_dwCurrentGroupIndex = ( m_dwCurrentGroupIndex + 1 ) % dwGroupCount;
        if( m_dwCurrentSettingIndex != ( DWORD )-1 )
            m_dwCurrentSettingIndex = 0;
    }
    else if( dwGroupCount > 0 && pGamepad->bPressedLeftTrigger )
    {
        m_dwCurrentGroupIndex = ( m_dwCurrentGroupIndex + dwGroupCount - 1 ) % dwGroupCount;
        if( m_dwCurrentSettingIndex != ( DWORD )-1 )
            m_dwCurrentSettingIndex = 0;
    }

    if( pCurrentSetting != NULL )
    {
        WORD ButtonMask = pGamepad->wPressedButtons;
        if( pCurrentSetting->Type == ST_FLOAT )
        {
            ButtonMask = pGamepad->wLastButtons;
        }
        if( ButtonMask & XINPUT_GAMEPAD_RIGHT_SHOULDER )
            fDeltaTime *= 10.0f;
        if( ButtonMask & XINPUT_GAMEPAD_LEFT_SHOULDER )
            fDeltaTime *= 0.1f;

        BOOL bAllowLeftRight = ( pCurrentSetting->Type != ST_COMMAND );
        BOOL bAllowAButton = ( pCurrentSetting->Type == ST_COMMAND || pCurrentSetting->Type == ST_BOOL );

        if( bAllowLeftRight )
        {
            if( ButtonMask & XINPUT_GAMEPAD_DPAD_RIGHT && m_fDebounceTime <= 0 )
            {
                m_dwReturnValue = pCurrentSetting->IncrementValue( fDeltaTime );
            }
            else if( ButtonMask & XINPUT_GAMEPAD_DPAD_LEFT && m_fDebounceTime <= 0 )
            {
                m_dwReturnValue = pCurrentSetting->DecrementValue( fDeltaTime );
            }
        }

        if( bAllowAButton )
        {
            if( ButtonMask & XINPUT_GAMEPAD_A )
            {
                m_dwReturnValue = pCurrentSetting->IncrementValue( 0 );
            }
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        {
            m_fDebounceTime = g_fDebounceTime;
            SettingsGroup* pGroup = GetGroup( m_dwCurrentGroupIndex );
            m_dwCurrentSettingIndex = GetPreviousSettingIndex( pGroup, m_dwCurrentSettingIndex );
        }
        else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            m_fDebounceTime = g_fDebounceTime;
            SettingsGroup* pGroup = GetGroup( m_dwCurrentGroupIndex );
            m_dwCurrentSettingIndex = GetNextSettingIndex( pGroup, m_dwCurrentSettingIndex );
        }
    }

    return m_dwReturnValue;
}


VOID SettingsPanel::SetVisible( BOOL bVisible )
{
    if( bVisible )
    {
        m_ActivationState = AS_VISIBLE;
    }
    else
    {
        m_ActivationState = AS_DISABLED;
    }
    m_fTransitionPercent = 1.0f;
}


VOID SettingsPanel::ShowMenuItem( DWORD dwMenuIndex, DWORD dwItemIndex )
{
    m_dwCurrentGroupIndex = min( dwMenuIndex, m_Groups.size() - 1 );
    SettingsGroup* pGroup = GetGroup( m_dwCurrentGroupIndex );
    m_dwCurrentSettingIndex = min( dwItemIndex, pGroup->m_Settings.size() );
    SetVisible( TRUE );
}


SettingsEntry* SettingsPanel::GetCurrentSetting()
{
    SettingsGroup* pGroup = GetGroup( m_dwCurrentGroupIndex );
    if( pGroup == NULL )
        return NULL;
    DWORD dwSettingsCount = ( DWORD )pGroup->m_Settings.size();
    if( m_dwCurrentSettingIndex < dwSettingsCount )
        return &pGroup->m_Settings[ m_dwCurrentSettingIndex ];
    return NULL;
}

VOID SettingsPanel::Render()
{
    if( m_ActivationState == AS_DISABLED )
        return;

    SettingsGroup* pGroup = GetGroup( m_dwCurrentGroupIndex );
    if( pGroup != NULL )
    {
        RenderGroup( pGroup );
    }
    RenderMenuBar();

    if( m_ActivationState == AS_VISIBLE )
    {
        m_MenuBarFont.SetScaleFactors( 0.8f, 0.8f );
        m_MenuBarFont.Begin();
        m_MenuBarFont.DrawText( 640, 613, 0xFFFFFFFF, L"Use the dpad and triggers to navigate the menus.",
                                ATGFONT_CENTER_X );
        m_MenuBarFont.End();
    }
}

VOID SettingsPanel::RenderMenuBar()
{
    const FLOAT fBarHeight = 70.0f;
    const FLOAT fBarTop = 72.0f;
    const FLOAT fCursorCenterXPos = 640.0f;
    static FLOAT s_fCurrentCursor = 0.0f;

    FLOAT fYPos = -fBarHeight + m_fTransitionPercent * ( fBarHeight + fBarTop );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    D3DRECT rectMenuBar =
    {
        0, ( DWORD )fYPos, 1280, ( DWORD )( fYPos + fBarHeight )
    };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( rectMenuBar, m_pMenuBandTexture, FALSE );
    ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( 0, fYPos ), XMFLOAT2( 1280, fBarHeight ), 2.0f, 0xFF808080 );

    DWORD dwGroupCount = ( DWORD )m_Groups.size();
    FLOAT fTotalWidth = 0.0f;
    FLOAT fCurrentXPos = 0.0f;
    for( DWORD i = 0; i < dwGroupCount; ++i )
    {
        FLOAT fWidth = m_Groups[i].m_fTitleWidth;
        if( m_dwCurrentGroupIndex == i )
            fCurrentXPos = fTotalWidth + ( fWidth * 0.5f );
        fTotalWidth += ( fWidth + g_fTitleSpacing );
    }

    FLOAT fAmount = 20.0f * m_fDeltaTime;
    fAmount = min( fAmount, 1.0f );
    FLOAT fDelta = s_fCurrentCursor - fCurrentXPos;
    fDelta *= fAmount;
    s_fCurrentCursor -= fDelta;

    m_MenuBarFont.SetScaleFactors( g_fTitleFontSize, g_fTitleFontSize );
    FLOAT fFontHeight = m_MenuBarFont.GetFontHeight();

    fYPos += fBarHeight * 0.5f;
    if( fYPos >= 0.0f )
    {
        D3DVIEWPORT9 OldVP;
        m_pd3dDevice->GetViewport( &OldVP );
        D3DVIEWPORT9 vp =
        {
            195, 0, 1280, 720, 0, 1
        };
        m_pd3dDevice->SetViewport( &vp );

        fTotalWidth = 0.0f;
        for( DWORD i = 0; i < dwGroupCount; ++i )
        {
            SettingsGroup* pGroup = GetGroup( i );
            FLOAT fXPos = fTotalWidth + fCursorCenterXPos - s_fCurrentCursor;
            DWORD dwColor = 0xFFFFFFFF;
            if( i == m_dwCurrentGroupIndex )
            {
                if( m_dwCurrentSettingIndex == ( DWORD )-1 )
                {
                    const FLOAT fBorder = 6.0f;
                    FLOAT fWidth = pGroup->m_fTitleWidth;
                    ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( fXPos - fBorder,
                                                                   fYPos - fFontHeight * 0.5f ),
                                                         XMFLOAT2( fWidth + fBorder * 2,
                                                                   fFontHeight ),
                                                         -1, 0xFF404040 );
                }
                dwColor = 0xFFFFFF80;
            }
            if( fXPos >= 0.0f && fXPos < 1280.0f )
            {
                m_MenuBarFont.Begin();
                m_MenuBarFont.DrawText( fXPos, fYPos, dwColor, pGroup->m_strTitle, ATGFONT_CENTER_Y );
                m_MenuBarFont.End();
            }

            fTotalWidth += ( pGroup->m_fTitleWidth + g_fTitleSpacing );
        }
        m_pd3dDevice->SetViewport( &OldVP );
    }
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
}


VOID SettingsPanel::RenderGroup( SettingsGroup* pGroup )
{
    const FLOAT fGroupTop = 160.0f;
    const FLOAT fGroupLeft = 240.0f;
    const FLOAT fGroupWidth = 800.0f;
    const FLOAT fGroupHeight = 450.0f;

    FLOAT fXPos = -fGroupWidth + m_fTransitionPercent * ( fGroupLeft + fGroupWidth );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_SRCALPHA );
    D3DRECT rectGroup =
    {
        ( DWORD )fXPos, ( DWORD )fGroupTop, ( DWORD )( fXPos + fGroupWidth ), ( DWORD )
        ( fGroupTop + fGroupHeight )
    };
    if( !pGroup->m_bHelpMenu )
    {
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( rectGroup, m_pGroupBoxTexture, FALSE );
    }
    else
    {
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( rectGroup, m_pGroupHelpBoxTexture, FALSE );
    }
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( fXPos, fGroupTop ), XMFLOAT2( fGroupWidth, fGroupHeight ), 2.0f,
                                         0xFF808080 );

    if( pGroup->m_bHelpMenu )
    {
        DWORD dwIndex = m_dwCurrentSettingIndex;
        if( dwIndex == ( DWORD )-1 )
            dwIndex = 0;
        RenderHelp( &pGroup->m_Settings[dwIndex], fXPos, fGroupTop, dwIndex == m_dwCurrentSettingIndex );
        return;
    }

    DWORD dwSettingsCount = ( DWORD )pGroup->m_Settings.size();

    if( dwSettingsCount == 0 )
    {
        return;
    }

    FLOAT fDisplayCount = ( fGroupHeight / g_fSettingsSpacing ) - 1;
    DWORD dwDisplayCount = ( DWORD )fDisplayCount;
    DWORD dwStartIndex = pGroup->m_dwDisplayStartIndex;
    DWORD dwCursorIndex = m_dwCurrentSettingIndex;
    if( dwCursorIndex == ( DWORD )-1 )
        dwCursorIndex = 0;
    if( dwCursorIndex < dwStartIndex )
    {
        dwStartIndex = dwCursorIndex;
    }
    else if( dwCursorIndex >= ( dwStartIndex + dwDisplayCount ) )
    {
        dwStartIndex = dwCursorIndex - dwDisplayCount + 1;
    }
    pGroup->m_dwDisplayStartIndex = dwStartIndex;
    DWORD dwEndIndex = dwStartIndex + dwDisplayCount;
    dwEndIndex = min( dwEndIndex, dwSettingsCount );

    m_SettingsFont.SetScaleFactors( g_fSettingsFontSize, g_fSettingsFontSize );
    FLOAT fFontHeight = m_SettingsFont.GetFontHeight();

    FLOAT fYPos = fGroupTop + g_fSettingsSpacing * 1.0f;
    fXPos += g_fSettingsSpacing * 0.5f;
    if( fXPos > 0 )
    {
        if( m_dwCurrentSettingIndex != ( DWORD )-1 )
        {
            const FLOAT fBorder = 6.0f;
            FLOAT fTop = fYPos + ( FLOAT )( m_dwCurrentSettingIndex - dwStartIndex ) * g_fSettingsSpacing;
            FLOAT fWidth = fGroupWidth - g_fSettingsSpacing;
            ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( fXPos - fBorder,
                                                           fTop - fFontHeight * 0.5f ),
                                                 XMFLOAT2( fWidth + fBorder * 2,
                                                           fFontHeight ),
                                                 -1, 0xFF606060 );
        }
        m_SettingsFont.Begin();
        for( DWORD i = dwStartIndex; i < dwEndIndex; ++i )
        {
            RenderOneEntry( &pGroup->m_Settings[i], fXPos, fYPos, 1.0f, i == m_dwCurrentSettingIndex );
            fYPos += g_fSettingsSpacing;
        }
        m_SettingsFont.End();
    }
}


VOID SettingsPanel::RenderOneEntry( SettingsEntry* pEntry, FLOAT fXPos, FLOAT fYPos, FLOAT fAlpha, BOOL bActive )
{
    if( pEntry->Type == ST_HELPSCREEN )
        return;

    const FLOAT fValueXDistance = 460.0f;

    DWORD dwFontFlags = ATGFONT_CENTER_Y;

    BYTE bAlpha = ( BYTE )( 255.0f * fAlpha );
    D3DCOLOR LabelColor = D3DCOLOR_ARGB( bAlpha, 208, 208, 255 );
    D3DCOLOR ValueColor = D3DCOLOR_ARGB( bAlpha, 255, 255, 0 );

    const FLOAT fValuePos = fXPos + fValueXDistance;
    m_SettingsFont.SetScaleFactors( g_fSettingsFontSize, g_fSettingsFontSize );
    FLOAT fWidth = 0;
    if( pEntry->strName != NULL )
    {
        fWidth = m_SettingsFont.GetTextWidth( pEntry->strName );
        FLOAT fMaxWidth = fValuePos - fXPos - 40.0f;
        if( fWidth > fMaxWidth )
        {
            FLOAT fNewWidthFactor = g_fSettingsFontSize * ( fMaxWidth / fWidth );
            m_SettingsFont.SetScaleFactors( fNewWidthFactor, g_fSettingsFontSize );
        }
        else
        {
            m_SettingsFont.SetScaleFactors( g_fSettingsFontSize, g_fSettingsFontSize );
        }
        fYPos = max( 0, fYPos );
        m_SettingsFont.DrawText( fXPos, fYPos, LabelColor, pEntry->strName, dwFontFlags );
        m_SettingsFont.SetScaleFactors( g_fSettingsFontSize, g_fSettingsFontSize );
    }

    if( bActive && pEntry->Type != ST_COMMAND && pEntry->Type != ST_SEPARATOR )
        m_SettingsFont.DrawText( fValuePos - 30, fYPos, LabelColor, GLYPH_LR_ARROW, dwFontFlags );

    switch( pEntry->Type )
    {
        case ST_BOOL:
        {
            BOOL bValue = *( BOOL* )pEntry->pData;
            m_SettingsFont.DrawText( fValuePos, fYPos, ValueColor, bValue ? L"True" : L"False", dwFontFlags );
            break;
        }
        case ST_FLOAT:
        case ST_FLOATSTEP:
            {
                FLOAT fValue = *( FLOAT* )pEntry->pData;
                WCHAR strTemp[40];
                swprintf_s( strTemp, L"%0.4f", fValue );
                m_SettingsFont.DrawText( fValuePos, fYPos, ValueColor, strTemp, dwFontFlags );
                break;
            }
        case ST_INTEGER:
        {
            INT iValue = *( INT* )pEntry->pData;
            WCHAR strTemp[40];
            swprintf_s( strTemp, L"%d", iValue );
            m_SettingsFont.DrawText( fValuePos, fYPos, ValueColor, strTemp, dwFontFlags );
            break;
        }
        case ST_ENUM:
        {
            if( pEntry->iEnumIndex < 0 || pEntry->iEnumIndex >= ( INT )pEntry->EnumEntries.size() )
                break;

            const WCHAR* strName = pEntry->EnumEntries[ pEntry->iEnumIndex ].strName;
            m_SettingsFont.DrawText( fValuePos, fYPos, ValueColor, strName, dwFontFlags );
            break;
        }
    case ST_COMMAND:
        if( bActive )
            m_SettingsFont.DrawText( fValuePos, fYPos, ValueColor, L"Press " GLYPH_A_BUTTON, dwFontFlags );
            break;
        case ST_SEPARATOR:
        {
            break;
        }
    }
}


VOID SettingsPanel::RenderHelp( SettingsEntry* pEntry, FLOAT fXPos, FLOAT fYPos, BOOL bActive )
{
    if( fXPos <= 0 )
        return;

    struct HelpPlacement
    {
        DWORD dwButtonX, dwButtonY;
        DWORD dwTextX, dwTextY;
        DWORD dwTextFlags;
    };
    const HelpPlacement Placements[] =
    {
        { 272, 217, 183, 207, ATGFONT_RIGHT },   // L stick
        { 455, 275, 464, 369, ATGFONT_LEFT },    // R stick
        { 335, 265, 300, 351, ATGFONT_RIGHT },   // Dpad
        { 357, 204, 355, 50, ATGFONT_RIGHT },    // Back
        { 443, 204, 441, 50, ATGFONT_LEFT },     // Start
        { 485, 202, 506, 281, ATGFONT_LEFT },    // X
        { 518, 176, 592, 148, ATGFONT_LEFT },    // Y
        { 516, 227, 572, 240, ATGFONT_LEFT },    // A
        { 550, 200, 594, 200, ATGFONT_LEFT },    // B
        { 275, 132, 211, 117, ATGFONT_RIGHT },   // L bumper
        { 525, 132, 580, 117, ATGFONT_LEFT },    // R bumper
        { 15, 422, 0, 0, ATGFONT_LEFT },         // Bottom left
        { 400, 422, 0, 0, ATGFONT_CENTER_X },    // Bottom center
        { 785, 422, 0, 0, ATGFONT_RIGHT },       // Bottom right
        { 297, 112, 240, 83, ATGFONT_RIGHT },    // L trigger
        { 498, 112, 547, 83, ATGFONT_LEFT }      // R trigger
    };

    if( pEntry->pHelpCallouts == NULL )
        return;

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    ATG::DebugDraw::SetViewProjection( XMMatrixIdentity() );
    for( DWORD i = 0; i < pEntry->dwHelpCalloutCount; ++i )
    {
        ATG::HELP_CALLOUT* pCallout = &pEntry->pHelpCallouts[i];
        if( pCallout->strText == NULL )
            continue;
        if( pCallout->wControl >= ARRAYSIZE( Placements ) )
            continue;
        const HelpPlacement* pPlacement = &Placements[pCallout->wControl];
        if( pPlacement->dwButtonX != 0 )
        {
            ATG::DebugDraw::DrawLineSegment( XMFLOAT3( ( FLOAT )pPlacement->dwButtonX + fXPos,
                                                       ( FLOAT )pPlacement->dwButtonY + fYPos, 0 ),
                                             XMFLOAT3( ( FLOAT )pPlacement->dwTextX + fXPos,
                                                       ( FLOAT )pPlacement->dwTextY + fYPos, 0 ),
                                             0xFFFFFF00 );
        }
    }
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

    if( bActive )
    {
        ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( fXPos + 5, fYPos + 5 ), XMFLOAT2( 794, 29 ), -1, 0xFF606060 );
    }

    m_SettingsFont.Begin();
    m_SettingsFont.SetScaleFactors( 1.0f, 1.0f );
    for( DWORD i = 0; i < pEntry->dwHelpCalloutCount; ++i )
    {
        ATG::HELP_CALLOUT* pCallout = &pEntry->pHelpCallouts[i];
        if( pCallout->strText == NULL )
            continue;
        if( pCallout->wControl >= ARRAYSIZE( Placements ) )
            continue;
        const HelpPlacement* pPlacement = &Placements[pCallout->wControl];
        if( pPlacement->dwButtonX != 0 )
        {
            FLOAT fXPosText = ( FLOAT )pPlacement->dwTextX + fXPos;
            if( pPlacement->dwTextFlags & ATGFONT_RIGHT )
                fXPosText -= 3.0f;
            else
                fXPosText += 3.0f;
            m_SettingsFont.DrawText( fXPosText, ( FLOAT )pPlacement->dwTextY + fYPos, 0xFF80FFFF, pCallout->strText,
                                     pPlacement->dwTextFlags | ATGFONT_CENTER_Y );
        }
    }
    m_SettingsFont.SetScaleFactors( 1.0f, 1.0f );
    m_SettingsFont.DrawText( 400 + fXPos, fYPos + 5, 0xFFFFFFFF, pEntry->strName, ATGFONT_CENTER_X );
    if( bActive )
        m_SettingsFont.DrawText( 5 + fXPos, fYPos + 5, 0xFFFFFFFF, GLYPH_UD_ARROW );
    m_SettingsFont.End();
}


DWORD SettingsPanel::CreateNewGroup( const WCHAR* strTitle )
{
    SettingsGroup sg;
    sg.m_strTitle = strTitle;
    sg.m_bVisible = TRUE;
    sg.m_dwDisplayStartIndex = 0;
    sg.m_bHelpMenu = FALSE;
    m_MenuBarFont.SetScaleFactors( g_fTitleFontSize, g_fTitleFontSize );
    sg.m_fTitleWidth = m_MenuBarFont.GetTextWidth( strTitle );
    DWORD dwIndex = ( DWORD )m_Groups.size();
    m_Groups.push_back( sg );
    return dwIndex;
}


SettingsGroup* SettingsPanel::GetGroup( DWORD dwIndex )
{
    if( dwIndex == ( DWORD )-1 )
        dwIndex = ( DWORD )m_Groups.size() - 1;
    if( dwIndex >= ( DWORD )m_Groups.size() )
        return NULL;
    return &m_Groups[dwIndex];
}


SettingsGroup* SettingsPanel::FindGroup( const WCHAR* strSearch )
{
    DWORD dwCount = GetGroupCount();
    for( DWORD i = 0; i < dwCount; ++i )
    {
        const WCHAR* strName = m_Groups[i].m_strTitle;
        if( _wcsicmp( strName, strSearch ) == 0 )
            return &m_Groups[i];
    }
    return NULL;
}


DWORD SettingsGroup::AddBoolean( BOOL* pBoolValue, const WCHAR* strName )
{
    SettingsEntry Entry;
    Entry.pData = pBoolValue;
    Entry.strName = strName;
    Entry.Type = ST_BOOL;
    DWORD dwIndex = m_Settings.size();
    m_Settings.push_back( Entry );
    return dwIndex;
}

DWORD SettingsGroup::AddFloatBounded( FLOAT* pFloatValue, const WCHAR* strName, FLOAT fMin, FLOAT fMax, FLOAT fSpeed )
{
    SettingsEntry Entry;
    Entry.pData = pFloatValue;
    Entry.strName = strName;
    Entry.fMin = fMin;
    Entry.fMax = fMax;
    Entry.fSpeed = fSpeed;
    Entry.Type = ST_FLOAT;
    DWORD dwIndex = m_Settings.size();
    m_Settings.push_back( Entry );
    return dwIndex;
}

DWORD SettingsGroup::AddFloatStepped( FLOAT* pFloatValue, const WCHAR* strName, FLOAT fMin, FLOAT fMax,
                                      FLOAT fStepValue )
{
    SettingsEntry Entry;
    Entry.pData = pFloatValue;
    Entry.strName = strName;
    Entry.fMin = fMin;
    Entry.fMax = fMax;
    Entry.fSpeed = fStepValue;
    Entry.Type = ST_FLOATSTEP;
    DWORD dwIndex = m_Settings.size();
    m_Settings.push_back( Entry );
    return dwIndex;
}

DWORD SettingsGroup::AddIntegerBounded( INT* pIntValue, const WCHAR* strName, INT iMin, INT iMax, INT iStep )
{
    SettingsEntry Entry;
    Entry.pData = pIntValue;
    Entry.strName = strName;
    Entry.iMin = iMin;
    Entry.iMax = iMax;
    Entry.iStep = iStep;
    Entry.Type = ST_INTEGER;
    DWORD dwIndex = m_Settings.size();
    m_Settings.push_back( Entry );
    return dwIndex;
}

DWORD SettingsGroup::AddEnum( DWORD* pEnumValue, const WCHAR* strName )
{
    SettingsEntry Entry;
    Entry.pData = pEnumValue;
    Entry.strName = strName;
    Entry.iMin = 0;
    Entry.iMax = 0;
    Entry.iEnumIndex = 0;
    Entry.Type = ST_ENUM;
    DWORD dwIndex = m_Settings.size();
    m_Settings.push_back( Entry );
    return dwIndex;
}

VOID SettingsGroup::AddEnumEntry( DWORD dwIndex, const WCHAR* strEnumName, DWORD dwValue )
{
    if( dwIndex == ( DWORD )-1 )
        dwIndex = ( ( DWORD )m_Settings.size() - 1 );
    assert( dwIndex < m_Settings.size() );
    SettingsEnumEntry EnumEntry;
    EnumEntry.strName = strEnumName;
    EnumEntry.dwValue = dwValue;
    DWORD dwCurrentValue = *( DWORD* )m_Settings[dwIndex].pData;
    if( dwValue == dwCurrentValue )
        m_Settings[dwIndex].iEnumIndex = ( INT )m_Settings[dwIndex].EnumEntries.size();
    m_Settings[dwIndex].EnumEntries.push_back( EnumEntry );
    m_Settings[dwIndex].iMax = ( INT )m_Settings[dwIndex].EnumEntries.size() - 1;
}

DWORD SettingsGroup::AddCommand( DWORD dwCommandID, const WCHAR* strName )
{
    assert( dwCommandID != 0 );
    SettingsEntry Entry;
    Entry.pData = NULL;
    Entry.strName = strName;
    Entry.iMax = ( INT )dwCommandID;
    Entry.iMin = 0;
    Entry.Type = ST_COMMAND;
    DWORD dwIndex = m_Settings.size();
    m_Settings.push_back( Entry );
    return dwIndex;
}

DWORD SettingsGroup::AddSeparator( const WCHAR* strTitle )
{
    SettingsEntry Entry;
    Entry.pData = NULL;
    Entry.strName = strTitle;
    Entry.Type = ST_SEPARATOR;
    DWORD dwIndex = m_Settings.size();
    m_Settings.push_back( Entry );
    return dwIndex;
}

DWORD SettingsGroup::AddHelpScreen( const WCHAR* strTitle, ATG::HELP_CALLOUT* pCallouts, DWORD dwCalloutCount )
{
    SettingsEntry Entry;
    Entry.pHelpCallouts = pCallouts;
    Entry.dwHelpCalloutCount = dwCalloutCount;
    Entry.strName = strTitle;
    Entry.Type = ST_HELPSCREEN;
    DWORD dwIndex = m_Settings.size();
    m_Settings.push_back( Entry );
    return dwIndex;
}

VOID SettingsGroup::ClearEnumEntries( DWORD dwIndex )
{
    assert( dwIndex < m_Settings.size() );
    m_Settings[dwIndex].EnumEntries.clear();
    m_Settings[dwIndex].iEnumIndex = 0;
}

VOID SettingsGroup::SetCriticalSection( DWORD dwIndex, CRITICAL_SECTION* pCriticalSection )
{
    if( dwIndex == ( DWORD )-1 )
        dwIndex = ( ( DWORD )m_Settings.size() - 1 );
    assert( dwIndex < m_Settings.size() );
    m_Settings[dwIndex].pCriticalSection = pCriticalSection;
}

VOID SettingsGroup::SetRange( DWORD dwIndex, INT iMin, INT iMax )
{
    assert( dwIndex < m_Settings.size() );
    m_Settings[dwIndex].iMin = iMin;
    m_Settings[dwIndex].iMax = iMax;
}

VOID SettingsGroup::SetRange( DWORD dwIndex, FLOAT fMin, FLOAT fMax )
{
    assert( dwIndex < m_Settings.size() );
    m_Settings[dwIndex].fMin = fMin;
    m_Settings[dwIndex].fMax = fMax;
}

DWORD SettingsGroup::FindEntryIndex( const WCHAR* strTitle )
{
    DWORD dwCount = ( DWORD )m_Settings.size();
    for( DWORD i = 0; i < dwCount; ++i )
    {
        const WCHAR* strEntry = m_Settings[i].strName;
        if( _wcsicmp( strEntry, strTitle ) == 0 )
            return i;
    }
    return ( DWORD )-1;
}

DWORD SettingsEntry::IncrementValue( FLOAT fDeltaTime )
{
    switch( Type )
    {
        case ST_BOOL:
        {
            BOOL bValue = *( BOOL* )pData;
            if( pCriticalSection != NULL ) EnterCriticalSection( pCriticalSection );
            *( BOOL* )pData = !bValue;
            if( pCriticalSection != NULL ) LeaveCriticalSection( pCriticalSection );
            return 0;
        }
        case ST_INTEGER:
        {
            INT iValue = *( INT* )pData;
            iValue += iStep;
            if( iValue > iMax )
                iValue = iMax;
            if( pCriticalSection != NULL ) EnterCriticalSection( pCriticalSection );
            *( INT* )pData = iValue;
            if( pCriticalSection != NULL ) LeaveCriticalSection( pCriticalSection );
            return 0;
        }
        case ST_FLOAT:
        {
            FLOAT fValue = *( FLOAT* )pData;
            fValue += ( fSpeed * fDeltaTime );
            if( fValue > fMax )
                fValue = fMax;
            if( pCriticalSection != NULL ) EnterCriticalSection( pCriticalSection );
            *( FLOAT* )pData = fValue;
            if( pCriticalSection != NULL ) LeaveCriticalSection( pCriticalSection );
            return 0;
        }
        case ST_FLOATSTEP:
        {
            FLOAT fValue = *( FLOAT* )pData;
            fValue += fSpeed;
            if( fValue > fMax )
                fValue = fMax;
            if( pCriticalSection != NULL ) EnterCriticalSection( pCriticalSection );
            *( FLOAT* )pData = fValue;
            if( pCriticalSection != NULL ) LeaveCriticalSection( pCriticalSection );
            return 0;
        }
        case ST_ENUM:
        {
            INT iIndex = iEnumIndex;
            iIndex++;
            if( iIndex > iMax )
                iIndex = 0;
            iEnumIndex = iIndex;
            if( iIndex >= ( INT )EnumEntries.size() )
                return 0;
            if( pCriticalSection != NULL ) EnterCriticalSection( pCriticalSection );
            *( DWORD* )pData = EnumEntries[iIndex].dwValue;
            if( pCriticalSection != NULL ) LeaveCriticalSection( pCriticalSection );
            return 0;
        }
        case ST_COMMAND:
        {
            return ( DWORD )iMax;
        }
    }
    return 0;
}

DWORD SettingsEntry::DecrementValue( FLOAT fDeltaTime )
{
    switch( Type )
    {
        case ST_BOOL:
        {
            BOOL bValue = *( BOOL* )pData;
            if( pCriticalSection != NULL ) EnterCriticalSection( pCriticalSection );
            *( BOOL* )pData = !bValue;
            if( pCriticalSection != NULL ) LeaveCriticalSection( pCriticalSection );
            return 0;
        }
        case ST_INTEGER:
        {
            INT iValue = *( INT* )pData;
            iValue -= iStep;
            if( iValue < iMin )
                iValue = iMin;
            if( pCriticalSection != NULL ) EnterCriticalSection( pCriticalSection );
            *( INT* )pData = iValue;
            if( pCriticalSection != NULL ) LeaveCriticalSection( pCriticalSection );
            return 0;
        }
        case ST_FLOAT:
        {
            FLOAT fValue = *( FLOAT* )pData;
            fValue -= ( fSpeed * fDeltaTime );
            if( fValue < fMin )
                fValue = fMin;
            if( pCriticalSection != NULL ) EnterCriticalSection( pCriticalSection );
            *( FLOAT* )pData = fValue;
            if( pCriticalSection != NULL ) LeaveCriticalSection( pCriticalSection );
            return 0;
        }
        case ST_FLOATSTEP:
        {
            FLOAT fValue = *( FLOAT* )pData;
            fValue -= fSpeed;
            if( fValue < fMin )
                fValue = fMin;
            if( pCriticalSection != NULL ) EnterCriticalSection( pCriticalSection );
            *( FLOAT* )pData = fValue;
            if( pCriticalSection != NULL ) LeaveCriticalSection( pCriticalSection );
            return 0;
        }
        case ST_ENUM:
        {
            INT iIndex = iEnumIndex;
            iIndex--;
            if( iIndex < 0 )
                iIndex = iMax;
            iEnumIndex = iIndex;
            if( iIndex >= ( INT )EnumEntries.size() )
                return 0;
            if( pCriticalSection != NULL ) EnterCriticalSection( pCriticalSection );
            *( DWORD* )pData = EnumEntries[iIndex].dwValue;
            if( pCriticalSection != NULL ) LeaveCriticalSection( pCriticalSection );
            return 0;
        }
        case ST_COMMAND:
        {
            return ( DWORD )iMax;
        }
    }
    return 0;
}



