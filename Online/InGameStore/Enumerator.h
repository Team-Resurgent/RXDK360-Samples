//--------------------------------------------------------------------------------------
// Enumerator.h
//
// A lightweight reusable class to demonstrate asychronous enumeration using overlapped
// I/O and the XEnumerate API.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ENUMERATOR_H
#define ENUMERATOR_H

//--------------------------------------------------------------------------------------
// Max number items in each call to XEnumerate (XEnumerate may be called multiple times
// before all results have been enumerated).
//--------------------------------------------------------------------------------------
const DWORD MAX_ENUMERATION_RESULTS = 10;

//--------------------------------------------------------------------------------------
// Base class is reusable for different types of enumeration. For example, it is used
// for both content enumeration and offer enumerattion.
//--------------------------------------------------------------------------------------
class Enumerator
{

public:

    Enumerator();
    ~Enumerator();

public:

    //--------------------------------------------------------------------------------------
    // The following methods will be overriden by the derrived class to enable
    // enumeration of different types of data.
    //--------------------------------------------------------------------------------------

    //--------------------------------------------------------------------------------------
    // Name: Create
    // Desc:
    // Implementation must create the enumeration handle (E.g. XContentCreateEnumerator,
    // XMarketplaceCreateOfferEnumerator).
    //
    // Implementation must also compute the size of the buffer needed to store the
    // results of each enumeration call.
    //--------------------------------------------------------------------------------------
    virtual HRESULT Create( HANDLE &o_hEnueratoion, DWORD& o_dwBufferSize ) = 0;

    //--------------------------------------------------------------------------------------
    // Name: OnData
    // Desc:
    // Called each time that data is available during enumeration
    // Implementation should copy data from the internal buffer to title specific memory
    //--------------------------------------------------------------------------------------
    virtual VOID OnData( DWORD dwCount, VOID* pBuffer ) = 0;

public:

    //--------------------------------------------------------------------------------------
    // The following methods drive the enumeration process
    //--------------------------------------------------------------------------------------

    //--------------------------------------------------------------------------------------
    // Name: Initialize
    // Desc:
    // Calls Create and then allocates the internal buffer for storing enumeration results
    //--------------------------------------------------------------------------------------
    HRESULT Initialize();

    //--------------------------------------------------------------------------------------
    // Name: Cancel
    // Desc: Cancel any overlapped operations and revert to uninitialized state
    //--------------------------------------------------------------------------------------
    HRESULT Cancel();

    //--------------------------------------------------------------------------------------
    // Name: Update
    // Desc: Called every frame to perform enumeration
    //--------------------------------------------------------------------------------------
    HRESULT Update();

public:

    //--------------------------------------------------------------------------------------
    // The following methods provide status information for the enumeration
    //--------------------------------------------------------------------------------------

    //--------------------------------------------------------------------------------------
    // Name: IsInitialized
    // Desc: Check to see if it is intialized before calling Update
    //--------------------------------------------------------------------------------------
    BOOL IsInitialized() { return m_bIsInitialized; }

    //--------------------------------------------------------------------------------------
    // Name: GetErrorStatus
    // Desc: Call this method to determine whether there was an error during enumeration
    //--------------------------------------------------------------------------------------
    HRESULT GetErrorStatus() { return m_hrErrorStatus; }
    
private:

    //--------------------------------------------------------------------------------------
    // Name: Cleanup
    // Desc: Free memory and resources
    //--------------------------------------------------------------------------------------
    VOID Cleanup();

private:

    //--------------------------------------------------------------------------------------
    // Enumeration State
    //--------------------------------------------------------------------------------------

    HANDLE      m_hEnumeration; // Enumeration handle
    XOVERLAPPED m_Overlapped;   // Overlapped structure for asynchronous I/O
    DWORD       m_dwBufferSize; // Size of the enumeration buffer
    BYTE*       m_pBuffer;      // Pointer to the enumeration buffer

    BOOL        m_bIsInitialized; // The Enumerator is initialized and can call update
    BOOL        m_bEnumerateNext; // Time to enumerate the next several items
    HRESULT     m_hrErrorStatus;  // Error status of the current enumeration

private:

    //--------------------------------------------------------------------------------------
    // Prevent copying instances of this class
    //--------------------------------------------------------------------------------------
    Enumerator( const Enumerator& ) {}
    Enumerator& operator=( const Enumerator& rhs ) { return *this; }
};

#endif // ENUMERATOR_H