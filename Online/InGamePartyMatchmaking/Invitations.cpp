//--------------------------------------------------------------------------------------
// Invitations.cpp
//
// Code for handling receiving accepted invitations and joining the invited session
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "XPlat.h"

//--------------------------------------------------------------------------------------
// Name: HandleAcceptedInvitation()
// Desc: Check to see if we have an "invite accepted" notification and respond
//--------------------------------------------------------------------------------------
BOOL Sample::HandleAcceptedInvitation( const XINVITE_INFO& InviteInfo )
{
    // Ignore invites from a different title
    if( InviteInfo.dwTitleID != TITLEID_INGAMEPARTYMATCHMAKING_SAMPLE )
    {
        DebugSpew( "Ignoring invite from different title ID 0x%08x\n", InviteInfo.dwTitleID );
        return TRUE;
    }

    const XNKID sessionID = InviteInfo.hostInfo.sessionID;

    // Don't bother dealing with invitations to a session we're
    // already in
    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID );
    if( !pSessionMgr )
    {
        // We need to query the session host for some details in order to 
        // join this session. Note that the socket we use is non-blocking
        // so it's safe to go ahead and initiate a message from
        // this client to the host within this XN_LIVE_INVITE_ACCEPTED
        // handler
        JoinSessionFromInviteInfoPreamble( InviteInfo );
    }

    return TRUE;
}
