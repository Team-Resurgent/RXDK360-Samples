//----------------------------------------------------------------------------------------------------------------------
// RecordedData.cpp
// 
// Demonstrates usage of the Kinect XED Recorded Data File APIs.
//
// Provides a number of operations accessible from the command line which can be applied
// to pre-recorded data files.
//
// These files can then be reviewed in Xbox Studio, and played
// back on the Xbox via Xbox Studio on systems without Sensor Arrays, or to allow for
// repeated testing on the same dataset.
//
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "CXEDFile.h"
#include <string>   // Use the C++ library string object for easy and safe string manipulation.

const int DEFAULT_PRE_ROLL_TIME = 30;

int DisplayFileInfo( LPCTSTR pszInputFile );
int ConcatenateFiles( LPCTSTR pszFileA, LPCTSTR pszFileB, LPCTSTR pszOutputFile );
int TrimFile( LPCTSTR pszInputFile, LPCTSTR pszOutputFile,
              LPCTSTR pszTrimStart, LPCTSTR pszTrimEnd );
int DumpSkeletons( LPCTSTR pszInputFile, LPCTSTR pszOutputFile );
int DumpEvents( LPCTSTR pszInputFile, LPCTSTR pszOutputFile );

// Help text for the console application.

static LPCTSTR s_pszHelpText = 
__T("\n")
__T("Performs an operation on a data file recorded by Xbox Studio.\n")
__T("\n")
__T("RecordedData [ /I | /C | /T | /D<TYPE> ] <parameters>\n")
__T("\n")
__T("   /I infile.xed\n")
__T("   Displays information about the input data file.\n")
__T("\n")
__T("   /C fileA.xed fileB.xed outfile.xed\n")
__T("   Concatenates two files together.\n")
__T("\n")
__T("   /T infile.xed outfile.xed starttrim endtrim\n")
__T("   Trims time from the start/end of the clip.\n")
__T("\n")
__T("     starttime  time in milliseconds to trim from the start\n")
__T("     endtime    time in milliseconds to trim from the end\n")
__T("\n")
__T("   /D<TYPE> infile.xed [outputfile.csv]\n")
__T("   Removes the background from the clip.\n")
__T("     /DEVENTS\n") 
__T("       Dumps information regarding all events to the output in CSV format.\n")
__T("     /DSKELETON\n")
__T("       Dumps all skeleton joints to the output in CSV format.\n")
__T("\n")
__T("Developed by the Microsoft Advanced Technology Group (ATG).\n")
__T("\n");


//----------------------------------------------------------------------------------------------------------------------
// Name: ShowHelp
// Desc: Outputs end user help to the console.
//----------------------------------------------------------------------------------------------------------------------
VOID ShowHelp()
{
    _putts( s_pszHelpText );
}

//----------------------------------------------------------------------------------------------------------------------
// Name; Main entry point for the console application.
//----------------------------------------------------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
    // The xedfile.dll contains the functionality that this sample needs. However
    // this DLL is not in the system path, and shouldn't be. To avoid this we
    // get DebugConsole to add %XEDK%\bin\win32 to its own copy of the path.
    // Then it loads xbdm.dll. This works because the project specifies that
    // xbdm.dll should be delay loaded, so the Win32 loader doesn't load it
    // when the process loads. 
    char* path;
    _dupenv_s( &path, NULL, "path" );

    char* xedkDir;
    _dupenv_s( &xedkDir, NULL, "xedk" );

    if( !xedkDir )
        xedkDir = "";
    // Build up a new path using std::string to handle memory management.
    std::string newpath = "path=" + std::string( path ) + ";" + std::string( xedkDir ) + "\\bin\\win32";
    // Set the path to the update version.
    _putenv( newpath.c_str() );


    if ( argc == 1 ||
         _tcscmp( argv[1], _T("-h") ) == 0 ||
         _tcscmp( argv[1], _T("-help") ) == 0 ||
         _tcscmp( argv[1], _T("/?") ) == 0
       )
    {
        ShowHelp();
        return 0;
    }

    if ( _tcscmp( argv[1], _T("/I") ) == 0 && argc == 3 )
    {
        // Display info about file
        // /I inputfile.xed

        return DisplayFileInfo( argv[2] );
    }
    else if ( _tcscmp( argv[1], _T("/C") ) == 0 && argc == 5 )
    {
        // Concatenate files
        // /C fileA.xed fileB.xed outfile.xed
        
        return ConcatenateFiles( argv[2], argv[3], argv[4] ); 
    }
    else if ( _tcscmp( argv[1], _T("/T") ) == 0 && argc == 6 ) 
    {
        // Trims the file
        // /T inputfile.xed outputfile.xed trimstarttime_ms trimendtime_ms

        return TrimFile( argv[2], argv[3], argv[4], argv[5] ); 
    }
    else if ( _tcscmp( argv[1], _T("/DSKELETON") ) == 0 && argc >= 3 && argc <= 4 ) 
    {
        // Dump skeleton data as CSV
        // /DSKELETONS inputfile.xed [outfile.csv]
        
        LPCTSTR pszOutFile = NULL;
        if (argc == 4)
        {
            pszOutFile = argv[3];
        }

        return DumpSkeletons( argv[2], pszOutFile );
    }
    else if ( _tcscmp( argv[1], _T("/DEVENTS") ) == 0 && argc >= 3 && argc <= 4) 
    {
        // Dump events as CSV
        // /DEVENTS inputfile.xed [outfile.csv]

        LPCTSTR pszOutFile = NULL;
        if (argc == 4)
        {
            pszOutFile = argv[3];
        }

        return DumpEvents( argv[2], pszOutFile );
    }
    else
    {
        ShowHelp();
        return -1;
    }
}
