//----------------------------------------------------------------------------------------------------------------------
// XEDFileIterators.inl
//
// Inline implementation file for XED File Iterator methods. 
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDEventIterator::GetType
// Desc: Obtains the type of events that this iterator iterates over.
//----------------------------------------------------------------------------------------------------------------------
inline XEDFRAMETYPE CXEDEventIterator::GetType() const
{
    return m_type;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDEventIterator::GetCurrentFrameInfo
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
inline const XEDFrameInfo& CXEDEventIterator::GetCurrentFrameInfo()
{
    return m_currentFrame;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDEventIterator::GetNextFrameInfo
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
inline const XEDFrameInfo& CXEDEventIterator::GetNextFrameInfo()
{
    return m_nextFrame;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDEventIterator::GetCurrentIndex
// Desc: Gets the current event index for the cursor.
//----------------------------------------------------------------------------------------------------------------------
inline UINT CXEDEventIterator::GetCurrentIndex() const
{
    return m_currentEventIndex;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDEventIterator::GetCount
// Desc: Gets the total number of events of this type in the file. 
//----------------------------------------------------------------------------------------------------------------------
inline UINT CXEDEventIterator::Count() const
{
    return m_eventCount;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFwdEventIterator::IsEOF
// Desc: Returns TRUE if the last event of the file has already been reached.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL CXEDFwdEventIterator::IsEOF()
{
    return ( m_currentEventIndex >= m_eventCount );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDReverseEventIterator::IsEOF
// Desc: Returns TRUE if the last event of the file has already been reached.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL CXEDReverseEventIterator::IsEOF()
{
    return ( m_currentEventIndex == (UINT)(-1));
}


//----------------------------------------------------------------------------------------------------------------------
// Name: TXEDMuxFrameIterator<TEventIterator>::TXEDMuxFrameIterator
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator>
inline TXEDMuxFrameIterator<TEventIterator>::TXEDMuxFrameIterator( CXEDFile& file )
: m_file( file ),
  m_colorEvents( file, XED_COLOR ),
  m_depthEvents( file, XED_DEPTH ),
  m_skeletonEvents( file, XED_SKELETON )
{
    Reset();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: TXEDMuxFrameIterator<TEventIterator>::IsEOF
// Desc: Returns TRUE if the iterator is at the end of the file.
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator>
inline BOOL TXEDMuxFrameIterator<TEventIterator>::IsEOF()
{
    return m_colorEvents.IsEOF() &&
           m_depthEvents.IsEOF() &&
           m_skeletonEvents.IsEOF();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: TXEDMuxFrameIterator<TEventIterator>::Reset
// Desc: Resets the iterator. Note: You need to call Reset before using the iterator,
//       as it may cause errors.
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator>
inline VOID TXEDMuxFrameIterator<TEventIterator>::Reset()
{
    m_logicalEventIndex = 0;
    m_colorEvents.Reset();
    m_depthEvents.Reset();
    m_skeletonEvents.Reset();

    // Find the first of the events.
    TEventIterator& first = XEDLeastOf( m_skeletonEvents, XEDLeastOf( m_colorEvents, m_depthEvents ) );

    m_currentFrameType = first.GetType();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: TXEDMuxFrameIterator<TEventIterator>::MoveNext
// Desc: Moves to the next event in the stream; returns TRUE if there is more data
//       available.
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator>
inline BOOL TXEDMuxFrameIterator<TEventIterator>::MoveNext()
{
    assert( !IsEOF() && "Is at end of file" );
 
    if ( IsEOF() )
    {
        return FALSE;
    }

    // Get the iterator for the current event.

    TEventIterator* pIt1 = &GetCurrentEventIterator();
    ++m_logicalEventIndex;
    pIt1->MoveNext();

    TEventIterator* pIt2;
    TEventIterator* pIt3;

    // Gets the event iterators for the other two streams.
    // NOTE: the ordering of events in each case of this switch is important; we always try to keep events coming in
    // the order Color->Depth->Skeleton if they have identical timestamps.

    switch ( pIt1->GetType() )
    {
        case XED_COLOR:
        {
            pIt2 = &m_depthEvents;
            pIt3 = &m_skeletonEvents;
            break;
        }
        case XED_DEPTH:
        {
            pIt2 = &m_skeletonEvents;
            pIt3 = &m_colorEvents;
            break;
        }
        case XED_SKELETON:
        {
            pIt2 = &m_colorEvents;
            pIt3 = &m_depthEvents;
            break;
        }
        default:
        {
            NODEFAULT;
        }
    }
    
    TEventIterator* pNextIterator = &XEDLeastOf( *pIt1, XEDLeastOf( *pIt2, *pIt3 ) );
    m_currentFrameType = pNextIterator->GetType();

    return !pNextIterator->IsEOF();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: TXEDMuxFrameIterator<TEventIterator>::GetLogicalEventIndex
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator>
inline UINT TXEDMuxFrameIterator<TEventIterator>::GetLogicalEventIndex()
{
    return m_logicalEventIndex;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: TXEDMuxFrameIterator<TEventIterator>::GetFrameEventIndex
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator>
inline UINT TXEDMuxFrameIterator<TEventIterator>::GetFrameEventIndex()
{
    return GetCurrentEventIterator().GetCurrentIndex();
}

//----------------------------------------------------------------------------------------------------------------------
// Name: TXEDMuxFrameIterator<TEventIterator>::GetCurrentType
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator>
inline XEDFRAMETYPE TXEDMuxFrameIterator<TEventIterator>::GetCurrentType()
{
    return m_currentFrameType;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: TXEDMuxFrameIterator<TEventIterator>::GetCurrentInfo
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator>
inline XEDFrameInfo TXEDMuxFrameIterator<TEventIterator>::GetCurrentInfo()
{
    return GetCurrentEventIterator().GetCurrentFrameInfo();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: TXEDMuxFrameIterator<TEventIterator>::GetCurrentEventIterator
// Desc: Gets the last iterated-over event cursor.
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator>
inline TEventIterator& TXEDMuxFrameIterator<TEventIterator>::GetCurrentEventIterator()
{
    switch ( m_currentFrameType )
    {
    case XED_COLOR:
        return m_colorEvents;
    case XED_DEPTH:
        return m_depthEvents;
    case XED_SKELETON:
        return m_skeletonEvents;
    default:
        NODEFAULT;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: TXEDMuxFrameIterator<TEventIterator>::GetCurrentFrame
// Desc: Obtains the current frame's actual data from the iterator. Only use this when you want to modify or export the
//       actual data, not timestamp info. For timestamps, call GetCurrentFrameInfo on the iterator.
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator>
inline HRESULT TXEDMuxFrameIterator<TEventIterator>::GetCurrentFrame( CXEDFrame* pOutFrame )
{
    assert( !IsEOF() && "Reached end of file." );

    if ( IsEOF() )
        return HRESULT_FROM_WIN32( ERROR_HANDLE_EOF );

    UINT currentFrame = GetFrameEventIndex();
    
    switch ( m_currentFrameType )
    {
    case XED_COLOR:
        {
            CXEDColorFrame* pFrame = static_cast<CXEDColorFrame*>(pOutFrame);
            return m_file.ReadFrame( currentFrame, pFrame );
        }
    case XED_DEPTH:
        {
            CXEDDepthFrame* pFrame = static_cast<CXEDDepthFrame*>(pOutFrame);
            return m_file.ReadFrame( currentFrame, pFrame );
        }
    case XED_SKELETON:
        {
            CXEDSkeletonFrame* pFrame = static_cast<CXEDSkeletonFrame*>(pOutFrame);
            return m_file.ReadFrame( currentFrame, pFrame );
        }
    default:

        NODEFAULT;

    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: TXEDMuxFrameIterator<TEventIterator>::Count
// Desc: Returns the total number of events in the file.
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator>
inline UINT TXEDMuxFrameIterator<TEventIterator>::Count()
{
    return m_colorEvents.Count + m_depthEvents.Count() + m_skeletonEvents.Count();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDLeastOf
// Desc: Returns the forward event iterator with an event index closest to the start of the file.
//----------------------------------------------------------------------------------------------------------------------
inline CXEDFwdEventIterator& XEDLeastOf( CXEDFwdEventIterator& a,
                                       CXEDFwdEventIterator& b )
{
    if ( b.IsEOF() )
        return a;

    if ( !a.IsEOF() && a.GetCurrentFrameInfo().CompareTimestamps( b.GetCurrentFrameInfo() ) <= 0 )
    {
        return a;
    }
    else
    {
        return b;
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDGreatestOf
// Desc: Returns the forward event iterator with an event index closest to the end of the file.
//----------------------------------------------------------------------------------------------------------------------
inline CXEDFwdEventIterator& XEDGreatestOf( CXEDFwdEventIterator& a,
                                          CXEDFwdEventIterator& b )
{
    if ( b.IsEOF() )
        return a;

    if ( !a.IsEOF() && a.GetCurrentFrameInfo().CompareTimestamps( b.GetCurrentFrameInfo() ) > 0 )
    {
        return a;
    }
    else
    {
        return b;
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDLeastOf 
// Desc: Returns the reverse event iterator with an event index closest to the end of the file.
//----------------------------------------------------------------------------------------------------------------------
inline CXEDReverseEventIterator& XEDLeastOf( CXEDReverseEventIterator& a,
                                           CXEDReverseEventIterator& b )
{
    if ( b.IsEOF() || b.Count() == 0 )
        return a;

    if ( ( !a.IsEOF() && !a.Count() == 0 ) && a.GetCurrentFrameInfo().CompareTimestamps( b.GetCurrentFrameInfo() ) >= 0 )
    {
        return a;
    }
    else
    {
        return b;
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDGreatestOf
// Desc: Returns the reverse event iterator with an event index closest to the end of
//       the file.
//----------------------------------------------------------------------------------------------------------------------
inline CXEDReverseEventIterator& XEDGreatestOf( CXEDReverseEventIterator& a,
                                              CXEDReverseEventIterator& b )
{
    if ( b.IsEOF() || b.Count() == 0 )
        return a;

    if ( (!a.IsEOF() && !a.Count() == 0) && a.GetCurrentFrameInfo().CompareTimestamps( b.GetCurrentFrameInfo() ) < 0 )
    {
        return a;
    }
    else
    {
        return b;
    }
}
