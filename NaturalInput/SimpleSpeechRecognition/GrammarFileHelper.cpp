//--------------------------------------------------------------------------------------
// GrammarFileHelper.cpp
// 
// Implementation of helper functions to find grammar files.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <NuiApi.h>
#include <algorithm>
#include "AtgUtil.h"
#include "GrammarFileHelper.h"
#include <iostream>
#include <fstream>
#include <malloc.h>

// Where compiled grammar files are stored
#define GRAMMAR_LOCATION "game:\\Media\\Grammars\\"

// File extension used for them.
#define GRAMMAR_FILEEXTENSION "*.cfg"

// The file extension to use for descriptions of the grammars
#define DESCRIPTION_FILE_EXT ".grtxt"

// File extension length including the .
const size_t GRAMMAR_FILE_EXT_LEN = 4;  

// File language tag length
const size_t LANGUAGE_TAG_LEN = 5;

// Path to look in for grammar files
#define GRAMMAR_SEARCHPATH GRAMMAR_LOCATION GRAMMAR_FILEEXTENSION

//----------------------------------------------------------------------------------------------------------------------
// Language text-string to NUI Speech language value mapping.
//----------------------------------------------------------------------------------------------------------------------
struct LANGUAGETAG
{
    const char* pstrTagName;
    const wchar_t* pstrTagFriendlyName;
    NUI_SPEECH_LANGUAGE langID;
};

LANGUAGETAG s_languages[] = {
    { "en_us", L"US English",           NUI_SPEECH_LANGUAGE_EN_US },
    { "fr_ca", L"French Canadian",      NUI_SPEECH_LANGUAGE_FR_CA },	
    { "en_gb", L"British English",      NUI_SPEECH_LANGUAGE_EN_GB },
    { "es_mx", L"Mexican Spanish",      NUI_SPEECH_LANGUAGE_ES_MX },
    { "ja_jp", L"Japanese",             NUI_SPEECH_LANGUAGE_JA_JP },
    { "fr_fr", L"French French",        NUI_SPEECH_LANGUAGE_FR_FR },
    { "es_es", L"Spanish Spanish",      NUI_SPEECH_LANGUAGE_ES_ES },
	{ "de_de", L"German German",        NUI_SPEECH_LANGUAGE_DE_DE },
    { "it_it", L"Italian",              NUI_SPEECH_LANGUAGE_IT_IT },
    { "en_au", L"Australian English",   NUI_SPEECH_LANGUAGE_EN_AU },
    { "pt_br", L"Brazilian Portuguese", NUI_SPEECH_LANGUAGE_PT_BR },
    { "de_at", L"Austrian German",      NUI_SPEECH_LANGUAGE_DE_AT },
	{ "de_ch", L"Swiss German",         NUI_SPEECH_LANGUAGE_DE_CH },
	{ "fr_ch", L"Swiss French",         NUI_SPEECH_LANGUAGE_FR_CH }
};

GrammarFile& FindOrCreateContainer( const std::string& simplename, GrammarFileList& list )
{
    for ( GrammarFileList::iterator i = list.begin(); i != list.end(); ++i )
    {
        if ( strcmp( simplename.c_str(), (*i).simplename.c_str() ) == 0 )
        {
            return *i;
        }
    }
    
    GrammarFile gf;
    gf.simplename = simplename;
    list.push_back( gf );
    return list.back();
}

//----------------------------------------------------------------------------------------------------------------------
// Name: GetFilenameNoExtFromPath
// Desc: Gets the filename without an extension from the path provided.
//       Example: game:\Media\Grammars\Phonetic_Alphabet_en_us.cfg => Phonetic_Alphabet_en_us
//----------------------------------------------------------------------------------------------------------------------
std::string GetFilenameNoExtFromPath( const std::string& path )
{
    // Note: Special cased code - we know the length of the extension (otherwise we
    //       wouldn't have the filename here to process).

    size_t seploc = path.find_last_of( '\\' ) + 1;
    size_t newlen = path.length() - seploc - GRAMMAR_FILE_EXT_LEN;

    return path.substr( seploc, newlen );
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CrackSimpleName
// Desc: Pulls out the language of a grammar file from its full path.
//       Example: game:\Media\Grammars\Phonetic_Alphabet_en_us.cfg => NUI_LANGUAGE_EN_US
//----------------------------------------------------------------------------------------------------------------------
NUI_SPEECH_LANGUAGE CrackLanguage( const std::string& strFilenameNoExt )
{
    // all language codes are 5 characters (XX_XX).
    const char* pstrFname = strFilenameNoExt.c_str();
    pstrFname = pstrFname + strFilenameNoExt.length() - LANGUAGE_TAG_LEN;

    for ( size_t i = 0; i < ARRAYSIZE( s_languages ); ++i )
    {
        if ( strcmp( pstrFname, s_languages[ i ].pstrTagName ) == 0 )
        {
            return s_languages[ i ].langID;
        }
    }

    // Couldn't find a matching language... for now, just assume it's US english and spew an error.    
    ATG::DebugSpew( "Couldn't find a language match for file %s... assuming US English\n", strFilenameNoExt.c_str() );

    return NUI_SPEECH_LANGUAGE_EN_US;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: CrackSimpleName
// Desc: Pulls out just the simple name of a grammar file from its full path.
//       Example: game:\Media\Grammars\Phonetic_Alphabet_en_us.cfg => "Phonetic Alphabet"
//----------------------------------------------------------------------------------------------------------------------
std::string CrackSimpleName( const std::string& strFilenameNoExt )
{
    // We know that our grammars end in _XX_XX (where XX_XX is the language specifier). So just
    // do some cleanup on the filename.

    // Strip the last 6 characters
    std::string strName = strFilenameNoExt.substr( 0, strFilenameNoExt.length() - ( LANGUAGE_TAG_LEN + 1 ) );

    // Replace '_' with " " for display in the UI.
    replace( strName.begin(), strName.end(), '_', ' ' );
    
    return strName;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: FindGrammarFiles
// Desc: Builds a list of grammar files on the system.
//----------------------------------------------------------------------------------------------------------------------
void FindGrammarFiles( GrammarFileList& grammarFileList )
{
    WIN32_FIND_DATA wfd;
    HANDLE hFind = FindFirstFile( GRAMMAR_SEARCHPATH, &wfd );

    grammarFileList.clear();

    if ( hFind == INVALID_HANDLE_VALUE ) 
    {
        ATG::FatalError( "Couldn't search for grammar files in location %s\n", GRAMMAR_SEARCHPATH );
    }

    do 
    {
        GrammarFileLangVariant gf;
        std::string path = GRAMMAR_LOCATION;
        path += wfd.cFileName;
        gf.path = path;

        std::string filenameNoExt = GetFilenameNoExtFromPath( path );
        gf.language = CrackLanguage( filenameNoExt );
        std::string simplename = CrackSimpleName( filenameNoExt );
        GrammarFile& gfContainer = FindOrCreateContainer( simplename, grammarFileList );
        gfContainer.variants.push_back( gf );

    } while ( FindNextFile( hFind, &wfd ) );

    FindClose( hFind );
}

//----------------------------------------------------------------------------------------------------------------------
// Name: GrammarFileLanguageToString
// Desc: 
//----------------------------------------------------------------------------------------------------------------------
const wchar_t* GrammarFileLanguageToString( NUI_SPEECH_LANGUAGE language )
{
    //Note: Order of items in s_languages must be the same as items in NUI_SPEECH_LANGUAGE enum.

    return s_languages[ language ].pstrTagFriendlyName;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: GrammarFile::GetGrammarForLanguage
// Desc: Returns a pointer to a grammar file variant (if one exists) for the specified language. If there is no variant
//       for that language, NULL will be returned.
//----------------------------------------------------------------------------------------------------------------------
GrammarFileLangVariant* GrammarFile::GetGrammarForLanguage( NUI_SPEECH_LANGUAGE language )
{
    for( GrammarFileVariantList::iterator i = variants.begin() ; i != variants.end(); ++i )
    {
        if ( i->language == language )
            return &(*i);
    }

    return NULL;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: GrammarFileLangVariant::LoadDescriptionFromFile
// Desc: Attempts to load a description for this grammar file from the media as a string.
//----------------------------------------------------------------------------------------------------------------------
std::wstring GrammarFileLangVariant::LoadDescriptionFromFile()
{
    // hack the path to add the description
    std::string path2 = path.substr( 0, path.length() - GRAMMAR_FILE_EXT_LEN ) + DESCRIPTION_FILE_EXT;

    // Read the entire file into memory.

    HANDLE hFile = CreateFile( path2.c_str(), FILE_GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN, NULL );

    // Failed to open file?
    if ( hFile == INVALID_HANDLE_VALUE )
    {
        ATG::DebugSpew( "Error - couldn't open description file %s\n", path2.c_str() );
        return L"";
    }

    DWORD dwFileSize = GetFileSize( hFile, NULL );

    const DWORD MIN_FILE_SIZE = 4;  // The minimum file size we care about; 1 wide char + BOM = 4 bytes.

    // Is it empty?
    if ( dwFileSize == 0 )
    {
        ATG::DebugSpew( "Error - empty description file %s\n", path2.c_str() );
        CloseHandle( hFile );
        return L"";
    }
    else if ( ( dwFileSize & 1 ) == 1 )
    {
        ATG::DebugSpew( "Description file %s had an odd # of chars in it - is it really UCS2?\n", path2.c_str() );
        CloseHandle( hFile );
        return L"";
    }
    else if ( dwFileSize < MIN_FILE_SIZE )
    {
        ATG::DebugSpew( "Description file %s is too small to be useful.\n", path2.c_str() );
        CloseHandle( hFile );
        return L"";
    }

    // Get enough space to read the file.
    WCHAR* pBuffer = (WCHAR*)malloc( dwFileSize );
    if ( !pBuffer )
    {
        ATG::FatalError( "Out of memory reading description file.\n" );
    }

    // Perform a blocking read, and read the entire file.
    DWORD dwBytesRead;
    if ( !ReadFile( hFile, pBuffer, dwFileSize, &dwBytesRead, NULL ) )
    {
        ATG::DebugSpew( "Error reading description text from file %s\n", path2.c_str() );
        free( pBuffer );
        CloseHandle( hFile );
        return L"";
    }

    // Done with the file, so close it.
    CloseHandle( hFile );

    DWORD dwCharCount = dwFileSize >> 1;

    // Now parse the text. We start by looking for a BOM - if any.
    
    WCHAR byteOrderMark = pBuffer[0];

    const EXPECTED_BOM = 0xFEFF;            // The byte order mark we expect for Xbox-native Unicode
    const ENDIANSWAPPED_BOM = 0xFFFE;       // Endian-swapped byte order mark.

    if ( byteOrderMark == ENDIANSWAPPED_BOM )
    {
        // We should be saving out in native-format unicode, but as this is not performance-critical code, it's easier
        // to just handle it and be a little more robust.

        ATG::DebugSpew( "Endian-swapped BOM found in file %s. Fixing data.\n", path2.c_str() );
        for ( DWORD dwIndex = 0; dwIndex < dwCharCount; ++dwIndex )
        {
            WCHAR c = pBuffer[ dwIndex ];
            c = (c >> 8) | (c << 8);
            pBuffer[ dwIndex ] = c;
        }
    }
    else if ( byteOrderMark != EXPECTED_BOM )
    {
        ATG::DebugSpew( "Missing BOM in file %s (1st char is 0x%X) \n", path2.c_str(), (DWORD)byteOrderMark );
    }

    const WCHAR UNICODE_LINESEPARATOR = 0x2028;
    const WCHAR UNICODE_CARRIAGERETURN = 0x000D;
    const WCHAR UNICODE_LINEFEED = 0x000A;


    DWORD dwStart = 1;
    DWORD dwCurrent = 1;

    std::wstring description;

    while ( dwCurrent < dwCharCount )
    {
        switch ( pBuffer[ dwCurrent ] )
        {

        case UNICODE_CARRIAGERETURN:
            {
                if ( dwCurrent - dwStart > 0 )
                {
                    description.append( &pBuffer[ dwStart ], dwCurrent - dwStart);
                }

                description.append( L"\n");

                // Is the next char a linefeed? If so, we should eat that too.
                if ( dwCurrent + 1  < dwCharCount && pBuffer[ dwCurrent + 1 ] == UNICODE_LINEFEED )
                {
                    dwCurrent += 2;
                }
                else
                {
                    ++dwCurrent;
                }

                dwStart = dwCurrent;
                break;
            }

        case UNICODE_LINEFEED:
        case UNICODE_LINESEPARATOR:
            {
                if ( dwCurrent - dwStart > 0 )
                {
                    description.append( &pBuffer[ dwStart ], dwCurrent - dwStart);
                }

                description.append( L"\n" );

                ++dwCurrent;
                dwStart = dwCurrent;
                break;
            }

        default:
            {
                ++dwCurrent;
            }
        }
    }

    // Grab the last line...
    if ( dwStart != dwCharCount )
    {
        description.append( &pBuffer[ dwStart ], dwCharCount - dwStart );
        description.append( L"\n" );
    }


    free( pBuffer );

    return description;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: ANSItoWstr
// Desc: Converts an ANSI string to a std::wstring
//----------------------------------------------------------------------------------------------------------------------
std::wstring ANSItoWstr( const char* pstrAnsi )
{
    const size_t SCRATCH_SIZE = 1024;
    wchar_t temp[ SCRATCH_SIZE ];

    size_t convCount;
    mbstowcs_s( &convCount, temp, SCRATCH_SIZE, pstrAnsi, strlen( pstrAnsi ) );

    return std::wstring( temp );
}
