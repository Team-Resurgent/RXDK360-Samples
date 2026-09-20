//--------------------------------------------------------------------------------------
// WordwrapUtil.h
//
// The wordwrap functions define where in a line a break can occur.  It accomplishes 
// this by having knowledge of what characters can't be present at the beginning or
// end of a given line for each language.  This knowledge is contained in a list of
// UNICODE characters.
//
// Developed by Microsoft Game Studios Tools and Technology Group
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef WORDWRAPUTIL_H
#define WORDWRAPUTIL_H

typedef INT (*CB_GetWidthW )( WCHAR );
typedef INT (*CB_Reserved )( VOID );

void WordWrap_SetOption( INT iOption );
void WordWrap_SetCallback( CB_GetWidthW pGetWidthW, CB_Reserved pReserved );

const WCHAR*    WordWrap_FindNextLine( const WCHAR* pwszSource, INT iWidth, const WCHAR** ppwszEOL );

const DWORD     WW_PROHIBITION = 0x00000001;
const DWORD     WW_NOHANGULWRAP = 0x00000002;   // disable Hangul Character's WordWrap

bool WordWrap_CanBreakLineAt( const WCHAR* pwsz, const WCHAR* pwszStart );
const WCHAR*    WordWrap_FindNonWhiteSpaceForward( const WCHAR* pwsz );
#define WordWrap_IsWhiteSpace(c) ( ( c ) == L'\t' || ( c ) == L'\r' || ( c ) == L' ' || ( c ) == 0x3000 )
#define WordWrap_IsLineFeed(c) ( ( c ) == L'\n' )

#endif //WORDWRAPUTIL_H
