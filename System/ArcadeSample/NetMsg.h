//--------------------------------------------------------------------------------------
// NetMsg.h
//
// Net Message classes for this sample
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_NETMSG_H
#define ARCADESAMPLE_NETMSG_H


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Network Messages
//--------------------------------------------------------------------------------------
#pragma pack( push,1 )
struct MSG_BALL_UPDATE
{
    MSG_BALL_UPDATE() : id( BALL_UPDATE )
    {
    }
    BYTE id;
    float xBall;
    float yBall;
    float vxBall;
    float vyBall;
};

struct MSG_PADDLE_UPDATE
{
    MSG_PADDLE_UPDATE() : id( PADDLE_UPDATE )
    {
    }
    BYTE id;
    float paddle;
    XUID playerXuid;
};

struct MSG_SCORE_UPDATE
{
    MSG_SCORE_UPDATE() : id( SCORE_UPDATE )
    {
    }
    BYTE id;
    eTeam team;
    DWORD score;
};
#pragma pack( pop )

} // namespace ArcadeSample

#endif // ARCADESAMPLE_NETMSG_H
