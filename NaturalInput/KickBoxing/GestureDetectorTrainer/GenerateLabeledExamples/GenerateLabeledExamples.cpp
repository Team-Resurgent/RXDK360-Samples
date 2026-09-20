//--------------------------------------------------------------------------------------
// GenerateLabeledExamples.cpp
//
// This sample demonstrates how the time consuming art of manually fine tuning input
// parameters and thresholds for gesture detection implementations can be replaced by
// an automatic process where a computer finds the best input paramters and thresholds
// by using machine learning. The gesture detection system used in this sample consist
// of training component which runs on the PC (GenerateLabeledExamples and TrainGesture)
// and a runtime component that is used in this sample on the Xbox 360.
//
// The goal of this application is to take tagged xed files as input and outputs
// labeled examples that can be used as ground truth data when training and testing.
// An example batch file is included to show how to use this application
//
//      \GestureDetectorTrainer\TrainGestures.bat
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <iostream>
#include <fstream>
#include <vector>
#include "..\..\GestureDetector\GestureDetector.h"

using namespace std;
using namespace ATGGestureDetector;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------

BOOL ValidateCommandlineParameters( const INT nNumParameters );
VOID FindXedFiles( const CHAR* szDataFolder, const CHAR* szSubFolder, vector<wstring>& szXedFiles );


//--------------------------------------------------------------------------------------
// Name: main
// Desc: main entry point for application
//--------------------------------------------------------------------------------------

INT main( INT nNumParameters, CHAR* szParameters[] )
{ 
    CHAR szPath[ MAX_PATH ];
    vector<wstring> szXedFilesForTraining;
    vector<wstring> szXedFilesForTesting;
    GestureDetectorTrainer gestureDetectorTrainer;

    printf( "\nGenerateLabeledExamples"
            "\n-----------------------\n" );

    // First validate the parameters
    if ( !ValidateCommandlineParameters( nNumParameters ))
    {
        return -1;
    }

    // We want to time how long this takes
    DWORD dwStart = GetTickCount();

    const CHAR* szGestureName   = szParameters[ 1 ];
    const CHAR* szDataFolder    = szParameters[ 2 ];
    const CHAR* szOutputFolder  = szParameters[ 3 ];

    BOOL bUseRawSkeletonData    = TRUE;
    if ( nNumParameters > 4 )
    {
        bUseRawSkeletonData     = ( _stricmp( szParameters[ 4 ], "true" ) == 0 );
    }

    // Find out if we're using an input folder or a text file with multiple input folders
    if ( strstr( szDataFolder, ".txt" ) )
    {
        ifstream inputFile( szDataFolder );

        if ( !inputFile )
        {
             printf ( "\nERROR: Could not open %s\n", szDataFolder );
             return -1;
        }

        // Read the context of the text file, use it as folders and file the xed files in those folders
        while ( !inputFile.eof() )
        {
            inputFile >> szPath;
            FindXedFiles( szPath, g_szTraining, szXedFilesForTraining );
            FindXedFiles( szPath, g_szTesting, szXedFilesForTesting );
        }

        inputFile.close();
    }
    else
    {
        // Find the xed files in the folder specified in the input
        FindXedFiles( szDataFolder, g_szTraining, szXedFilesForTraining );
        FindXedFiles( szDataFolder, g_szTesting, szXedFilesForTesting );
    }

    // Generate labaled examples for training
    sprintf_s( szPath, sizeof( szPath ), "%s\\%s_%s.le", szOutputFolder, szGestureName, g_szTraining );
    if ( szXedFilesForTraining.size() == 0 ||
         FAILED( gestureDetectorTrainer.GenerateLabeledExamples( szGestureName, szXedFilesForTraining, szPath, bUseRawSkeletonData ) ) )
    {
        // Delete any old file so that the trainer don't accidentally use stale data
        std::remove( szPath );
        printf( "\nERROR: Failed to generate labeled examples for training data\n" );
        return -1;
    }

    const UINT nNumTrainingExamples = gestureDetectorTrainer.GetNumExamples();
    const UINT nNumGestures = gestureDetectorTrainer.GetNumGestures();
    const UINT nNumTrainingGestures = gestureDetectorTrainer.GetNumTrainingGestures();
    const FLOAT fRatioPosNegExamples = (FLOAT)gestureDetectorTrainer.GetNumNegativeExamples() / (FLOAT)gestureDetectorTrainer.GetNumPositiveExamples();
    const FLOAT fRatioPosNegGestures = (FLOAT)(nNumGestures - nNumTrainingGestures) / (FLOAT)nNumTrainingGestures;

    // Generate labaled examples for testing
    sprintf_s( szPath, sizeof( szPath ), "%s\\%s_%s.le", szOutputFolder, szGestureName, g_szTesting );
    if ( szXedFilesForTesting.size() == 0 ||
         FAILED( gestureDetectorTrainer.GenerateLabeledExamples( szGestureName, szXedFilesForTesting, szPath, bUseRawSkeletonData ) ) )
    {
        // Delete any old file so that the trainer don't accidentally use stale data
        std::remove( szPath );

        // This is not a fail case, you could choose not to have testing samples, so allow the app to continue
        printf( "\nWARNING: Failed to generate labeled examples for testing data\n" );
    }

    const UINT nNumTestingExamples = gestureDetectorTrainer.GetNumExamples();

    DWORD dwStop = GetTickCount();
    DWORD dwSeconds = ( (dwStop - dwStart ) / 1000 ) % 60;
    DWORD dwMinutes = ( (dwStop - dwStart ) / 1000) / 60;

    printf( "\n\nDuration: %d minutes, %d seconds",  dwMinutes, dwSeconds );
    printf( "\n\n%s", g_szTraining );
    printf( "\n\tNum Labeled Examples: %d", nNumTrainingExamples );
    printf( "\n\tRatio positive to negative labeled examples: 1.0 : %f", fRatioPosNegExamples );
    printf( "\n\tTotal Num Gestures: %d", nNumGestures );
    printf( "\n\tNum Gestures Labeled as %s: %d", szGestureName, nNumTrainingGestures );
    printf( "\n\tRatio %s to all other gestures: 1.0 : %f", szGestureName, fRatioPosNegGestures );
    printf( "\n\n%s", g_szTesting );
    printf( "\n\tNum Labeled Examples: %d", nNumTestingExamples );
    printf( "\n\tRatio positive to negative labeled examples: 1.0 : %f", (FLOAT)gestureDetectorTrainer.GetNumNegativeExamples() / (FLOAT)gestureDetectorTrainer.GetNumPositiveExamples() );
    printf( "\n\tTotal Num Gestures: %d", gestureDetectorTrainer.GetNumGestures() );
    printf( "\n\tNum Gestures Labeled as %s: %d", szGestureName, gestureDetectorTrainer.GetNumTrainingGestures() );
    printf( "\n\tRatio %s to all other gestures: 1.0 : %f", szGestureName, (FLOAT)(gestureDetectorTrainer.GetNumGestures() - gestureDetectorTrainer.GetNumTrainingGestures()) / (FLOAT)gestureDetectorTrainer.GetNumTrainingGestures() );
    printf( "\n\nDone\n" );

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: ValidateCommandlineParameters
// Desc: Validates the command line parameters
//--------------------------------------------------------------------------------------

BOOL ValidateCommandlineParameters( const INT nNumParameters )
{
    BOOL bFail = FALSE;

    // Check number of parameters
    if ( nNumParameters < 4 )
    {
        bFail = TRUE;
    }

    if ( bFail )
    {
        printf( "\nInstructions:"
                "\n    Use the XedFileTagger sample to add gesture tags to a xed file, "
                "\n    e.g. tag the frames that represent a left punch as 'Punch_Left'."
                "\n\nUsage:\n"
                "    GenerateLabeledExamples <GestureName> <PathToXedFiles or TextFileWithListOfPaths> <OutputFolder> <UseRawSkeletonData>\n\n"
                "      <GestureName>\n"
                "        The gesture to train. The gesture should be tagged with this name\n"
                "        using the XedFileTagger.\n\n"
                "      <PathToXedFiles>\n"
                "        Xed files tagged using the XedFileTagger. This can be a path to a single\n"
                "        data folder that has \\Training and \\Testing sub folders, or a text\n"
                "        file with paths to several folders so that data can be organized better.\n\n"
                "      <OutputFolder>\n"
                "        Output folder where training example file will be created. This is the\n"
                "        file that will get used by TrainGesture.exe when training.\n\n"
                "      <UseRawSkeletonData>\n"
                "        true for using raw skeleton data, false to use filtered skeleton data\n"
                "        that was recorded as title data. Default is true.\n\n"
                "Examples:\n"
                "    GenerateLabeledExamples.exe Punch_Left .\\Data .\\Data true\n"
                "    GenerateLabeledExamples.exe Punch_Left ListOfDataFolders.txt ..\\Data true\n\n" );

        return FALSE;
    }

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: FindXedFiles
// Desc: Find xed files in a folder and add them to a list
//--------------------------------------------------------------------------------------

VOID FindXedFiles( const CHAR* szDataFolder, const CHAR* szSubFolder, vector<wstring>& szXedFiles )
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;

    CHAR szPath[ MAX_PATH ];
    sprintf_s( szPath, MAX_PATH, "%s\\%s\\*.xed", szDataFolder, szSubFolder );

    WCHAR szwPath[ MAX_PATH ];
    size_t uConverted;
    mbstowcs_s( &uConverted, szwPath, szPath, MAX_PATH );
 
    hFind = FindFirstFile( szwPath, &FindFileData );
    if ( hFind == INVALID_HANDLE_VALUE )
    {
      printf ( "\nNo files found in %s\\%s\n", szDataFolder, szSubFolder );
      return;
    }

    sprintf_s( szPath, MAX_PATH, "%s\\%s\\", szDataFolder, szSubFolder );

    WCHAR szXedFileNameW[ MAX_PATH ];
    do
    {
        mbstowcs_s( &uConverted, szXedFileNameW, szPath, MAX_PATH );
        wcscat_s( szXedFileNameW, MAX_PATH, FindFileData.cFileName );
        szXedFiles.push_back( szXedFileNameW );
    } while ( FindNextFile( hFind, &FindFileData ) != 0 );

    FindClose(hFind);
}