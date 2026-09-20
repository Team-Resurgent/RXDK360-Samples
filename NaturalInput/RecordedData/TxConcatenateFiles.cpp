//----------------------------------------------------------------------------------------------------------------------
// TxConcatenateFiles.cpp
//
// Provides support for concatenating two XED files together.
//
// Note: It is recommended that the two files are recorded with the camera in the same position, as skeleton data (such
// as floor-plane and up-vector information) is not automatically reconciled between the two files.
//
// Timestamps and framenumbers are rebased during the concatenate operation; the new timestamp starts at 0, as do the
// frame numbers for the color stream (although the depth + skeleton streams are shifted as a pair).
//
// Also note that a time of 1/30th of a second is inserted between the last frame to arrive in the first file, and the
// first frame in the second file.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "CXEDFile.h"
#include "XEDFileIterators.h"
#include "Utility.h"

ProgressBar<UINT> progressBar;

//----------------------------------------------------------------------------------------------------------------------
// Name: ConcatOffsets
// Desc: Class used to manage frame offsets and written frame counts while concatenating files.
//----------------------------------------------------------------------------------------------------------------------
struct ConcatOffsets
{
    UINT64 baseTimeOffset;              // The base time offset for all frames in this file.
    UINT baseFrameOffset[XEDFRAMETYPE_COUNT];    // The base frame number offset applied to each type of frame in this file.
    UINT64 lastTimeOut;                 // The last timestamp written per stream type.
    UINT lastFrameOut[XEDFRAMETYPE_COUNT];       // The last frame number written per stream type.

    VOID CalculateFileAShift( CXEDFile& fileA );
    VOID CalculateFileBShift( CXEDFile& fileB );
    VOID UpdateLastFrame( const XEDFrameInfo& info );
};

//----------------------------------------------------------------------------------------------------------------------
// Name: ConcatOffsets::UpdateLastFrame
// Desc: Keeps track of the last frame that was written (copied) to the output file.
//----------------------------------------------------------------------------------------------------------------------
VOID ConcatOffsets::UpdateLastFrame( const XEDFrameInfo& info )
{
    lastFrameOut[ info.type ] = info.frameNumber;
    lastTimeOut = info.timestampInUsec;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: ConcatOffsets::CalculateFileAShift
// Desc: Calculates the timestamp and frame number offsets which should be applied to the 1st file's timestamps and
//       frame numbers, so that they are rebased at 0.
//----------------------------------------------------------------------------------------------------------------------
VOID ConcatOffsets::CalculateFileAShift( CXEDFile& fileA )
{
    // Reset the timestamps.
    lastTimeOut = XED_TIMESTAMP_INVALID;

    for (int i = 0; i < XEDFRAMETYPE_COUNT; ++i)
    {
        lastFrameOut[i] = XED_FRAMENUMBER_INVALID;
    }

    XEDMultiFrameRange range = fileA.GetMultiFrameInfo();
    
    BOOL bHasDepth = range.HasDepthFrames();
    BOOL bHasSkeleton = range.HasSkeletonFrames();

    baseTimeOffset = 1 - range.GetFirstFrameTime();
    baseFrameOffset[XED_COLOR] = 0 - range.color.start.frameNumber;
    
    // Figure out the depth/skeleton frame # shifts. Depth/Skeleton frames have to remain in order relative to one
    // another for the file to be valid.

    // Find least of first skeleton frame / first depth frame.

    UINT minFrame = XED_FRAMENUMBER_INVALID;
    if ( bHasDepth )
    {
        minFrame = range.depth.start.frameNumber;
    }

    if ( bHasSkeleton )
    {
        minFrame = min( minFrame, range.skeleton.start.frameNumber );
    }

    baseFrameOffset[XED_DEPTH] = 0 - minFrame;
    baseFrameOffset[XED_SKELETON] = 0 - minFrame;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: ConcatOffsets::CalculateFileBShift
// Desc: Calculates the delta to be applied to the 2nd file's timestamps and frame numbers, so that they are contiguous
//       with the first file's.
//----------------------------------------------------------------------------------------------------------------------
VOID ConcatOffsets::CalculateFileBShift( CXEDFile& fileB )
{
    // Frames in the 2nd file should maintain their relationship in time, so we need to offset them by the timestamp
    // of the last frame of any type in the first file, plus a delay of SINGLE_NUI_FRAME_TIME_USEC.
    //
    // Frame numbers, however, align differently. Color frames should be sequential with those in the first file.
    // Depth and Skeleton frames need to be sequential with the last depth or skeleton frame in the first file, plus 1.
    // This is because Depth & Skeleton frame numbers should (in normal operation) correspond with one another.

    XEDMultiFrameRange rangeB = fileB.GetMultiFrameInfo();
    BOOL bAHasDepth = lastFrameOut[ XED_DEPTH ] != XED_FRAMENUMBER_INVALID;
    BOOL bAHasSkeleton = lastFrameOut[ XED_SKELETON ] != XED_FRAMENUMBER_INVALID;

    // If we've never written out a frame (file A was empty), CalculateFileBShift degenerates to CalculateFileAShift.
    if ( lastTimeOut == XED_TIMESTAMP_INVALID )
    {
        CalculateFileAShift( fileB );
        return;
    }

    // Line up the timestamps so that the 2nd file is placed after the 1st file in time.
    baseTimeOffset = lastTimeOut + SINGLE_NUI_FRAME_TIME_USEC - rangeB.GetFirstFrameTime();

    // Same deal with color frames; they run consecutively after their previous frame.
    baseFrameOffset[ XED_COLOR ] = lastFrameOut[ XED_COLOR ] + 1 - rangeB.color.start.frameNumber;

    // Depth & skeleton frames, again work differently. We do the same shift-right trick we did for fileA, and if
    // necessary we leave a gap in frame numbers so that the two streams in the second file remain in comparative order.

    // First, find the highest of the frame numbers in the first file we wrote out, and move to the next number. If we
    // don't have any depth/skeleton data in the first file, start at 0.

    UINT lastAFrameNumber = 0; 
    
    if ( bAHasDepth && bAHasSkeleton )
    {
        lastAFrameNumber = max( lastFrameOut[ XED_DEPTH ], lastFrameOut[ XED_SKELETON ] );
    }
    else if ( bAHasDepth )
    {
        lastAFrameNumber = lastFrameOut[ XED_DEPTH ];
    }
    else if ( bAHasSkeleton )
    {
        lastAFrameNumber = lastFrameOut[ XED_SKELETON ];
    }

    // Now, find the lowest of the frame numbers in the second file, so we can remove that offset.
    // Note: because XED_FRAMENUMNBER_INVALID is the largest number that can be stored in a UINT,
    //       we can just use a min() here.

    UINT firstBFrameNumber = min( rangeB.depth.start.frameNumber, rangeB.skeleton.start.frameNumber );

    UINT depthSkelFrameOffset = lastAFrameNumber - firstBFrameNumber + 1;
    baseFrameOffset[ XED_DEPTH ] = depthSkelFrameOffset;
    baseFrameOffset[ XED_SKELETON ] = depthSkelFrameOffset;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CopyAndOffset
// Desc: Copies a frame, and offsets its timestamps and framenumbers.
//----------------------------------------------------------------------------------------------------------------------
BOOL CopyAndOffset( CXEDFile &sourceFile, CXEDFile& outputFile, ConcatOffsets& cfi )
{
    if ( sourceFile.HasAnyData() )
    {
        XEDFrameInfo frameInfo;

        CXEDMuxFwdFrameIterator it( sourceFile );
        while ( !it.IsEOF() )
        {
            frameInfo = it.GetCurrentInfo();
            frameInfo.frameNumber += cfi.baseFrameOffset[frameInfo.type];
            frameInfo.timestampInUsec += cfi.baseTimeOffset;

            cfi.UpdateLastFrame( frameInfo );

            // Copy the frame to the output file.

            HRESULT hr = CXEDFile::CopyFrame( sourceFile, outputFile, frameInfo );
            if ( FAILED( hr ) )
            {
                return FALSE;
            }

            progressBar.Tick();

            it.MoveNext();
        }
    }

    return TRUE;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: ConcatenateFiles
// Desc: Takes two XED files, and combines them into a third file. The output file should not have the same filename as
//       either input file.
//----------------------------------------------------------------------------------------------------------------------
int ConcatenateFiles( LPCTSTR pszFileA, LPCTSTR pszFileB, LPCTSTR pszOutputFile )
{
    CXEDFile fileA;
    CXEDFile fileB;
    CXEDFile outputFile;

    assert( pszFileA && pszFileB && pszOutputFile && "NULL filename" );

    // Open the files

    if ( !EnsureUniqueFiles( pszFileA, pszOutputFile ) || 
         !EnsureUniqueFiles( pszFileB, pszOutputFile ) )
    {
        _putts( _T("\nError: the output file cannot be the same as either input file.\n") );
        return -1;
    }

    if ( FAILED( fileA.Open( pszFileA ) ) )
    {
        _tprintf( _T("\nError opening input file \'%s\'\n"), pszFileA );
        return -1;
    }

    if ( FAILED( fileB.Open( pszFileB ) ) )
    {
        _tprintf( _T("\nError opening input file \'%s\'\n"), pszFileB );
        return -1;
    }

    if ( FAILED( outputFile.Create( pszOutputFile, TRUE, &fileA ) ) )
    {
        _tprintf( _T("\nError opening output file \'%s\'\n"), pszOutputFile );
    }

    _tprintf(_T("Concatenating file \'%s\' and \'%s\' to \'%s\':"), pszFileA,
             pszFileB, pszOutputFile ); 

    // Create a progress bar for the console.

    progressBar.SetMode( ProgressBar<UINT>::BarMode_Normal );
    progressBar.SetLimits( 0, fileA.GetTotalEventCount() + fileB.GetTotalEventCount(), 0 );
    progressBar.Start();

    ConcatOffsets frameOffsets;

    frameOffsets.CalculateFileAShift( fileA );

    BOOL bSuccess = CopyAndOffset(fileA, outputFile, frameOffsets );

    if ( !bSuccess )
    {
        _tprintf( _T("\nError occurred while writing to the output file \'%s\'\n"), pszOutputFile );

        // Close the output file.
        if ( FAILED( outputFile.Close() ) )
        {
            _tprintf( _T("Error occurred while closing the output file \'%s\'\n"), pszOutputFile );
        }
 
		//TODO: CLOSE THE INPUT FILES!!!
        return -1;
    }

    // If the second file's empty, we're done early.

    if ( !fileB.HasAnyData() )
    {
        progressBar.End();

        _tprintf( _T("\nWarning: input file \'%s\' is empty.\n"), pszFileB );

        // Close the output file.

        if ( FAILED( outputFile.Close() ) )
        {
            _tprintf( _T("Error occurred while closing the output file \'%s\'\n"), pszOutputFile );
            return -1;
        }

        return 0;
    }

    frameOffsets.CalculateFileBShift( fileB );

    bSuccess = CopyAndOffset( fileB, outputFile, frameOffsets );
    progressBar.End();

    if ( bSuccess )
    {
        _tprintf(_T("\nCompleted.\n"));
    }
    else
    {
        _tprintf( _T("\nError occurred while writing to the output file \'%s\'\n"), pszOutputFile );
    }
    
    if ( FAILED( outputFile.Close() ) )
    {
        _tprintf( _T("Error occurred while closing the output file \'%s\'\n"), pszOutputFile );
        return -1;
    }

    return 0;
}
