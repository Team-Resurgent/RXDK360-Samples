//----------------------------------------------------------------------------------------------------------------------
// GrammarFileHelper.h
// 
// Utility file which helps locate demo speech grammars on the disk.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#ifndef GRAMMARFILEHELPER_H_GUARD
#define GRAMMARFILEHELPER_H_GUARD

// Default language to use (US English)
const NUI_SPEECH_LANGUAGE DEFAULTSPEECHLANGUAGE = NUI_SPEECH_LANGUAGE_EN_US;

//--------------------------------------------------------------------------------------
// Name: GrammarFileLangVariant
// Desc: A specific-language variant of a grammar file.
//--------------------------------------------------------------------------------------
struct GrammarFileLangVariant
{
    NUI_SPEECH_LANGUAGE language;
    std::string path;
    
    std::wstring LoadDescriptionFromFile();
};

typedef std::vector<GrammarFileLangVariant> GrammarFileVariantList;

//--------------------------------------------------------------------------------------
// Name: GrammarFile
// Desc: A collection of grammar files in different languages
//--------------------------------------------------------------------------------------
struct GrammarFile
{
    std::string simplename;
    GrammarFileVariantList variants;

    GrammarFileLangVariant* GetGrammarForLanguage( NUI_SPEECH_LANGUAGE language );
};

typedef std::vector<GrammarFile> GrammarFileList;

void FindGrammarFiles( GrammarFileList& grammarFileList );

const wchar_t* GrammarFileLanguageToString( NUI_SPEECH_LANGUAGE language );

std::wstring ANSItoWstr( const char* pstrAnsi );

#endif //GRAMMARFILEHELPER_H_GUARD