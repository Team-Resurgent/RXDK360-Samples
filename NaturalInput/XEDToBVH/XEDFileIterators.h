//----------------------------------------------------------------------------------------------------------------------
// XEDFileIterators.h
//
// A collection of classes which can be used to iterate over the events in a given
// XED file.
//
// CXEDMuxFwdFrameIterator and CXEDMuxReverseFrameIterator are iterators which
// will traverse all of the events in a file, of all types, returning each frame in
// order of time.
//
// If a multiple frames of data have the same frame number, then they will be returned
// in the order Color, Depth, then Skeleton.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#ifndef XEDFILEITERATORS_H_GUARD
#define XEDFILEITERATORS_H_GUARD

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
// Name; CXEDEventIterator
// Desc: The base class for both the forward and reverse event iterators. Cannot be directly created; use
//       CXEDFwdEventIterator or CXEDReverseEventIterator.
//----------------------------------------------------------------------------------------------------------------------
class CXEDEventIterator
{
public:
    const XEDFrameInfo& GetCurrentFrameInfo();
    UINT GetCurrentIndex() const;
    const XEDFrameInfo& GetNextFrameInfo();
    XEDFRAMETYPE GetType() const;
    UINT Count() const;

protected:

    CXEDEventIterator( CXEDFile& file, XEDFRAMETYPE type ); // cannot create

private:
    CXEDEventIterator(); // disabled
    CXEDEventIterator& operator=(CXEDEventIterator& rhs); // disabled

protected:

    CXEDFile& m_file;
    XEDFRAMETYPE m_type;
    UINT m_currentEventIndex;
    UINT m_eventCount;
    XEDFrameInfo m_currentFrame;
    XEDFrameInfo m_nextFrame;
};


//----------------------------------------------------------------------------------------------------------------------
// Name; CXEDFwdEventIterator
// Desc: A cursor object which keeps track of current frame information for a file, and can advance forwards through
//       the file.
//----------------------------------------------------------------------------------------------------------------------
class CXEDFwdEventIterator : public CXEDEventIterator
{
public:
    CXEDFwdEventIterator( CXEDFile& file, XEDFRAMETYPE type );
    VOID Reset();
    BOOL MoveNext();
    BOOL IsEOF();

};


//----------------------------------------------------------------------------------------------------------------------
// Name; CXEDReverseEventIterator
// Desc: A cursor object which keeps track of current frame information for a file, and can advance backwards through
//       the file.
//----------------------------------------------------------------------------------------------------------------------
class CXEDReverseEventIterator : public CXEDEventIterator
{
public:

    CXEDReverseEventIterator( CXEDFile& file, XEDFRAMETYPE type );
    VOID Reset();
    BOOL MoveNext();
    BOOL IsEOF();

};


//----------------------------------------------------------------------------------------------------------------------
// Cursor comparison functions
//----------------------------------------------------------------------------------------------------------------------
inline CXEDReverseEventIterator& XEDLeastOf( CXEDReverseEventIterator& a, CXEDReverseEventIterator& b );
inline CXEDFwdEventIterator& XEDLeastOf( CXEDFwdEventIterator& a, CXEDFwdEventIterator& b );
inline CXEDReverseEventIterator& XEDGreatestOf( CXEDReverseEventIterator& a, CXEDReverseEventIterator& b );
inline CXEDFwdEventIterator& XEDGreatestOf( CXEDFwdEventIterator& a, CXEDFwdEventIterator& b );


//----------------------------------------------------------------------------------------------------------------------
// Name: TMuxFrameIterator
// Desc: An iterator which multiplexes and enumerates all of the frame types in the associated file.
//----------------------------------------------------------------------------------------------------------------------
template <class TEventIterator> class TXEDMuxFrameIterator
{
public:

    TXEDMuxFrameIterator( CXEDFile& file );

    VOID Reset();
    BOOL MoveNext();
    BOOL IsEOF();

    CXEDFile& GetHostFile();
    UINT GetLogicalEventIndex();
    UINT GetFrameEventIndex();
    XEDFRAMETYPE GetCurrentType();
    XEDFrameInfo GetCurrentInfo();
    HRESULT GetCurrentFrame( CXEDFrame* pOutFrame );
    UINT Count();

private:
    CXEDFile& m_file;
    TEventIterator& GetCurrentEventIterator();

    TEventIterator m_colorEvents;
    TEventIterator m_depthEvents;
    TEventIterator m_skeletonEvents;
    XEDFRAMETYPE m_currentFrameType;

    UINT m_logicalEventIndex;
    UINT m_maxEventIndex;
};


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDMuxFwdFrameIterator
// Desc: Iterates from the start of the file through a multiplexed stream of all of the frames in the file.
//----------------------------------------------------------------------------------------------------------------------
typedef TXEDMuxFrameIterator< CXEDFwdEventIterator > CXEDMuxFwdFrameIterator;


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDMuxReverseFrameIterator
// Desc: Iterates from the end of the file through a multiplexed stream of all of the frames in the file.
//----------------------------------------------------------------------------------------------------------------------
typedef TXEDMuxFrameIterator< CXEDReverseEventIterator > CXEDMuxReverseFrameIterator;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "XEDFileIterators.inl"

#endif //XEDFILEITERATORS_H_GUARD