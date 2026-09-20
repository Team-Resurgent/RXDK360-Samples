#pragma once


//--------------------------------------------------------------------------------------
// Define a message for user connection state changes
//--------------------------------------------------------------------------------------
#define XM_USER_CONNECTIONS_CHANGED  XM_USER

typedef struct
{
    BYTE bUserIndex;
    BOOL fConnected;
} MessageUserConnectionsChanged;

void InitMessageUserConnectionsChanged( XUIMessage* pMsg, MessageUserConnectionsChanged* pData, BYTE bUserIndex,
                                        BOOL fConnected )
{
    XuiMessage( pMsg, XM_USER_CONNECTIONS_CHANGED );
    _XuiMessageExtra( pMsg, ( XUIMessageData* )pData, sizeof( *pData ) );
    pData->bUserIndex = bUserIndex;
    pData->fConnected = fConnected;
}

// Sig: HRESULT OnUserConnectionsChanged( BYTE bUserIndex, BOOL fConnected, BOOL& bHandled )
#define ON_XM_USER_CONNECTIONS_CHANGED( MemberFunc )\
    if( pMessage->dwMessage == XM_USER_CONNECTIONS_CHANGED )\
    {\
        MessageUserConnectionsChanged *pData = ( MessageUserConnectionsChanged* )pMessage->pvData;\
        return MemberFunc( pData->bUserIndex, pData->fConnected, pMessage->bHandled );\
    }


//--------------------------------------------------------------------------------------
// Define player and player array structures
//--------------------------------------------------------------------------------------
struct PlayerInfo
{
public:
                    PlayerInfo()
                    {
                        m_bUserIndex = XUSER_INDEX_NONE;
                        m_fConnected = FALSE;
                        m_szCarModel = 0;
                        m_szCarColor = 0;
                    }

                    ~PlayerInfo()
                    {
                        delete[] m_szCarModel;
                        delete[] m_szCarColor;
                    }

    inline void     SetCarModel( LPCWSTR szCarModel )
    {
        delete[] m_szCarModel;
        m_szCarModel = 0;
        if( szCarModel != 0 && *szCarModel != 0 )
        {
            int size = wcslen( szCarModel ) + 1;
            m_szCarModel = new WCHAR[size];
            wcscpy_s( m_szCarModel, size, szCarModel );
        }
    }

    inline LPCWSTR  GetCarModel()
    {
        return m_szCarModel;
    }

    inline void     SetCarColor( LPCWSTR szCarColor )
    {
        delete[] m_szCarColor;
        m_szCarColor = 0;
        if( szCarColor != 0 && *szCarColor != 0 )
        {
            int size = wcslen( szCarColor ) + 1;
            m_szCarColor = new WCHAR[size];
            wcscpy_s( m_szCarColor, size, szCarColor );
        }
    }

    inline LPCWSTR  GetCarColor()
    {
        return m_szCarColor;
    }

    inline void     SetUserIndex( BYTE bUserIndex )
    {
        m_bUserIndex = bUserIndex;
    }

    inline BYTE     GetUserIndex()
    {
        return m_bUserIndex;
    }

    inline void     SetConnected( BOOL fConnected )
    {
        m_fConnected = fConnected;
    }

    inline BOOL     GetConnected()
    {
        return m_fConnected;
    }

private:

    BYTE m_bUserIndex;
    BOOL m_fConnected;
    LPWSTR m_szCarModel;
    LPWSTR m_szCarColor;
};

struct PlayerArray
{
                PlayerArray()
                {
                    for( BYTE i = 0; i < 4; ++i )
                        players[i].SetUserIndex( i );
                }

    PlayerInfo  players[4];
};



class CMultiInputScene : public CXuiSceneImpl
{
protected:

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_TIMER( OnTimer )
    XUI_END_MSG_MAP()

    PlayerArray* m_pPlayerArray;
    HXUIOBJ m_hContainers[4];

public:

            CMultiInputScene();

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnTimer( XUIMessageTimer* pXUIMessageTimer, BOOL& bHandled );

    XUI_IMPLEMENT_CLASS( CMultiInputScene, L"MultiInputScene", XUI_CLASS_SCENE )
};



class CUserContainerScene : public CXuiSceneImpl
{
protected:

    PlayerInfo* m_pPlayerInfo;

public:

                CUserContainerScene();

    inline void SetPlayerInfo( PlayerInfo* pPlayerInfo );
    inline PlayerInfo* GetPlayerInfo();

    XUI_IMPLEMENT_CLASS( CUserContainerScene, L"UserContainerScene", XUI_CLASS_SCENE )
};


class CUserBase :
public CXuiSceneImpl
{
protected:

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_TRANSITION_START( OnTransitionStart )
    ON_XM_USER_CONNECTIONS_CHANGED( OnUserConnectionsChanged )
    XUI_END_MSG_MAP()

    virtual HRESULT UpdateControls( PlayerInfo* pPlayerInfo );

public:

    HRESULT OnTransitionStart( XUIMessageTransition* pTransData, BOOL& bHandled );
    HRESULT OnUserConnectionsChanged( BYTE bUserIndex, BOOL fConnected, BOOL& bHandled );
};

class CUserNotConnectedScene : public CUserBase
{
protected:

    HRESULT UpdateControls( PlayerInfo* pPlayerInfo );

public:

    XUI_IMPLEMENT_CLASS( CUserNotConnectedScene, L"UserNotConnectedScene", XUI_CLASS_SCENE )
};


class CUserMenuScene : public CUserBase
{
protected:

    HRESULT UpdateControls( PlayerInfo* pPlayerInfo );

public:

    XUI_IMPLEMENT_CLASS( CUserMenuScene, L"UserMenuScene", XUI_CLASS_SCENE )
};


class CSelectCarModelScene : public CUserBase
{
protected:

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()

    HRESULT UpdateControls( PlayerInfo* pPlayerInfo );

public:

    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled );

    XUI_IMPLEMENT_CLASS( CSelectCarModelScene, L"SelectCarModelScene", XUI_CLASS_SCENE )
};



class CSelectCarColorScene : public CUserBase
{
protected:

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()

    HRESULT UpdateControls( PlayerInfo* pPlayerInfo );

public:

    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled );

    XUI_IMPLEMENT_CLASS( CSelectCarColorScene, L"SelectCarColorScene", XUI_CLASS_SCENE )
};


