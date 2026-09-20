
//--------------------------------------------------------------------------------------
// TitleStorageRest.h
//
// Title managed storage for Xbox LIVE endpoint sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef TITLESTORAGEOBJECTS_H
#define TITLESTORAGEOBJECTS_H

//--------------------------------------------------------------------------------------
// INCLUDES
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <malloc.h>
#include <stdio.h>
#include "AtgInput.h"
#include "AtgUtil.h"
#include "AtgConsole.h"
#include "AtgHttp.h"
#include "AtgJson.h"
#include "ETagMap.h"

const INT NO_STRSIZE          =0;
const INT TITLEID_STRSIZE     =12;
const INT GAMERTAG_STRSIZE    =16;
const INT ETAG_SIZE           =32;
const INT ENDPOINT_KEY_SIZE   =35;
const INT GUID_STRSIZE        =40;
const INT DEFAULT_STRSIZE     =256;
const INT URL_STRSIZE         =384;
const INT MAX_RESPONSESIZE    =8192;

class TitleStorageRest;

//--------------------------------------------------------------------------------------
// PagingInfo
//--------------------------------------------------------------------------------------
class PagingInfo
{
public:

    CHAR continuationToken[DEFAULT_STRSIZE];
    INT totalItems;

    const CHAR* ToString();

private:

    friend TitleStorageRest;
    friend ATG::JSON::Collection<PagingInfo>;
    static ATG::JSON::PropertyTag tags[];
    static DWORD tagsize;
};


//--------------------------------------------------------------------------------------
// Blob
//--------------------------------------------------------------------------------------
class Blob
{
public:

    CHAR fileName[DEFAULT_STRSIZE];
    CHAR clientFileTime[DEFAULT_STRSIZE]; 
    CHAR displayName[DEFAULT_STRSIZE];
    CHAR etag[ETAG_SIZE];
    INT size; 

    const CHAR* ToString();

private:

    friend TitleStorageRest;
    friend ATG::JSON::Collection<Blob>;
    static ATG::JSON::PropertyTag tags[];
    static DWORD tagsize;
};


//--------------------------------------------------------------------------------------
// FileList
//--------------------------------------------------------------------------------------
class FileList
{
public:

    ATG::JSON::Collection<Blob> blobs;
    ATG::JSON::Collection<PagingInfo> pagingInfo;

    const CHAR* ToString();

private: 

    friend TitleStorageRest;
    static ATG::JSON::PropertyTag tags[];
    static DWORD tagsize;
};


//--------------------------------------------------------------------------------------
// JsonFile
//--------------------------------------------------------------------------------------
class JsonFile
{
public:

    INT fileNumber;
    CHAR message[DEFAULT_STRSIZE];

    const CHAR* ToString();

private: 

    friend TitleStorageRest;
    static ATG::JSON::PropertyTag tags[];
    static DWORD tagsize;
};

//--------------------------------------------------------------------------------------
// QuotaInfo
//--------------------------------------------------------------------------------------
class QuotaInfo
{
public:

    INT usedBytesInVerifiedStorage;
    INT quotaBytesForVerifiedStorage;
    INT usedBytesInUnverifiedStorage;
    INT quotaBytesForUnverifiedStorage;

    const CHAR* ToString();

private: 

    friend TitleStorageRest;
    static ATG::JSON::PropertyTag tags[];
    static DWORD tagsize;
};

//--------------------------------------------------------------------------------------
// ContinuationToken
//--------------------------------------------------------------------------------------
class ContinuationToken
{
public:

    CHAR continuationToken[DEFAULT_STRSIZE];

private: 

    friend TitleStorageRest;
    static ATG::JSON::PropertyTag tags[];
    static DWORD tagsize;
};

//--------------------------------------------------------------------------------------
// MessageSelect
//--------------------------------------------------------------------------------------
class MessageSelect
{
public:

    CHAR message[DEFAULT_STRSIZE];

private: 

    friend TitleStorageRest;
    static ATG::JSON::PropertyTag tags[];
    static DWORD tagsize;
};

//--------------------------------------------------------------------------------------
// HttpResponse
//--------------------------------------------------------------------------------------
class HttpResponse
{
public:

    DWORD returnCode;
    CHAR buffer[MAX_RESPONSESIZE];
};


//--------------------------------------------------------------------------------------
// UsersMe
//--------------------------------------------------------------------------------------
class UsersMe
{
public:

    XUID xuid;
    CHAR gamerTag[GAMERTAG_STRSIZE]; // string sizes are put into tags

private:

    friend TitleStorageRest;
    static ATG::JSON::PropertyTag tags[];
    static DWORD tagsize;
};


//--------------------------------------------------------------------------------------
// TitleStorageRest
//--------------------------------------------------------------------------------------
class TitleStorageRest 
{
public:

    TitleStorageRest() {};

    HRESULT Initialize(CHAR *groupId, DWORD dwUserIndex);
    HRESULT Shutdown();

    template<typename Type>
    Type* MakeWebRequest(CHAR *customHeader, CHAR *url, CHAR *verb, CHAR *data, Type *p, CHAR *ETag, HttpResponse &response);
    VOID MakeWebRequest(CHAR *customHeader, CHAR *url, CHAR *verb, CHAR *data, CHAR *ETag, HttpResponse &response);

private:

    ATG::HTTP::AuthManager  m_Auth;
    HANDLE                  m_hWorkerThread;
    CHAR                    m_groupId[GUID_STRSIZE];
};

//--------------------------------------------------------------------------------------
// TitleStorageRest::MakeWebRequest<>
//
// shared function used in each wrapper based on the custom JSON type we are working
// with
//--------------------------------------------------------------------------------------
template<typename Type> 
Type* TitleStorageRest::MakeWebRequest(CHAR *customHeader, CHAR *url, CHAR *verb, CHAR *data, Type *returnedType, CHAR *ETag, HttpResponse &response)
{
    ATG::HTTP::AuthManager::AuthEndpoint *endpoint;
    endpoint = m_Auth.CreateEndpoint(url, TRUE);

    DWORD len = 0;
    if (data != NULL)
    {
        len = strlen(data);
    }

    // open up a request to the endpoint 
    HRESULT hr = endpoint->MakeSyncRequest(verb, customHeader, data, len);
    if (hr != S_OK)
    {
        ATG::FatalError("Failed to OpenRequest: %x\n", hr);
    }

    returnedType = NULL;

    response.returnCode = endpoint->GetHTTPStatusCode();
    strcpy_s<MAX_RESPONSESIZE>(response.buffer, reinterpret_cast<const char*>(endpoint->GetReadBuffer()));
    if (response.returnCode >= HTTP_STATUS_OK && response.returnCode < HTTP_STATUS_AMBIGUOUS) 
    {
        if(ETag != NULL)
        {
            CONST CHAR* buffer = endpoint->GetETagHeader();
            if(buffer == NULL)
            {
                ETag = NULL;
            }
            else
            {
                strcpy_s(ETag, ETAG_SIZE, buffer);
            }
        }

        returnedType = new Type();
        ATG::JSON::AtgJsonReader::Parse(endpoint->GetReadBuffer(), returnedType, returnedType->tags, returnedType->tagsize);
    }
    
    endpoint->CloseRequest();

    m_Auth.RemoveEndpoint(endpoint);

    return returnedType;
}

#endif