//----------------------------------------------------------------------------------------------------------------------
// XEDFile.inl
//
// Inline implementation file for XEDFile methods. 
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// XEDFrameRange Methods
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFrameRange::XEDFrameRange
// Desc: Constructor for the frame range. Marks this range as being invalid.
//----------------------------------------------------------------------------------------------------------------------
inline XEDFrameRange::XEDFrameRange()
{
    // This space intentionally left blank.
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFrameRange::IsValid
// Desc: Returns true if the frame range is valid, FALSE otherwise.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL XEDFrameRange::IsValid() const
{
    // We use the MAX_TIME_USEC value as a sentinel for "no data"
    return ( start.timestampInUsec != XED_TIMESTAMP_INVALID );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFrameRange::GetFrameRange
// Desc: Gets the number of frames between the start and end of this range.
//----------------------------------------------------------------------------------------------------------------------
inline UINT XEDFrameRange::GetFrameRange() const
{
    assert( IsValid() && "Range is not valid" );
    return ( end.frameNumber - start.frameNumber ) + 1;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFrameRange::GetTimeRange
// Desc: Gets the time in microseconds between the start and end of this range.
//----------------------------------------------------------------------------------------------------------------------
inline UINT64 XEDFrameRange::GetTimeRange() const
{
    assert( IsValid() && "Range is not valid" );
    return ( end.timestampInUsec - start.timestampInUsec );
}

//----------------------------------------------------------------------------------------------------------------------
// XEDFrameInfo Methods
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFrameInfo::IsValid
// Desc: Returns TRUE if this object refers to a valid frame.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL XEDFrameInfo::IsValid() const
{
    return (eventIndex != XED_EVENTINDEX_INVALID );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFrameInfo::operator==
// Desc: Compares the timestamps and framenumbers of two frames.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL XEDFrameInfo::operator==( const XEDFrameInfo& rhs )
{
    return ( rhs.frameNumber == frameNumber ) && ( rhs.timestampInUsec == rhs.timestampInUsec );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFrameInfo::CompareFrameNumbers
// Desc: Compares the framenumbers of two frames. Returns -1 if this frame is less than
//       rhs, 1 if this frame is greater than rhs, and 0 if they are equal.
//----------------------------------------------------------------------------------------------------------------------
inline INT XEDFrameInfo::CompareFrameNumbers( const XEDFrameInfo& rhs ) const
{
    if ( frameNumber > rhs.frameNumber)
    {
        return 1;
    }
    else if ( frameNumber < rhs.frameNumber )
    {
        return -1;
    }
    else
    {
        return 0;
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFrameInfo::CompareTimestamps
// Desc: Compares the timestamps of two frames. Returns -1 if this frame is less than
//       rhs, 1 if this frame is greater than rhs, and 0 if they are equal.
//----------------------------------------------------------------------------------------------------------------------
inline INT XEDFrameInfo::CompareTimestamps( const XEDFrameInfo& rhs ) const
{
    if ( timestampInUsec > rhs.timestampInUsec)
    {
        return 1;
    }
    else if ( timestampInUsec < rhs.timestampInUsec )
    {
        return -1;
    }
    else
    {
        return 0;
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFrameInfo::XEDFrameInfo
// Desc: Empty constructor for XEDFrameInfo
//----------------------------------------------------------------------------------------------------------------------
inline XEDFrameInfo::XEDFrameInfo()
: type( XED_COLOR ),
  eventIndex( XED_EVENTINDEX_INVALID ),
  frameNumber( XED_FRAMENUMBER_INVALID ),
  timestampInUsec( XED_TIMESTAMP_INVALID )
{
    // This space intentionally left blank.
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFrameInfo::XEDFrameInfo
// Desc: Parameterized constructor for the frame.
//----------------------------------------------------------------------------------------------------------------------
inline XEDFrameInfo::XEDFrameInfo( XEDFRAMETYPE frametype, UINT index, UINT frame, UINT64 time )
: type( frametype ),
  eventIndex( index ),
  frameNumber( frame ),
  timestampInUsec( time )
{
    // This space intentionally left blank.
}


//----------------------------------------------------------------------------------------------------------------------
// XEDFrameInfo Methods
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: XEDMultiFrameRange::HasColorFrames
// Desc: Returns true if there are color frames in the source file.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL XEDMultiFrameRange::HasColorFrames() const
{
    return color.IsValid();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDMultiFrameRange::HasDepthFrames
// Desc: Returns true if there are depth frames in the source file.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL XEDMultiFrameRange::HasDepthFrames() const
{
    return depth.IsValid();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDMultiFrameRange::HasSkeletonFrames
// Desc: Returns true if there are skeleton frames in the file.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL XEDMultiFrameRange::HasSkeletonFrames() const
{
    return skeleton.IsValid();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDMultiFrameRange::GetFirstFrameTime
// Desc: Gets the timestamp of the first frame in the file.
//----------------------------------------------------------------------------------------------------------------------
inline UINT64 XEDMultiFrameRange::GetFirstFrameTime() const
{
    //NOTE: XED_TIMESTAMP_INVALID is all bits set, and is the greatest possible timestamp
    //      value. 

    UINT64 firstTime = XED_TIMESTAMP_INVALID;   
    
    if ( color.IsValid() )
    {
        firstTime = std::min( color.start.timestampInUsec, firstTime );
    }
    
    if ( depth.IsValid() )
    {
        firstTime = std::min( depth.start.timestampInUsec, firstTime );
    }

    if ( skeleton.IsValid() )
    {
        firstTime = std::min( skeleton.start.timestampInUsec, firstTime );
    }

    return firstTime;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDMultiFrameRange::GetLastFrameTime
// Desc: Gets the timestamp of the last frame in the file.
//----------------------------------------------------------------------------------------------------------------------
inline UINT64 XEDMultiFrameRange::GetLastFrameTime() const
{
    UINT64 lastTime = 0UL;
    
    if ( color.IsValid() )
    {
        lastTime = std::max( color.start.timestampInUsec, lastTime );
    }

    if ( depth.IsValid() )
    {
        lastTime = std::max( depth.start.timestampInUsec, lastTime );
    }

    if ( skeleton.IsValid() )
    {
        lastTime = std::max( skeleton.start.timestampInUsec, lastTime );
    }

    return lastTime;
}


//----------------------------------------------------------------------------------------------------------------------
// XEDFile Methods
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::CXEDFile
//----------------------------------------------------------------------------------------------------------------------
inline CXEDFile::CXEDFile()
  : m_pFileContext( NULL )
{
    // Intentionally left blank
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::operator XED_CONTEXT*
// Desc: Conversion operator to obtain the underlying XED_CONTEXT for direct use
//       with the C API.
//----------------------------------------------------------------------------------------------------------------------
inline CXEDFile::operator XED_CONTEXT* ()
{
    return m_pFileContext;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::Create
// Desc: Creates a new XED file with the specified path, using another CXEDFile as a 
//       template if desired.
//----------------------------------------------------------------------------------------------------------------------
inline HRESULT CXEDFile::Create( LPCSTR pszFilePath, BOOL fOverwrite /*= TRUE*/, CXEDFile* pTemplate /*= NULL */ )
{
    assert( !IsOpen() && "File already open" );
    assert( pszFilePath && "Filename is NULL" );

    XED_CONTEXT* pTemplateContext = pTemplate ? (XED_CONTEXT*)(*pTemplate) : NULL;
    
    XED_CONTEXT* pFileContext = NULL;
    
    HRESULT hr;

    for( ; ; ) {
        hr = XedCreateFile( pszFilePath, pTemplateContext, 0, &pFileContext );
        if ( SUCCEEDED( hr ) )
        {
            m_pFileContext = pFileContext;
            break;
        }
        else if ( fOverwrite && hr == HRESULT_FROM_WIN32( ERROR_FILE_EXISTS ) )
        {
            if ( !DeleteFileA( pszFilePath ) )
            {
                return HRESULT_FROM_WIN32( GetLastError() );
            }
            continue;
        }

        break;
    }
    
    return hr;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::Open
// Desc: Opens an existing XED file.
//----------------------------------------------------------------------------------------------------------------------
inline HRESULT CXEDFile::Open( LPCSTR pszFilePath )
{
    assert( !IsOpen() && "File already open" );
    assert( pszFilePath && "Filename is NULL" );

    XED_CONTEXT* pFileContext = NULL;

    HRESULT hr = XedOpenFile( pszFilePath, 0, &pFileContext );
    
    if ( SUCCEEDED(hr) )
    {
        m_pFileContext = pFileContext;
    }

    return hr;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::Close
// Desc: Closes the file.
//----------------------------------------------------------------------------------------------------------------------
inline HRESULT CXEDFile::Close()
{
    HRESULT hr = S_OK;

    // NOTE: We don't assert here; it's ok to close a file twice.
    if ( m_pFileContext )
    {
        hr = XedCloseFile( &m_pFileContext );
        assert( m_pFileContext == NULL && "File context was not nulled out by API.");
        m_pFileContext = NULL;
    }

    return hr;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::IsOpen
// Desc: Returns TRUE if the CXEDFile object holds a reference to an open file.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL CXEDFile::IsOpen() const
{
    return m_pFileContext ? TRUE : FALSE;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::HasAnyData
// Desc: Returns TRUE if any data is in the file.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL CXEDFile::HasAnyData()
{
    assert( IsOpen() && "File is not open" );

    return ( HasColorData() || HasDepthData() || HasSkeletonData() );
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::HasSkeletonData
// Desc: Returns TRUE if the file contains any Skeleton Tracking data.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL CXEDFile::HasSkeletonData()
{
    assert( IsOpen() && "File is not open" );

    return GetEventCount( XED_SKELETON ) != 0;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::HasColorData
// Desc: Returns TRUE if the file contains any Color buffer frames.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL CXEDFile::HasColorData()
{
    assert( IsOpen() && "File is not open" );

    return GetEventCount( XED_COLOR ) != 0;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::HasDepthData
// Desc: Returns TRUE if the file contains any Depth buffer frames.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL CXEDFile::HasDepthData()
{
    assert( IsOpen() && "File is not open" );

    return GetEventCount( XED_DEPTH ) != 0;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile::GetEventCount
// Desc: Gets the number of events present in the file for all event types.
//----------------------------------------------------------------------------------------------------------------------
inline UINT CXEDFile::GetTotalEventCount()
{
    assert( IsOpen() && "File is not open" );
    return XedGetNuiColorEventCount( m_pFileContext ) +  XedGetNuiDepthEventCount( m_pFileContext )
           + XedGetNuiSkeletonEventCount( m_pFileContext );
}


//----------------------------------------------------------------------------------------------------------------------
// CXEDFrame Methods
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFrame::CXEDFrame
// Desc: Constructor.
//----------------------------------------------------------------------------------------------------------------------
inline CXEDFrame::CXEDFrame()
{
    // intentionally left blank.
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFrame::~CXEDFrame
// Desc: Destructor.
//----------------------------------------------------------------------------------------------------------------------
inline CXEDFrame::~CXEDFrame()
{
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFrame::GetFrameNumber
// Desc: Gets the frame number associated with this frame of data.
//----------------------------------------------------------------------------------------------------------------------
inline UINT CXEDFrame::GetFrameNumber() const
{
    return m_frameInfo.frameNumber;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFrame::SetFrameNumber
// Desc: Sets the frame number associated with this frame of data.
//----------------------------------------------------------------------------------------------------------------------
inline VOID CXEDFrame::SetFrameNumber( UINT frameNumber )
{
    m_frameInfo.frameNumber = frameNumber;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFrame::GetTimestamp
// Desc: Gets the timestamp in microseconds associated with this frame of data.
//----------------------------------------------------------------------------------------------------------------------
inline UINT64 CXEDFrame::GetTimestamp() const
{
    return m_frameInfo.timestampInUsec;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFrame::SetTimestamp
// Desc: Sets the timestamp in microseconds associated with this frame of data.
//----------------------------------------------------------------------------------------------------------------------
inline VOID CXEDFrame::SetTimestamp( UINT64 microseconds )
{
    m_frameInfo.timestampInUsec = microseconds;
}


//----------------------------------------------------------------------------------------------------------------------
// CXEDDepthFrame Methods
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDDepthFrame::CXEDDepthFrame
// Desc: Constructor.
//----------------------------------------------------------------------------------------------------------------------
inline CXEDDepthFrame::CXEDDepthFrame()
: m_pBuffer( NULL )
{
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDDepthFrame::operator=
// Desc: Assigns the contents of the rhs frame buffer to this one, taking ownership.
//----------------------------------------------------------------------------------------------------------------------
inline CXEDDepthFrame& CXEDDepthFrame::operator=( CXEDDepthFrame& rhs )
{
    m_frameInfo = rhs.m_frameInfo;
    m_pBuffer = rhs.m_pBuffer;

    // Remove its buffer.
    rhs.m_pBuffer = NULL;

    // Give it an empty frame.
    XEDFrameInfo fi;
    rhs.m_frameInfo = fi;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDDepthFrame::GetData
// Desc: Obtains a pointer to the depth buffer data.
//----------------------------------------------------------------------------------------------------------------------
inline WORD* CXEDDepthFrame::GetData()
{
    return m_pBuffer;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDDepthFrame::GetType
// Desc: Gets the type of this frame of data. (Skeleton, Color or Depth).
//----------------------------------------------------------------------------------------------------------------------
inline XEDFRAMETYPE CXEDDepthFrame::GetType()
{
    return XED_DEPTH;
}


//----------------------------------------------------------------------------------------------------------------------
// CXEDColorFrame Methods
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDColorFrame::CXEDColorFrame
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
inline CXEDColorFrame::CXEDColorFrame()
: m_pBuffer( NULL )
{
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDColorFrame::GetData
// Desc: Obtains a pointer to the depth buffer data.
//----------------------------------------------------------------------------------------------------------------------
inline DWORD* CXEDColorFrame::GetData()
{
    return m_pBuffer;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDColorFrame::GetType
// Desc: Gets the type of this frame of data. (Skeleton, Color or Depth).
//----------------------------------------------------------------------------------------------------------------------
inline XEDFRAMETYPE CXEDColorFrame::GetType()
{
    return XED_COLOR;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDColorFrame::operator=
// Desc: Assigns the contents of the rhs frame buffer to this one, taking ownership.
//----------------------------------------------------------------------------------------------------------------------
inline CXEDColorFrame& CXEDColorFrame::operator=( CXEDColorFrame& rhs )
{
    m_frameInfo = rhs.m_frameInfo;
    m_pBuffer = rhs.m_pBuffer;

    // Remove its buffer.
    rhs.m_pBuffer = NULL;

    // Give it an empty frame.
    XEDFrameInfo fi;
    rhs.m_frameInfo = fi;
}


//----------------------------------------------------------------------------------------------------------------------
// CXEDSkeletonFrame Methods
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDSkeletonFrame::CXEDSkeletonFrame
//----------------------------------------------------------------------------------------------------------------------
inline CXEDSkeletonFrame::CXEDSkeletonFrame()
{
    // intentionally left blank.
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDSkeletonFrame::GetData
// Desc: Obtains a pointer to the depth buffer data.
//----------------------------------------------------------------------------------------------------------------------
inline NUI_SKELETON_FRAME* CXEDSkeletonFrame::GetData()
{
    return &m_skeletonFrame;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDSkeletonFrame::GetType
// Desc: Gets the type of this frame of data. (Skeleton, Color or Depth).
//----------------------------------------------------------------------------------------------------------------------
inline XEDFRAMETYPE CXEDSkeletonFrame::GetType()
{
    return XED_SKELETON;
}


//----------------------------------------------------------------------------------------------------------------------
// CXEDTitleDataFrame Methods
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDTitleDataFrame::CXEDTitleDataFrame
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
inline CXEDTitleDataFrame::CXEDTitleDataFrame() :   m_pBuffer( NULL ),
                                                    m_uBufferSize( 0 )
{
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDColorFrame::GetData
// Desc: Obtains a pointer to the depth buffer data.
//----------------------------------------------------------------------------------------------------------------------
inline DWORD* CXEDTitleDataFrame::GetData()
{
    return m_pBuffer;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDColorFrame::GetType
// Desc: Gets the type of this frame of data. (Skeleton, Color or Depth).
//----------------------------------------------------------------------------------------------------------------------
inline XEDFRAMETYPE CXEDTitleDataFrame::GetType()
{
    return XED_TITLE_DATA;
}

