//--------------------------------------------------------------------------------------
// Player.h
//
// Per controller player class
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_PLAYER_H
#define ARCADESAMPLE_PLAYER_H

#include "XuiPaddle.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// PlayerInfo
//--------------------------------------------------------------------------------------
struct PlayerInfo
{
    PlayerInfo() : Xuid( 0 ),
                   Team( eTeam_Invalid ),
                   Gamertag()
    {
    }

    XUID Xuid;
    eTeam Team;
    std::wstring Gamertag;
};

//--------------------------------------------------------------------------------------
// Player
//--------------------------------------------------------------------------------------
class Player : public IAppXuiControl
{
public:
    // Default Ctor - AI Player
                        Player( eTeam team );

    // Ctor - Human Player
                        Player( eTeam team, IQNetPlayer* pQNetPlayer );

    virtual             ~Player()
    {
    }

    BOOL                IsAI() const
    {
        return m_pQNetPlayer == NULL;
    }
    VOID                ConvertToAI()
    {
        m_pQNetPlayer = NULL;
    }
    VOID                SetQNetPlayer( IQNetPlayer* pQNetPlayer )
    {
        m_pQNetPlayer = pQNetPlayer;
    }

    // IQNetPlayer Interface
    XUID                GetXuid();
    DWORD               GetUserIndex();
    LPCWSTR             GetGamertag();
    XONLINE_NAT_TYPE    GetNatType();
    HRESULT             GetXnaddr( XNADDR* pxnaddr );
    DWORD               GetCurrentRtt();
    BOOL                IsLocal();
    BOOL                IsHost();
    BOOL                IsPrivateSlot();
    BOOL                HasVoice();
    BOOL                HasCamera();
    BOOL                IsTalking();
    BOOL                IsReady();
    BOOL                IsMutedByLocalUser( DWORD dwUserIndex );
    BOOL                IsSameSystem( Player* pPlayerOther );
    HRESULT             SetReady( BOOL bReady );
    HRESULT             SendData( Player* pPlayerTarget, const void* pvData, DWORD dwDataSize, DWORD dwFlags );
        HRESULT WriteStats( DWORD dwNumViews, CONST XSESSION_VIEW_PROPERTIES* pViews );
    VOID                GetCommFilter( DWORD* pdwVoiceSendMask, DWORD* pdwVoiceRecvMask, DWORD* pdwCameraSendMask,
                                       DWORD* pdwCameraRecvMask );
    HRESULT             SetCommFilter( DWORD dwVoiceSendMask, DWORD dwVoiceRecvMask, DWORD dwCameraSendMask,
                                       DWORD dwCameraRecvMask );

    eTeam               GetTeam() const
    {
        return m_Team;
    }
    IQNetPlayer* QNetPlayer()
    {
        return m_pQNetPlayer;
    }

    VOID                BindXuiPaddleControl( CXuiControl* ctrl );
    VOID                UnbindXuiPaddleControl();

    VOID                GetPaddleDetails( MSG_PADDLE_UPDATE* pMsg );
    VOID                SetPaddleDetails( MSG_PADDLE_UPDATE const& msg );

    VOID                ResetPaddle( Rect const& arenaRect );

    BOOL                CollideAndUpdate( XuiBall& ball );

    VOID                ProcessInput( FLOAT speed, Rect const& arena );
    VOID                AIMovePaddle( FLOAT speed, Rect const& arena, Rect const& ball );

protected:
    virtual VOID        OnUpdateXui();

private:
    eTeam m_Team;
    DWORD m_Uid;

    IQNetPlayer* m_pQNetPlayer;
    XuiPaddle m_XuiPaddle;
};

//--------------------------------------------------------------------------------------
// Player GetXuid
//--------------------------------------------------------------------------------------
inline XUID Player::GetXuid()
{
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->GetXuid()
        : static_cast<XUID>( m_Uid );
}

//--------------------------------------------------------------------------------------
// Player GetUserIndex
//--------------------------------------------------------------------------------------
inline DWORD Player::GetUserIndex()
{
    ATG_Verify( m_pQNetPlayer );
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->GetUserIndex()
        : -1;
}

//--------------------------------------------------------------------------------------
// Player GetGamertag
//--------------------------------------------------------------------------------------
inline LPCWSTR Player::GetGamertag()
{
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->GetGamertag()
        : L"Computer Player";
}

//--------------------------------------------------------------------------------------
// Player GetNatType
//--------------------------------------------------------------------------------------
inline XONLINE_NAT_TYPE Player::GetNatType()
{
    ATG_Verify( m_pQNetPlayer );
    return m_pQNetPlayer->GetNatType();
}

//--------------------------------------------------------------------------------------
// Player GetXnaddr
//--------------------------------------------------------------------------------------
inline HRESULT Player::GetXnaddr( XNADDR* pxnaddr )
{
    ATG_Verify( m_pQNetPlayer );
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->GetXnaddr( pxnaddr )
        : E_FAIL;
}

//--------------------------------------------------------------------------------------
// Player GetCurrentRtt
//--------------------------------------------------------------------------------------
inline DWORD Player::GetCurrentRtt()
{
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->GetCurrentRtt()
        : 0;
}

//--------------------------------------------------------------------------------------
// Player IsLocal
//--------------------------------------------------------------------------------------
inline BOOL Player::IsLocal()
{
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->IsLocal()
        : TRUE;
}

//--------------------------------------------------------------------------------------
// Player IsHost
//--------------------------------------------------------------------------------------
inline BOOL Player::IsHost()
{
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->IsHost()
        : TRUE;
}

//--------------------------------------------------------------------------------------
// Player IsPrivateSlot
//--------------------------------------------------------------------------------------
inline BOOL Player::IsPrivateSlot()
{
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->IsPrivateSlot()
        : FALSE;
}

//--------------------------------------------------------------------------------------
// Player HasVoice
//--------------------------------------------------------------------------------------
inline BOOL Player::HasVoice()
{
    ATG_Verify( m_pQNetPlayer );
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->HasVoice()
        : FALSE;
}

//--------------------------------------------------------------------------------------
// Player HasCamera
//--------------------------------------------------------------------------------------
inline BOOL Player::HasCamera()
{
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->HasCamera()
        : FALSE;
}

//--------------------------------------------------------------------------------------
// Player IsTalking
//--------------------------------------------------------------------------------------
inline BOOL Player::IsTalking()
{
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->IsTalking()
        : FALSE;
}

//--------------------------------------------------------------------------------------
// Player IsReady
//--------------------------------------------------------------------------------------
inline BOOL Player::IsReady()
{
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->IsReady()
        : TRUE;
}

//--------------------------------------------------------------------------------------
// Player IsMutedByLocalUser
//--------------------------------------------------------------------------------------
inline BOOL Player::IsMutedByLocalUser( DWORD dwUserIndex )
{
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->IsMutedByLocalUser( dwUserIndex )
        : FALSE;
}

//--------------------------------------------------------------------------------------
// Player IsSameSystem
//--------------------------------------------------------------------------------------
inline BOOL Player::IsSameSystem( Player* pPlayerOther )
{
    return ( this->IsAI() )
        ? pPlayerOther->m_pQNetPlayer->IsLocal()
        : m_pQNetPlayer->IsSameSystem( pPlayerOther->m_pQNetPlayer );
}

//--------------------------------------------------------------------------------------
// Player set ready
//--------------------------------------------------------------------------------------
inline HRESULT Player::SetReady( BOOL bReady )
{
    ATG_Verify( m_pQNetPlayer );
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->SetReady( bReady )
        : E_FAIL;
}

//--------------------------------------------------------------------------------------
// Player send data
//--------------------------------------------------------------------------------------
inline HRESULT Player::SendData( Player* pPlayerTarget, const void* pvData, DWORD dwDataSize, DWORD dwFlags )
{
    ATG_Verify( m_pQNetPlayer );
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->SendData(
        ( pPlayerTarget ) ? pPlayerTarget->m_pQNetPlayer : NULL, pvData, dwDataSize, dwFlags )
        : E_FAIL;
}

//--------------------------------------------------------------------------------------
// Player Write Stats
//--------------------------------------------------------------------------------------
    inline HRESULT Player::WriteStats( DWORD dwNumViews, CONST XSESSION_VIEW_PROPERTIES* pViews )
 {
        ATG_Verify( m_pQNetPlayer );
        return ( m_pQNetPlayer )
            ? m_pQNetPlayer->WriteStats( dwNumViews, pViews )
            : E_FAIL;
    }

//--------------------------------------------------------------------------------------
// Player get communication filter
//--------------------------------------------------------------------------------------
inline VOID Player::GetCommFilter( DWORD* pdwVoiceSendMask, DWORD* pdwVoiceRecvMask, DWORD* pdwCameraSendMask,
                                   DWORD* pdwCameraRecvMask )
{
    ATG_Verify( m_pQNetPlayer );
    if( m_pQNetPlayer )
        m_pQNetPlayer->GetCommFilter( pdwVoiceSendMask, pdwVoiceRecvMask, pdwCameraSendMask, pdwCameraRecvMask );
}

//--------------------------------------------------------------------------------------
// Player set communication filter
//--------------------------------------------------------------------------------------
inline HRESULT Player::SetCommFilter( DWORD dwVoiceSendMask, DWORD dwVoiceRecvMask, DWORD dwCameraSendMask,
                                      DWORD dwCameraRecvMask )
{
    ATG_Verify( m_pQNetPlayer );
    return ( m_pQNetPlayer )
        ? m_pQNetPlayer->SetCommFilter( dwVoiceSendMask, dwVoiceRecvMask, dwCameraSendMask, dwCameraRecvMask )
        : E_FAIL;
}

//--------------------------------------------------------------------------------------
// Bind XuiPaddle Control
//--------------------------------------------------------------------------------------
inline VOID Player::BindXuiPaddleControl( CXuiControl* ctrl )
{
    m_XuiPaddle.BindControl( ctrl );
    m_XuiPaddle.SetShow( TRUE );
}

//--------------------------------------------------------------------------------------
// Unbind XuiPaddle Control
//--------------------------------------------------------------------------------------
inline VOID Player::UnbindXuiPaddleControl()
{
    m_XuiPaddle.SetShow( FALSE );
    m_XuiPaddle.BindControl( NULL );
}

//--------------------------------------------------------------------------------------
// Get XuiPaddle Details
//--------------------------------------------------------------------------------------
inline VOID Player::GetPaddleDetails( MSG_PADDLE_UPDATE* pMsg )
{
    m_XuiPaddle.GetDetails( pMsg );
    pMsg->playerXuid = GetXuid();
}

//--------------------------------------------------------------------------------------
// Set XuiPaddle Details
//--------------------------------------------------------------------------------------
inline VOID Player::SetPaddleDetails( MSG_PADDLE_UPDATE const& msg )
{
    ATG_Verify( msg.playerXuid == GetXuid() );
    m_XuiPaddle.SetDetails( msg );
}

//--------------------------------------------------------------------------------------
// Reset XuiPaddle given the arena dimensions
//--------------------------------------------------------------------------------------
inline VOID Player::ResetPaddle( Rect const& arenaRect )
{
    m_XuiPaddle.Reset( arenaRect );
}

//--------------------------------------------------------------------------------------
// Collide and Update the XuiBall with this player's paddle
//--------------------------------------------------------------------------------------
inline BOOL Player::CollideAndUpdate( XuiBall& ball )
{
    return m_XuiPaddle.CollideAndUpdate( m_Team, ball );
}

//--------------------------------------------------------------------------------------
// Move Paddle as AI would
//--------------------------------------------------------------------------------------
inline VOID Player::OnUpdateXui()
{
    m_XuiPaddle.UpdateControl();
}

} // namespace ArcadeSample

#endif // ARCADESAMPLE_PLAYER_H
