//--------------------------------------------------------------------------------------
// Player.cpp
//
// Per controller player class
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "Player.h"

namespace ArcadeSample
{
namespace
{
DWORD UID_Sequence = 0;
}

//--------------------------------------------------------------------------------------
// AI Player Ctor
//--------------------------------------------------------------------------------------
Player::Player( eTeam team ) : m_Team( team ),
                               m_pQNetPlayer( NULL ),
                               m_Uid( ++UID_Sequence ),
                               m_XuiPaddle()
{
}

//--------------------------------------------------------------------------------------
// Human Player Ctor 
//--------------------------------------------------------------------------------------
Player::Player( eTeam team, IQNetPlayer* pQNetPlayer ) : m_Team( team ),
                                                         m_pQNetPlayer( pQNetPlayer ),
                                                         m_Uid( ++UID_Sequence ),
                                                         m_XuiPaddle()
{
}

//--------------------------------------------------------------------------------------
// Process this player's input
//--------------------------------------------------------------------------------------
VOID Player::ProcessInput( FLOAT speed, Rect const& arena )
{
    XINPUT_STATE input = { 0 };
    Rect rect = m_XuiPaddle.GetRect();

    DWORD dwUserIndex = GetUserIndex();
    XInputGetState( dwUserIndex, &input );

    const FLOAT deadzone = 0.25f;
    FLOAT pos = ( FLOAT )input.Gamepad.sThumbLY / ( FLOAT )0x7FFF;

    if( pos > deadzone )
    {
        pos = ( pos - deadzone ) / ( 1.0f - deadzone );
    }
    else if( pos < -deadzone )
    {
        pos = ( pos + deadzone ) / ( 1.0f - deadzone );
    }
    else
    {
        return;
    }

    rect.y -= 5.0f * pos * speed;

    if( rect.y < arena.y )
    {
        rect.y = arena.y;
    }
    else if( ( rect.y + rect.height ) > ( arena.y + arena.height ) )
    {
        rect.y = arena.y + arena.height - rect.height;
    }

    m_XuiPaddle.SetRect( rect );

    MSG_PADDLE_UPDATE paddle;
    GetPaddleDetails( &paddle );

    SendData(
        NULL, ( BYTE* )&paddle, sizeof( paddle ),
        0 );
}

//--------------------------------------------------------------------------------------
// Move Paddle as AI would
//--------------------------------------------------------------------------------------
VOID Player::AIMovePaddle( FLOAT speed, Rect const& arena, Rect const& ball )
{
    Rect rect = m_XuiPaddle.GetRect();
    if( ( rect.y + rect.height ) < ball.y )
    {
        rect.y += 2.5f * speed;
    }
    else if( rect.y > ( ball.y + ball.height ) )
    {
        rect.y -= 2.5f * speed;
    }

    if( rect.y < arena.y )
    {
        rect.y = arena.y;
    }
    else if( ( rect.y + rect.height ) > ( arena.y + arena.height ) )
    {
        rect.y = arena.y + arena.height - rect.height;
    }
    m_XuiPaddle.SetRect( rect );
}

} // namespace ArcadeSample

