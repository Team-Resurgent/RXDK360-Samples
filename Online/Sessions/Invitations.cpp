//--------------------------------------------------------------------------------------
// Invitations.cpp
//
// Code for handling receiving accepted invitations and joining the invited session
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgUtil.h>
#include "Sessions.h"

//--------------------------------------------------------------------------------------
// Name: CheckForAcceptedInvitation()
// Desc: Check to see if we have an "invite accepted" notification and respond
//--------------------------------------------------------------------------------------
BOOL Sample::CheckForAcceptedInvitation()
{
    // Check for system notifications
    DWORD dwNotificationID;
    ULONG_PTR ulParam;

    XINVITE_INFO InviteInfo;

    if( XNotifyGetNext( m_hLiveListener, 0, &dwNotificationID, &ulParam ) )
    {
        switch( dwNotificationID )
        {
            case XN_LIVE_INVITE_ACCEPTED:

                // if we are in a session, clean it up before jumping into another
                m_Session.Cleanup();

                // We've received an invitation!
                XInviteGetAcceptedInfo( ulParam, &InviteInfo );

                // Don't bother dealing with invitations to the current session
                if( !m_Session.IsSameSession( InviteInfo.hostInfo ) )
                {
                    // Configure the session and join it
                    m_Session.SetSessionInfo( InviteInfo.hostInfo );

                    m_Session.SetSessionOwner( ulParam );
                    JoinSession( InviteInfo.fFromGameInvite );
                }

                return TRUE;
        }
    }

    // No invitation has been received
    return FALSE;
}
