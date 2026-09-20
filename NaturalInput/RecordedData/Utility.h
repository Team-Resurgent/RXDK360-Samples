//----------------------------------------------------------------------------------------------------------------------
// Utility.h
//
// Utility functions and classes.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#ifndef UTILITY_H_GUARD
#define UTILITY_H_GUARD

BOOL EnsureUniqueFiles( LPCTSTR pszPath1, LPCTSTR pszPath2, LPCTSTR pszPath3 = NULL );
VOID FreeAbsolutePath( LPTSTR pPath );
LPTSTR ConvertRelativePathToAbsolutePath( LPCTSTR pFilePath );

template <typename TValue>
class ProgressBar
{
public:
    
    enum BarMode
    {
        BarMode_Throb,
        BarMode_Normal
    };

    ProgressBar();
    ~ProgressBar();

    VOID SetLimits( TValue minValue = 0, TValue maxValue = 100, TValue currentValue = 0 );
    VOID SetMode( BarMode mode = BarMode_Normal );

    VOID Start();
    VOID SetValue( TValue m_iCurrent );
    VOID Tick();
    VOID End();

private:
    VOID WriteProgressBar();
    
    HANDLE m_hStdOut;
    LPCTSTR m_pszSpinnerPos;
    COORD m_cStart;
    COORD m_cEnd;

    BarMode m_bThrob;
    TValue m_iMin;
    TValue m_iCurrent;
    TValue m_iMax;
};

// The rolling spinner character.
extern LPCTSTR s_pszSpinner;
extern LPCTSTR s_pszProgFormat;
const size_t SCRATCHSIZE = 40;
extern TCHAR scratch[ SCRATCHSIZE ];

#include "Utility.inl"

#endif //UTILITY_H_GUARD
