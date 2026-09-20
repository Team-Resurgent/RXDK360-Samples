//--------------------------------------------------------------------------------------
// XuiArena.cpp
//
// XuiArena class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "XuiArena.h"
#include "XuiBall.h"

namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Collide the Ball with the Arena. Update the ball if collision detected.
// Return:
//  eArenaCollide_None if no collision detected
//  eArenaCollide_Wall if collision with arena wall detected
//  eArenaCollide_GoalLeft if collision with the left goal detected
//  eArenaCollide_GoalRight if collision with the right goal detected
//--------------------------------------------------------------------------------------
eArenaCollide XuiArena::CollideAndUpdate( XuiBall& ball )
{
    eArenaCollide collide = eArenaCollide_None;

    Rect const& rcBall = ball.GetRect();
    Vec const& vBall = ball.GetVelocity();

    Rect rcBallNew = rcBall;
    Vec vBallNew = vBall;

    if( ( rcBall.x + rcBall.width ) > ( m_Rect.x + m_Rect.width ) )
    {
        collide = eArenaCollide_GoalRight;
    }
    if( rcBall.x < m_Rect.x )
    {
        collide = eArenaCollide_GoalLeft;
    }
    if( ( rcBall.y + rcBall.height ) > ( m_Rect.y + m_Rect.height ) )
    {
        rcBallNew.y = m_Rect.y + m_Rect.height - rcBall.height;
        vBallNew.y = -vBall.y;

        collide = eArenaCollide_Wall;
    }
    if( rcBall.y < m_Rect.y )
    {
        rcBallNew.y = m_Rect.y;
        vBallNew.y = -vBall.y;

        collide = eArenaCollide_Wall;
    }

    if( collide == eArenaCollide_Wall )
    {
        ball.SetRect( rcBallNew );
        ball.SetVelocity( vBallNew );
    }

    return collide;
}

} // namespace ArcadeSample
