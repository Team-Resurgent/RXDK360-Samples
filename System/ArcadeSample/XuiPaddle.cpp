//--------------------------------------------------------------------------------------
// XuiPaddle.cpp
//
// XUI Pong Paddle
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "XuiPaddle.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Collide the Ball with the paddle. Update the ball if collision detected. 
// Return TRUE if a collision happened, FALSE otherwise.
//--------------------------------------------------------------------------------------
BOOL XuiPaddle::CollideAndUpdate( eTeam team, XuiBall& ball )
{
    BOOL bCollide = FALSE;

    Rect const& rcBall = ball.GetRect();
    Vec const& vBall = ball.GetVelocity();

    Rect rcBallNew = rcBall;
    Vec vBallNew = vBall;

    if( team == eTeam_Left )
    {
        if( rcBall.x < ( m_Rect.x + m_Rect.width ) )
        {
            if( ( ( rcBall.y + rcBall.height ) > m_Rect.y ) &&
                ( rcBall.y < ( m_Rect.y + m_Rect.height ) ) )
            {
                rcBallNew.x = m_Rect.x + m_Rect.width;

                vBallNew.x = -VELOCITY_MULT_X * vBall.x;
                if( vBallNew.x > VELOCITY_MAX_X )
                {
                    vBallNew.x = VELOCITY_MAX_X;
                }

                vBallNew.y += RandomFloat_NegToPos( VELOCITY_MAX_Y );
                bCollide = TRUE;
            }
        }
    }
    else if( team == eTeam_Right )
    {
        if( ( rcBall.x + rcBall.width ) > m_Rect.x )
        {
            if( ( ( rcBall.y + rcBall.height ) > m_Rect.y ) &&
                ( rcBall.y < ( m_Rect.y + m_Rect.height ) ) )
            {
                rcBallNew.x = m_Rect.x - rcBall.width;

                vBallNew.x = -VELOCITY_MULT_X * vBall.x;
                if( vBallNew.x < -VELOCITY_MAX_X )
                {
                    vBallNew.x = -VELOCITY_MAX_X;
                }

                vBallNew.y += RandomFloat_NegToPos( VELOCITY_MAX_Y );
                bCollide = TRUE;
            }
        }
    }

    if( bCollide )
    {
        ball.SetRect( rcBallNew );
        ball.SetVelocity( vBallNew );
    }

    return bCollide;
}

} // namespace ArcadeSample

