//----------------------------------------------------------------------------------------------------------------------
// XEDFile.h
// 
// Provides a C++ wrapper around the XEDFile APIs.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#ifndef XEDFILE_H_GUARD
#define XEDFILE_H_GUARD

#include <XnaMath.h>
#include <NuiTools.h>
#include <XStudio.h>
#include <xedfile.h>

//----------------------------------------------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------------------------------------------
class CXEDFile;
class CXEDFrame;
class CXEDDepthFrame;
class CXEDColorFrame;
class CXEDSkeletonFrame;

//----------------------------------------------------------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------------------------------------------------------
// Time for one single Kinect Sensor Array cycle (1/30th sec).
const UINT64 SINGLE_NUI_FRAME_TIME_USEC = 33333UL;
const UINT   DEPTH_BUFFER_WIDTH         = 320;
const UINT   DEPTH_BUFFER_HEIGHT        = 240;
const UINT   COLOR_BUFFER_WIDTH         = 640;
const UINT   COLOR_BUFFER_HEIGHT        = 480;

//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFRAMETYPE
// Desc: Enumerates the different kinds of frames which can be found in an XED file.
//----------------------------------------------------------------------------------------------------------------------
enum XEDFRAMETYPE
{
    XED_COLOR = 0,
    XED_DEPTH,
    XED_SKELETON,

    XEDFRAMETYPE_COUNT
};

//----------------------------------------------------------------------------------------------------------------------
// Name: XEDFrameInfo
// Desc: Struct containing the timestamp and frame number information associated with a
//       given event.
//----------------------------------------------------------------------------------------------------------------------
struct XEDFrameInfo
{
    UINT eventIndex;
    XEDFRAMETYPE type;
    UINT frameNumber;
    UINT64 timestampInUsec;

    XEDFrameInfo();
    XEDFrameInfo( XEDFRAMETYPE type, UINT eventIndex, UINT frame, UINT64 time );

    BOOL operator==( const XEDFrameInfo& rhs );
    BOOL IsValid() const;
    INT CompareFrameNumbers( const XEDFrameInfo& rhs ) const;
    INT CompareTimestamps( const XEDFrameInfo& rhs ) const;
};


//----------------------------------------------------------------------------------------------------------------------
// Name; XEDFrameRange
// Desc: Contains the start and end frames and times for this type of frame.
//----------------------------------------------------------------------------------------------------------------------
struct XEDFrameRange
{
    UINT eventCount;
    XEDFrameInfo start;
    XEDFrameInfo end;

    XEDFrameRange();

    BOOL IsValid() const;
    UINT GetFrameRange() const;
    UINT64 GetTimeRange() const;
};


//----------------------------------------------------------------------------------------------------------------------
// Name: XEDMultiFrameRange
// Desc: Contains the range of frames of different types contained in the file.
//----------------------------------------------------------------------------------------------------------------------
struct XEDMultiFrameRange
{
    XEDFrameRange color;
    XEDFrameRange depth;
    XEDFrameRange skeleton;
    
    BOOL HasColorFrames() const;
    BOOL HasDepthFrames() const;
    BOOL HasSkeletonFrames() const;

    UINT GetTotalFrameRange() const;
    UINT64 GetTotalTimeRange() const;

    UINT64 GetFirstFrameTime() const;
    UINT64 GetLastFrameTime() const;
};


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFile
// Desc: C++ Class which wraps an XED_CONTEXT and the XedFile API for use by C++ 
//       applications.
//----------------------------------------------------------------------------------------------------------------------
class CXEDFile
{
public:
    CXEDFile();
    ~CXEDFile();

    operator XED_CONTEXT* ();

    BOOL IsOpen() const;
    BOOL HasAnyData();
    BOOL HasSkeletonData();
    BOOL HasColorData();
    BOOL HasDepthData();

    HRESULT Create( LPCWSTR pszFilePath, BOOL fOverwrite = TRUE, CXEDFile* pTemplate = NULL );
    HRESULT Create( LPCSTR pszFilePath, BOOL fOverwrite = TRUE, CXEDFile* pTemplate = NULL );

    HRESULT Open( LPCWSTR pszFilePath );
    HRESULT Open( LPCSTR pszFilePath );

    HRESULT Close();

    XEDMultiFrameRange GetMultiFrameInfo();
    UINT64 GetFileDuration();
    UINT GetEventCount( XEDFRAMETYPE frameType );
    UINT GetTotalEventCount();
    UINT GetEventIndexFromFrameNumber( XEDFRAMETYPE frameType, UINT frameNumber );
    UINT GetEventIndexFromTime( XEDFRAMETYPE frameType, UINT64 microseconds );

    XEDFrameInfo ReadFrameInfo( XEDFRAMETYPE frameType, UINT eventIndex );
    HRESULT ReadFrame( UINT eventIndex, CXEDDepthFrame* pOutFrame, BOOL bRegisterWithColor = FALSE );
    HRESULT ReadFrame( UINT eventIndex, CXEDColorFrame* pOutFrame, BOOL bRegisterWithDepth = FALSE );
    HRESULT ReadFrame( UINT eventIndex, CXEDSkeletonFrame* pOutFrame );

    HRESULT WriteFrame( CXEDFrame* pFrame );

    static HRESULT CopyFrame( CXEDFile& readFile, CXEDFile& writeFile, XEDFrameInfo& copyInfo );

private:

    XED_CONTEXT* m_pFileContext;
};


//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDFrame
// Desc: Base class for Xed Data buffers.
//----------------------------------------------------------------------------------------------------------------------
class CXEDFrame
{
public:

    ~CXEDFrame();

    UINT GetFrameNumber() const;
    UINT64 GetTimestamp() const;
    VOID SetFrameNumber( UINT frameNumber );
    VOID SetTimestamp( UINT64 microseconds );

    virtual XEDFRAMETYPE GetType() = 0;

private:

    CXEDFrame( const CXEDFrame& ); // disabled

protected:

    CXEDFrame(); // disabled

    XEDFrameInfo m_frameInfo;

    friend CXEDFile;
};

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDDepthFrame
// Desc: Class which wraps depth buffer frames returned by the XedFile API.
//----------------------------------------------------------------------------------------------------------------------
class CXEDDepthFrame : public CXEDFrame
{
public:

    CXEDDepthFrame();
    CXEDDepthFrame& operator=( CXEDDepthFrame& rhs );
    ~CXEDDepthFrame();

    HRESULT AllocateBuffer();

    WORD* GetData();

    virtual XEDFRAMETYPE GetType();

    static SIZE GetDimensions();
    static DWORD GetBufferLength();

private:

    CXEDDepthFrame( const CXEDDepthFrame& ); // disabled

    WORD* m_pBuffer;

    friend CXEDFile;
};

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDColorFrame
// Desc: Class which wraps color buffer frames returned by the XedFile API.
//----------------------------------------------------------------------------------------------------------------------
class CXEDColorFrame : public CXEDFrame
{
public:

    CXEDColorFrame();
    CXEDColorFrame& operator=( CXEDColorFrame& rhs );
    ~CXEDColorFrame();

    HRESULT AllocateBuffer();

    DWORD* GetData();

    virtual XEDFRAMETYPE GetType();

    static SIZE GetDimensions();
    static DWORD GetBufferLength();

private:

    CXEDColorFrame( const CXEDColorFrame& ); // disabled

    DWORD* m_pBuffer;

    friend CXEDFile;
};

//----------------------------------------------------------------------------------------------------------------------
// Name: CXEDSkeletonFrame
// Desc: Class which wraps skeleton data frames returned by the XedFile API.
//----------------------------------------------------------------------------------------------------------------------
class CXEDSkeletonFrame : public CXEDFrame
{
public:

    CXEDSkeletonFrame();

    NUI_SKELETON_FRAME* GetData();

    virtual XEDFRAMETYPE GetType();

private:

    CXEDSkeletonFrame( const CXEDSkeletonFrame& ); // disabled

    NUI_SKELETON_FRAME m_skeletonFrame;

    friend CXEDFile;
};

#include "CXEDFile.inl"

#endif //XEDFILE_H_GUARD
