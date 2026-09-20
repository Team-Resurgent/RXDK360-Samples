//--------------------------------------------------------------------------------------
// SettingsUI.h
//
// This class is a simple yet flexible UI for tagging and manipulating variables in
// memory.  SceneViewer2 uses it to change many different scene settings which are
// stored as member variables in the SceneViewer2 class.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef SETTINGSUI_H
#define SETTINGSUI_H

#include <xtl.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <vector>

enum SettingType
{
    ST_INTEGER = 0,
    ST_BOOL,
    ST_FLOAT,
    ST_FLOATSTEP,
    ST_ENUM,
    ST_COMMAND,
    ST_SEPARATOR,
    ST_HELPSCREEN,
};

struct SettingsEnumEntry
{
    const WCHAR* strName;
    DWORD dwValue;
};

struct SettingsEntry
{
            SettingsEntry() : pData( NULL ),
                              pCriticalSection( NULL )
            {
            }
    SettingType Type;
    union
    {
        VOID* pData;
        ATG::HELP_CALLOUT* pHelpCallouts;
    };
    const WCHAR* strName;
    union
    {
        INT iMin;
        DWORD dwHelpCalloutCount;
    };
    INT iMax;
    FLOAT fMin;
    FLOAT fMax;
    FLOAT fSpeed;
    INT iStep;
    INT iEnumIndex;
    CRITICAL_SECTION* pCriticalSection;
    std::vector <SettingsEnumEntry> EnumEntries;
    BOOL bEnabled;

    DWORD   IncrementValue( FLOAT fDeltaTime );
    DWORD   DecrementValue( FLOAT fDeltaTime );
};

typedef std::vector <SettingsEntry> SettingsEntryList;

class SettingsGroup
{
public:
            SettingsGroup() : m_strTitle( NULL )
            {
            }
    DWORD   AddSpace();
    DWORD   AddHeading( const WCHAR* strName );
    DWORD   AddBoolean( BOOL* pBoolValue, const WCHAR* strName );
    DWORD   AddFloatStepped( FLOAT* pFloatValue, const WCHAR* strName, FLOAT fMin, FLOAT fMax, FLOAT fStepValue );
    DWORD   AddFloatBounded( FLOAT* pFloatValue, const WCHAR* strName, FLOAT fMin, FLOAT fMax, FLOAT fSpeed );
    DWORD   AddFloat( FLOAT* pFloatValue, const WCHAR* strName )
    {
        return AddFloatBounded( pFloatValue, strName, -1e10f, 1e10f, 1.0f );
    }
    DWORD   AddIntegerBounded( INT* pIntValue, const WCHAR* strName, INT iMin, INT iMax, INT iStep = 1 );
    DWORD   AddInteger( INT* pIntValue, const WCHAR* strName, INT iStep = 1 )
    {
        return AddIntegerBounded( pIntValue, strName, INT_MIN, INT_MAX, iStep );
    }
    DWORD   AddEnum( DWORD* pEnumValue, const WCHAR* strName );
    VOID    AddEnumEntry( DWORD dwIndex, const WCHAR* strEnumName, DWORD dwValue );
    DWORD   AddCommand( DWORD dwCommandID, const WCHAR* strName );
    DWORD   AddSeparator( const WCHAR* strTitle = NULL );
    DWORD   AddHelpScreen( const WCHAR* strTitle, ATG::HELP_CALLOUT* pCallouts, DWORD dwCalloutCount );
    VOID    ClearEnumEntries( DWORD dwIndex );
    VOID    SetCriticalSection( DWORD dwIndex, CRITICAL_SECTION* pCriticalSection );
    VOID    SetRange( DWORD dwIndex, INT iMin, INT iMax );
    VOID    SetRange( DWORD dwIndex, FLOAT fMin, FLOAT fMax );

    DWORD   FindEntryIndex( const WCHAR* strTitle );
    SettingsEntry* FindEntry( const WCHAR* strTitle )
    {
        return GetEntry( FindEntryIndex( strTitle ) );
    }
    SettingsEntry* GetEntry( DWORD dwIndex )
    {
        if( dwIndex == ( DWORD )-1 )
            return NULL;
        return &m_Settings[dwIndex];
    }
public:
    const WCHAR* m_strTitle;
    FLOAT m_fTitleWidth;
    SettingsEntryList m_Settings;
    BOOL m_bVisible;
    BOOL m_bHelpMenu;
    DWORD m_dwDisplayStartIndex;
};

typedef std::vector <SettingsGroup> SettingsGroupList;

class SettingsPanel
{
public:
    enum ActivationState
    {
        AS_DISABLED = 0,
        AS_STARTUP,
        AS_VISIBLE,
        AS_SHUTDOWN
    };
public:
    VOID    Initialize( D3DDevice* pd3dDevice );
    DWORD   Update( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, FLOAT fAppTime );
    VOID    Render();

    VOID    SetVisible( BOOL bVisible );
    BOOL    IsVisible() const
    {
        return m_ActivationState != AS_DISABLED;
    }
    BOOL    HasFocus() const
    {
        return m_ActivationState == AS_STARTUP || m_ActivationState == AS_VISIBLE;
    }
    VOID    ShowMenuItem( DWORD dwMenuIndex, DWORD dwItemIndex );

    DWORD   CreateNewGroup( const WCHAR* strTitle );
    SettingsGroup* GetGroup( DWORD dwIndex = ( DWORD )-1 );
    SettingsGroup* FindGroup( const WCHAR* strSearch );
    DWORD   GetGroupCount() const
    {
        return ( DWORD )m_Groups.size();
    }
    VOID    SetCurrentGroup( DWORD dwIndex )
    {
        m_dwCurrentGroupIndex = dwIndex;
    }

protected:
    SettingsEntry* GetCurrentSetting();
    VOID    RenderMenuBar();
    VOID    RenderGroup( SettingsGroup* pGroup );
    VOID    RenderHelp( SettingsEntry* pEntry, FLOAT fXPos, FLOAT fYPos, BOOL bActive );
    VOID    RenderOneEntry( SettingsEntry* pEntry, FLOAT fXPos, FLOAT fYPos, FLOAT fAlpha, BOOL bActive );

protected:
    SettingsGroupList m_Groups;

    ActivationState m_ActivationState;
    FLOAT m_fActivationTime;
    FLOAT m_fTransitionPercent;
    FLOAT m_fDebounceTime;
    FLOAT m_fDeltaTime;
    DWORD m_dwReturnValue;

    ATG::Font m_MenuBarFont;
    ATG::Font m_SettingsFont;
    D3DDevice* m_pd3dDevice;

    ATG::PackedResource m_MenuArtwork;
    D3DTexture* m_pMenuBandTexture;
    D3DTexture* m_pGroupBoxTexture;
    D3DTexture* m_pGroupHelpBoxTexture;

    DWORD m_dwCurrentGroupIndex;
    DWORD m_dwCurrentSettingIndex;
};

#endif
