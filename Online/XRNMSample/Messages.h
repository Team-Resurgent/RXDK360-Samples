//--------------------------------------------------------------------------------------
// Messages.h
//
// Message classes for XRNM sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

class CMessage;
class CClientInfoMsg;

#include "XRNMSample.h"

//-----------------------------------------------------------------------------
// Message IDs
//-----------------------------------------------------------------------------

enum
{
    MSG_CLIENTINFO,      // information about a client
    MSG_BUTTONPRESSED,   // a button has been pressed
    MSG_POSITIONUPDATE,  // periodic updates on position
};

// Constant for message destinations
static const UINT MSG_DESTINATION_ALL = UINT_MAX;


//-----------------------------------------------------------------------------
// ISerializable interface
//
// Supports streaming data to a byte array
//-----------------------------------------------------------------------------

class ByteStream;

class ISerializable
{
public:
    virtual HRESULT BeginSerialize( _Inout_ ByteStream* pStream ) = 0;
    virtual HRESULT Serialize( _Inout_ ByteStream* pStream ) = 0;
    virtual HRESULT EndSerialize( _Inout_ ByteStream* pStream ) = 0;
};


//-----------------------------------------------------------------------------
// ByteStream class
//
// Allows data to be streamed to and from an array of bytes
//-----------------------------------------------------------------------------
class ByteStream
{
public:
                    ByteStream( BOOL bWrite, _Inout_opt_ BYTE* pbStream, UINT cbStream ) : m_bWrite( bWrite ),
                                                                               m_pbStream( pbStream ),
                                                                               m_cbStream( cbStream ),
                                                                               m_hr( S_OK ),
                                                                               m_cbStreamed( 0 )
                    {
                    }


    const BYTE* GetStream()     const
    {
        return m_pbStream;
    }
    UINT            GetStreamSize() const
    {
        return m_cbStream;
    }
    UINT            GetStreamed()   const
    {
        return m_cbStreamed;
    }
    HRESULT         GetHRESULT()    const
    {
        return m_hr;
    }

    HRESULT         Stream( _Inout_ ISerializable* pSerialize )
    {
        if( SUCCEEDED( m_hr ) )
        {
            m_hr = pSerialize->BeginSerialize( this );
        }
        if( SUCCEEDED( m_hr ) )
        {
            m_hr = pSerialize->Serialize( this );
        }
        if( SUCCEEDED( m_hr ) )
        {
            m_hr = pSerialize->EndSerialize( this );
        }

        return m_hr;
    }

    HRESULT         StreamPeek( _Inout_bytecount_(cbData) BYTE* pbData, UINT cbData )
    {
        return StreamData( pbData, cbData, TRUE );
    }

    HRESULT         StreamData( _Inout_bytecount_(cbData) BYTE* pbData, UINT cbData, BOOL bPeek = FALSE )
    {
        if( SUCCEEDED( m_hr ) )
        {
            if( m_pbStream )
            {
                if( cbData + m_cbStreamed > m_cbStream )
                {
                    m_hr = E_FAIL;
                }
                else
                {
                    if( m_bWrite )
                    {
                        memcpy_s( m_pbStream + m_cbStreamed,
                                  m_cbStream - m_cbStreamed,
                                  pbData,
                                  cbData );
                    }
                    else
                    {
                        memcpy_s( pbData,
                                  cbData,
                                  m_pbStream + m_cbStreamed,
                                  cbData );
                    }
                }
            }

            if( !bPeek )
            {
                m_cbStreamed += cbData;
            }
        }

        return m_hr;
    }


    HRESULT         StreamString( _Inout_z_ WCHAR* wstr )
    {
        UINT cbLength = 0;

        if( m_bWrite )
        {
            cbLength = ( wcslen( wstr ) + 1 ) * sizeof( WCHAR );
        }

        StreamData( &cbLength );
        StreamData( ( BYTE* )wstr, cbLength );

        return m_hr;
    }

    template<class T> HRESULT StreamData( _Inout_ T* data )
                      {
                        return StreamData( ( BYTE* )data, sizeof( T ) );
                      }

    template<class T> HRESULT StreamPeek( _Inout_ T* data )
                      {
                        return StreamPeek( ( BYTE* )data, sizeof( T ) );
                      }

    static HRESULT  CalculateSize( _Inout_ ISerializable* pSerialize, _Out_ UINT* pSize )
    {
        ByteStream streamCalc( TRUE, NULL, 0 );
        streamCalc.Stream( pSerialize );

        *pSize = streamCalc.GetStreamed();
        return streamCalc.GetHRESULT();
    }

private:

    BYTE* m_pbStream;
    UINT m_cbStream;
    UINT m_cbStreamed;
    BOOL m_bWrite;

    HRESULT m_hr;
};


//-----------------------------------------------------------------------------
// CMessage class (abstract)
//
// Data message 
//-----------------------------------------------------------------------------

class CClientInfoMsg;
class CButtonPressedMsg;
class CPositionUpdateMsg;

class CMessage : public ISerializable
{
protected:
                    CMessage( UCHAR MessageID, UINT nDestination ) : m_MessageID( MessageID ),
                                                                     m_nDestination( nDestination )
                    {
                    }

    UCHAR m_MessageID;
    UINT m_nDestination;
    UINT m_dwFlags;

public:
    // Class factory
    static HRESULT  CreateMessage( const BYTE* pbStream, UINT cbStream, _Inout_ CMessage** ppMsg );

    HRESULT         Serialize( _Inout_ ByteStream* pStream )
    {
        pStream->StreamData( &m_MessageID );
        pStream->StreamData( &m_nDestination );
        pStream->StreamData( &m_dwFlags );

        return pStream->GetHRESULT();
    }

    UCHAR           GetMessageID( VOID ) const
    {
        return m_MessageID;
    }
    UINT            GetDestination( VOID ) const
    {
        return m_nDestination;
    }
    UINT            GetFlags( VOID )       const
    {
        return m_dwFlags;
    }
    VOID            SetFlags( UINT dwFlags )
    {
        m_dwFlags = dwFlags;
    }
};


//-----------------------------------------------------------------------------
// CPositionUpdateMsg class 
//
// Message giving the position and velocity of one or more clients
//-----------------------------------------------------------------------------
class CPositionUpdateMsg : public CMessage
{
public:
            CPositionUpdateMsg() : CMessage( MSG_POSITIONUPDATE, MSG_DESTINATION_ALL )
            {
            }

    XNADDR xnaddr;
    UCHAR NumPlayers;

    struct
    {
        UINT nPlayerID;

        FLOAT fXPos;         // X position as a percentage of screen width
        FLOAT fYPos;         // Y position as a percentage of screen height
        FLOAT fXStick;       // X position of controller left thumbstick
        FLOAT fYStick;       // Y position of controller right thumbstick
        FLOAT fXVelocity;    // X velocity (max magnitude = 1)
        FLOAT fYVelocity;    // Y velocity (max magnitude = 1)
    }       Updates[ XUSER_MAX_COUNT ];

    HRESULT BeginSerialize( _Inout_ ByteStream* pStream )
    {
        return pStream->GetHRESULT();
    }
    HRESULT EndSerialize( _Inout_ ByteStream* pStream )
    {
        return pStream->GetHRESULT();
    }

    HRESULT Serialize( _Inout_ ByteStream* pStream )
    {
        CMessage::Serialize( pStream );

        pStream->StreamData( &xnaddr );
        pStream->StreamData( &NumPlayers );

        for( UCHAR i = 0; i < NumPlayers; i++ )
        {
            pStream->StreamData( &Updates[ i ].nPlayerID );

            pStream->StreamData( &Updates[ i ].fXPos );
            pStream->StreamData( &Updates[ i ].fYPos );
            pStream->StreamData( &Updates[ i ].fXStick );
            pStream->StreamData( &Updates[ i ].fYStick );
            pStream->StreamData( &Updates[ i ].fXVelocity );
            pStream->StreamData( &Updates[ i ].fYVelocity );
        }

        return pStream->GetHRESULT();
    }
};


//-----------------------------------------------------------------------------
// CButtonPressedMsg class 
//
// Message informing peers that a button has been pressed
//-----------------------------------------------------------------------------
class CButtonPressedMsg : public CMessage
{
public:
                    CButtonPressedMsg() : CMessage( MSG_BUTTONPRESSED, MSG_DESTINATION_ALL )
                    {
                    }

    UINT nPlayerID;
    UINT nButton;

    HRESULT         BeginSerialize( _Inout_ ByteStream* pStream )
    {
        return pStream->GetHRESULT();
    }
    HRESULT         EndSerialize( _Inout_ ByteStream* pStream )
    {
        return pStream->GetHRESULT();
    }

    virtual HRESULT Serialize( _Inout_ ByteStream* pStream )
    {
        CMessage::Serialize( pStream );

        pStream->StreamData( &nPlayerID );
        pStream->StreamData( &nButton );

        return pStream->GetHRESULT();
    }
};


//-----------------------------------------------------------------------------
// CClientInfoMsg class 
//
// Message about one or more clients
//-----------------------------------------------------------------------------
class CClientInfoMsg : public CMessage
{
public:
            CClientInfoMsg() : CMessage( MSG_CLIENTINFO, 0 )
            {
            }

    UINT cConsoles;

    struct _Console
    {
        XNADDR xnaddr;

        UCHAR cPlayers;
        struct _Player
        {
            UINT nPlayerID;
            UCHAR nLocalController;
            XUID xuid;
            WCHAR wstrGamertag[ XUSER_NAME_SIZE ];

            FLOAT fXPos;         // X position as a percentage of screen width
            FLOAT fYPos;         // Y position as a percentage of screen height

            FLOAT fXStick;       // X position of controller left thumbstick
            FLOAT fYStick;       // Y position of controller right thumbstick

            FLOAT fXVelocity;    // X velocity (max magnitude = 1)
            FLOAT fYVelocity;    // Y velocity (max magnitude = 1)
        } rgPlayers[ XUSER_MAX_COUNT ];

    }       rgConsoles[ MAXCONSOLES ];

    HRESULT BeginSerialize( _Inout_ ByteStream* pStream )
    {
        return pStream->GetHRESULT();
    }
    HRESULT EndSerialize( _Inout_ ByteStream* pStream )
    {
        return pStream->GetHRESULT();
    }

    HRESULT Serialize( _Inout_ ByteStream* pStream )
    {
        CMessage::Serialize( pStream );

        pStream->StreamData( &cConsoles );

        for( UINT iConsole = 0; iConsole < cConsoles; iConsole++ )
        {
            _Console& Console = rgConsoles[ iConsole ];

            pStream->StreamData( &Console.xnaddr );
            pStream->StreamData( &Console.cPlayers );
            for( UCHAR iPlayer = 0; iPlayer < Console.cPlayers; iPlayer++ )
            {
                _Console::_Player& Player = Console.rgPlayers[ iPlayer ];

                pStream->StreamData( &Player.nPlayerID );
                pStream->StreamData( &Player.nLocalController );
                pStream->StreamData( &Player.xuid );
                pStream->StreamString( Player.wstrGamertag );
                pStream->StreamData( &Player.fXPos );
                pStream->StreamData( &Player.fYPos );
                pStream->StreamData( &Player.fXStick );
                pStream->StreamData( &Player.fYStick );
                pStream->StreamData( &Player.fXVelocity );
                pStream->StreamData( &Player.fYVelocity );
            }
        }

        return pStream->GetHRESULT();
    }
};

