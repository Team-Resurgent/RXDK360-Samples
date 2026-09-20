//--------------------------------------------------------------------------------------
// TitleStorageRest.cpp
//
// Title managed storage for Xbox LIVE endpoint sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "TitleStorageRest.h"

//--------------------------------------------------------------------------------------
// GLOBALS
//--------------------------------------------------------------------------------------
CHAR responseString[MAX_RESPONSESIZE];

//--------------------------------------------------------------------------------------
// TitleStorageRest::Initialize
//
//--------------------------------------------------------------------------------------
HRESULT TitleStorageRest::Initialize(CHAR *groupId, DWORD dwUserIndex) 
{
    HRESULT hr = m_Auth.Startup(NULL, FALSE, &m_hWorkerThread, TRUE, dwUserIndex);
    if(hr != S_OK)
    {
        ATG::FatalError("Failed to Initialize: %x\n", hr);
    }

    if(groupId != NULL)
    {
        strcpy_s(m_groupId, groupId);
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// TitleStorageRest::Shutdown
//
//--------------------------------------------------------------------------------------
HRESULT TitleStorageRest::Shutdown()
{
    m_Auth.Shutdown(); 
    return S_OK;
}


//--------------------------------------------------------------------------------------
// TitleStorageRest::MakeWebRequest
//
// shared function used in each wrapper based on the custom JSON type we are working
// with
//--------------------------------------------------------------------------------------
VOID TitleStorageRest::MakeWebRequest(CHAR *customHeader, CHAR *url, CHAR* verb, CHAR *data, CHAR* ETag, HttpResponse &response)
{
    ATG::HTTP::AuthManager::AuthEndpoint *endpoint;
    endpoint = m_Auth.CreateEndpoint(url, TRUE);

    DWORD len = 0;
    if(data != NULL)
    {
        len = strlen(data);
    }

    // open up a request to the endpoint 
    HRESULT hr = endpoint->MakeSyncRequest(verb, customHeader, data, len);
    if(hr != S_OK)
    {
        ATG::FatalError("Failed to OpenRequest: %x\n", hr);
    }

    response.returnCode = endpoint->GetHTTPStatusCode();
    strcpy_s<MAX_RESPONSESIZE>(response.buffer, reinterpret_cast<const char*>(endpoint->GetReadBuffer()));
    if(ETag != NULL && response.returnCode >= HTTP_STATUS_OK && response.returnCode< HTTP_STATUS_AMBIGUOUS)
    {
        CONST CHAR* buffer = endpoint->GetETagHeader();
        if(buffer == NULL)
        {
            memset(ETag, 0, ETAG_SIZE);
        }
        else
        {
            strcpy_s(ETag, ETAG_SIZE, buffer);
        }
    }

    endpoint->CloseRequest();

    m_Auth.RemoveEndpoint(endpoint);
}


//--------------------------------------------------------------------------------------
// PagingInfo
//--------------------------------------------------------------------------------------
ATG::JSON::PropertyTag PagingInfo::tags[] = {
    L"continuationToken",   ATG::JSON::DT_STRING,   offsetof(PagingInfo, continuationToken ),     DEFAULT_STRSIZE,
    L"totalItems",          ATG::JSON::DT_INT,      offsetof(PagingInfo, totalItems),            NO_STRSIZE,
};
DWORD PagingInfo::tagsize = ARRAY_SIZE(PagingInfo::tags);

const CHAR* PagingInfo::ToString()
{
    sprintf_s(responseString, "continuationToken:%s\ntotalFiles:%d", continuationToken, totalItems);
    return responseString;
}


//--------------------------------------------------------------------------------------
// Blob
//--------------------------------------------------------------------------------------
ATG::JSON::PropertyTag Blob::tags[] = {
    L"fileName",        ATG::JSON::DT_STRING,     offsetof(Blob, fileName),        DEFAULT_STRSIZE,
    L"clientFileTime",  ATG::JSON::DT_STRING,     offsetof(Blob, clientFileTime),  DEFAULT_STRSIZE,
    L"displayName",     ATG::JSON::DT_STRING,     offsetof(Blob, displayName),     DEFAULT_STRSIZE,
    L"etag",            ATG::JSON::DT_STRING,     offsetof(Blob, etag),            ETAG_SIZE,
    L"size",            ATG::JSON::DT_INT,        offsetof(Blob, size),            NO_STRSIZE
};
DWORD Blob::tagsize = ARRAY_SIZE(Blob::tags);

const CHAR* Blob::ToString()
{
    sprintf_s(
        responseString,
        "Blob: %s\n--displayName:%s\n--clientFileTime:%s\n--etag:%s\n--size:%d\n",
        fileName, 
        displayName, 
        clientFileTime, 
        etag, 
        size);

    return responseString;
}


//--------------------------------------------------------------------------------------
// FileList
//--------------------------------------------------------------------------------------
ATG::JSON::PropertyTag FileList::tags[] = {
    L"blobs",       ATG::JSON::DT_ARRAY,   offsetof(FileList, blobs),      NO_STRSIZE,
    L"pagingInfo",  ATG::JSON::DT_ARRAY,   offsetof(FileList, pagingInfo), NO_STRSIZE, 
};
DWORD FileList::tagsize = ARRAY_SIZE(FileList::tags);

const CHAR* FileList::ToString()
{
    std::string str("Blobs\n");

    for(int i = 0; i < blobs.GetArraySize(); ++i)
    {
        str.append(blobs[i]->ToString());
    }

    str.append("\n");

    for(int i = 0; i < pagingInfo.GetArraySize(); ++i)
    {
        str.append(pagingInfo[i]->ToString());
    }

    sprintf_s(
        responseString,
        "%s\n",
        str.c_str());

    return responseString;
}


//--------------------------------------------------------------------------------------
// JsonFile
//--------------------------------------------------------------------------------------
ATG::JSON::PropertyTag JsonFile::tags[] = {
    L"fileNumber",  ATG::JSON::DT_INT,     offsetof(JsonFile, fileNumber),  NO_STRSIZE,
    L"message",     ATG::JSON::DT_STRING,  offsetof(JsonFile, message),     DEFAULT_STRSIZE,
};
DWORD JsonFile::tagsize = ARRAY_SIZE(JsonFile::tags);

const CHAR* JsonFile::ToString()
{
    sprintf_s(responseString, "FileNumber:%d\nMessage:%s", fileNumber, message);
    return responseString;
}

//--------------------------------------------------------------------------------------
// QuotaInfo
//--------------------------------------------------------------------------------------
ATG::JSON::PropertyTag QuotaInfo::tags[] = {
    L"UsedBytesInVerifiedStorage",      ATG::JSON::DT_INT,  offsetof(QuotaInfo, usedBytesInVerifiedStorage),    NO_STRSIZE,
    L"QuotaBytesForVerifiedStorage",    ATG::JSON::DT_INT,  offsetof(QuotaInfo, quotaBytesForVerifiedStorage),  NO_STRSIZE,
    L"UsedBytesInUnverifiedStorage",    ATG::JSON::DT_INT,  offsetof(QuotaInfo, usedBytesInUnverifiedStorage),  NO_STRSIZE,
    L"QuotaBytesForUnverifiedStorage",  ATG::JSON::DT_INT,  offsetof(QuotaInfo, quotaBytesForUnverifiedStorage),NO_STRSIZE,
};
DWORD QuotaInfo::tagsize = ARRAY_SIZE(QuotaInfo::tags);

const CHAR* QuotaInfo::ToString()
{
    sprintf_s(
        responseString, 
        "TitleQuotaInfo:\n--usedBytesInVerifiedStorage:%d\n--quotaBytesForVerifiedStorage:%d\n--usedBytesInUnverifiedStorage:%d\n--quotaBytesForUnverifiedStorage:%d\n",
        usedBytesInVerifiedStorage,
        quotaBytesForVerifiedStorage,
        usedBytesInUnverifiedStorage,
        quotaBytesForUnverifiedStorage);

    return responseString;
}


//--------------------------------------------------------------------------------------
// ContinuationToken
//--------------------------------------------------------------------------------------
ATG::JSON::PropertyTag ContinuationToken::tags[] = {
    L"continuationToken",   ATG::JSON::DT_STRING,   offsetof(PagingInfo, continuationToken),     DEFAULT_STRSIZE,
};
DWORD ContinuationToken::tagsize = ARRAY_SIZE(ContinuationToken::tags);


//--------------------------------------------------------------------------------------
// MessageSelect
//--------------------------------------------------------------------------------------
ATG::JSON::PropertyTag MessageSelect::tags[] = {
    L"message",   ATG::JSON::DT_STRING,   offsetof(PagingInfo, continuationToken),     DEFAULT_STRSIZE,
};
DWORD MessageSelect::tagsize = ARRAY_SIZE(MessageSelect::tags);



//--------------------------------------------------------------------------------------
// UsersMe
//--------------------------------------------------------------------------------------
ATG::JSON::PropertyTag UsersMe::tags[] = {
    L"xuid",        ATG::JSON::DT_INT64,   offsetof(UsersMe, xuid),     NO_STRSIZE,
    L"gamerTag",    ATG::JSON::DT_STRING,  offsetof(UsersMe, gamerTag), GAMERTAG_STRSIZE,
};
DWORD UsersMe::tagsize = ARRAY_SIZE(UsersMe::tags);

