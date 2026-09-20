//----------------------------------------------------------------------------------------------------------------------
// XEDFileIterators.cpp
//
// Implementation file for XEDFileIterators methods. 
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "CXEDFile.h"
#include "XEDFileIterators.h"

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFwdEventIterator::CXEDFwdEventIterator
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
CXEDFwdEventIterator::CXEDFwdEventIterator( CXEDFile& file, XEDFRAMETYPE type )
: CXEDEventIterator( file, type )
{
    Reset();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFwdEventIterator::Reset
// Desc: Initializes the event iterator to the start of the file.
//----------------------------------------------------------------------------------------------------------------------
VOID CXEDFwdEventIterator::Reset()
{
    m_eventCount = m_file.GetEventCount( m_type );

    if ( m_eventCount == 0 )
    {
        return;
    }

    m_currentEventIndex = 0;

    // Get the initial (and next) frame info.

    m_currentFrame = m_file.ReadFrameInfo( m_type, 0 );

    if ( m_eventCount > 1 )
    {
        m_nextFrame = m_file.ReadFrameInfo( m_type, 1);
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFwdEventIterator::MoveNext
// Desc: Moves the event iterator to the next event in the file.
//----------------------------------------------------------------------------------------------------------------------
BOOL CXEDFwdEventIterator::MoveNext()
{
    assert( !IsEOF() && "Cannot advance past the end of the file");

    if ( IsEOF() )
    {
        return FALSE;
    }

    m_currentFrame = m_nextFrame;

    m_currentEventIndex++;

    if ( !IsEOF() )
    {
        m_currentFrame = m_file.ReadFrameInfo( m_type, m_currentEventIndex );
    }

    return TRUE;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDReverseEventIterator::CXEDReverseEventIterator
// Desc: Creates a new instance of the event iterator, set to iterate through the specified file, and through events of
//       the specified type.
//----------------------------------------------------------------------------------------------------------------------
CXEDReverseEventIterator::CXEDReverseEventIterator( CXEDFile& file, XEDFRAMETYPE type )
    : CXEDEventIterator( file, type )
{
    // this space intentionally left blank
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDReverseEventIterator::Reset
// Desc: Initializes the event iterator to the end of the file.
//----------------------------------------------------------------------------------------------------------------------
VOID CXEDReverseEventIterator::Reset()
{
    m_eventCount = m_file.GetEventCount( m_type );

    if ( m_eventCount == 0 )
    {
        return;
    }

    m_currentEventIndex = m_eventCount - 1;

    // Get the initial (and next) frame info.

    m_currentFrame = m_file.ReadFrameInfo( m_type, m_currentEventIndex );

    if ( m_eventCount > 1 )
    {
        m_nextFrame = m_file.ReadFrameInfo( m_type, m_currentEventIndex - 1 );
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDReverseEventIterator::MoveNext
// Desc: Moves the event iterator to the previous event in the file.
//----------------------------------------------------------------------------------------------------------------------
BOOL CXEDReverseEventIterator::MoveNext()
{
    assert( !IsEOF() && "Cannot advance past the end of the file");

    if ( IsEOF() )
    {
        return FALSE;
    }

    m_currentFrame = m_nextFrame;

    m_currentEventIndex--;

    if ( !IsEOF() )
    {
        m_nextFrame = m_file.ReadFrameInfo( m_type, m_currentEventIndex );
    }

    return TRUE;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDEventIterator::CXEDEventIterator
// Desc: Constructor for the event iterator; binds to an XED file and a specific frame type. As this is a base type, no
//       other work is performed.
//----------------------------------------------------------------------------------------------------------------------
CXEDEventIterator::CXEDEventIterator( CXEDFile& file, XEDFRAMETYPE type )
: m_file(file),
  m_type(type)
{
   // intentionally left blank. 
}
