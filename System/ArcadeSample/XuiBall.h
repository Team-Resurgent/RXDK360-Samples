//--------------------------------------------------------------------------------------
// XuiBall.h
//
// XUI Pong Ball
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUIBALL_H
#define ARCADESAMPLE_XUIBALL_H


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// XuiBall Class
//--------------------------------------------------------------------------------------
class XuiBall : public AppXuiRectControl
{
public:
            XuiBall()
            {
            }
    virtual ~XuiBall()
    {
    }

    VOID    SetRandomVelocity();

    VOID    GetDetails( MSG_BALL_UPDATE* pMsg );
    VOID    SetDetails( MSG_BALL_UPDATE const& msg );

    VOID    Reset( Rect const& arenaRect );
    VOID    Move( FLOAT speed );

    Vec const& GetVelocity() const;
    VOID    SetVelocity( Vec const& vBall );

private:
    Vec m_Velocity;
};


//--------------------------------------------------------------------------------------
// Get the current velociy of the ball
//--------------------------------------------------------------------------------------
inline Vec const& XuiBall::GetVelocity() const
{
    return m_Velocity;
}

//--------------------------------------------------------------------------------------
// Set the velociy of the ball
//--------------------------------------------------------------------------------------
inline VOID XuiBall::SetVelocity( Vec const& vBall )
{
    m_Velocity = vBall;
}

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUIBALL_H
