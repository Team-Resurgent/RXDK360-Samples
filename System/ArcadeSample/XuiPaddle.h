//--------------------------------------------------------------------------------------
// XuiPaddle.h
//
// XUI Pong Paddle
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUIPADDLE_H
#define ARCADESAMPLE_XUIPADDLE_H

#include "XuiBall.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// XuiPaddle
//--------------------------------------------------------------------------------------
class XuiPaddle : public AppXuiRectControl
{
public:
            XuiPaddle()
            {
            }
    virtual ~XuiPaddle()
    {
    }

    VOID    GetDetails( MSG_PADDLE_UPDATE* pMsg ) const;
    VOID    SetDetails( MSG_PADDLE_UPDATE const& msg );

    VOID    Reset( Rect const& arenaRect );

    BOOL    CollideAndUpdate( eTeam team, XuiBall& ball );
};

//--------------------------------------------------------------------------------------
// Set the network message with the paddle details
//--------------------------------------------------------------------------------------
inline VOID XuiPaddle::GetDetails( MSG_PADDLE_UPDATE* pMsg ) const
{
    pMsg->paddle = m_Rect.y;
}

//--------------------------------------------------------------------------------------
// Set the ball details with the network message
//--------------------------------------------------------------------------------------
inline VOID XuiPaddle::SetDetails( MSG_PADDLE_UPDATE const& msg )
{
    m_Rect.y = msg.paddle;
}

//--------------------------------------------------------------------------------------
// Reset the paddle to the middle of the arena
//--------------------------------------------------------------------------------------
inline VOID XuiPaddle::Reset( Rect const& arenaRect )
{
    m_Rect.y = arenaRect.y + arenaRect.height / 2 - m_Rect.height / 2;
}

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUIPADDLE_H
