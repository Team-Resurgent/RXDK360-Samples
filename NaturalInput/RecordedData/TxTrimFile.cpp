//----------------------------------------------------------------------------------------------------------------------
// TxTrimFile.cpp
//
// Provides support for trimming 
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "CXEDFile.h"
#include "XEDFileIterators.h"
#include "Utility.h"
#include <stdlib.h>
#include <errno.h>

const UINT64 MILLISEC_TO_MICROSEC = 1000UL;

int TrimFileInternal( LPCTSTR pszInputFile, LPCTSTR pszOutputFile, UINT64 trimStart_uSec, UINT64 trimEnd_uSec );

//----------------------------------------------------------------------------------------------------------------------
// Name: TrimFile
// Desc: Prepares the input parameters for the TrimFileInternal command.
//----------------------------------------------------------------------------------------------------------------------
int TrimFile( LPCTSTR pszInputFile, LPCTSTR pszOutputFile,
              LPCTSTR pszTrimStart, LPCTSTR pszTrimEnd )
{
    assert( pszInputFile && pszOutputFile && pszTrimStart && pszTrimEnd &&  "Invalid input parameter" );

    // Check that the input and output filenames are unique.

    if ( !EnsureUniqueFiles( pszInputFile, pszOutputFile ) )
    {
        _putts( _T("\nError: input and output files are the same.\n") );
        return -1;
    }

    
    LPTSTR pszTemp;
    UINT64 trimStart_uSec = MILLISEC_TO_MICROSEC * (UINT64)_tcstoul( pszTrimStart, &pszTemp, 10 );

    if ( *pszTemp != _T('\0') )
    {
        _putts( _T("\nError: Trim Start parameter is invalid; should be an integer in milliseconds.\n") );
        return -1;
    }

    UINT64 trimEnd_uSec = MILLISEC_TO_MICROSEC * (UINT64)_tcstoul( pszTrimEnd, &pszTemp, 10 );

    if ( *pszTemp != _T('\0') )
    {
        _putts( _T("\nError: Trim End parameter is invalid; should be an integer in milliseconds.\n") );
        return -1;
    }

    // Do the actual work.
    _tprintf( _T("Trimming %sms from start, and %sms from end of \'%s\'. Results in \'%s\'\n"),
              pszTrimStart, pszTrimEnd, pszInputFile, pszOutputFile );

    return TrimFileInternal( pszInputFile, pszOutputFile, trimStart_uSec, trimEnd_uSec );
}

//----------------------------------------------------------------------------------------------------------------------
// Name: TrimFileInternal
// Desc: Trims a specific number of microseconds from the start and end of the file. We also rebase the timestamps
//       so that the start of the trimmed file has a timestamp of 0.
//----------------------------------------------------------------------------------------------------------------------
int TrimFileInternal( LPCTSTR pszInputFile, LPCTSTR pszOutputFile, UINT64 trimStartInUSec, UINT64 trimEndInUSec )
{
    // Open the files.

    CXEDFile inputFile;
    CXEDFile outputFile;

    if ( FAILED( inputFile.Open( pszInputFile ) ) )
    {
        _tprintf( _T("\nError opening input file \'%s\'.\n"), pszInputFile );
        return -1;
    }

    if ( FAILED( outputFile.Create( pszOutputFile, TRUE, &inputFile ) ) )
    {
        _tprintf( _T("\nError opening output file \'%s\'.\n"), pszOutputFile );
        return -1;
    }

    // Get the timestamp of the first and last frame in the file.

    CXEDMuxFwdFrameIterator it(inputFile);
    CXEDMuxReverseFrameIterator rit(inputFile);

    XEDMultiFrameRange range = inputFile.GetMultiFrameInfo();

    UINT64 firstFrameTimestamp = it.GetCurrentInfo().timestampInUsec;
    UINT64 lastFrameTimestamp = rit.GetCurrentInfo().timestampInUsec;

    // Do we have any data to copy in our new range?
    if ( (lastFrameTimestamp - firstFrameTimestamp) > ( trimStartInUSec + trimEndInUSec ) )
    {
        _tprintf( _T("\nTrimming file... ") );

        // Set up a progress bar for the console. For now, don't show full progress; just throb with each frame we
        // process.
        ProgressBar<UINT> pbar;
        pbar.SetMode( ProgressBar<UINT>::BarMode_Throb );
        pbar.Start();

        // Offset the first and last timestamps, and use them to gate the range.
        firstFrameTimestamp += trimStartInUSec;
        lastFrameTimestamp -= trimEndInUSec;

        while ( !it.IsEOF() )
        {
            XEDFrameInfo currentFrame = it.GetCurrentInfo();
            UINT64 currentFrameTime = it.GetCurrentInfo().timestampInUsec;

            // If we're not yet into the range we're preserving, keep scanning for
            // timestamps.
            
            if ( currentFrameTime < firstFrameTimestamp )
            {
                it.MoveNext();
                continue;
            }
            else if ( currentFrameTime > lastFrameTimestamp )
            {
                // If we're now past the end of the range we're preserving, we're done
                break;
            }

            if ( FAILED( CXEDFile::CopyFrame( inputFile, outputFile, currentFrame ) ) )
            {
                _putts( _T("\nError while writing data frame to output file.\n") );
                return -1;
            }

            pbar.Tick();

            it.MoveNext();
        }

        pbar.End();
    }
    else
    {
        _putts( _T("\nWarning: you trimmed more than the duration of the file; an empty")
                _T("file was created using the original file as a template.\n") );
    }

    // Close out - we're done.

    if ( FAILED( outputFile.Close() ) )
    {
        _putts( _T("\nError while closing output file.\n") );
        return -1;
    }

    return 0; 
}
