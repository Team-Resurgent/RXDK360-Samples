//--------------------------------------------------------------------------------------
// WordwrapUtil.cpp
//
// The wordwrap functions define where in a line a break can occur.  It accomplishes 
// this by having knowledge of what characters can't be present at the beginning or
// end of a given line for each language.  This knowledge is contained in a list of
// UNICODE characters.
//
// Developed by Microsoft Game Studios Tools and Technology Group
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <malloc.h>
#include "WordwrapUtil.h"

//--------------------------------------------------------------------------------------
// defines
//--------------------------------------------------------------------------------------

#define COUNTOF(a) ( sizeof( a ) / sizeof( (a)[0] ) )

struct BreakInfo
{
    WCHAR wch;
    bool isNonBeginningChar;
    bool isNonEndingChar;
};

//--------------------------------------------------------------------------------------
// Array of characters that can't be Beginning or ending characters on a line
//--------------------------------------------------------------------------------------
static BreakInfo BreakArray[] =
{
    {0x0021, true,  false}, // 0021: !                                         fr, etc. 
    {0x0022, true,  true},  // 0022: "                                         tc, kr 
    {0x0024, false, true},  // 0024: $                                         kr 
    {0x0025, true,  false}, // 0025: %                                         tc, kr 
    {0x0027, true,  true},  // 0027: '                                         fr 
    {0x0028, false, true},  // 0028: (                                         tc, kr 
    {0x0029, true,  false}, // 0029: )                                         fr, etc. 
    {0x002E, true,  false}, // 002E: .                                         fr 
    {0x002F, true,  true},  // 002F: /                                         fr, etc. 
    {0x003A, true,  false}, // 003A: :                                         tc, kr 
    {0x003B, true,  false}, // 003B: ;                                         tc, kr 
    {0x003F, true,  false}, // 003F: ?                                         fr 
    {0x005B, false, true},  // 005B: [                                         tc, kr 
    {0x005C, false, true},  // 005C: Reverse Solidus                           kr 
    {0x005D, true,  false}, // 005D: ]                                         tc, kr 
    {0x007B, false, true},  // 007B: {                                         tc, kr 
    {0x007D, true,  false}, // 007D: }                                         tc, kr 
    {0x00A3, false, true},  // 00A3: Pound Sign                                tc 
    {0x00A5, false, true},  // 00A5: Yen Sign                                  tc 
    {0x00A7, false, true},  // 00A7: Section Sign                              ja 
    //{0x00A9, true,  false}, // 00A9: copyright sign (if necessary) 
    //{0x00AE, true,  false}, // 00AE: registered sign (if necessary) 
    {0x00B0, true,  false}, // 00B0: Degree Sign                               kr 
    {0x00B7, true,  false}, // 00B7: Middle Dot                                tc 
    {0x2013, true,  false}, // 2013: En Dash                                   tc 
    {0x2014, true,  false}, // 2014: Em Dash                                   tc 
    {0x2018, false, true},  // 2018: Left Single Quotation Mark                tc, kr 
    {0x2019, true,  false}, // 2019: Right Single Quotation Mark               tc, fr, kr 
    {0x201C, false, true},  // 201C: Left Double Quotation Mark                tc, kr 
    {0x201D, true,  false}, // 201D: Right Double Quotation Mark               de, kr 
    {0x2025, true,  false}, // 2025: Two Dot Leader                            tc 
    {0x2026, true,  false}, // 2026: Horizontal Ellpsis                        tc 
    {0x2027, true,  false}, // 2027: Hyphenation Point                         tc 
    {0x2032, true,  false}, // 2032: Prime                                     tc, kr 
    {0x2033, true,  false}, // 2033: Double Prime                              kr 
    {0x2035, false, true},  // 2035: Reversed Prime                            tc 
    {0x2103, true,  false}, // 2103: Degree Celsius                            ja, kr 
    //{0x2122, true,  false}, // 2122: trade mark sign (if necessary) 
    {0x2574, true,  false}, // 2574: Box Drawings Light Left                   tc 
    {0x266F, false, true},  // 266F: Music Sharp Sign                          ja 
    {0x3001, true,  false}, // 3001: fullwidth ideographic comma               ja, tc 
    {0x3002, true,  false}, // 3002: fullwidth full stop                       ja, tc 
    {0x3005, true,  false}, // 3005: Ideographic Iteration Mark                ja 
    {0x3008, false, true},  // 3008: Left Angle Bracket                        tc, kr 
    {0x3009, true,  false}, // 3009: Right Angle Bracket                       tc, kr 
    {0x300A, false, true},  // 300A: Left Double Angle Bracket                 tc, kr 
    {0x300B, true,  false}, // 300B: Right Double Angle Bracket                tc, kr 
    {0x300C, false, true},  // 300C: Left Corner Bracket                       ja, tc, kr 
    {0x300D, true,  false}, // 300D: Right Corner Bracket                      ja, tc, kr 
    {0x300E, false, true},  // 300E: Left White Corner Bracket                 tc, kr 
    {0x300F, true,  false}, // 300F: Rignt White Corner Bracket                tc, kr 
    {0x3010, false, true},  // 3010: Left Black Lenticular Bracket             ja, tc, kr 
    {0x3011, true,  false}, // 3011: right black lenticular bracket            ja, tc, kr 
    {0x3012, false, true},  // 3012: Postal Mark                               ja 
    {0x3014, false, true},  // 3014: Left Tortoise Shell Bracket               tc, kr 
    {0x3015, true,  false}, // 3015: Right Tortoise Shell Bracket              tc, kr 
    {0x301D, false, true},  // 301D: Reversed Double Prime Quotation Mark      tc 
    {0x301E, true,  false}, // 301E: Double Prime Quotation Mark               tc 
    {0x301F, true,  false}, // 301F: Low Double Prime Quotation Mark           tc 
    {0x3041, true,  false}, // 3041: Hiragana Letter Small A                   ja 
    {0x3043, true,  false}, // 3043: Hiragana Letter Small I                   ja 
    {0x3045, true,  false}, // 3045: Hiragana Letter Small U                   ja 
    {0x3047, true,  false}, // 3047: Hiragana Letter Small E                   ja 
    {0x3049, true,  false}, // 3049: Hiragana Letter Small O                   ja 
    {0x3063, true,  false}, // 3063: Hiragana Letter Small Tu                  ja 
    {0x3083, true,  false}, // 3083: Hiragana Letter Small Ya                  ja 
    {0x3085, true,  false}, // 3085: Hiragana Letter Small Yu                  ja 
    {0x3087, true,  false}, // 3087: Hiragana Letter Small Yo                  ja 
    {0x308E, true,  false}, // 308E: Hiragana Letter Small Wa                  ja 
    //{0x3099, true,  false}, // 3099: Combining Katakana-Hiragana Voiced Sound Mark (if necessary) 
    //{0x309A, true,  false}, // 309A: Combining Katakana-Hiragana Semi-Voiced Sound Mark (if necessary) 
    {0x309B, true,  false}, // 309B: Katakana-Hiragana Voiced Sound Mark       ja 
    {0x309C, true,  false}, // 309C: Katakana-Hiragana Semi-Voiced Sound Mark  ja 
    {0x309D, true,  false}, // 309D: Hiragana Iteration Mark                   ja 
    {0x309E, true,  false}, // 309E: Hiragana Voiced Iteration Mark            ja 
    {0x30A1, true,  false}, // 30A1: Katakana Letter Small A                   ja 
    {0x30A3, true,  false}, // 30A3: Katakana Letter Small I                   ja 
    {0x30A5, true,  false}, // 30A5: Katakana Letter Small U                   ja 
    {0x30A7, true,  false}, // 30A7: Katakana Letter Small E                   ja 
    {0x30A9, true,  false}, // 30A9: Katakana Letter Small O                   ja 
    {0x30C3, true,  false}, // 30C3: Katakana Letter Small Tu                  ja 
    {0x30E3, true,  false}, // 30E3: Katakana Letter Small Ya                  ja 
    {0x30E5, true,  false}, // 30E5: Katakana Letter Small Yu                  ja 
    {0x30E7, true,  false}, // 30E7: Katakana Letter Small Yo                  ja 
    {0x30EE, true,  false}, // 30EE: Katakana Letter Small Wa                  ja 
    {0x30F5, true,  false}, // 30F5: Katakana Letter Small Ka                  ja 
    {0x30F6, true,  false}, // 30F6: Katakana Letter Small Ke                  ja 
    {0x30FB, true,  false}, // 30FB: katakana middle dot                       ja 
    {0x30FC, true,  false}, // 30FC: Katakana-Hiragana Prolonged Sound Mark    ja 
    {0x30FD, true,  false}, // 30FD: Katakana Iteration Mark                   ja 
    {0x30FE, true,  false}, // 30FE: Katakana Voiced Iteration Mark            ja 
    {0xFE50, true,  false}, // FE50: Small Comma                               tc 
    {0xFE51, true,  false}, // FE51: Small Ideographic Comma                   tc 
    {0xFE52, true,  false}, // FE52: Small Full Stop                           tc 
    {0xFE54, true,  false}, // FE54: Small Semicolon                           tc 
    {0xFE55, true,  false}, // FE55: Small Colon                               tc 
    {0xFE56, true,  false}, // FE56: Small Question Mark                       tc 
    {0xFE57, true,  false}, // FE57: Small Exclamation Mark                    tc 
    {0xFE59, false, true},  // FE59: Small Left Parenthesis                    tc 
    {0xFE5A, true,  false}, // FE5A: Small Right Parenthesis                   tc 
    {0xFE5B, false, true},  // FE5B: Small Left Curly Bracket                  tc 
    {0xFE5C, true,  false}, // FE5C: Small Right Curly Bracket                 tc 
    {0xFE5D, false, true},  // FE5D: Small Left Tortoise Shell Bracket         tc 
    {0xFE5E, true,  false}, // FE5E: Small Right Tortoise Shell Bracket        tc 
    {0xFF01, true,  false}, // FF01: Fullwidth Exclamation Mark                ja, tc 
    {0xFF04, false, true},  // FF04: Fullwidth Dollar Sign                     kr 
    {0xFF05, true,  false}, // FF05: Fullwidth Percent Sign                    kr 
    {0xFF08, false, true},  // FF08: Fullwidth Left Parenthesis                ja, tc 
    {0xFF09, true,  false}, // FF09: Fullwidth Right Parenthesis               ja, tc 
    {0xFF0C, true,  false}, // FF0C: Fullwidth Comma                           ja, tc, kr 
    {0xFF0E, true,  false}, // FF0E: Fullwidth Full Stop                       ja, tc, kr 
    {0xFF1A, true,  false}, // FF1A: Fullwidth Colon                           ja, tc, kr 
    {0xFF1B, true,  false}, // FF1B: Fullwidth Semicolon                       ja, tc, kr 
    {0xFF1F, true,  false}, // FF1F: Fullwidth Quation Mark                    ja, tc 
    {0xFF20, false, true},  // FF20: Fullwidth Commercial At                   ja 
    {0xFF3B, false, true},  // FF3B: Fullwidth Left Square Bracket             kr 
    {0xFF3D, true,  false}, // FF3D: Fullwidth Right Square Bracket            kr 
    {0xFF5B, false, true},  // FF5B: Fullwidth Left Curly Bracket              ja, tc 
    {0xFF5D, true,  false}, // FF5D: Fullwidth Right Curly Bracket             kr 
    {0xFFE0, true,  true},  // FFE0: Fullwidth Cent Sign                       ja, kr 
    {0xFFE1, false, true},  // FFE1: Fullwidth Pound Sign                      fr, kr 
    {0xFFE5, false, true},  // FFE5: Fullwidth Yen Sign                        ja 
    {0xFFE6, false, true}   // FFE6: Fullwidth Won Sign                        kr 
};

const INT       g_iNumBreakCharacters = COUNTOF(BreakArray);

//--------------------------------------------------------------------------------------
// globals
//--------------------------------------------------------------------------------------

CB_GetWidthW    GetWidthW = NULL;
CB_Reserved     Reserved = NULL;

INT             g_iOption = 0;

//--------------------------------------------------------------------------------------
// Name: WordWrap_SetCallback
//--------------------------------------------------------------------------------------
void WordWrap_SetCallback( CB_GetWidthW pGetWidthW, CB_Reserved pReserved )
{
    if( pGetWidthW )
    {
        GetWidthW = pGetWidthW;
    }
    if( pReserved )
    {
        Reserved = pReserved;
    }
}

//--------------------------------------------------------------------------------------
// Name: WordWrap_SetOption
//--------------------------------------------------------------------------------------
void WordWrap_SetOption( INT iOption )
{
    g_iOption = iOption;
}

//--------------------------------------------------------------------------------------
// Name: IsNonBeginningChar
//--------------------------------------------------------------------------------------
static bool IsNonBeginningChar( WCHAR c )
{
    if( !( g_iOption & WW_PROHIBITION ) )
        return FALSE;

    INT iLeft = 0;
    INT iRight = g_iNumBreakCharacters;
    INT iMid = 0;

    // Binary search through the array of break characters
    while( iLeft <= iRight )
    {
        iMid = ( ( iRight - iLeft ) / 2 ) + iLeft;
        if( BreakArray[iMid].wch == c )
        {
            return BreakArray[iMid].isNonBeginningChar;
        }
        if( c < BreakArray[iMid].wch )
        {
            iRight = iMid - 1;
        }
        else
        {
            iLeft = iMid + 1;
        }

    }
    return false;
}

//--------------------------------------------------------------------------------------
// Name: IsNonEndingChar
//--------------------------------------------------------------------------------------
static bool IsNonEndingChar( WCHAR c )
{
    if( !( g_iOption & WW_PROHIBITION ) )
        return FALSE;

    INT iLeft = 0;
    INT iRight = g_iNumBreakCharacters;
    INT iMid = 0;

    // Binary search through the array of break characters
    while( iLeft <= iRight )
    {
        iMid = ( ( iRight - iLeft ) / 2 ) + iLeft;
        if( BreakArray[iMid].wch == c )
        {
            return BreakArray[iMid].isNonEndingChar;
        }
        if( c < BreakArray[iMid].wch )
        {
            iRight = iMid - 1;
        }
        else
        {
            iLeft = iMid + 1;
        }

    }
    return false;
}

//--------------------------------------------------------------------------------------
// Name: IsEastAsianChar
//--------------------------------------------------------------------------------------
static bool IsEastAsianChar( WCHAR c )
{
    if( g_iOption & WW_NOHANGULWRAP )
    {
        if( ( 0x1100 <= c ) && ( c <= 0x11FF ) ||       // Hangul Jamo
            ( 0x3130 <= c ) && ( c <= 0x318F ) ||       // Hangul Compatibility Jamo
            ( 0xAC00 <= c ) && ( c <= 0xD7A3 ) )         // Hangul Syllables
            return FALSE;
    }

    return( ( 0x1100 <= c ) && ( c <= 0x11FF ) ||       // Hangul Jamo
            ( 0x3000 <= c ) && ( c <= 0xD7AF ) ||       // CJK symbols - Hangul Syllables
            ( 0xF900 <= c ) && ( c <= 0xFAFF ) ||       // CJK compat
            ( 0xFF00 <= c ) && ( c <= 0xFFDC ) );        // Halfwidth / Fullwidth
}
//--------------------------------------------------------------------------------------
// Name: WordWrap_CanBreakLineAt
//--------------------------------------------------------------------------------------
bool WordWrap_CanBreakLineAt( LPCWSTR pwsz, LPCWSTR pwszStart )
{
    if( pwsz == pwszStart )
    {
        return false;   // leave at least one character in a line
    }
    return ( ( WordWrap_IsWhiteSpace( *pwsz ) || IsEastAsianChar( *pwsz ) ||
               IsEastAsianChar( pwsz[-1] ) || pwsz[-1] == L'-' ) )
        && !IsNonBeginningChar( *pwsz ) && !IsNonEndingChar( pwsz[-1] );
}

//--------------------------------------------------------------------------------------
// Name: WordWrap_FindNonWhiteSpaceForward
//--------------------------------------------------------------------------------------
const WCHAR* WordWrap_FindNonWhiteSpaceForward( const WCHAR* pwsz )
{
    while( WordWrap_IsWhiteSpace( *pwsz ) )
    {
        ++pwsz;
    }
    if( pwsz && WordWrap_IsLineFeed(*pwsz) )
    {
        ++pwsz;
    }

    return ( *pwsz ) ? pwsz : NULL;
}

//--------------------------------------------------------------------------------------
// Name: FindNonWhiteSpaceBackward
//--------------------------------------------------------------------------------------
static const WCHAR* FindNonWhiteSpaceBackward( const WCHAR* pwsz, const WCHAR* pwszSource )
{
    while( pwsz >= pwszSource && ( WordWrap_IsWhiteSpace( *pwsz ) || WordWrap_IsLineFeed( *pwsz ) ) )
    {
        pwsz--;
    }
    return ( pwsz < pwszSource ) ? NULL : pwsz;
}

//--------------------------------------------------------------------------------------
// Name: WordWrap_FindNextLine
// param:   pwszSource: (in) the line to break
//          iWidth: (in) width to wrap the line
//          ppwszEOL: (out) receive the pointer to the last non-white space char in the line. 
// return:  The beginning of next line. NULL if the line fits within the specified width.
//          *ppwszEOL will be NULL if the wrapped line consists of only white space chars, 
//
// Case / Expected
// - Line width less than a character width / The line will contain at least one character
// - All white spaces ||
// - All unbreakable character ||
// - All non-starting characters ||
// - All non-terminating characters / Break at the character that overflows the width
// - Zero length line / return NULL w/ EOL NULL
//--------------------------------------------------------------------------------------
const WCHAR* WordWrap_FindNextLine( const WCHAR* pwszSource, INT iWidth, const WCHAR** ppwszEOL )
{
    if( GetWidthW == NULL || pwszSource == NULL || !pwszSource[0] )
    {
        *ppwszEOL = NULL;
        return NULL;
    }

    const WCHAR* pwsz = pwszSource;
    INT iCurrent;

    for( iCurrent = 0; *pwsz && !WordWrap_IsLineFeed( *pwsz ); pwsz++ )
    {
        iCurrent += GetWidthW( *pwsz );
        if( iCurrent > iWidth )
        {
            break;
        }
    }
    if( pwszSource == pwsz )
    {
        // Break at second character if line width is less than character width
        *ppwszEOL = FindNonWhiteSpaceBackward( pwsz, pwszSource );
        return WordWrap_FindNonWhiteSpaceForward( pwsz + 1 );
    }
    if( WordWrap_IsLineFeed( *pwsz ) )
    {
        pwsz++;
    }
    if( iCurrent <= iWidth )
    {
        // The line is shorter than the width
        *ppwszEOL = FindNonWhiteSpaceBackward( pwsz - 1, pwszSource );
        if( pwsz - 1 >= pwszSource && WordWrap_IsLineFeed( *(pwsz-1) ) )
        {
            return pwsz;
        }
        return ( *pwsz ) ? WordWrap_FindNonWhiteSpaceForward( pwsz ) : NULL;
    }

    const WCHAR* pwszOverflow = pwsz;

    while( pwsz > pwszSource )
    {
        if( WordWrap_IsWhiteSpace( *pwsz ) )
        {
            *ppwszEOL = FindNonWhiteSpaceBackward( pwsz, pwszSource );
            if( !*ppwszEOL )
            {
                return WordWrap_FindNonWhiteSpaceForward( pwsz + 1 );
            }
            pwsz = *ppwszEOL + 1;
        }
        if( WordWrap_CanBreakLineAt( pwsz, pwszSource ) )
        {
            break;
        }
        --pwsz;
    }
    if( pwsz <= pwszSource )
    {
        // couldn't find any character to break the line
        *ppwszEOL = pwszOverflow - 1;
        return pwszOverflow;
    }
    *ppwszEOL = pwsz - 1;
    return WordWrap_FindNonWhiteSpaceForward( pwsz );
}
