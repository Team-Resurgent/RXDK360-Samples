//----------------------------------------------------------------------------------------------------------------------
// TxDump.cpp
//
// Provides support for dumping the contents of an XED file to the console or another
// file in CSV format.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "CXEDFile.h"
#include "XEDFileIterators.h"

static TCHAR s_cEOL = _T('\n');

static LPCTSTR s_pszEventCSVHeader = _T("Index,Frame #,Timestamp (us),Frame Type\n");
static LPCTSTR s_pszEventRowFormat = _T("%u,%u,%I64u,%s\n");

static LPCTSTR s_pszSkeletonCSVHeaderID = _T("Index,Frame #,Timestamp (us),");
static LPCTSTR s_pszSkeletonHeaderFormat = _T("%u,%u,%I64u,");
static LPCTSTR s_pszSkeletonCSVHeaderIDSkip = _T(",,,");
static LPCTSTR s_pszSkeletonVectorCols = _T("x,y,z,State,");
static LPCTSTR s_pszSkeletonVectorSkip = _T(",,,,");
static LPCTSTR s_pszSkeletonVectorFormat = _T("%.5f,%.5f,%.5f,%s,");
static LPCTSTR s_pszSkeletonName = _T("Skeleton %d,");
static LPCTSTR s_pszSkeletonTracked = _T("Skel %d Tracked,");
static LPCTSTR s_pszTracked = _T("Tracked,");
static LPCTSTR s_pszNotTracked = _T("Not Tracked,");
static TCHAR s_cSkipColumn = _T(',');

static const int COLUMNS_PER_JOINT = 4;

static LPCTSTR s_ppszTrackingState[] = 
{
    _T("Not Tracked"), // Must match NUI_SKELETON_POSITION_TRACKING_STATE enum!
    _T("Inferred"),
    _T("Tracked")
};


struct SkelCSVOutput
{
    NUI_SKELETON_POSITION_INDEX index;
    LPCTSTR pszText;
};


static SkelCSVOutput s_skelCSVJoints[] = 
{
    { NUI_SKELETON_POSITION_HIP_CENTER,      _T("Hip Center") },
    { NUI_SKELETON_POSITION_SPINE,           _T("Spine") },
    { NUI_SKELETON_POSITION_SHOULDER_CENTER, _T("Shoulder Center") },
    { NUI_SKELETON_POSITION_HEAD,            _T("Head") },
    { NUI_SKELETON_POSITION_SHOULDER_LEFT,   _T("Shoulder Left") },
    { NUI_SKELETON_POSITION_ELBOW_LEFT,      _T("Elbow Left") },
    { NUI_SKELETON_POSITION_WRIST_LEFT,      _T("Wrist Left") },
    { NUI_SKELETON_POSITION_HAND_LEFT,       _T("Hand Left") },
    { NUI_SKELETON_POSITION_SHOULDER_RIGHT,  _T("Shoulder Right") },
    { NUI_SKELETON_POSITION_ELBOW_RIGHT,     _T("Elbow Right") },
    { NUI_SKELETON_POSITION_WRIST_RIGHT,     _T("Wrist Right") },
    { NUI_SKELETON_POSITION_HAND_RIGHT,      _T("Hand Right") },
    { NUI_SKELETON_POSITION_HIP_LEFT,        _T("Hip Left") },
    { NUI_SKELETON_POSITION_KNEE_LEFT,       _T("Knee Left") },
    { NUI_SKELETON_POSITION_ANKLE_LEFT,      _T("Ankle Left") },
    { NUI_SKELETON_POSITION_FOOT_LEFT,       _T("Foot Left") },
    { NUI_SKELETON_POSITION_HIP_RIGHT,       _T("Hip Right") },
    { NUI_SKELETON_POSITION_KNEE_RIGHT,      _T("Knee Right") },
    { NUI_SKELETON_POSITION_ANKLE_RIGHT,     _T("Ankle Right") },
    { NUI_SKELETON_POSITION_FOOT_RIGHT,      _T("Foot Right") }
};


enum DumpType
{
    Dump_SkeletonData,
    Dump_EventData
};

LPCTSTR GetFrameType( XEDFRAMETYPE type );
int DumpXEDToCSV(DumpType type, LPCTSTR pszInputFile, LPCTSTR pszOutputFile );
int DumpSkeletonFile( CXEDFile& file, FILE* pOutFile );
int DumpEventFile( CXEDFile& file, FILE* pOutFile );
int DumpSkelHeader( FILE* pOutFile );
BOOL DumpSkelRow( FILE* pOutFile, CXEDSkeletonFrame& skelFrame, int index );

//----------------------------------------------------------------------------------------------------------------------
// Name: GetFrameType
// Desc: Gets the frame type as a string.
//----------------------------------------------------------------------------------------------------------------------
LPCTSTR GetFrameType( XEDFRAMETYPE type )
{
    static LPCTSTR s_pszTypes[] = {
        _T("Color"),
        _T("Depth"),
        _T("Skeleton")
    };

    return s_pszTypes[ type ];
}

//----------------------------------------------------------------------------------------------------------------------
// Name: DumpSkeletons
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
int DumpSkeletons( LPCTSTR pszInputFile, LPCTSTR pszOutputFile )
{
    return DumpXEDToCSV( Dump_SkeletonData, pszInputFile, pszOutputFile );
}

//----------------------------------------------------------------------------------------------------------------------
// Name: DumpEvents
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
int DumpEvents( LPCTSTR pszInputFile, LPCTSTR pszOutputFile )
{
    return DumpXEDToCSV( Dump_EventData, pszInputFile, pszOutputFile );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: DumpXEDToCSV
// Desc: Dumps the contents of the file out.
//----------------------------------------------------------------------------------------------------------------------
int DumpXEDToCSV(DumpType type, LPCTSTR pszInputFile, LPCTSTR pszOutputFile )
{
    CXEDFile input;
    HRESULT hr;

    hr = input.Open( pszInputFile );
    if ( FAILED( hr ) )
    {
        _tprintf( _T("\nError: Couldn't open input file \'%s\' (error: %x).\n"),
                  pszInputFile, hr );
        return -1;
    }


    FILE* pOutFile;

    if ( pszOutputFile != NULL)
    {

        if ( _tfopen_s( &pOutFile, pszOutputFile, _T("wt") ) != 0)
        {
            _tprintf( _T("\nError: Couldn't open output file \'%s\' (error: %x).\n"),
                      pszOutputFile, hr );
            return -1;
        }

    }
    else
    {
        pOutFile = stdout;
    }

    int err;

    switch( type )
    {
        case Dump_SkeletonData:
            err = DumpSkeletonFile( input, pOutFile );
            break;
        case Dump_EventData:
            err = DumpEventFile( input, pOutFile );
            break;
        default:
			assert( false );
            NODEFAULT;
    }

    if ( fclose( pOutFile ) == _TEOF )
    {
        _putts( _T("\nError closing output file.\n") );
        return -1;
    }

    return err;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: DumpSkeletonFile
// Desc: Iterates over all of the skeleton data frames in the provided XED file, and
//       writes it out to a file handle (which may be STDOUT) in CSV format.
//----------------------------------------------------------------------------------------------------------------------
int DumpSkeletonFile( CXEDFile& file, FILE* pOutFile )
{
    if ( !DumpSkelHeader( pOutFile ) )
    {
        _putts( _T("\nError writing to output file") );
        return -1;
    }

    CXEDSkeletonFrame sf;

    int i = 0;

    for ( CXEDFwdEventIterator it( file, XED_SKELETON ); !it.IsEOF(); it.MoveNext() )
    {
        if ( FAILED( file.ReadFrame( it.GetCurrentIndex(), &sf ) ) )
        {
            _putts( _T("\nError reading input file") );
            return -1;
        }

        if ( !DumpSkelRow( pOutFile, sf, i++ ))
        {
            _putts( _T("\nError writing to output file") );
        }
    }

    return 0;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: DumpEventFile
// Desc: Iterates over all of the events in the provided XED file, and writes it out to
//       a file handle (which may be STDOUT) in CSV format.
//----------------------------------------------------------------------------------------------------------------------
int DumpEventFile( CXEDFile& file, FILE* pOutFile )
{
    if (_fputts( s_pszEventCSVHeader, pOutFile ) == _TEOF)
    {
        _putts( _T("\nError writing to output file.\n") );
        return -1;
    }

    int i = 0;
    for ( CXEDMuxFwdFrameIterator it( file ); !it.IsEOF(); it.MoveNext() )
    {
        XEDFrameInfo info = it.GetCurrentInfo();
        XEDFRAMETYPE type = it.GetCurrentType();
        
        if ( _ftprintf( pOutFile, s_pszEventRowFormat, i++, info.frameNumber,
                   info.timestampInUsec, GetFrameType( type ) ) == _TEOF )
        {
            _putts( _T("\nError writing to output file.\n") );
            return -1;
        }
    }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: DumpSkelRow
// Desc: Writes a single row of skeleton data to the CSV file.
//----------------------------------------------------------------------------------------------------------------------
BOOL DumpSkelRow( FILE* pOutFile, CXEDSkeletonFrame& skelFrame, int index )
{
    NUI_SKELETON_FRAME& sf = *skelFrame.GetData();

    if ( _ftprintf( pOutFile, s_pszSkeletonHeaderFormat, index,
                    skelFrame.GetFrameNumber(),
                    skelFrame.GetTimestamp() ) == _TEOF )
        return FALSE;

    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i )
    {
        if ( _fputts( (sf.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED) ? s_pszTracked : s_pszNotTracked,
            pOutFile ) == _TEOF )
            return FALSE;
    }

    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i )
    {
        for ( int j = 0; j < ARRAYSIZE( s_skelCSVJoints ); ++j )
        {
            if ( sf.SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
            {
                 float x,y,z;
                 x = XMVectorGetX( 
                    sf.SkeletonData[ i ].SkeletonPositions[ s_skelCSVJoints[ j ].index ]
                 );
                 y = XMVectorGetY(
                    sf.SkeletonData[ i ].SkeletonPositions[ s_skelCSVJoints[ j ].index ]
                 );
                 z = XMVectorGetZ(
                    sf.SkeletonData[ i ].SkeletonPositions[ s_skelCSVJoints[ j ].index ]
                 );

                 LPCTSTR pszConf = s_ppszTrackingState[ sf.SkeletonData[ i ].eSkeletonPositionTrackingState[ s_skelCSVJoints[ j ].index ] ];
        
                if ( _ftprintf( pOutFile, s_pszSkeletonVectorFormat, x, y, z, pszConf )
                     == _TEOF)
                    return FALSE;
            }
            else
            {
                if ( _fputts( s_pszSkeletonVectorSkip, pOutFile ) == _TEOF )
                    return FALSE;
            }
        }
    }

    // Write newline.
    if ( _fputtc( s_cEOL, pOutFile ) == _TEOF)
        return FALSE;

    return TRUE;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: DumpSkelHeader
// Desc: Writes the CSV header to the file (for dumping skeleton information)
//----------------------------------------------------------------------------------------------------------------------
BOOL DumpSkelHeader( FILE* pOutFile )
{
    // Skip over the columns for the CSV header.
    if ( _fputts( s_pszSkeletonCSVHeaderIDSkip, pOutFile ) == _TEOF )
        return FALSE;

    // Skip over the "skeleton Tracked" columns
    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i )
    {
        if ( _fputtc( s_cSkipColumn, pOutFile ) == _TEOF )
            return FALSE;
    }

    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i )
    {
       // Output the skeleton name for the first skeleton.
        if ( _ftprintf( pOutFile, s_pszSkeletonName, i ) == _TEOF )
            return FALSE;

        // Output skip columns for the skeleton joints.
        for (int j = 1; j < COLUMNS_PER_JOINT * ARRAYSIZE( s_skelCSVJoints ); ++j )
        {
            if ( _fputtc( _T(','), pOutFile ) == _TEOF )
                return FALSE;
        }
    }

    // Write newline.
    if ( _fputtc( s_cEOL, pOutFile ) == _TEOF)
        return FALSE;

    // Secondary header line: joint names.

    // Skip over the columns for the frame IDs.
    if ( _fputts( s_pszSkeletonCSVHeaderIDSkip, pOutFile ) == _TEOF )
        return FALSE;

    // Skip over the "skeleton Tracked" columns
    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i )
    {
        if ( _fputtc( s_cSkipColumn, pOutFile ) == _TEOF )
            return FALSE;
    }

    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i )
    {
        // Output skip columns for the skeleton joints.
        for (int j = 0; j < ARRAYSIZE( s_skelCSVJoints ); ++j )
        {
            if ( _fputts( s_skelCSVJoints[ j ].pszText, pOutFile ) == _TEOF )
                return FALSE;

            // Add in the column skips.
            if ( _fputts( s_pszSkeletonVectorSkip, pOutFile ) == _TEOF )
                return FALSE;
        }
    }

    // Write newline.
    if ( _fputtc( s_cEOL, pOutFile ) == _TEOF)
        return FALSE;

    // Write the frame header IDs, and joint vector component names.

    if ( _fputts( s_pszSkeletonCSVHeaderID, pOutFile) == _TEOF )
        return FALSE;

    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i )
    {
        if ( _ftprintf( pOutFile, s_pszSkeletonTracked, i ) == _TEOF )
            return FALSE;
    }


    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i )
    {
        // Output skip columns for the skeleton joints.
        for (int j = 0; j < ARRAYSIZE( s_skelCSVJoints ); ++j )
        {
            if ( _fputts( s_pszSkeletonVectorCols, pOutFile ) == _TEOF )
                return FALSE;
        }
    }

    // Write newline.
    if ( _fputtc( s_cEOL, pOutFile ) == _TEOF)
        return FALSE;

    return TRUE;
}