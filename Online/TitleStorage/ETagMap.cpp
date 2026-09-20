//--------------------------------------------------------------------------------------
// ETagMap.ppp
//
// Map of ETag and filenames
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// INCLUDES
//--------------------------------------------------------------------------------------
#include "ETagMap.h"
#include "TitleStorageRest.h"

//--------------------------------------------------------------------------------------
// ETagMap::AddOrUpdateETag
//
// Adds the ETag or updates the ETag we already have
//--------------------------------------------------------------------------------------
VOID ETagMap::AddOrUpdateETag(CONST CHAR* filename, CONST CHAR* ETag)
{
    m_ETags[filename] = ETag;
}

//--------------------------------------------------------------------------------------
// ETagMap::GetETag
//
// Return the ETag associated with the filename
//--------------------------------------------------------------------------------------
BOOL ETagMap::GetETag(CONST CHAR* filename, CHAR* ETag)
{
    ETagMapIter iter = m_ETags.find(filename);
    if(iter == m_ETags.end())
    {
        memset(ETag, 0, ETAG_SIZE);
        return FALSE;
    }

    sprintf_s(ETag, ETAG_SIZE, "%s", iter->second);
    return TRUE;
}

//--------------------------------------------------------------------------------------
// ETagMap::Compare
//
// Compares supplied ETag with the one associated with the filename, true if Equal.
//--------------------------------------------------------------------------------------
BOOL ETagMap::Compare(CONST CHAR* filename, CONST CHAR* ETag)
{
    ETagMapIter iter = m_ETags.find(filename);
    if(iter == m_ETags.end())
    {
        return FALSE;
    }

    return strncmp(ETag, iter->second, ETAG_SIZE) == 0;
}