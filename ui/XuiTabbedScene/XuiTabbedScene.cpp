//----------------------------------------------------------------------------------
// XuiTabbedScene.cpp
//
// An example of a XUI tabbed scene.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include "AtgMediaLocator.h"



//----------------------------------------------------------------------------------
// Name: CXuiWaitList
// Desc: Class used to wait on handles.  When a handle becomes signalled, a 
//       user-specified action (send specified XUI message or call specified 
//       callback function) is taken.
//----------------------------------------------------------------------------------
typedef void (*PFN_WAITCOMPLETION )( void* pvContext );

class CXuiWaitList
{

protected:

    enum
    {
        WAIT_HANDLE_F_NONE      = 0,
        WAIT_HANDLE_F_NOREMOVE  = 1,
    };

    struct WaitEntry
    {
        HXUIOBJ hObj;
        DWORD dwMessageId;
        PFN_WAITCOMPLETION pfnCompletionRoutine;
        void* pvContext;
        DWORD dwFlags;
    };

    int m_nNumWaitHandles;
    HANDLE      m_WaitHandles[ MAXIMUM_WAIT_OBJECTS ];
    WaitEntry   m_WaitEntries[ MAXIMUM_WAIT_OBJECTS ];

    //------------------------------------------------------------------------------
    // Dispatches the registered entry at the specified index.
    //------------------------------------------------------------------------------
    void        DispatchWaitEntry( int nIndex );

    //------------------------------------------------------------------------------
    // Removes the specified index from the wait list.
    //------------------------------------------------------------------------------
    void        RemoveWaitEntry( int nIndex );

public:

    //------------------------------------------------------------------------------
    // Registers a handle for async operations.
    //------------------------------------------------------------------------------
    BOOL        RegisterWaitHandle( HANDLE hWait, HXUIOBJ hObj, DWORD dwMessageId, BOOL bRemoveAfterSignaled );

    //------------------------------------------------------------------------------
    // Registers a callback for when the given event fires.
    //------------------------------------------------------------------------------
    BOOL        RegisterWaitHandleFunc( HANDLE hWait, PFN_WAITCOMPLETION pfnCompletion, void* pvContext,
                                        BOOL bRemoveAfterSignaled );

    //------------------------------------------------------------------------------
    // Removes the specified handle from the wait list.
    //------------------------------------------------------------------------------
    void        UnregisterWaitHandle( HANDLE hWait );

    //------------------------------------------------------------------------------
    // Checks the list of registered wait handles.
    //------------------------------------------------------------------------------
    void        ProcessWaitHandles();

};


//----------------------------------------------------------------------------------
// Name: RegisterWaitHandle
// Desc: Registers a handle for async operations.
//       When the specified handle is signaled, the application will send the 
//       specified message to hObj.  If bRemoveAfterSignaled, the handle is 
//       unregistered before the message is sent.  If the caller needs the handle 
//       to persist, then it must be registered again.
//----------------------------------------------------------------------------------
BOOL CXuiWaitList::RegisterWaitHandle( HANDLE hWait, HXUIOBJ hObj, DWORD dwMessageId, BOOL bRemoveAfterSignaled )
{
    ASSERT( m_nNumWaitHandles < MAXIMUM_WAIT_OBJECTS );
    if( m_nNumWaitHandles >= MAXIMUM_WAIT_OBJECTS )
        return FALSE;

    // Append to the wait handle array.
    m_WaitHandles[ m_nNumWaitHandles ] = hWait;

    memset( &m_WaitEntries[ m_nNumWaitHandles ], 0x00, sizeof( m_WaitEntries[ 0 ] ) );
    m_WaitEntries[ m_nNumWaitHandles ].hObj = hObj;
    m_WaitEntries[ m_nNumWaitHandles ].dwMessageId = dwMessageId;
    if( bRemoveAfterSignaled )
    {
        m_WaitEntries[ m_nNumWaitHandles ].dwFlags = WAIT_HANDLE_F_NONE;
    }
    else
    {
        m_WaitEntries[ m_nNumWaitHandles ].dwFlags = WAIT_HANDLE_F_NOREMOVE;
    }

    ++m_nNumWaitHandles;
    return TRUE;
}


//----------------------------------------------------------------------------------
// Name: RegisterWaitHandleFunc
// Desc: Registers a callback for when the given event fires.
//----------------------------------------------------------------------------------
BOOL CXuiWaitList::RegisterWaitHandleFunc( HANDLE hWait, PFN_WAITCOMPLETION pfnCompletion, void* pvContext,
                                           BOOL bRemoveAfterSignaled )
{
    ASSERT( m_nNumWaitHandles < MAXIMUM_WAIT_OBJECTS );
    if( m_nNumWaitHandles >= MAXIMUM_WAIT_OBJECTS )
        return FALSE;

    // Append to the wait handle array.
    m_WaitHandles[ m_nNumWaitHandles ] = hWait;

    memset( &m_WaitEntries[ m_nNumWaitHandles ], 0x00, sizeof( m_WaitEntries[ 0 ] ) );
    m_WaitEntries[ m_nNumWaitHandles ].pfnCompletionRoutine = pfnCompletion;
    m_WaitEntries[ m_nNumWaitHandles ].pvContext = pvContext;
    if( bRemoveAfterSignaled )
    {
        m_WaitEntries[ m_nNumWaitHandles ].dwFlags = WAIT_HANDLE_F_NONE;
    }
    else
    {
        m_WaitEntries[ m_nNumWaitHandles ].dwFlags = WAIT_HANDLE_F_NOREMOVE;
    }

    ++m_nNumWaitHandles;
    return TRUE;
}


//----------------------------------------------------------------------------------
// Name: ProcessWaitHandles
// Desc: Checks the list of registered wait handles.  If a handle is signaled, the 
//       message that was passed to RegisterWaitHandle is sent to the associated 
//       object.
//----------------------------------------------------------------------------------
void CXuiWaitList::ProcessWaitHandles()
{
    if( m_nNumWaitHandles < 1 )
        return;

    DWORD dwRet = WaitForMultipleObjects( m_nNumWaitHandles, m_WaitHandles, FALSE, 0 );
    if( dwRet >= WAIT_OBJECT_0 && dwRet <= WAIT_OBJECT_0 + ( DWORD )m_nNumWaitHandles )
    {
        DispatchWaitEntry( dwRet - WAIT_OBJECT_0 );
    }
    else if( dwRet >= WAIT_ABANDONED_0 && dwRet <= WAIT_ABANDONED_0 + ( DWORD )m_nNumWaitHandles )
    {
        DispatchWaitEntry( dwRet - WAIT_ABANDONED_0 );
    }
    else
    {
        ASSERT( dwRet == WAIT_TIMEOUT );
    }
}


//----------------------------------------------------------------------------------
// Name: DispatchWaitEntry
// Desc: Dispatches the registered entry at the specified index.
//       This will send the message and remove the handle from the list.
//----------------------------------------------------------------------------------
void CXuiWaitList::DispatchWaitEntry( int nIndex )
{
    ASSERT( nIndex >= 0 && nIndex < m_nNumWaitHandles );
    if( nIndex < 0 || nIndex >= m_nNumWaitHandles )
        return;

    HXUIOBJ hObj = m_WaitEntries[ nIndex ].hObj;
    DWORD dwMessage = m_WaitEntries[ nIndex ].dwMessageId;

    // Now remove the entry from the wait arrays.
    if( ( m_WaitEntries[ nIndex ].dwFlags & WAIT_HANDLE_F_NOREMOVE ) == 0 )
        RemoveWaitEntry( nIndex );

    if( m_WaitEntries[ nIndex ].hObj != NULL )
    {
        // Now send the message.
        XUIMessage msg;
        XuiMessage( &msg, dwMessage );
        XuiSendMessage( hObj, &msg );
    }
    else
    {
        m_WaitEntries[ nIndex ].pfnCompletionRoutine( m_WaitEntries[ nIndex ].pvContext );
    }
}


//----------------------------------------------------------------------------------
// Name: RemoveWaitEntry
// Desc: Removes the specified index from the wait list.
//----------------------------------------------------------------------------------
void CXuiWaitList::RemoveWaitEntry( int nIndex )
{
    if( nIndex < m_nNumWaitHandles - 1 )
    {
        memmove( &m_WaitHandles[ nIndex ], &m_WaitHandles[ nIndex + 1 ], sizeof( HANDLE ) *
                 ( m_nNumWaitHandles - nIndex - 1 ) );
        memmove( &m_WaitEntries[ nIndex ], &m_WaitEntries[ nIndex + 1 ], sizeof( WaitEntry ) *
                 ( m_nNumWaitHandles - nIndex - 1 ) );
    }
    --m_nNumWaitHandles;
}


//----------------------------------------------------------------------------------
// Name: UnregisterWaitHandle
// Desc: Removes the specified handle from the wait list.
//----------------------------------------------------------------------------------
void CXuiWaitList::UnregisterWaitHandle( HANDLE hWait )
{
    int nEntryIndex = -1;
    for( int i = 0; i < m_nNumWaitHandles; ++i )
    {
        if( m_WaitHandles[ i ] == hWait )
        {
            nEntryIndex = i;
            break;
        }
    }
    ASSERT( nEntryIndex >= 0 );
    if( nEntryIndex < 0 )
        return;

    RemoveWaitEntry( nEntryIndex );
}




//----------------------------------------------------------------------------------
// Name: CMyApp
// Desc: Main XUI host class. It is responsible for registering scene classes and 
//       providing basic initialization, scene loading and rendering capability.
//----------------------------------------------------------------------------------
class CMyApp : public CXuiModule
{

public:

    CXuiWaitList m_waitlist;

protected:

    // Override RegisterXuiClasses so that CMyApp can register classes.
    virtual HRESULT RegisterXuiClasses();

    // Override UnregisterXuiClasses so that CMyApp can unregister classes. 
    virtual HRESULT UnregisterXuiClasses();

    // Override ProcessInput so that CMyApp can trap the return-to-Launcher input chord.
    virtual HRESULT ProcessInput();

    // Override RunFrame so that CMyApp can trap the user-defined event.
    virtual void    RunFrame();
};




//----------------------------------------------------------------------------------
// Name: g_myapp
// Desc: Global that represents the application.
//----------------------------------------------------------------------------------
CMyApp g_myapp;




//----------------------------------------------------------------------------------
// Name: CMySummaryScene
// Desc: Implements the scene for the RPG character's name and race choice.
//----------------------------------------------------------------------------------
class CMySummaryScene : public CXuiSceneImpl
{

public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CMySummaryScene, L"MySummaryScene", XUI_CLASS_SCENE )

    //------------------------------------------------------------------------------
    // Method that returns the summary scene if it is a sibling of the given hObj.
    //------------------------------------------------------------------------------
    static CMySummaryScene* GetSummarySceneSibling( HXUIOBJ hObj );

    //------------------------------------------------------------------------------
    // Method to set the value of the Name control in the character summary.
    //------------------------------------------------------------------------------
    HRESULT SetName( LPCWSTR lpszNewName );

    //------------------------------------------------------------------------------
    // Method to set the value of the Race control in the character summary.
    //------------------------------------------------------------------------------
    HRESULT SetRace( LPCWSTR lpszNewRace );

    //------------------------------------------------------------------------------
    // Method to set the value of the Armor control in the character summary.
    //------------------------------------------------------------------------------
    HRESULT SetArmor( LPCWSTR lpszNewArmor );

    //------------------------------------------------------------------------------
    // Method to set the value of the Weapon control in the character summary.
    //------------------------------------------------------------------------------
    HRESULT SetWeapon( LPCWSTR lpszNewWeapon );

    //------------------------------------------------------------------------------
    // Method to set the value of the Spell control in the character summary.
    //------------------------------------------------------------------------------
    HRESULT SetSpell( LPCWSTR lpszNewSpell );

    //------------------------------------------------------------------------------
    // Method to set the value of the Item control in the character summary.
    //------------------------------------------------------------------------------
    HRESULT SetItem( LPCWSTR lpszNewItem );

protected:

    // Control and Element wrapper objects.
    CXuiTextElement m_txtName;
    CXuiTextElement m_txtRace;
    CXuiImageElement m_imgPortrait;
    CXuiTextElement m_txtArmor;
    CXuiTextElement m_txtWeapon;
    CXuiTextElement m_txtSpell;
    CXuiTextElement m_txtItem;

    // Message map.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
    XUI_END_MSG_MAP()

    //------------------------------------------------------------------------------
    // Performs initialization tasks - retrieves controls.
    //------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
};


//----------------------------------------------------------------------------------
// Name: GetSummarySceneSibling
// Desc: Method that returns the summary scene if it is a sibling of a given tabbed 
//       scene.
//----------------------------------------------------------------------------------
CMySummaryScene* CMySummaryScene::GetSummarySceneSibling( HXUIOBJ hObjTab )
{
    // Get the parent tabbed scene.
    CXuiScene tabbedScene( hObjTab );
    if( FAILED( tabbedScene.GetParent( &tabbedScene ) ) )
    {
        return NULL;
    }
    // Get a handle to the sibling Character Summary scene.
    HXUIOBJ hObj = NULL;
    if( FAILED( tabbedScene.GetChildById( L"CharacterSummary", &hObj ) ) )
    {
        return NULL;
    }
    CMySummaryScene* pscnCharacterSummary;
    if( FAILED( XuiObjectFromHandle( hObj, ( VOID** )&pscnCharacterSummary ) ) )
    {
        return NULL;
    }
    return pscnCharacterSummary;
}


//----------------------------------------------------------------------------------
// Name: SetName
// Desc: Method to set the value of the Name control in the character summary.
//----------------------------------------------------------------------------------
HRESULT CMySummaryScene::SetName( LPCWSTR lpszNewName )
{
    if( NULL == lpszNewName || L'\0' == lpszNewName[ 0 ] )
    {
        // Don't allow a blank name.
        return m_txtName.SetText( L"Character Name" );
    }
    return m_txtName.SetText( lpszNewName );
}


//----------------------------------------------------------------------------------
// Name: SetRace
// Desc: Method to set the value of the Race control in the character summary.
//----------------------------------------------------------------------------------
HRESULT CMySummaryScene::SetRace( LPCWSTR lpszNewRace )
{
    if( NULL == lpszNewRace )
    {
        return E_INVALIDARG;
    }
    return m_txtRace.SetText( lpszNewRace );
}


//----------------------------------------------------------------------------------
// Name: SetArmor
// Desc: Method to set the value of the Armor control in the character summary.
//----------------------------------------------------------------------------------
HRESULT CMySummaryScene::SetArmor( LPCWSTR lpszNewArmor )
{
    if( NULL == lpszNewArmor )
    {
        return E_INVALIDARG;
    }
    return m_txtArmor.SetText( lpszNewArmor );
}


//----------------------------------------------------------------------------------
// Name: SetWeapon
// Desc: Method to set the value of the Weapon control in the character summary.
//----------------------------------------------------------------------------------
HRESULT CMySummaryScene::SetWeapon( LPCWSTR lpszNewWeapon )
{
    if( NULL == lpszNewWeapon )
    {
        return E_INVALIDARG;
    }
    return m_txtWeapon.SetText( lpszNewWeapon );
}


//----------------------------------------------------------------------------------
// Name: SetSpell
// Desc: Method to set the value of the Spell control in the character summary.
//----------------------------------------------------------------------------------
HRESULT CMySummaryScene::SetSpell( LPCWSTR lpszNewSpell )
{
    if( NULL == lpszNewSpell )
    {
        return E_INVALIDARG;
    }
    return m_txtSpell.SetText( lpszNewSpell );
}


//----------------------------------------------------------------------------------
// Name: SetItem
// Desc: Method to set the value of the Item control in the character summary.
//----------------------------------------------------------------------------------
HRESULT CMySummaryScene::SetItem( LPCWSTR lpszNewItem )
{
    if( NULL == lpszNewItem )
    {
        return E_INVALIDARG;
    }
    return m_txtItem.SetText( lpszNewItem );
}


//----------------------------------------------------------------------------------
// Name: OnInit
// Desc: Performs initialization tasks - retrieves controls.
//----------------------------------------------------------------------------------
HRESULT CMySummaryScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hr = S_OK;

    // Retrieve controls for later use.
    if( FAILED( hr = GetChildById( L"NameValue", &m_txtName ) ) )
    {
        return hr;
    }
    if( FAILED( hr = GetChildById( L"RaceValue", &m_txtRace ) ) )
    {
        return hr;
    }
    if( FAILED( hr = GetChildById( L"PortraitValue", &m_imgPortrait ) ) )
    {
        return hr;
    }
    if( FAILED( hr = GetChildById( L"EquippedArmorValue", &m_txtArmor ) ) )
    {
        return hr;
    }
    if( FAILED( hr = GetChildById( L"EquippedWeaponValue", &m_txtWeapon ) ) )
    {
        return hr;
    }
    if( FAILED( hr = GetChildById( L"EquippedSpellValue", &m_txtSpell ) ) )
    {
        return hr;
    }
    if( FAILED( hr = GetChildById( L"EquippedMagicItemValue", &m_txtItem ) ) )
    {
        return hr;
    }
    return hr;
}




//----------------------------------------------------------------------------------
// Name: CMyCharacterScene
// Desc: Implements the scene for the RPG character's name and race choice.
//----------------------------------------------------------------------------------
class CMyCharacterScene : public CXuiSceneImpl
{

public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CMyCharacterScene, L"MyCharacterScene", XUI_CLASS_SCENE )

            CMyCharacterScene()
            {
                m_pszName = NULL;
            }

            ~CMyCharacterScene()
            {
                if( NULL != m_hEvent )
                {
                    CloseHandle( m_hEvent );
                    m_hEvent = NULL;
                }
                delete []m_pszName;
                m_pszName = NULL;
            }

protected:

    // Control and Element wrapper objects.
    CXuiEdit m_edtName;
    LPWSTR m_pszName;          // Used by XShowKeyboardUI().
    XOVERLAPPED m_Overlapped;       // Used by XShowKeyboardUI().
    HANDLE m_hEvent;           // Used by XShowKeyboardUI().
    BOOL m_bKeyboardActive;
    CXuiControl m_btnKeyboard;
    CXuiList m_lstRace;
    int m_iSelectedRace;

    // Message map.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
        XUI_ON_XM_KEYUP( OnKeyup )
        XUI_ON_XM_USER( OnUser )
    XUI_END_MSG_MAP()

    //------------------------------------------------------------------------------
    // Performs initialization tasks - retrieves controls.
    //------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );

    //------------------------------------------------------------------------------
    // Handler for the button press message.
    //------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled );

    //------------------------------------------------------------------------------
    // Handler for the key release message.
    //------------------------------------------------------------------------------
    HRESULT OnKeyup( XUIMessageInput* pInputData, BOOL& bHandled );

    //------------------------------------------------------------------------------
    // Handler for a user-defined event.
    //------------------------------------------------------------------------------
    HRESULT OnUser( XUIMessage* pXUIMessage, BOOL& bHandled );

    //------------------------------------------------------------------------------
    // Updates the Character Summary scene.
    //------------------------------------------------------------------------------
    HRESULT UpdateSummaryScene();
};


//----------------------------------------------------------------------------------
// Name: OnInit
// Desc: Performs initialization tasks - retrieves controls.
//----------------------------------------------------------------------------------
HRESULT CMyCharacterScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hr = S_OK;
    m_pszName = NULL;
    m_bKeyboardActive = FALSE;
    m_iSelectedRace = -1;           // Initially, no race has been selected.

    m_hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

    // Retrieve controls for later use.
    if( FAILED( hr = GetChildById( L"NameEdit", &m_edtName ) ) )
    {
        return hr;
    }
    if( FAILED( hr = GetChildById( L"NameVKbdButton", &m_btnKeyboard ) ) )
    {
        return hr;
    }
    if( FAILED( hr = GetChildById( L"RaceList", &m_lstRace ) ) )
    {
        return hr;
    }
    UINT cchMax = m_edtName.GetTextLimit() + 1;
    if( !m_pszName )
    {
        m_pszName = new WCHAR[ cchMax ];
    }
    if( m_pszName )
    {
        m_pszName[ 0 ] = L'\0';
    }
    else
    {
        return E_OUTOFMEMORY;
    }

    return hr;
}


//----------------------------------------------------------------------------------
// Name: OnNotifyPress
// Desc: Handler for the button press message.
//----------------------------------------------------------------------------------
HRESULT CMyCharacterScene::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
    HRESULT hr = S_OK;
    if( hObjPressed == m_lstRace )
    {
        // Make sure that the pressed item is the only one checked.
        CXuiList list = m_lstRace;
        CXuiListItem item;
        int iSelected = list.GetCurSel( &item );
        if( -1 != m_iSelectedRace )
        {
            list.SetItemCheck( m_iSelectedRace, FALSE );
        }
        list.SetItemCheck( iSelected, TRUE );
        m_iSelectedRace = iSelected;
        if( FAILED( hr = UpdateSummaryScene() ) )
        {
            return E_FAIL;
        }
        bHandled = TRUE;
    }
    if( ( hObjPressed == m_btnKeyboard ) && ( !m_bKeyboardActive ) )
    {
        if( NULL == m_hEvent )
        {
            return E_FAIL;
        }
        ZeroMemory( &m_Overlapped, sizeof( XOVERLAPPED ) );
        m_Overlapped.hEvent = m_hEvent;
        g_myapp.m_waitlist.RegisterWaitHandle(
            m_Overlapped.hEvent,
            m_hObj,
            XM_USER,
            TRUE
            );
        DWORD dwResult = XShowKeyboardUI(
            XUSER_INDEX_ANY,
            0,
            m_edtName.GetText(),
            L"Character Name",
            L"Enter the name of your character:",
            m_pszName,
            m_edtName.GetTextLimit() + 1,
            &m_Overlapped
            );
        if( ERROR_IO_PENDING != dwResult )
        {
            return E_UNEXPECTED;
        }
        m_bKeyboardActive = TRUE;
        // Disable input to the scene until the virtual keyboard closes.
        // This is to close the window of opportunity for the user to
        // send input to the scene while the virtual keyboard opens or closes.
        SetEnable( FALSE );
        bHandled = TRUE;
    }
    return hr;
}


//----------------------------------------------------------------------------------
// Name: OnKeyup
// Desc: Handler for the key release message.
//----------------------------------------------------------------------------------
HRESULT CMyCharacterScene::OnKeyup( XUIMessageInput* pInputData, BOOL& bHandled )
{
    HRESULT hr = S_OK;
    // Copy the XuiEdit value into the Character Summary scene.
    if( FAILED( hr = UpdateSummaryScene() ) )
    {
        return E_FAIL;
    }
    bHandled = TRUE;
    return hr;
}


//----------------------------------------------------------------------------------
// Name: OnUser
// Desc: Handler for the user-defined event: closing the virtual keyboard.
//----------------------------------------------------------------------------------
HRESULT CMyCharacterScene::OnUser( XUIMessage* pXUIMessage, BOOL& bHandled )
{
    HRESULT hr = S_OK;

    // Once the virtual keyboard has closed, update the Character Summary scene.
    ASSERT( XHasOverlappedIoCompleted( &m_Overlapped ) );
    m_bKeyboardActive = FALSE;
    SetEnable( TRUE );

    // Do not overwrite the value of the edit control with the keyboard's value
    // if the user has closed the virtual keyboard by cancelling it or pressing
    // the Guide button (ERROR_CANCELLED), or if any other error occurs.
    if( ERROR_SUCCESS == m_Overlapped.dwExtendedError )
    {
        if( FAILED( hr = m_edtName.SetText( m_pszName ) ) )
        {
            return hr;
        }
        if( FAILED( hr = UpdateSummaryScene() ) )
        {
            return hr;
        }
        bHandled = TRUE;
    }
    return hr;
}


//----------------------------------------------------------------------------------
// Name: UpdateSummaryScene
// Desc: Updates the Character Summary scene with both the name and race choice.
//----------------------------------------------------------------------------------
HRESULT CMyCharacterScene::UpdateSummaryScene()
{
    HRESULT hr = S_OK;
    CMySummaryScene* pscnCharacterSummary = CMySummaryScene::GetSummarySceneSibling( m_hObj );
    if( NULL == pscnCharacterSummary )
    {
        return E_FAIL;
    }
    if( FAILED( hr = pscnCharacterSummary->SetName( m_edtName.GetText() ) ) )
    {
        return hr;
    }
    if( -1 != m_iSelectedRace )
    {
        CXuiListItem itemSelected;
        m_lstRace.GetItemControl( m_iSelectedRace, &itemSelected );
        hr = pscnCharacterSummary->SetRace( itemSelected.GetText() );
    }
    return hr;
}




//----------------------------------------------------------------------------------
// Name: OnNotifyPress
// Desc: Implements the scene for the RPG character's armor and weapon choice.
//----------------------------------------------------------------------------------
class CMyEquipmentScene : public CXuiSceneImpl
{

public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CMyEquipmentScene, L"MyEquipmentScene", XUI_CLASS_SCENE )

protected:

    // Control and Element wrapper objects.
    CXuiList m_lstArmor;
    int m_iSelectedArmor;
    CXuiList m_lstWeapons;
    int m_iSelectedWeapon;

    // Message map.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()

    //------------------------------------------------------------------------------
    // Performs initialization tasks - retrieves controls.
    //------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );

    //------------------------------------------------------------------------------
    // Handler for the button press message.
    //------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled );
};


//----------------------------------------------------------------------------------
// Name: OnInit
// Desc: Performs initialization tasks - retrieves controls.
//----------------------------------------------------------------------------------
HRESULT CMyEquipmentScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hr = S_OK;
    m_iSelectedArmor = -1;   // initially, none are selected
    m_iSelectedWeapon = -1;   // initially, none are selected

    // Retrieve controls for later use.
    if( FAILED( hr = GetChildById( L"ArmorList", &m_lstArmor ) ) )
    {
        return hr;
    }
    if( FAILED( hr = GetChildById( L"WeaponsList", &m_lstWeapons ) ) )
    {
        return hr;
    }
    return hr;
}


//----------------------------------------------------------------------------------
// Name: OnNotifyPress
// Desc: Handler for the button press message.
//----------------------------------------------------------------------------------
HRESULT CMyEquipmentScene::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
    HRESULT hr = S_OK;
    if( hObjPressed == m_lstArmor )
    {
        // Make sure that the pressed item is the only one checked.
        CXuiList list = m_lstArmor;
        CXuiListItem item;
        int iSelected = list.GetCurSel( &item );
        if( -1 != m_iSelectedArmor )
        {
            list.SetItemCheck( m_iSelectedArmor, FALSE );
        }
        list.SetItemCheck( iSelected, TRUE );
        m_iSelectedArmor = iSelected;
        CMySummaryScene* pscnCharacterSummary = CMySummaryScene::GetSummarySceneSibling( m_hObj );
        if( NULL == pscnCharacterSummary )
        {
            return E_FAIL;
        }
        if( FAILED( hr = pscnCharacterSummary->SetArmor( item.GetText() ) ) )
        {
            return hr;
        }
        bHandled = TRUE;
    }
    if( hObjPressed == m_lstWeapons )
    {
        // Make sure that the pressed item is the only one checked.
        CXuiList list = m_lstWeapons;
        CXuiListItem item;
        int iSelected = list.GetCurSel( &item );
        if( -1 != m_iSelectedWeapon )
        {
            list.SetItemCheck( m_iSelectedWeapon, FALSE );
        }
        list.SetItemCheck( iSelected, TRUE );
        m_iSelectedWeapon = iSelected;
        CMySummaryScene* pscnCharacterSummary = CMySummaryScene::GetSummarySceneSibling( m_hObj );
        if( NULL == pscnCharacterSummary )
        {
            return E_FAIL;
        }
        if( FAILED( hr = pscnCharacterSummary->SetWeapon( item.GetText() ) ) )
        {
            return hr;
        }
        bHandled = TRUE;
    }
    return hr;
}




//----------------------------------------------------------------------------------
// Name: CMyMagicScene
// Desc: Implements the scene for the RPG character's magical equipment.
//----------------------------------------------------------------------------------
class CMyMagicScene : public CXuiSceneImpl
{

public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CMyMagicScene, L"MyMagicScene", XUI_CLASS_SCENE )

protected:

    // Control and Element wrapper objects.
    CXuiList m_lstSpells;
    int m_iSelectedSpell;
    CXuiList m_lstItems;
    int m_iSelectedItem;

    // Message map.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()

    //------------------------------------------------------------------------------
    // Performs initialization tasks - retrieves controls.
    //------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );

    //------------------------------------------------------------------------------
    // Handler for the button press message.
    //------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled );
};


//----------------------------------------------------------------------------------
// Name: OnInit
// Performs initialization tasks - retrieves controls.
//----------------------------------------------------------------------------------
HRESULT CMyMagicScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hr = S_OK;
    m_iSelectedItem = -1;   // initially, none are selected
    m_iSelectedSpell = -1;   // initially, none are selected

    // Retrieve controls for later use.
    if( FAILED( hr = GetChildById( L"SpellsList", &m_lstSpells ) ) )
    {
        return hr;
    }
    if( FAILED( hr = GetChildById( L"ItemsList", &m_lstItems ) ) )
    {
        return hr;
    }
    return hr;
}


//----------------------------------------------------------------------------------
// Name: OnNotifyPress
// Desc: Handler for the button press message.
//----------------------------------------------------------------------------------
HRESULT CMyMagicScene::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
    HRESULT hr = S_OK;
    if( hObjPressed == m_lstSpells )
    {
        // Make sure that the pressed item is the only one checked.
        CXuiList list = m_lstSpells;
        CXuiListItem item;
        int iSelected = list.GetCurSel( &item );
        if( -1 != m_iSelectedSpell )
        {
            list.SetItemCheck( m_iSelectedSpell, FALSE );
        }
        list.SetItemCheck( iSelected, TRUE );
        m_iSelectedSpell = iSelected;
        CMySummaryScene* pscnCharacterSummary = CMySummaryScene::GetSummarySceneSibling( m_hObj );
        if( NULL == pscnCharacterSummary )
        {
            return E_FAIL;
        }
        if( FAILED( hr = pscnCharacterSummary->SetSpell( item.GetText() ) ) )
        {
            return hr;
        }
        bHandled = TRUE;
    }
    if( hObjPressed == m_lstItems )
    {
        // Make sure that the pressed item is the only one checked.
        CXuiList list = m_lstItems;
        CXuiListItem item;
        int iSelected = list.GetCurSel( &item );
        if( -1 != m_iSelectedItem )
        {
            list.SetItemCheck( m_iSelectedItem, FALSE );
        }
        list.SetItemCheck( iSelected, TRUE );
        m_iSelectedItem = iSelected;
        CMySummaryScene* pscnCharacterSummary = CMySummaryScene::GetSummarySceneSibling( m_hObj );
        if( NULL == pscnCharacterSummary )
        {
            return E_FAIL;
        }
        if( FAILED( hr = pscnCharacterSummary->SetItem( item.GetText() ) ) )
        {
            return hr;
        }
        bHandled = TRUE;
    }
    return hr;
}




//----------------------------------------------------------------------------------
// Tabbed scene implementation class.
//----------------------------------------------------------------------------------
class CMyTabbedScene : public CXuiTabSceneImpl
{

public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CMyTabbedScene, L"MyTabbedScene", XUI_CLASS_TABSCENE )

protected:

    // Control and Element wrapper objects.
    CXuiControl m_btnExit;

    // Message map.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()

    //------------------------------------------------------------------------------
    // Performs initialization tasks - retrieves controls.
    //------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );

    //------------------------------------------------------------------------------
    // Handler for the button press message.
    //------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled );
};


//----------------------------------------------------------------------------------
// Name: OnInit
// Desc: Performs initialization tasks - retrieves controls.
//----------------------------------------------------------------------------------
HRESULT CMyTabbedScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hr = S_OK;

    // Retrieve controls for later use.
    if( FAILED( hr = GetChildById( L"ExitButton", &m_btnExit ) ) )
    {
        return hr;
    }
    return hr;
}


//----------------------------------------------------------------------------------
// Name: OnNotifyPress
// Desc: Handler for the button press message.
//----------------------------------------------------------------------------------
HRESULT CMyTabbedScene::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
    if( hObjPressed == m_btnExit )
    {
        XLaunchNewImage( "", 0 );
    }
    return S_OK;
}




//----------------------------------------------------------------------------------
// Name: RegisterXuiClasses
// Desc: Registers all of the scene classes.
//----------------------------------------------------------------------------------
HRESULT CMyApp::RegisterXuiClasses()
{
    HRESULT hr = S_OK;
    if( FAILED( hr = CMySummaryScene::Register() ) )
    {
        return hr;
    }
    if( FAILED( hr = CMyCharacterScene::Register() ) )
    {
        return hr;
    }
    if( FAILED( hr = CMyEquipmentScene::Register() ) )
    {
        return hr;
    }
    if( FAILED( hr = CMyMagicScene::Register() ) )
    {
        return hr;
    }
    return CMyTabbedScene::Register();
}


//----------------------------------------------------------------------------------
// Name: UnregisterXuiClasses
// Desc: Unregisters all the scene classes.
//----------------------------------------------------------------------------------
HRESULT CMyApp::UnregisterXuiClasses()
{
    HRESULT hr = S_OK;
    if( FAILED( hr = CMySummaryScene::Unregister() ) )
    {
        return hr;
    }
    if( FAILED( hr = CMyCharacterScene::Unregister() ) )
    {
        return hr;
    }
    if( FAILED( hr = CMyEquipmentScene::Unregister() ) )
    {
        return hr;
    }
    if( FAILED( hr = CMyMagicScene::Unregister() ) )
    {
        return hr;
    }
    return CMyTabbedScene::Unregister();
}


//----------------------------------------------------------------------------------
// Name: ProcessInput
// Desc: Handles application input before any other handler.
//----------------------------------------------------------------------------------
HRESULT CMyApp::ProcessInput()
{
    // Find out if any controller is requesting that the sample reboot to the Launcher
    // using the Left Trigger + Right Trigger + Right Shoulder Button chord.
    for( DWORD dwPort = 0; dwPort < XUSER_MAX_COUNT; ++dwPort )
    {
        XINPUT_STATE state =
        {
            0
        };
        if(
            ( ERROR_SUCCESS == XInputGetState( dwPort, &state ) )
            &&
            ( state.Gamepad.bLeftTrigger > 128 )
            &&
            ( state.Gamepad.bRightTrigger > 128 )
            &&
            ( state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
            )
        {
            XLaunchNewImage( "", 0 );
        }
    }

    // Otherwise, allow XUI to process the input normally.
    XINPUT_KEYSTROKE keyStroke;
    if( ERROR_SUCCESS ==
        XInputGetKeystroke( XUSER_INDEX_ANY, XINPUT_FLAG_ANYDEVICE, &keyStroke ) )
    {
        XuiProcessInput( &keyStroke );
    }

    return S_OK;
}


//----------------------------------------------------------------------------------
// Name: RunFrame
// Desc: Overrides CXuiModule::RunFrame() to additionally poll the event handlers.
//----------------------------------------------------------------------------------
void CMyApp::RunFrame()
{
    m_waitlist.ProcessWaitHandles();
    CXuiModule::RunFrame();
}




//----------------------------------------------------------------------------------
// Name: main
// Desc: Application entry point.
//----------------------------------------------------------------------------------
INT __cdecl main()
{
    // Declare helper necessary to locate resources inside an xzp archive.
    ATG::MediaLocator mediaLocator( L"file://game:/media/tabbedscene.xzp" );
    WCHAR szResourceLocator[ ATG::LOCATOR_SIZE ];

    // Initialize the application.    
    HRESULT hr = g_myapp.Init( XuiD3DXTextureLoader );
    if( FAILED( hr ) )
    {
        OutputDebugString( "Failed intializing application.\n" );
        return 0;
    }

    // Register a default typeface.
    hr = g_myapp.RegisterDefaultTypeface( L"Arial Unicode MS", L"file://game:/media/xarialuni.ttf" );
    if( FAILED( hr ) )
    {
        OutputDebugString( "Failed to register default typeface.\n" );
        return 0;
    }

    // Load the skin file used for the scene.
    mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", L"simple_scene_skin.xur" ); 
    hr = g_myapp.LoadSkin( szResourceLocator );
    if( FAILED( hr ) )
    {
        OutputDebugString( "Failed to load the skin.\n" );
        return 0;
    }

    // Load the scene.
    mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", NULL ); 
    hr = g_myapp.LoadFirstScene( szResourceLocator, L"tabbed_scene.xur", NULL );
    if( FAILED( hr ) )
    {
        OutputDebugString( "Failed to load the scene.\n" );
        return 0;
    }

    // Run the scene.    
    g_myapp.Run();

    // Free resources, unregister custom classes, and exit.
    g_myapp.Uninit();
}
