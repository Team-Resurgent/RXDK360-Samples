//--------------------------------------------------------------------------------------
// Messages.cpp
//
// Message classes for XRNM sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include "Messages.h"

// Class factory
HRESULT CMessage::CreateMessage( const BYTE* pbStream, UINT cbStream, _Inout_ CMessage** ppMsg )
{
    ByteStream stream( FALSE, ( BYTE* )pbStream, cbStream );
    HRESULT hr = E_FAIL;

    UCHAR MessageID = 0;
    stream.StreamPeek( &MessageID );

    switch( MessageID )
    {
        case MSG_CLIENTINFO:
            *ppMsg = new CClientInfoMsg;
            break;
        case MSG_BUTTONPRESSED:
            *ppMsg = new CButtonPressedMsg;
            break;
        case MSG_POSITIONUPDATE:
            *ppMsg = new CPositionUpdateMsg();
            break;
    }

    if( *ppMsg )
    {
        ( *ppMsg )->m_MessageID = MessageID;
        hr = stream.Stream( *ppMsg );
    }

    return hr;
}
