//----------------------------------------------------------------------------------------------------------------------
// Utility.cpp
//
// Implementation file for Utility methods. 
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "Utility.h"

// The rolling spinner character used by the progress bar, and scratch space for the output.
LPCTSTR s_pszSpinner = _T("-\\|/");
LPCTSTR s_pszProgFormat = _T("%c [%3d%%]");
TCHAR scratch[ SCRATCHSIZE ];

//--------------------------------------------------------------------------------------
// Name: ConvertRelativePathToFullPath
// Desc: Converts a relative path to an absolute path.
//--------------------------------------------------------------------------------------
LPTSTR ConvertRelativePathToAbsolutePath( LPCTSTR pFilePath )
{
    DWORD len = GetFullPathName( pFilePath, 0, NULL, NULL );

    if (len == 0)
        return NULL;

    TCHAR* pOut = new TCHAR[ len ];
    if ( !pOut )
        return NULL;

    if ( GetFullPathName( pFilePath, len, pOut, NULL) == 0 )
    {
        delete[] pOut;
        return NULL;
    }

    return pOut;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: FreeAbsolutePath
// Desc: Releases the memory used to store the absolute path.
//----------------------------------------------------------------------------------------------------------------------
VOID FreeAbsolutePath( LPTSTR pPath )
{
    delete[] pPath;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: EnsureUniqueFiles
// Desc: Ensures that the passed-in filenames are all unique.
//----------------------------------------------------------------------------------------------------------------------
BOOL EnsureUniqueFiles( LPCTSTR pszPath1, LPCTSTR pszPath2, LPCTSTR pszPath3 /*= NULL */ )
{
    assert( pszPath1 && pszPath2 && "Null path was passed in" );

    BOOL bUnique = TRUE;
    LPTSTR pszPath1A = NULL; 
    LPTSTR pszPath2A = NULL; 
    LPTSTR pszPath3A = NULL; 

    // Ensure that all of the filenames are absolute, rather than relative.
    
    pszPath1A = ConvertRelativePathToAbsolutePath( pszPath1 );
    if (pszPath1A == NULL)
    {
        _tprintf( _T("Error: First filename \'%s\' is invalid."), pszPath1 );
        bUnique = FALSE;
        goto cleanup;
    }

    pszPath2A = ConvertRelativePathToAbsolutePath( pszPath2 );
    if (pszPath2A == NULL)
    {
        _tprintf( _T("Error: Second filename \'%s\' is invalid."), pszPath2 );
        bUnique = FALSE;
        goto cleanup;
    }    

    if (pszPath3)
    {
        pszPath3A = ConvertRelativePathToAbsolutePath( pszPath3 );

        if (pszPath3A == NULL)
        {
            _tprintf( _T("Error: Third file path \'%s\' is invalid."), pszPath3 );
            bUnique = FALSE;
            goto cleanup;
        }
    }

    // Now compare the absolute file names, and make sure that none of them match each
    // other.

    if ( _tcscmp( pszPath1A, pszPath2A ) == 0 || ( pszPath3A && ( _tcscmp( pszPath3A, pszPath1A ) == 0 ||
         _tcscmp( pszPath3A, pszPath2A ) == 0 ) ) )
    {
        bUnique = FALSE;
    }
    
cleanup:
    delete[] pszPath1A;
    delete[] pszPath2A;
    delete[] pszPath3A;
    return bUnique;
}
