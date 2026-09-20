//----------------------------------------------------------------------------------------------------------------------
// TxInfo.cpp
//
// Provides support for displaying high-level information about the input file. 
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "CXEDFile.h"

const double USEC_IN_1_SECOND = 1.0E6;

float TimestampUsecToSeconds( UINT64 timestamp_us )
{
    return (float)(((double)timestamp_us) / USEC_IN_1_SECOND );
}

//----------------------------------------------------------------------------------------------------------------------
// Name: DisplayFileInfo
// Desc: Dumps statistical data about the input file to the console.
//----------------------------------------------------------------------------------------------------------------------
int DisplayFileInfo( LPCTSTR pszInputFile )
{
    assert( pszInputFile && "filename is NULL" );

    CXEDFile input;
    if ( FAILED( input.Open( pszInputFile ) ) )
    {
        _putts( _T("\nError while opening the input file.\n") );
        return -1;
    }

    _tprintf( _T("Input file: %s\n\n"), pszInputFile );
    _tprintf( _T("Total File Duration: %.3fs\n\n"), TimestampUsecToSeconds(input.GetFileDuration() ) );

    XEDMultiFrameRange info = input.GetMultiFrameInfo();
        
    _putts( _T("Event Information:") );

    _tprintf( _T("    %u Color Events\n"), input.GetEventCount( XED_COLOR ) );
    _tprintf( _T("    %u Depth Events\n"), input.GetEventCount( XED_DEPTH ) );
    _tprintf( _T("    %u Skeleton Events\n\n"), input.GetEventCount( XED_SKELETON ) );

    if ( info.HasColorFrames() )
    {
        _putts( _T("Color Frames:") );

        _tprintf( _T("    Timestamps span %.3fs\n"), TimestampUsecToSeconds( info.color.GetTimeRange() ) );
        _tprintf( _T("    Frame numbers span %u frames\n\n"), info.color.GetFrameRange() );

        _tprintf( _T("     Start Time: %I64uus\n"), info.color.start.timestampInUsec );
        _tprintf( _T("       End Time: %I64uus\n\n"), info.color.end.timestampInUsec );
        _tprintf( _T("    Start Frame: %u\n"), info.color.start.frameNumber );
        _tprintf( _T("      End Frame: %u\n\n"), info.color.end.frameNumber );
    }
    else
    {
        _putts( _T("No Color information in file.\n") );
    }

    if ( info.HasDepthFrames() )
    {
        _putts( _T("Depth Frames:") );

        _tprintf( _T("    Timestamps span %.3fs\n"), TimestampUsecToSeconds( info.depth.GetTimeRange() ) );
        _tprintf( _T("    Frame numbers span %u frames\n\n"), info.depth.GetFrameRange() );

        _tprintf( _T("     Start Time: %I64uus\n"), info.depth.start.timestampInUsec );
        _tprintf( _T("       End Time: %I64uus\n\n"), info.depth.end.timestampInUsec );
        _tprintf( _T("    Start Frame: %u\n"), info.depth.start.frameNumber );
        _tprintf( _T("      End Frame: %u\n\n"), info.depth.end.frameNumber );
    }
    else
    {
        _putts( _T("No Depth information in file.\n") );
    }

    if ( info.HasSkeletonFrames() )
    {
        _putts( _T("Skeleton Frames:") );

        _tprintf( _T("    Timestamps span %.3fs\n"), TimestampUsecToSeconds( info.skeleton.GetTimeRange() ) );
        _tprintf( _T("    Frame numbers span %u frames\n\n"), info.skeleton.GetFrameRange() );

        _tprintf( _T("     Start Time: %I64uus\n"), info.skeleton.start.timestampInUsec );
        _tprintf( _T("       End Time: %I64uus\n\n"), info.skeleton.end.timestampInUsec );
        _tprintf( _T("    Start Frame: %u\n"), info.skeleton.start.frameNumber );
        _tprintf( _T("      End Frame: %u\n\n"), info.skeleton.end.frameNumber );
	}
    else
    {
        _putts( _T("No Skeleton information in file.\n") );
    }

    return 0;
}