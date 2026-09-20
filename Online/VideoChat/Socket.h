//-----------------------------------------------------------------------------
// File: Socket.h
//
// Desc: Wraps SOCKET object
//
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

//-----------------------------------------------------------------------------
// Name: class Socket
// Desc: Xbox socket object
//-----------------------------------------------------------------------------
class CSocket
{
    SOCKET m_Socket;

public:

    enum SocketType
    {
        Type_UDP,
        Type_TCP,
        Type_VDP
    };

    explicit    CSocket( SOCKET = INVALID_SOCKET );
    explicit    CSocket( SocketType );
                CSocket( INT iType, INT iProtocol );
                ~CSocket();

    BOOL        Open( SocketType );
    BOOL        Open( INT iType, INT iProtocol );
    BOOL        IsOpen() const;
    INT         Close();
    SOCKET      Accept( SOCKADDR_IN* = NULL );
    INT         Bind( const SOCKADDR_IN* );
    INT         Connect( const SOCKADDR_IN* );
    SOCKET      GetSocket() const;
    INT         GetSockName( SOCKADDR_IN* ) const;
    INT         GetSockOpt( INT iLevel, INT iName, VOID* pValue, INT* piSize ) const;
    INT         IoCtlSocket( LONG nCmd, DWORD* pArg );
    INT         Listen( INT iBacklog = SOMAXCONN );
    INT         Recv( VOID* pBuffer, INT iBytes );
    INT         RecvFrom( VOID* pBuffer, INT iBytes, SOCKADDR_IN* = NULL );
    INT         Select( BOOL* pbRead, BOOL* pbWrite, BOOL* pbError );
    INT         Send( const VOID* pBuffer, INT iBytes );
    INT         SendTo( const VOID* pBuffer, INT iBytes, const SOCKADDR_IN* = NULL );
    INT         SetSockOpt( INT iLevel, INT iName, const VOID* pValue, INT iBytes );
    INT         Shutdown( INT iHow );

private:

                CSocket( const CSocket& );
    CSocket& operator=( const CSocket& );

};

