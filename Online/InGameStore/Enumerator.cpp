//--------------------------------------------------------------------------------------
// Enumerator.h
//
// A lightweight reusable class to demonstrate asychronous enumeration using overlapped
// I/O and the XEnumerate API.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <assert.h>
#include <stdio.h>
#include <xtl.h>
#include <xonline.h>
#include "AtgSignIn.h"
#include "AtgUtil.h"
#include "Enumerator.h"

//--------------------------------------------------------------------------------------
// Enumerator implementation
//--------------------------------------------------------------------------------------
Enumerator::Enumerator()
: m_hEnumeration( INVALID_HANDLE_VALUE ),
m_dwBufferSize( 0 ),
m_pBuffer( NULL ),
m_bIsInitialized( FALSE ),
m_bEnumerateNext( FALSE ),
m_hrErrorStatus( S_OK )
{
    ZeroMemory( &m_Overlapped, sizeof( XOVERLAPPED ) );
    m_Overlapped.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    if( m_Overlapped.hEvent == NULL )
    {
        ATG::FatalError( "Failed to create Overlapped event.\n" );
    }
}

Enumerator::~Enumerator()
{
    Cancel();
    CloseHandle( m_Overlapped.hEvent );
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc:
// Calls Create and then allocates the internal buffer for storing enumeration results
//--------------------------------------------------------------------------------------
HRESULT
Enumerator::Initialize()
{
    HRESULT hr = S_OK;
    
    // Call Create to get an enumeration handle and buffer size
    hr = Create( m_hEnumeration, m_dwBufferSize );

    // Allocate a buffer for enumeration results
    if ( SUCCEEDED(hr) )
    {
        m_pBuffer = new BYTE[m_dwBufferSize];
        if ( !m_pBuffer )
        {
            hr = E_OUTOFMEMORY;
        }
    }

    if ( FAILED(hr) )
    {
        Cleanup();
    }
    else
    {
        // Ready to start enumeration
        m_bIsInitialized = TRUE;
        m_bEnumerateNext = TRUE;
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: Cancel
// Desc: Cancel any overlapped operations and revert to uninitialized state
//--------------------------------------------------------------------------------------
HRESULT
Enumerator::Cancel()
{
    HRESULT hr = S_OK;
    if ( !XHasOverlappedIoCompleted( &m_Overlapped) )
    {
        DWORD dw = XCancelOverlapped( &m_Overlapped );
        hr = HRESULT_FROM_WIN32(dw);
    }

    Cleanup();

    // Restore status flags/errors to indicate unitialized state
    m_bIsInitialized = FALSE;
    m_bEnumerateNext = FALSE;
    m_hrErrorStatus = hr;

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called every frame to perform enumeration
//--------------------------------------------------------------------------------------
HRESULT
Enumerator::Update()
{
    // Bail out early if the enumerator is not initialized
    assert( m_bIsInitialized );
    if ( !m_bIsInitialized )
    {
        return E_FAIL;
    }

    if ( FAILED(m_hrErrorStatus) )
    {
        return m_hrErrorStatus;
    }

    // Once we have called XEnumerate then next batch of results is ready 
    // when the overlaped operation is complete.
    if ( !m_bEnumerateNext && XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        DWORD dwCount = 0;
        DWORD dwStatus = XGetOverlappedResult( &m_Overlapped, &dwCount, TRUE );

        if ( ERROR_SUCCESS == dwStatus )
        {
            OnData( dwCount, m_pBuffer );
            m_bEnumerateNext = TRUE;
        }
        else
        {
            m_hrErrorStatus = XGetOverlappedExtendedError(&m_Overlapped);

            // ERROR_NO_MORE_FILES is expected when the enumeration process has completed
            assert ( m_hrErrorStatus == HRESULT_FROM_WIN32(ERROR_NO_MORE_FILES) );
            return m_hrErrorStatus;
        }
    }

    // Repeatedly call XEnumerate until we get ERROR_NO_MORE_FILES. m_bEnumerateNext
    // indicates if we still need to call XEnumerate
    if ( m_bEnumerateNext )
    {
        DWORD dwError = XEnumerate( m_hEnumeration, m_pBuffer, m_dwBufferSize, NULL, &m_Overlapped );
        m_bEnumerateNext = FALSE;
        
        // ERROR_IO_PENDING is expected for asynchronous I/O
        assert( ERROR_IO_PENDING == dwError );
        if ( ERROR_IO_PENDING != dwError )
        {
            m_hrErrorStatus = HRESULT_FROM_WIN32(dwError);
            return m_hrErrorStatus;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Cleanup
// Desc: Free memory and resources
//--------------------------------------------------------------------------------------
VOID
Enumerator::Cleanup()
{
    // Free the handle
    if ( m_hEnumeration != INVALID_HANDLE_VALUE )
    {
        XCloseHandle( m_hEnumeration );
        m_hEnumeration = INVALID_HANDLE_VALUE;
    }

    // Delete the buffer
    if (m_pBuffer)
    {
        delete [] m_pBuffer;
        m_pBuffer = NULL;
    }
    m_dwBufferSize = 0;
}
