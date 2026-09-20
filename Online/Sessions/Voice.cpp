//-----------------------------------------------------------------------------
// File: Voice.h
//
// Desc: Voice handler for Sessions sample
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "voice.h"
#include <AtgUtil.h>
#include <AtgSignIn.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include "sessions.h" // declaration of ClientInfo

#include <malloc.h>  // for alloca

//--------------------------------------------------------------------------------------
// Name: struct VoiceHeader
// Desc: header information to specify who voice data came from
//--------------------------------------------------------------------------------------
#pragma pack( push, 1 )
struct VoiceHeader
{
    BYTE nController;
    WORD wSize;
};
#pragma pack( pop )

//--------------------------------------------------------------------------------------
// Name: CVoice ctor
// Desc: Initialize variables
//--------------------------------------------------------------------------------------
CVoice::CVoice()
{
    m_pXHV = NULL;
    ZeroMemory( m_bLoopback, sizeof( m_bLoopback ) );
    ZeroMemory( m_bHasVoice, sizeof( m_bHasVoice ) );
}


//--------------------------------------------------------------------------------------
// Name: CVoice dtor
// Desc: Free resources
//--------------------------------------------------------------------------------------
CVoice::~CVoice()
{
    if( m_pXHV )
    {
        m_pXHV->Release();
        m_pXHV = NULL;
    }
}


//--------------------------------------------------------------------------------------
// Name: CVoice::Initialize
// Desc: Create XHV engine
//--------------------------------------------------------------------------------------
void CVoice::Initialize( LocalClientInfo* local )
{
    HRESULT hr;

    m_pLocal = local;

    // Initialize XAudio2
    IXAudio2 *pXAudio2 = NULL;
    UINT32 flags = 0;
    hr = XAudio2Create( &pXAudio2, flags );

    if( FAILED( hr ) )
    {
        ATG::FatalError( "Failed to create XAudio2, error code 0x%08x\n", hr );
    }

    // Create a mastering voice 
    IXAudio2MasteringVoice *pMasteringVoice = NULL;
    hr = pXAudio2->CreateMasteringVoice( &pMasteringVoice );

    if( FAILED( hr ) )
    {
        ATG::FatalError( "Failed to create mastering voice, error code 0x%08x\n", hr );
    }

    // at this point we have "MasteringVoice" and XAudio2 initialized
    // onto creating the XHV2 engine

    HANDLE m_hWorkerThread;
    XHV_PROCESSING_MODE rgModes[] = {XHV_VOICECHAT_MODE, XHV_LOOPBACK_MODE};

    // Set up parameters for the voice chat engine
    XHV_INIT_PARAMS xhvParams               = {0};
    xhvParams.dwMaxRemoteTalkers            = XHV_MAX_REMOTE_TALKERS;
    xhvParams.dwMaxLocalTalkers             = XHV_MAX_LOCAL_TALKERS;
    xhvParams.localTalkerEnabledModes       = rgModes;
    xhvParams.remoteTalkerEnabledModes      = rgModes;
    xhvParams.dwNumLocalTalkerEnabledModes  = 2;
    xhvParams.dwNumRemoteTalkerEnabledModes = 1;
    xhvParams.pXAudio2                      = pXAudio2;

    // Create the XHV2 engine
    hr = XHV2CreateEngine( &xhvParams, &m_hWorkerThread, &m_pXHV );

    if( FAILED( hr ) )
    {
        ATG::FatalError( "Failed to create XHV2, error code 0x%08x\n", hr );
    }

    m_dwLastVoiceSend = GetTickCount();
}


//--------------------------------------------------------------------------------------
// Name: CVoice::EndSession
// Desc: Unregister all remote talkers
//--------------------------------------------------------------------------------------
void CVoice::EndSession()
{
    DWORD dwNumRemoteTalkers = MAX_REMOTE_TALKERS;
    XUID RemoteTalkers[ MAX_REMOTE_TALKERS ];

    m_pXHV->GetRemoteTalkers( &dwNumRemoteTalkers, RemoteTalkers );

    for( UINT i = 0; i < dwNumRemoteTalkers; i++ )
    {
        m_pXHV->UnregisterRemoteTalker( RemoteTalkers[ i ] );
    }
}


//--------------------------------------------------------------------------------------
// Name: CVoice::LocalUsersChanged
// Desc: Process a change in the local users
//--------------------------------------------------------------------------------------
void CVoice::LocalUsersChanged()
{
    HRESULT hr;

    // Register everybody who's logged in and is capable of doing voice
    for( UINT i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        if( ATG::SignIn::IsUserOnline( i ) )
        {
            if( !m_bHasVoice[ i ] )
            {
                hr = m_pXHV->RegisterLocalTalker( i );
                if( FAILED( hr ) )
                {
                    ATG::FatalError( "Failed to register local talker %d, error 0x%08x\n", i, hr );
                }
            }

            hr = m_pXHV->StartLocalProcessingModes( i, &XHV_VOICECHAT_MODE, 1 );

            if( SUCCEEDED( hr ) )
            {
                hr = m_pXHV->StopLocalProcessingModes( i, &XHV_LOOPBACK_MODE, 1 );
            }

            if( FAILED( hr ) )
            {
                ATG::FatalError( "Failed to set processing modes for user %d, error 0x%08x\n", i, hr );
            }

            m_bLoopback[ i ] = FALSE;
            m_bHasVoice[ i ] = TRUE;

            m_wLocalDataSize[ i ] = 0;
        }
        else
        {
            if( m_bHasVoice[ i ] )
            {
                m_pXHV->UnregisterLocalTalker( i );
                m_bHasVoice[ i ] = FALSE;
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: CVoice::ToggleLoopbackMode
// Desc: Change the loopback status of a user
//--------------------------------------------------------------------------------------
VOID CVoice::ToggleLoopbackMode( UINT nController )
{
    HRESULT hr;

    // Don't do this unless the user actually has voice
    if( !m_bHasVoice[ nController ] )
    {
        return;
    }

    BOOL bLoopback = m_bLoopback[ nController ] = !m_bLoopback[ nController ];

    PXHV_PROCESSING_MODE pStopMode = bLoopback ? &XHV_VOICECHAT_MODE : &XHV_LOOPBACK_MODE;
    PXHV_PROCESSING_MODE pStartMode = bLoopback ? &XHV_LOOPBACK_MODE  : &XHV_VOICECHAT_MODE;

    hr = m_pXHV->StopLocalProcessingModes( nController, pStopMode, 1 );
    if( SUCCEEDED( hr ) )
    {
        hr = m_pXHV->StartLocalProcessingModes( nController, pStartMode, 1 );
    }

    if( FAILED( hr ) )
    {
        ATG::FatalError( "Failed to toggle loopback mode for user %d, error 0x%08x\n",
                         nController, hr );
    }
}


//--------------------------------------------------------------------------------------
// Name: CVoice::GetLocalVoice
// Desc: Return the current set of communicators
//--------------------------------------------------------------------------------------
VOID CVoice::GetLocalVoice( BOOL* pHasVoice )
{
    memcpy( pHasVoice, m_bHasVoice, sizeof( m_bHasVoice ) );
}


//--------------------------------------------------------------------------------------
// Name: CVoice::RegisterClient
// Desc: Register talkers for a new client
//--------------------------------------------------------------------------------------
VOID CVoice::RegisterClient( ClientInfo* pClient )
{
    for( UINT i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        if( pClient->bHasVoice[ i ] )
        {
            // register the new talker
            m_pXHV->RegisterRemoteTalker( pClient->xuids[ i ], NULL, NULL, NULL );
            m_pXHV->StartRemoteProcessingModes( pClient->xuids[ i ], &XHV_VOICECHAT_MODE, 1 );

            // by default, everyone has the same priority
            for( UINT iLocal = 0; iLocal < XUSER_MAX_COUNT; iLocal++ )
            {
                if( m_bHasVoice[ iLocal ] )
                {
                    BOOL bIsMuted = FALSE;
                    DWORD dwRet;

                    dwRet = XUserMuteListQuery( iLocal, pClient->xuids[ i ], &bIsMuted );
                    if( ERROR_SUCCESS != dwRet )
                    {
                        ATG::DebugSpew( "Warning: XUserMuteListQuery() returned 0x%08x for user %d\n", dwRet, iLocal );
                    }

                    m_pXHV->SetPlaybackPriority(
                        pClient->xuids[ i ],
                        iLocal,
                        bIsMuted ? XHV_PLAYBACK_PRIORITY_NEVER : XHV_PLAYBACK_PRIORITY_MAX );
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: CVoice::UnregisterClient
// Desc: Removes talkers for a client that's left
//--------------------------------------------------------------------------------------
VOID CVoice::UnregisterClient( ClientInfo* pClient )
{
    for( UINT i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        if( pClient->bHasVoice[ i ] )
        {
            // unregister the new talker
            m_pXHV->UnregisterRemoteTalker( pClient->xuids[ i ] );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: CVoice::ProcessVoice
// Desc: Check for incoming voice data
//       Returns TRUE if a voice packet should be sent
//--------------------------------------------------------------------------------------
BOOL CVoice::ProcessVoice()
{
    // Count how many bytes of data we have available
    WORD wVoiceBytes = 0;

    // Get the users that have voice data available
    DWORD dwVoiceFlags = m_pXHV->GetDataReadyFlags();

    BOOL bNeedToSend = FALSE;

    // Look for any new incoming data
    for( UINT i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        if( m_bHasVoice[ i ] )
        {
            if( dwVoiceFlags & ( 1 << i ) )
            {
                // Buffer the received voice data
                DWORD dwNumPackets;
                DWORD dwBytes;

                dwBytes = m_ChatBufferSize - m_wLocalDataSize[ i ];

                if( dwBytes < XHV_VOICECHAT_MODE_PACKET_SIZE )
                {
                    bNeedToSend = TRUE;
                }

                else
                {
                    m_pXHV->GetLocalChatData(
                        i,
                        m_ChatBuffer[ i ] + m_wLocalDataSize[ i ],
                        &dwBytes,
                        &dwNumPackets );

                    m_wLocalDataSize[ i ] += ( ( WORD )dwBytes ) & MAXWORD;

                    if( m_wLocalDataSize[ i ] > ( ( m_ChatBufferSize * 7 ) / 10 ) )
                    {
                        bNeedToSend = TRUE;
                    }
                }
            }

            // Keep a running count of voice bytes ready to go
            wVoiceBytes += m_wLocalDataSize[ i ] & MAXWORD;
        }
    }

    // Send voice if we have any data and enough time has elapsed,
    // or if any buffer is more than 75% full

    return ( bNeedToSend ||
             ( wVoiceBytes &&
               ( GetTickCount() - m_dwLastVoiceSend ) > MAX_VOICE_BUFFER_TIME ) );
}


//--------------------------------------------------------------------------------------
// Name: CVoice::GetVoiceData
// Desc: Get locally buffered voice for transmission
//--------------------------------------------------------------------------------------
WORD CVoice::GetVoiceData( BYTE* pBuffer, WORD wBufferSize )
{
    BOOL bAllSent = TRUE;
    WORD wRet = 0;

    // Check to see who has voice waiting
    for( UINT i = 0; i < XUSER_MAX_COUNT && wBufferSize; i++ )
    {
        if( m_wLocalDataSize[ i ] )
        {
            if( wBufferSize >= sizeof( VoiceHeader ) + m_wLocalDataSize[ i ] )
            {
                // Save out the voice data
                VoiceHeader* pHeader = ( VoiceHeader* )pBuffer;

                pHeader->nController = ( BYTE )i;
                pHeader->wSize = m_wLocalDataSize[ i ];

                pBuffer += sizeof( VoiceHeader );
                wRet += sizeof( VoiceHeader );
                wBufferSize -= sizeof( VoiceHeader );

                memcpy_s( pBuffer, wBufferSize, m_ChatBuffer[ i ], m_wLocalDataSize[ i ] );

                pBuffer += m_wLocalDataSize[ i ] & MAXWORD;
                wRet += m_wLocalDataSize[ i ] & MAXWORD;
                wBufferSize -= m_wLocalDataSize[ i ] & MAXWORD;
                m_wLocalDataSize[ i ] = 0;
            }
            else
            {
                // The buffer wasn't big enough, so don't reset the last voice send
                bAllSent = FALSE;
            }
        }
    }

    if( bAllSent )
    {
        m_dwLastVoiceSend = GetTickCount();
    }

    return wRet;
}


//--------------------------------------------------------------------------------------
// Name: CVoice::SubmitVoiceData
// Desc: Handle voice data received over the network
//--------------------------------------------------------------------------------------
VOID CVoice::SubmitVoiceData( BYTE* pBuffer, WORD wBufferSize, ClientInfo* pClient )
{
    while( wBufferSize )
    {
        VoiceHeader* pHeader = ( VoiceHeader* )pBuffer;

        DWORD dwSubmitted = pHeader->wSize;

        // Give the data to XHV
        m_pXHV->SubmitIncomingChatData(
            pClient->xuids[ pHeader->nController ],
            pBuffer + sizeof( VoiceHeader ),
            &dwSubmitted );

        wBufferSize -= sizeof( VoiceHeader );
        wBufferSize -= pHeader->wSize & MAXWORD;
        pBuffer += sizeof( VoiceHeader );
        pBuffer += pHeader->wSize;
    }
}


//--------------------------------------------------------------------------------------
// Name: CVoice::ProcessMutelists
// Desc: Reevaluate current mutelist settings
//--------------------------------------------------------------------------------------
VOID CVoice::ProcessMutelists( MsgMute* msg )
{
    // Retrieve the remote talkers
    DWORD dwNumRemoteTalkers = MAX_REMOTE_TALKERS;
    XUID RemoteTalkers[ MAX_REMOTE_TALKERS ];

    m_pXHV->GetRemoteTalkers( &dwNumRemoteTalkers, RemoteTalkers );

    msg->cPlayers = 0;

    // Loop through local players
    for( UINT iLocal = 0; iLocal < XUSER_MAX_COUNT; iLocal++ )
    {
        msg->cMuted[ iLocal ] = 0;

        if( !m_bHasVoice[ iLocal ] ) continue;

        msg->xuid[ msg->cPlayers ] = m_pLocal->xuids[ iLocal ];

        // Loop through remote talkers
        for( UINT iRemote = 0; iRemote < dwNumRemoteTalkers; iRemote++ )
        {
            BOOL bIsMuted = FALSE;
            DWORD dwRet;

            dwRet = XUserMuteListQuery( iLocal, RemoteTalkers[ iRemote ], &bIsMuted );
            if( ERROR_SUCCESS != dwRet )
            {
                ATG::DebugSpew( "Warning: XUserMuteListQuery() returned 0x%08x for user %d\n", dwRet, iLocal );
            }

            if( bIsMuted )
            {
                msg->xuidMuted[ msg->cPlayers ][ msg->cMuted[ msg->cPlayers ] ] = RemoteTalkers[ iRemote ];
                ++msg->cMuted[ msg->cPlayers ];
            }
            else
            {
                bIsMuted = m_pLocal->IsMutedFor( iLocal, RemoteTalkers[ iRemote ] );
            }

            m_pXHV->SetPlaybackPriority(
                RemoteTalkers[ iRemote ],
                iLocal,
                bIsMuted ? XHV_PLAYBACK_PRIORITY_NEVER : XHV_PLAYBACK_PRIORITY_MAX );
        }

        msg->cPlayers++;
    }
}


//--------------------------------------------------------------------------------------
// Name: CVoice::AddMute
// Desc: Mute a player for a local user (in response to a mute message, sent to notify
// that the other player has muted the local user).
//--------------------------------------------------------------------------------------
VOID CVoice::ProcessMute( UINT nController, XUID xuid )
{
    BOOL bIsMuted = m_pLocal->IsMutedFor( nController, xuid );
    if( !bIsMuted )
    {
        XUserMuteListQuery( nController, xuid, &bIsMuted );
    }

    if( bIsMuted )
    {
        m_pXHV->SetPlaybackPriority( xuid, nController, XHV_PLAYBACK_PRIORITY_NEVER );
    }
    else
    {
        m_pXHV->SetPlaybackPriority( xuid, nController, XHV_PLAYBACK_PRIORITY_MAX );
    }
}
