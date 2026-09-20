//----------------------------------------------------------------------------------------------------------------------
// XEDFile.cpp
//
// Implementation file for XEDFile. 
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "CXEDFile.h"
#include <malloc.h>

//----------------------------------------------------------------------------------------------------------------------
// Name: XEDMultiFrameRange::GetTotalFrameRange
// Desc: Returns the full count of frames across all stream types between the first and the last frame numbers.
//----------------------------------------------------------------------------------------------------------------------
UINT XEDMultiFrameRange::GetTotalFrameRange() const
{
    BOOL bHasValidData = FALSE;

    //NOTE: XED_FRAMENUMBER_INVALID is also 1 past the highest possible frame number so we can use it as a sentinel here

    UINT startFrame = XED_FRAMENUMBER_INVALID;
    UINT endFrame = 0;

    if ( color.IsValid() )
    {
        bHasValidData = TRUE;
        startFrame = min( color.start.frameNumber, startFrame );
        endFrame = max( color.end.frameNumber, endFrame );
    }

    if ( depth.IsValid() )
    {
        bHasValidData = TRUE;
        startFrame = min( depth.start.frameNumber, startFrame );
        endFrame = max( depth.end.frameNumber, endFrame );
    }

    if ( skeleton.IsValid() )
    {
        bHasValidData = TRUE;
        startFrame = min( skeleton.start.frameNumber, startFrame );
        endFrame = max( skeleton.end.frameNumber, endFrame );
    }

    if ( !bHasValidData )
        return 0;

    return endFrame - startFrame + 1;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDMultiFrameRange::GetTotalTimeRange
// Desc: Obtains the total duration for this range across all frame types. If there are no frames, 0 is returned. If
//       there is 1 or more, SINGLE_NUI_FRAME_TIME_USEC worth of time is added to handle ranges with only one frame.
//----------------------------------------------------------------------------------------------------------------------
UINT64 XEDMultiFrameRange::GetTotalTimeRange() const
{
    BOOL bHasValidData = FALSE;

    //NOTE: XED_TIMESTAMP_INVALID is also 1 past the highest possible timestamp. so we can use it as a sentinel here.
    UINT64 startTime = XED_TIMESTAMP_INVALID;
    UINT64 endTime = 0;

    if ( color.IsValid() )
    {
        bHasValidData = TRUE;
        startTime = min( color.start.timestampInUsec, startTime );
        endTime = max( color.end.timestampInUsec, endTime );
    }

    if ( depth.IsValid() )
    {
        bHasValidData = TRUE;
        startTime = min( depth.start.timestampInUsec, startTime );
        endTime = max( depth.end.timestampInUsec, endTime );
    }

    if ( skeleton.IsValid() )
    {
        bHasValidData = TRUE;
        startTime = min( skeleton.start.timestampInUsec, startTime );
        endTime = max( skeleton.end.timestampInUsec, endTime );
    }

    if ( !bHasValidData )
        return 0;

    return endTime - startTime + SINGLE_NUI_FRAME_TIME_USEC;
	//NOTE: might not hold as additional frame types are added.
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::~CXEDFile
//----------------------------------------------------------------------------------------------------------------------
CXEDFile::~CXEDFile()
{
    if ( IsOpen() )
    {
        HRESULT hr = Close();

        if ( FAILED(hr) )
        {
            printf( "File failed to close (HR 0x%X)", hr );
            assert( !"~CXEDFile() failed to close underlying handle" );
        }
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::GetFileDuration
//----------------------------------------------------------------------------------------------------------------------
UINT64 CXEDFile::GetFileDuration()
{
    XEDMultiFrameRange range = GetMultiFrameInfo();
    return range.GetTotalTimeRange();
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::GetEventCount
// Desc: Gets the number of frames present in the file for the specified frame type.
//----------------------------------------------------------------------------------------------------------------------
UINT CXEDFile::GetEventCount( XEDFRAMETYPE frameType )
{
    assert( IsOpen() && "File is not open" );

    switch (frameType)
    {
        case XED_COLOR:
        {
            return XedGetNuiColorEventCount( m_pFileContext );
        }
        case XED_DEPTH:
        {
            return XedGetNuiDepthEventCount( m_pFileContext );
        }
        case XED_SKELETON:
        {
            return XedGetNuiSkeletonEventCount( m_pFileContext );
        }
        default:
        {
            return XED_EVENTINDEX_INVALID;
        }
          
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::GetEventIndexFromFrameNumber
// Desc: Gets the event index for the given frame type which corresponds to the specified frame number.
//----------------------------------------------------------------------------------------------------------------------
UINT CXEDFile::GetEventIndexFromFrameNumber( XEDFRAMETYPE frameType, UINT frameNumber )
{
    assert( IsOpen() && "File is not open" );

    switch (frameType)
    {
    case XED_COLOR:
        {
            return XedGetNuiColorEventIndexFromFrameNumber( m_pFileContext, frameNumber );
        }
    case XED_DEPTH:
        {
            return XedGetNuiDepthEventIndexFromFrameNumber( m_pFileContext, frameNumber );
        }
    case XED_SKELETON:
        {
            return XedGetNuiSkeletonEventIndexFromFrameNumber( m_pFileContext, frameNumber );
        }
    default:
        {
            return XED_EVENTINDEX_INVALID;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::GetEventIndexFromTime
// Desc: Gets the event index for the given frame type which corresponds to the
//       specified timestamp.
//----------------------------------------------------------------------------------------------------------------------
UINT CXEDFile::GetEventIndexFromTime( XEDFRAMETYPE frameType, UINT64 microseconds )
{
    assert( IsOpen() && "File is not open" );

    switch (frameType)
    {
    case XED_COLOR:
        {
            return XedGetNuiColorEventIndexFromMicroseconds( m_pFileContext, microseconds );
        }
    case XED_DEPTH:
        {
            return XedGetNuiDepthEventIndexFromMicroseconds( m_pFileContext, microseconds );
        }
    case XED_SKELETON:
        {
            return XedGetNuiSkeletonEventIndexFromMicroseconds( m_pFileContext, microseconds );
        }
    default:
        {
            return XED_EVENTINDEX_INVALID;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::ReadFrameInfo
// Desc: Reads the timestamp information (frame# and time) for a specified event index and frame type.
//----------------------------------------------------------------------------------------------------------------------

XEDFrameInfo CXEDFile::ReadFrameInfo( XEDFRAMETYPE frameType, UINT eventIndex )
{
    assert( IsOpen() && "File is not open" );

    XEDFrameInfo frameInfo;
    frameInfo.type = frameType;

    HRESULT hr = E_FAIL;

    switch (frameType)
    {
    case XED_COLOR:
        {
            hr = XedReadNuiColorFrame( m_pFileContext, eventIndex, &frameInfo.frameNumber, &frameInfo.timestampInUsec,
                                       NULL, 0, FALSE );
            break;
        }
    case XED_DEPTH:
        {
            hr = XedReadNuiDepthFrame( m_pFileContext, eventIndex, &frameInfo.frameNumber, &frameInfo.timestampInUsec,
                                       NULL, 0, FALSE );
            break;
        }
    case XED_SKELETON:
        {
            hr = XedReadNuiSkeletonFrame( m_pFileContext, eventIndex, &frameInfo.frameNumber,
                                          &frameInfo.timestampInUsec, NULL, 0 );
            break;
        }
    default:
        return frameInfo; // will be marked invalid by frameInfo constructor.
    }

    //NOTE: XedFileXXXReadFrame should NEVER return an error once the file has been successfully opened if we're just
    //      reading timestamp info. We assert here just to be safe.

    assert( SUCCEEDED( hr ) );

    frameInfo.eventIndex = eventIndex;
    return frameInfo;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::ReadFrame
// Desc: Reads a Color frame from the specified event index in the file.
//----------------------------------------------------------------------------------------------------------------------
HRESULT CXEDFile::ReadFrame( UINT eventIndex, CXEDColorFrame* pOutFrame,
                             const BOOL bRegisterWithDepth /*= FALSE */ )
{
    assert( IsOpen() && "File is not open" );
    assert( pOutFrame != NULL && "Frame pointer is null.");
    assert( pOutFrame->GetData() != NULL && "Frame buffer was not allocated.");

    if ( pOutFrame == NULL || pOutFrame->GetData() == NULL )
    {
        return E_INVALIDARG;
    }

    pOutFrame->m_frameInfo.eventIndex = XED_EVENTINDEX_INVALID;

    HRESULT hr = XedReadNuiColorFrame( m_pFileContext, eventIndex,  &pOutFrame->m_frameInfo.frameNumber,
                                       &pOutFrame->m_frameInfo.timestampInUsec, pOutFrame->GetData(),
                                       pOutFrame->GetBufferLength(), bRegisterWithDepth );

    if ( SUCCEEDED( hr ) )
    {
        pOutFrame->m_frameInfo.eventIndex = eventIndex;
        pOutFrame->m_frameInfo.type = XED_COLOR;
    }

    return hr;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::ReadFrame
// Desc: Reads a Depth frame from the specified event index in the file.
//----------------------------------------------------------------------------------------------------------------------
HRESULT CXEDFile::ReadFrame( UINT eventIndex, CXEDDepthFrame* pOutFrame,
                             const BOOL bRegisterWithColor /*= FALSE */ )
{
    assert( IsOpen() && "File is not open" );
    assert( pOutFrame != NULL && "Frame pointer is null.");
    assert( pOutFrame->GetData() != NULL && "Frame buffer was not allocated.");

    if ( pOutFrame == NULL || pOutFrame->GetData() == NULL )
    {
        return E_INVALIDARG;
    }

    pOutFrame->m_frameInfo.eventIndex = XED_EVENTINDEX_INVALID;

    HRESULT hr = XedReadNuiDepthFrame( m_pFileContext, eventIndex, &pOutFrame->m_frameInfo.frameNumber,
                                       &pOutFrame->m_frameInfo.timestampInUsec, pOutFrame->GetData(),
                                       pOutFrame->GetBufferLength(), bRegisterWithColor );
    
    if ( SUCCEEDED( hr ) )
    {
        pOutFrame->m_frameInfo.eventIndex = eventIndex;
        pOutFrame->m_frameInfo.type = XED_DEPTH;
    }

    return hr;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::ReadFrame
// Desc: Reads a frame of skeleton data from the file.
//----------------------------------------------------------------------------------------------------------------------
HRESULT CXEDFile::ReadFrame( UINT eventIndex, CXEDSkeletonFrame* pOutFrame )
{
    assert( IsOpen() && "File is not open" );
    assert( pOutFrame != NULL && "Output frame pointer is null.");

    // Note: Skeleton frames don't have a separate buffer; we store the data in the frame object itself, so no
    //       need to check if the buffer has been allocated.

    if ( pOutFrame == NULL )
    {
        return E_INVALIDARG;
    }

    pOutFrame->m_frameInfo.eventIndex = XED_EVENTINDEX_INVALID;

    HRESULT hr = XedReadNuiSkeletonFrame( m_pFileContext, eventIndex, &pOutFrame->m_frameInfo.frameNumber,
                                          &pOutFrame->m_frameInfo.timestampInUsec, pOutFrame->GetData(),
                                          sizeof( NUI_SKELETON_FRAME ) );

    if ( SUCCEEDED( hr ) )
    {
        pOutFrame->m_frameInfo.eventIndex = eventIndex;
        pOutFrame->m_frameInfo.type = XED_SKELETON;
    }

    return hr;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::WriteFrame
// Desc: Writes a single frame (of type CXEDColorFrame, CXEDDepthFrame or 
//       CXEDSkeletonFrame) to the file.
//----------------------------------------------------------------------------------------------------------------------
HRESULT CXEDFile::WriteFrame( CXEDFrame* pFrame )
{
    assert( IsOpen() && "File is not open" );
    assert( pFrame != NULL && "Null frame pointer" );
    
    HRESULT hr;

    if ( pFrame == NULL )
    {
        return E_INVALIDARG;
    }

    switch( pFrame->GetType() )
    {
        case XED_COLOR:
        {
            CXEDColorFrame* pColorFrame = static_cast<CXEDColorFrame*>(pFrame);
            
            assert( pColorFrame->GetData() != NULL && "Frame buffer was not allocated.");
            if ( pColorFrame->GetData() == NULL )
            {
                return E_INVALIDARG;
            }

            hr = XedWriteNuiColorFrame( m_pFileContext, pColorFrame->GetFrameNumber(), pColorFrame->GetTimestamp(),
                                        pColorFrame->GetData(), pColorFrame->GetBufferLength() );
            return hr;
        }

        case XED_DEPTH:
        {
            CXEDDepthFrame* pDepthFrame = static_cast<CXEDDepthFrame*>(pFrame);

            assert( pDepthFrame->GetData() != NULL && "Frame buffer was not allocated.");
            if ( pDepthFrame->GetData() == NULL )
            {
                return E_INVALIDARG;
            }

            hr = XedWriteNuiDepthFrame( m_pFileContext, pDepthFrame->GetFrameNumber(), pDepthFrame->GetTimestamp(),
                                        pDepthFrame->GetData(), pDepthFrame->GetBufferLength() );
            return hr;
        }
        
        case XED_SKELETON:
        {
            CXEDSkeletonFrame* pSkeletonFrame = static_cast<CXEDSkeletonFrame*>(pFrame);
            
            // Note: Skeleton frames don't have a separate buffer; we store the data in the frame object itself, so no
            //       need to check.

            hr = XedWriteNuiSkeletonFrame( m_pFileContext, pSkeletonFrame->GetFrameNumber(),
                                           pSkeletonFrame->GetTimestamp(), pSkeletonFrame->GetData(),
                                           sizeof( NUI_SKELETON_FRAME ) );
            return hr;
        }

        default:
            return E_INVALIDARG;
    }

}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::CopyFrame
// Desc: Copies a single frame from the input file to the output file.
//       readFile  - the file to copy from.
//       writeFile - the file to copy to.
//       copyInfo  - contains the type of frame to copy, the event index in the read file to copy the frame from, and
//                   the framenumber and timestamp to use for the copy of the frame.
//----------------------------------------------------------------------------------------------------------------------
HRESULT CXEDFile::CopyFrame( CXEDFile& readFile, CXEDFile& writeFile, XEDFrameInfo& copyInfo )
{
    assert( readFile.IsOpen() && "From File was not open" );
    assert( writeFile.IsOpen() && "To File was not open" );
    assert( copyInfo.eventIndex < readFile.GetEventCount(copyInfo.type) && "Event to copy out of range" );

    switch( copyInfo.type )
    {
        case XED_COLOR:
        {
            return XedCopyNuiColorFrame( (XED_CONTEXT*)readFile, copyInfo.eventIndex, (XED_CONTEXT*)writeFile,
                                         copyInfo.frameNumber, copyInfo.timestampInUsec );
        }

        case XED_DEPTH:
        {
            return XedCopyNuiDepthFrame( (XED_CONTEXT*)readFile, copyInfo.eventIndex, (XED_CONTEXT*)writeFile,
                                          copyInfo.frameNumber, copyInfo.timestampInUsec );
        }
        
        case XED_SKELETON:
        {
            return XedCopyNuiSkeletonFrame( (XED_CONTEXT*)readFile, copyInfo.eventIndex, (XED_CONTEXT*)writeFile,
                                            copyInfo.frameNumber, copyInfo.timestampInUsec );
        }

        default:
        {
            return E_INVALIDARG;
        }
    }

}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::GetMultiFrameInfo
// Desc: Gets the time and frame number ranges for all of the frame types in the file.
//----------------------------------------------------------------------------------------------------------------------
XEDMultiFrameRange CXEDFile::GetMultiFrameInfo()
{
    XEDMultiFrameRange i;

    assert( IsOpen() && "File is not open" );

    i.color.eventCount = GetEventCount( XED_COLOR );
    i.depth.eventCount = GetEventCount( XED_DEPTH );
    i.skeleton.eventCount = GetEventCount( XED_SKELETON );

    // Get the highest and lowest time stamps of all the different types of events.

    if ( i.color.eventCount > 0 )
    {
        i.color.start = ReadFrameInfo( XED_COLOR, 0 );
        i.color.end = ReadFrameInfo( XED_COLOR, i.color.eventCount - 1 );
    }

    if ( i.depth.eventCount > 0 )
    {
        i.depth.start = ReadFrameInfo( XED_DEPTH, 0 );
        i.depth.end = ReadFrameInfo( XED_DEPTH, i.depth.eventCount - 1 );
    }

    if ( i.skeleton.eventCount > 0 )
    {
        i.skeleton.start = ReadFrameInfo( XED_SKELETON, 0 );
        i.skeleton.end = ReadFrameInfo( XED_SKELETON, i.skeleton.eventCount - 1 );
    }

    return i;

}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::Create (ANSI)
// Desc: Converts the filename to ANSI, and then calls the ANSI version of this function.
//----------------------------------------------------------------------------------------------------------------------
HRESULT CXEDFile::Create( LPCWSTR pszFilePath, BOOL fOverwrite /*= TRUE*/, CXEDFile* pTemplate /*= NULL */ )
{
    if (pszFilePath == NULL)
    {
        return E_INVALIDARG;
    }

    int origlen = wcslen( pszFilePath );
    int templen = ::WideCharToMultiByte( CP_ACP, 0, pszFilePath, origlen, NULL, 0, NULL, NULL );

    char* pszTemp = (char*)_malloca( templen + 1);
    BOOL bFailed = ( 0 == ::WideCharToMultiByte( CP_ACP, 0, pszFilePath, origlen, pszTemp, templen, NULL, NULL ) );		

    HRESULT hr = E_FAIL;

    if (!bFailed)
    {
        pszTemp[ templen ] = L'\0';
        hr = Create( pszTemp, fOverwrite, pTemplate );
    }

    _freea( pszTemp );
    return hr;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::Open (ANSI)
// Desc: Converts the filename to ANSI, and then calls the ANSI version of this function.
//----------------------------------------------------------------------------------------------------------------------
HRESULT CXEDFile::Open( LPCWSTR pszFilePath )
{
    if (pszFilePath == NULL)
    {
        return E_INVALIDARG;
    }

    int origlen = wcslen( pszFilePath );
    int templen = ::WideCharToMultiByte( CP_ACP, 0, pszFilePath, origlen, NULL, 0, NULL, NULL );

    char* pszTemp = (char*)_malloca( templen + 1 );
    BOOL bFailed = (0 == ::WideCharToMultiByte( CP_ACP, 0, pszFilePath, origlen, pszTemp, templen, NULL, NULL ));		

    HRESULT hr = E_FAIL;

    if (!bFailed)
    {
        pszTemp[ templen ] = L'\0';
        hr = Open( pszTemp );
    }

    _freea( pszTemp );
    return hr;
}


//----------------------------------------------------------------------------------------------------------------------
// CXEDDepthFrame Methods
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDDepthFrame::GetDimensions
// Desc: Returns the dimensions of the depth buffer in pixels.
//----------------------------------------------------------------------------------------------------------------------
SIZE CXEDDepthFrame::GetDimensions()
{
    SIZE s; 
    s.cx = DEPTH_BUFFER_WIDTH;
    s.cy = DEPTH_BUFFER_HEIGHT;
    return s;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDepthFrame::GetBufferLength
// Desc: Returns the size of the depth frame buffer in bytes.
//----------------------------------------------------------------------------------------------------------------------
DWORD CXEDDepthFrame::GetBufferLength()
{
    return sizeof(WORD) * DEPTH_BUFFER_WIDTH * DEPTH_BUFFER_HEIGHT;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDDepthFrame::AllocateBuffer
// Desc: Allocates enough memory to store a single frame of depth buffer information.
//----------------------------------------------------------------------------------------------------------------------
HRESULT CXEDDepthFrame::AllocateBuffer()
{
    assert( m_pBuffer == NULL && "Buffer already allocated" );

    m_pBuffer = (WORD*)malloc( GetBufferLength() );
    
    if (m_pBuffer == NULL)
    {
        return E_OUTOFMEMORY;
    }
    
    return S_OK;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDDepthFrame::~CXEDDepthFrame
// Desc: Destroy the frame, freeing any allocated buffer.
//----------------------------------------------------------------------------------------------------------------------
CXEDDepthFrame::~CXEDDepthFrame()
{
    if( m_pBuffer )
    {
        free( m_pBuffer );
        m_pBuffer = NULL;
    }
}


//----------------------------------------------------------------------------------------------------------------------
// CXEDColorFrame Methods
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDColorFrame::GetDimensions
// Desc: Returns the dimensions of the color buffer in pixels.
//----------------------------------------------------------------------------------------------------------------------
SIZE CXEDColorFrame::GetDimensions()
{
    SIZE s; 
    s.cx = COLOR_BUFFER_WIDTH;
    s.cy = COLOR_BUFFER_HEIGHT;
    return s;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEColorFrame::GetBufferLength
// Desc: Returns the size of the Color frame buffer in bytes.
//----------------------------------------------------------------------------------------------------------------------
DWORD CXEDColorFrame::GetBufferLength()
{
    return sizeof(DWORD) * COLOR_BUFFER_WIDTH * COLOR_BUFFER_HEIGHT;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDColorFrame::AllocateBuffer
// Desc: Allocates enough memory to store a single frame of color buffer information.
//----------------------------------------------------------------------------------------------------------------------
HRESULT CXEDColorFrame::AllocateBuffer()
{
    assert( m_pBuffer == NULL && "Buffer already allocated" );
 
    m_pBuffer = (DWORD*)malloc( GetBufferLength() );

    if (m_pBuffer == NULL)
    {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDColorFrame::~CXEDColorFrame
// Desc: Destroy the frame, freeing any allocated buffer.
//----------------------------------------------------------------------------------------------------------------------
CXEDColorFrame::~CXEDColorFrame()
{
    if( m_pBuffer )
    {
        free( m_pBuffer );
        m_pBuffer = NULL;
    }
}