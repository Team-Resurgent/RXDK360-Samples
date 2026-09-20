//--------------------------------------------------------------------------------------
// XuiBall.cpp
//
// XUI Pong Ball
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "XuiBall.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Set Random Velocity
//--------------------------------------------------------------------------------------
VOID XuiBall::SetRandomVelocity()
{
    m_Velocity.x = RandomChoice( VELOCITY_INIT_X, -VELOCITY_INIT_X );
    m_Velocity.y = RandomFloat_NegToPos( VELOCITY_INIT_Y );
}

//--------------------------------------------------------------------------------------
// Set the network message with the ball details
//--------------------------------------------------------------------------------------
VOID XuiBall::GetDetails( MSG_BALL_UPDATE* pMsg )
{
    ATG_Verify( pMsg );
    pMsg->xBall = m_Rect.x;
    pMsg->yBall = m_Rect.y;

    pMsg->vxBall = m_Velocity.x;
    pMsg->vyBall = m_Velocity.y;
}

//--------------------------------------------------------------------------------------
// Set the ball details with the network message
//--------------------------------------------------------------------------------------
VOID XuiBall::SetDetails( MSG_BALL_UPDATE const& msg )
{
    m_Rect.x = msg.xBall;
    m_Rect.y = msg.yBall;
    m_Velocity.x = msg.vxBall;
    m_Velocity.y = msg.vyBall;
}

//--------------------------------------------------------------------------------------
// Reset the ball to the middle of the arena
//--------------------------------------------------------------------------------------
VOID XuiBall::Reset( Rect const& arenaRect )
{
    m_Rect.x = arenaRect.x + arenaRect.width / 2;
    m_Rect.y = arenaRect.y + arenaRect.height / 2;
    m_Velocity.x = 0;
    m_Velocity.y = 0;
}

//--------------------------------------------------------------------------------------
// Move accordingly
//--------------------------------------------------------------------------------------
VOID XuiBall::Move( FLOAT speed )
{
    m_Rect.x += m_Velocity.x * speed;
    m_Rect.y += m_Velocity.y * speed;
}

} // namespace ArcadeSample
