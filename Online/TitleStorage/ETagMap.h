//--------------------------------------------------------------------------------------
// ETagMap.h
//
// Map of ETag and filenames
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef ETAGMAP_H
#define ETAGMAP_H

//--------------------------------------------------------------------------------------
// INCLUDES
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <map>
#include <string>

//--------------------------------------------------------------------------------------
// ETagMap
//--------------------------------------------------------------------------------------
class ETagMap
{
public:
    
    VOID AddOrUpdateETag(CONST CHAR* filename, CONST CHAR* ETag);
    BOOL GetETag(CONST CHAR* filename, CHAR* ETag);
    BOOL Compare(CONST CHAR* filename, CONST CHAR* ETag);

private: 
    typedef std::pair<CONST CHAR*, CONST CHAR*> ETagMapPair;
    typedef std::map<CONST CHAR*, CONST CHAR*> ETagMapType;
    typedef std::map<CONST CHAR*, CONST CHAR*>::iterator ETagMapIter;

    ETagMapType m_ETags;

};

#endif