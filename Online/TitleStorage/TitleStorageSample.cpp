//--------------------------------------------------------------------------------------
// TitleStorageSample.cpp
//
// Simple sample to demonstrate title storage calls using the ATGFramework
// uses the representation class for a JSON object and displays it
// 
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// INCLUDES
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <malloc.h>
#include <stdio.h>
#include <queue>
#include <time.h>
#include <xgetserviceendpoint.h>

#include "AtgInput.h"
#include "AtgUtil.h"
#include "AtgConsole.h"
#include "AtgHttp.h"
#include "AtgSignIn.h"
#include "TitleStorage.spa.h"
#include "TitleStorageRest.h"

#define GROUP_ID "5A712B94-7CAF-4840-A78E-AA8E4F4ED9EC"
#define MakeTitleURL "/media/titlegroups/" ## GROUP_ID ## "/storage/data/%s"
#define MakeTitleQuotaURL "/media/titlegroups/" ## GROUP_ID ## "/storage"
#define MakeUserURL "/users/xuid(%llu)/storage/titlestorage/titlegroups/" ## GROUP_ID ## "/data/%s"
#define MakeUserQuotaURL "/users/xuid(%llu)/storage/titlestorage/titlegroups/" ## GROUP_ID

//--------------------------------------------------------------------------------------
// GLOBALS
//--------------------------------------------------------------------------------------
const CHAR TEST_JSON_FILE[] = "path1/testfile2.txt,json";
const CHAR TEST_BINARY_FILE[] = "path1/testfile1.txt,binary";
const CHAR TEST_UNVERIFIED_BINARY_FILE[] = "unverified/path1/testfile1.txt,binary";
const CHAR TEST_MULTIBLOCK_FILE[] = "path2/testfile3.txt,binary";
const CHAR TEST_CONFIG_FILE[]= "awesomeQuotes.config,config";
CONST CHAR RANGE_HEADER[] = "Range: bytes=%d-%d\r\n";
CONST CHAR IF_NONE_MATCH_HEADER[] = "If-None-Match: %s\r\n";
CONST CHAR IF_MATCH_HEADER[] = "If-Match: %s \r\n";
CONST CHAR DEFAULT_HEADER[] = "Content-Type: application/json\r\nx-xbl-contract-version: 1\r\n%s";
CHAR *TestUserBinaryFileContents = "This is user test file 1. \"Differences of habit and language are nothing at all if our aims are identical and our hearts are open.\"";
CHAR *TestUserJsonFileContents = "{\n  \"fileNumber\":\"2\",\n  \"message\":\"It is important to fight and fight again, and keep fighting, for only then can evil be kept at bay though never quite eradicated.\"\n}";
CHAR *TestMultiBlockFileContentsBlock1 = "This file was uploaded by the Xbox\n";
CHAR *TestMultiBlockFileContentsBlock2 = "\"Surely you can't be serious.\"\n";
CHAR *TestMultiBlockFileContentsBlock3 = "\"I am serious... and don't call me Shirley.\"";
BOOL g_isRunning = TRUE;


//--------------------------------------------------------------------------------------
// class TitleStorageSample
//
// main class to run this application.
//--------------------------------------------------------------------------------------
class TitleStorageSample 
{
public:

    HRESULT Initialize();
    VOID    Uninitialize();
    HRESULT Update();

    //Title Quota
    VOID GetTitleQuota();

    //Title list files
    VOID ListTitleFilesAll();
    VOID ListTitleFilesSkipItems();
    VOID ListTitleFilesContinuationToken();
    VOID ListTitleFilesMaxItems();

    //Title get files
    VOID GetTitleFileBinary();
    VOID GetTitleFileJSON();
    VOID GetTitleFileConfig();
    VOID GetTitleFileCustomSelector();

    //User quota
    VOID GetUserQuota();

    //User list files
    VOID ListUserFilesAll();
    VOID ListUserFilesSkipItems();
    VOID ListUserFilesContinuationToken();
    VOID ListUserFilesMaxItems();

    //User get files
    VOID GetUserFileBinary();
    VOID GetUserFileJSON();
    VOID GetUserFileMultiBlock();
    VOID GetUserFileBinaryUnverified();
    VOID GetUserFileSelect();

    //User put files
    VOID PutUserFileBinary();
    VOID PutUserFileJSON();
    VOID PutUserFileMultiBlock();
    VOID PutUserFileBinaryUnverified();

    //User delete files
    VOID DeleteUserFileBinary();
    VOID DeleteUserFileJSON();
    VOID DeleteUserFileBinaryUnverified();
    VOID DeleteUserFileMultiBlock();

private:

    // Application state items
    enum APPSTATE
    {
        APPSTATE_INITIALIZE,
        APPSTATE_RUN,
    };

    APPSTATE    m_State;
    INT         m_TitleStorageCall;
    CHAR        m_baseUrl[URL_STRSIZE];
    CHAR        m_continuationToken[DEFAULT_STRSIZE];
    CHAR        m_Header[MAX_HEADER_LENGTH];
    CHAR        *m_eTag;

    ATG::Console       m_Console;
    TitleStorageRest   m_REST;
    UsersMe*           m_User;

    ETagMap            m_ETags;

    BOOL GetHeaderIfMatch(CONST CHAR* fileName, CHAR* header);
    BOOL GetHeaderIfNoneMatch(CONST CHAR* fileName, CHAR* header);
    VOID GetCurrentTimeString(CHAR* timeStr);
};

typedef void (TitleStorageSample::*TitleStorageAction)(void);
typedef std::pair<const CHAR*, TitleStorageAction> NameActionPair;
const NameActionPair TitleStorageActions[] =
{ 
    //Title Quota
    std::make_pair("GET_Title: Get the storage quota.", &TitleStorageSample::GetTitleQuota),

    //Title list files
    std::make_pair("GET_Title: List all files." , &TitleStorageSample::ListTitleFilesAll),
    std::make_pair("GET_Title: List files, skipping the first few.", &TitleStorageSample::ListTitleFilesSkipItems),
    std::make_pair("GET_Title: List files with a limit to the number returned.", &TitleStorageSample::ListTitleFilesMaxItems),
    std::make_pair("GET_Title: List files using the continuation token.", &TitleStorageSample::ListTitleFilesContinuationToken),

    //Title get files
    std::make_pair("GET_Title: Get a binary file.", &TitleStorageSample::GetTitleFileBinary),
    std::make_pair("GET_Title: Get a JSON file.", &TitleStorageSample::GetTitleFileJSON),
    std::make_pair("GET_Title: Get a config file.", &TitleStorageSample::GetTitleFileConfig),
    std::make_pair("GET_Title: Get a selection from a config file.", &TitleStorageSample::GetTitleFileCustomSelector),

    //User quota
    std::make_pair("GET_User: Get the storage quota.", &TitleStorageSample::GetUserQuota),

    //User list files
    std::make_pair("GET_User: List all files.", &TitleStorageSample::ListUserFilesAll),
    std::make_pair("GET_User: List files, skipping the first few.", &TitleStorageSample::ListUserFilesSkipItems),
    std::make_pair("GET_User: List files with a limit to the number returned.", &TitleStorageSample::ListUserFilesMaxItems),
    std::make_pair("GET_User: List files using the continuation token.", &TitleStorageSample::ListUserFilesContinuationToken),

    //User get files
    std::make_pair("GET_User: Get a binary file.", &TitleStorageSample::GetUserFileBinary),
    std::make_pair("GET_User: Get a JSON file.", &TitleStorageSample::GetUserFileJSON),
    std::make_pair("GET_User: Get a binary file using range headers (multi-block)", &TitleStorageSample::GetUserFileMultiBlock),
    std::make_pair("GET_User: Get a binary file from the unverified storage.", &TitleStorageSample::GetUserFileBinaryUnverified),
    std::make_pair("GET_User: Get a query response from the JSON file.", &TitleStorageSample::GetUserFileSelect),

    //User put files
    std::make_pair("PUT_User: Add a binary file.", &TitleStorageSample::PutUserFileBinary),
    std::make_pair("PUT_User: Add a JSON file.", &TitleStorageSample::PutUserFileJSON),
    std::make_pair("PUT_User: Add a binary file using multiple blocks.", &TitleStorageSample::PutUserFileMultiBlock),
    std::make_pair("PUT_User: Add a binary file to the unverified storage.", &TitleStorageSample::PutUserFileBinaryUnverified),

    //User delete files 
    std::make_pair("DELETE_User: Delete the binary file.", &TitleStorageSample::DeleteUserFileBinary),
    std::make_pair("DELETE_User: Delete the JSON file.", &TitleStorageSample::DeleteUserFileJSON),
    std::make_pair("DELETE_User: Delete the binary file from unverified storage.", &TitleStorageSample::DeleteUserFileBinaryUnverified),
    std::make_pair("DELETE_User: Delete the multi-block file.", &TitleStorageSample::DeleteUserFileMultiBlock),
};

const INT TitleStorageActionsCount = ARRAYSIZE(TitleStorageActions);


//--------------------------------------------------------------------------------------
// Initialize
//
// Create the console used to render and initialize signin
//--------------------------------------------------------------------------------------
HRESULT TitleStorageSample::Initialize()
{
    HRESULT hr = S_OK;

    m_TitleStorageCall = 0;

    // Initialize the console window
    hr = m_Console.Create("game:\\Media\\Fonts\\Arial_12.xpr", 0xFF1F005F, 0xFFFFFFFF);
    if (hr != S_OK)
    {
        ATG::FatalError("Failed to create console: %x\n", hr);
    }

    ATG::SignIn::Initialize(1, 1, TRUE, 1);

    m_Console.Format("[TitleStorageSample]  Task started\n");

    m_User = NULL;
    m_TitleStorageCall = -1;
    m_State = APPSTATE_INITIALIZE;
    m_eTag = new CHAR[ETAG_SIZE];
    memset(m_eTag, 0, ETAG_SIZE);
    memset(m_baseUrl, 0, ENDPOINT_KEY_SIZE);
    memset(m_continuationToken, 0, DEFAULT_STRSIZE);
    memset(m_Header, 0, MAX_HEADER_LENGTH);

    return hr;
}


//--------------------------------------------------------------------------------------
// Uninitialize
//
// REST shutdown
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::Uninitialize()
{
    delete m_User;
    delete m_eTag;

    m_REST.Shutdown();

    m_Console.Format("[TitleStorageSample] Task completed!\n");
}


//--------------------------------------------------------------------------------------
// Update
//
// Ensure user is signed in and if so run the sample update
//--------------------------------------------------------------------------------------
HRESULT TitleStorageSample::Update()
{
    HRESULT hr = S_OK;

    // Get gamepad state
    ATG::GAMEPAD* pInput = ATG::Input::GetMergedInput(); 

    // Update autosignin
    ATG::SignIn::Update();

    // If we're not signed in, wait until we are
    if(!ATG::SignIn::AreUsersSignedIn())
    {
        return S_OK;
    }

    switch (m_State)
    {
    case APPSTATE_INITIALIZE:
        {
            // initialze rest components
            hr = m_REST.Initialize(GROUP_ID, ATG::SignIn::GetSignedInUser()); 
            if (hr != S_OK)
            {
                ATG::FatalError("Failed to Initialize: %x\n", hr);
            }

            // get url for services
            hr = XGetServiceEndpoint("services", m_baseUrl, URL_STRSIZE, NULL);
            if (hr != S_OK)
            {
                ATG::FatalError("Failed to aquire services base url from endpoint service: %x\n", hr);
            }

            // getting user XUID
            HttpResponse response;
            CHAR fullUrl[URL_STRSIZE]; 
            sprintf_s(fullUrl, URL_STRSIZE, "%s%s", m_baseUrl, "/users/me/id");
            m_User = m_REST.MakeWebRequest<UsersMe>(NULL, fullUrl, "GET", NULL, m_User, NULL, response);
            if(m_User == NULL)
            {
                m_Console.Format("[TitleStorageSample]  Failed to get user information. Exiting\n");
                g_isRunning = FALSE;
                return ERROR_NOT_SUPPORTED;
            }

            // get url for title storage
            hr = XGetServiceEndpoint("titlestorage", m_baseUrl, URL_STRSIZE, NULL);
            if (hr != S_OK)
            {
                ATG::FatalError("Failed to get title storage base url from endpoint service: %x\n", hr);
            }

            m_Console.Format("[TitleStorageSample]  Left/Right on D-pad to cycle through requests\n");

            m_State = APPSTATE_RUN;
        }
        break;
    case APPSTATE_RUN:
        {
            if (pInput->wButtons & XINPUT_GAMEPAD_A)
            {
                if(m_TitleStorageCall >= 0 && m_TitleStorageCall < TitleStorageActionsCount)
                {
                    TitleStorageAction action = TitleStorageActions[m_TitleStorageCall].second;
                    (this->*action)();
                }
            }
            if (pInput->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
            {
                if(m_TitleStorageCall <= 0)
                {
                   m_TitleStorageCall = TitleStorageActionsCount;
                }

                m_TitleStorageCall = --m_TitleStorageCall % TitleStorageActionsCount;
                m_Console.Clear();

                m_Console.Format(
                    "[TitleStorageSample]  Hit A to issue request or B to exit. D-pad to cycle through requests\n[TitleStorageSample] %s\n",
                    TitleStorageActions[m_TitleStorageCall].first);

                Sleep(200);
            }
            if (pInput->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
            {
                m_TitleStorageCall = ++m_TitleStorageCall % TitleStorageActionsCount;
                assert( m_TitleStorageCall >= 0 && m_TitleStorageCall < TitleStorageActionsCount );

                m_Console.Clear();
                
                m_Console.Format(
                    "[TitleStorageSample]  Hit A to issue request or B to exit. D-pad to cycle through requests\n[TitleStorageSample] %s\n",
                    TitleStorageActions[m_TitleStorageCall].first);
                
                Sleep(200);
            }         
            else if (pInput->wButtons & XINPUT_GAMEPAD_B)
            {
                g_isRunning = FALSE;
            }
        }
        break;
    };

    return hr;
}


//--------------------------------------------------------------------------------------
// GetTitleQuota
//
// Gets the global title quota
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::GetTitleQuota()
{
    m_Console.Format("[TitleStorageSample] Getting Title Quota\n");

    CHAR fullUrl[URL_STRSIZE]; 
    sprintf_s(fullUrl, URL_STRSIZE, "%s%s", m_baseUrl, MakeTitleQuotaURL);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    QuotaInfo *tqi = NULL;
    HttpResponse response;
    tqi = m_REST.MakeWebRequest<QuotaInfo>(NULL, fullUrl, "GET", NULL, tqi, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(tqi != NULL && response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", tqi->ToString());
    }

    delete tqi;
}


//--------------------------------------------------------------------------------------
// ListTitleFilesAll
//
// List all global title files
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::ListTitleFilesAll()
{
    m_Console.Format("[TitleStorageSample] Listing all Title files\n");

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeTitleURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, "");
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    FileList *fileList = NULL;
    HttpResponse response;
    fileList = m_REST.MakeWebRequest<FileList>(NULL, fullUrl, "GET", NULL, fileList, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(fileList != NULL && response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", fileList->ToString());
    }

    delete fileList;
}


//--------------------------------------------------------------------------------------
// ListTitleFilesSkipItems
//
// Lists ass the global title files, skipping the first 3
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::ListTitleFilesSkipItems()
{
    m_Console.Format("[TitleStorageSample] Listing all Title files {skiping the first three} \n");

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeTitleURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, "?skipItems=3");
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    FileList *fileList = NULL;
    HttpResponse response;
    fileList = m_REST.MakeWebRequest<FileList>(NULL, fullUrl, "GET", NULL, fileList, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(fileList != NULL && response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", fileList->ToString());
    }

    delete fileList;
}


//--------------------------------------------------------------------------------------
// ListTitleFilesContinuationToken
//
// Requests global title files up to a maxiumum of 3 and uses the continuation token from that
// to list the rest of the global title files
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::ListTitleFilesContinuationToken()
{
    ListTitleFilesMaxItems();

    m_Console.Format("[TitleStorageSample] Listing all Title files {with continuationtoken = %s} \n", m_continuationToken);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s%s", m_baseUrl, MakeTitleURL, "?continuationToken=%s");
    sprintf_s(fullUrl, URL_STRSIZE, url, "", m_continuationToken);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    FileList *fileList = NULL;
    HttpResponse response;
    fileList = m_REST.MakeWebRequest<FileList>(NULL, fullUrl, "GET", NULL, fileList, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(fileList != NULL && response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", fileList->ToString());
    }

    delete fileList;
}


//--------------------------------------------------------------------------------------
// ListTitleFilesMaxItems
//
// Lists the global title files with a max return count of 3
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::ListTitleFilesMaxItems()
{
    m_Console.Format("[TitleStorageSample] Listing all Title files {max return of 3} \n");

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeTitleURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, "?maxItems=3");
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    FileList *fileList = NULL;
    HttpResponse response;
    fileList = m_REST.MakeWebRequest<FileList>(NULL, fullUrl, "GET", NULL, fileList, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(fileList != NULL && response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", fileList->ToString());
        strcpy_s<DEFAULT_STRSIZE>(m_continuationToken, fileList->pagingInfo[0]->continuationToken);
    }

    delete fileList;
}


//--------------------------------------------------------------------------------------
// GetTitleFileBinary
//
// Get a global title binary file
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::GetTitleFileBinary()
{
    m_Console.Format("[TitleStorageSample] Getting %s\n", TEST_BINARY_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeTitleURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, TEST_BINARY_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfNoneMatch(fullUrl, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    HttpResponse response;
    m_REST.MakeWebRequest(header, fullUrl, "GET", NULL, m_eTag, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_OK)
    {
        if(strlen(m_eTag) != 0)
        {
            m_ETags.AddOrUpdateETag(fullUrl, m_eTag);
        }

        m_Console.Format("%s\n", response.buffer);
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("Already have the latest file.");
    }
}


//--------------------------------------------------------------------------------------
// GetTitleFileJSON
//
// Get a global title JSON file
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::GetTitleFileJSON()
{
    m_Console.Format("[TitleStorageSample] Getting %s\n", TEST_JSON_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeTitleURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, TEST_JSON_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfNoneMatch(fullUrl, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    JsonFile *file = NULL;
    HttpResponse response;
    file = m_REST.MakeWebRequest<JsonFile>(header, fullUrl, "GET", NULL, file, m_eTag, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_OK)
    {
        if(strlen(m_eTag) != 0)
        {
            m_ETags.AddOrUpdateETag(fullUrl, m_eTag);
        }

        m_Console.Format("%s\n", file->ToString());
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("Already have the latest file.");
    }

    delete file;
}


//--------------------------------------------------------------------------------------
// GetTitleFileConfig
//
// Gets the global title config file
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::GetTitleFileConfig()
{
    m_Console.Format("[TitleStorageSample] Getting %s\n", TEST_CONFIG_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeTitleURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, TEST_CONFIG_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfNoneMatch(fullUrl, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    HttpResponse response;
    m_REST.MakeWebRequest(header, fullUrl, "GET", NULL, m_eTag, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_OK)
    {
        if(strlen(m_eTag) != 0)
        {
            m_ETags.AddOrUpdateETag(fullUrl, m_eTag);
        }

        m_Console.Format("%s\n", response.buffer);
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("Already have the latest file.");
    }
}


//--------------------------------------------------------------------------------------
// GetTitleFileCustomSelector
//
// Gets the global title config file part using the 'customSelector' query
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::GetTitleFileCustomSelector()
{
    m_Console.Format("[TitleStorageSample] Getting %s\n", TEST_CONFIG_FILE);

    HttpResponse response;
    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s%s", m_baseUrl, MakeTitleURL, "?customSelector=source.%s");
    
    //Braveheart
    sprintf_s(fullUrl, URL_STRSIZE, url, TEST_CONFIG_FILE, "Braveheart");
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);
    m_REST.MakeWebRequest(NULL, fullUrl, "GET", NULL, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);
    if(response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", response.buffer);
    }

    //Conan
    sprintf_s(fullUrl, URL_STRSIZE, url, TEST_CONFIG_FILE, "Conan");
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);
    m_REST.MakeWebRequest(NULL, fullUrl, "GET", NULL, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);
    if(response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", response.buffer);
    }

    //Macbeth
    sprintf_s(fullUrl, URL_STRSIZE, url, TEST_CONFIG_FILE, "Macbeth");
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);
    m_REST.MakeWebRequest(NULL, fullUrl, "GET", NULL, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);
    if(response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", response.buffer);
    }

    //default
    sprintf_s(fullUrl, URL_STRSIZE, url, TEST_CONFIG_FILE, "ThisShouldReturnDefault");
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);
    m_REST.MakeWebRequest(NULL, fullUrl, "GET", NULL, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);
    if(response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", response.buffer);
    }
}


//--------------------------------------------------------------------------------------
// GetUserQuota
//
// Gets user quota
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::GetUserQuota()
{
    m_Console.Format("[TitleStorageSample] Getting User Quota\n");

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserQuotaURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    QuotaInfo *uqi = NULL;
    HttpResponse response;
    uqi = m_REST.MakeWebRequest<QuotaInfo>(NULL, fullUrl, "GET", NULL, uqi, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(uqi != NULL && response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", uqi->ToString());
    }

    delete uqi;
}


//--------------------------------------------------------------------------------------
// ListUserFilesAll
//
// Lists all user files
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::ListUserFilesAll()
{
    m_Console.Format("[TitleStorageSample] Listing all User files\n");

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, "");
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    FileList *fileList = NULL;
    HttpResponse response;
    fileList = m_REST.MakeWebRequest<FileList>(NULL, fullUrl, "GET", NULL, fileList, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(fileList != NULL && response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", fileList->ToString());
    }

    delete fileList;
}


//--------------------------------------------------------------------------------------
// ListUserFilesSkipItems
//
// Lists all user files, skipping the first two
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::ListUserFilesSkipItems()
{
    m_Console.Format("[TitleStorageSample] Listing all User files {skiping the first two} \n");

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, "?skipItems=2");
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    FileList *fileList = NULL;
    HttpResponse response;
    fileList = m_REST.MakeWebRequest<FileList>(NULL, fullUrl, "GET", NULL, fileList, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(fileList != NULL && response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", fileList->ToString());
    }

    delete fileList;
}


//--------------------------------------------------------------------------------------
// ListUserFilesContinuationToken
//
// Requests user files up to a maxiumum of 2 and uses the continuation token from that
// to list the rest of the user files
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::ListUserFilesContinuationToken()
{
    ListUserFilesMaxItems();

    m_Console.Format("[TitleStorageSample] Listing all User files {with continuationtoken = %s} \n", m_continuationToken);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s%s", m_baseUrl, MakeUserURL, "?continuationToken=%s");
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, "",  m_continuationToken);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    FileList *fileList = NULL;
    HttpResponse response;
    fileList = m_REST.MakeWebRequest<FileList>(NULL, fullUrl, "GET", NULL, fileList, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(fileList != NULL && response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", fileList->ToString());
    }

    delete fileList;
}


//--------------------------------------------------------------------------------------
// ListUserFilesMaxItems
//
// Lists the user files, with a maximum return count of 2
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::ListUserFilesMaxItems()
{
    m_Console.Format("[TitleStorageSample] Listing all User files {max return of 2} \n");

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, "?maxItems=2");
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    FileList *fileList = NULL;
    HttpResponse response;
    fileList = m_REST.MakeWebRequest<FileList>(NULL, fullUrl, "GET", NULL, fileList, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(fileList != NULL && response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("%s\n", fileList->ToString());
        strcpy_s<DEFAULT_STRSIZE>(m_continuationToken, fileList->pagingInfo[0]->continuationToken);
    }

    delete fileList;
}


//--------------------------------------------------------------------------------------
// GetUserFileBinary
//
// Gets binary user file
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::GetUserFileBinary()
{
    m_Console.Format("[TitleStorageSample] Getting %s\n", TEST_BINARY_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_BINARY_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfNoneMatch(TEST_BINARY_FILE, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    HttpResponse response;
    m_REST.MakeWebRequest(header, fullUrl, "GET", NULL, m_eTag, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_OK)
    {
        if(strlen(m_eTag) != 0)
        {
            m_ETags.AddOrUpdateETag(TEST_BINARY_FILE, m_eTag);
        }

        m_Console.Format("%s\n", response.buffer);
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("Already have the latest file.");
    }
}


//--------------------------------------------------------------------------------------
// GetUserFileJSON
//
// Gets JSON user file
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::GetUserFileJSON()
{
    m_Console.Format("[TitleStorageSample] Getting %s\n", TEST_JSON_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_JSON_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfNoneMatch(TEST_JSON_FILE, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    JsonFile *file = NULL;
    HttpResponse response;
    file = m_REST.MakeWebRequest<JsonFile>(header, fullUrl, "GET", NULL, file, m_eTag, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(file != NULL && response.returnCode == HTTP_STATUS_OK)
    {
        if(strlen(m_eTag) != 0)
        {
            m_ETags.AddOrUpdateETag(TEST_JSON_FILE, m_eTag);
        }

        m_Console.Format("%s\n", file->ToString());
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("Already have the latest file.");
    }

    delete file;
}


//--------------------------------------------------------------------------------------
// GetUserFileMultiBlock
//
// Gets the file uploaded by the PutUserFileMultiBlock
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::GetUserFileMultiBlock()
{
    m_Console.Format("[TitleStorageSample] Getting %s\n", TEST_MULTIBLOCK_FILE);

    //create url
    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_MULTIBLOCK_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    // Range header lengths
    INT blockLength1 = strlen(TestMultiBlockFileContentsBlock1);
    INT blockLength2 = strlen(TestMultiBlockFileContentsBlock2);
    INT blockLength3 = strlen(TestMultiBlockFileContentsBlock3);
    INT totalLength = blockLength1 + blockLength2 + blockLength3 + 1; //plus one for null terminator on final strcat_s()
    CHAR* multiBlockFile = new CHAR[MAX_RESPONSESIZE];
    memset(multiBlockFile, 0, totalLength);

    // If-None-Match header
    CHAR MATCH[DEFAULT_STRSIZE];
    CHAR DEFAULT_NONE_MATCH_HEADER[DEFAULT_STRSIZE];
    if(m_ETags.GetETag(TEST_MULTIBLOCK_FILE, m_eTag))
    {  
        sprintf_s(MATCH, DEFAULT_STRSIZE, IF_NONE_MATCH_HEADER, m_eTag);
        sprintf_s(DEFAULT_NONE_MATCH_HEADER, DEFAULT_STRSIZE, DEFAULT_HEADER, MATCH);
    }
    else
    {
        sprintf_s(DEFAULT_NONE_MATCH_HEADER, DEFAULT_STRSIZE, DEFAULT_HEADER, "");
    }
    
    // Range header
    CHAR DEFAULT_NONE_MATCH_RANGE_HEADER[DEFAULT_STRSIZE];
    sprintf_s(DEFAULT_NONE_MATCH_RANGE_HEADER, DEFAULT_STRSIZE, "%s%s", DEFAULT_NONE_MATCH_HEADER, RANGE_HEADER);

    // request first block (25 bytes)
    HttpResponse response;
    sprintf_s(m_Header, MAX_HEADER_LENGTH, DEFAULT_NONE_MATCH_RANGE_HEADER, 0, 24);
    m_REST.MakeWebRequest(m_Header, fullUrl, "GET", NULL, m_eTag, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);
    if(response.returnCode != HTTP_STATUS_PARTIAL_CONTENT)
    {
        if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
        {
            m_Console.Format("ETags matched, Already have the latest file.\n");
        }
        else
        {
            m_Console.Format("Unexpected response code\n");
        }

        delete [] multiBlockFile;
        return;
    }

    //copy response into buffer
    strcat_s(multiBlockFile, MAX_RESPONSESIZE, response.buffer);
    m_Console.Format("Downloaded %d bytes\n", strlen(response.buffer));

    // If-Match header
    CHAR DEFAULT_MATCH_HEADER[DEFAULT_STRSIZE];
    sprintf_s(MATCH, DEFAULT_STRSIZE, IF_MATCH_HEADER, m_eTag);
    sprintf_s(DEFAULT_MATCH_HEADER, DEFAULT_STRSIZE, DEFAULT_HEADER, MATCH);

    //request second block (next 25 bytes)
    CHAR DEFAULT_MATCH_RANGE_HEADER[DEFAULT_STRSIZE];
    sprintf_s(DEFAULT_MATCH_RANGE_HEADER, DEFAULT_STRSIZE, "%s%s", DEFAULT_MATCH_HEADER, RANGE_HEADER);
    sprintf_s(m_Header, MAX_HEADER_LENGTH, DEFAULT_MATCH_RANGE_HEADER, 25, 49);
    m_REST.MakeWebRequest(m_Header, fullUrl, "GET", NULL, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);
    if(response.returnCode != HTTP_STATUS_PARTIAL_CONTENT)
    {
        if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
        {
            m_Console.Format("ETags did not match, The ETag has changed since the start of this operation.\n");
            m_Console.Format("This indicates that the file has changed and the previous data is no longer valid.\n");
            m_Console.Format("Try restarting the operation.\n");
        }
        else
        {
            m_Console.Format("Unexpected response code\n");
        }

        delete [] multiBlockFile;
        return;
    }

    //copy response into buffer
    strcat_s(multiBlockFile, MAX_RESPONSESIZE, response.buffer);
    m_Console.Format("Downloaded %d bytes\n", strlen(response.buffer));

    //request third and final block           
    //by setting the range with MAX_RESPONSESIZE, it will grab the rest of the file.
    sprintf_s(m_Header, MAX_HEADER_LENGTH, DEFAULT_MATCH_RANGE_HEADER, 50, MAX_RESPONSESIZE); 
    m_REST.MakeWebRequest(m_Header, fullUrl, "GET", NULL, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);
    if(response.returnCode != HTTP_STATUS_PARTIAL_CONTENT)
    {
        if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
        {
            m_Console.Format("ETags did not match, The ETag has changed since the start of this operation.\n");
            m_Console.Format("This indicateds that the file has changed and the previous data is no longer valid.\n");
            m_Console.Format("Try restarting the operation.\n");
        }
        else
        {
            m_Console.Format("Unexpected response code\n");
        }

        delete [] multiBlockFile;
        return;
    }

    //Update the ETag
    m_ETags.AddOrUpdateETag(TEST_MULTIBLOCK_FILE, m_eTag);

    //copy response into buffer
    strcat_s(multiBlockFile, MAX_RESPONSESIZE, response.buffer);
    m_Console.Format("Downloaded %d bytes\n", strlen(response.buffer));
    m_Console.Format("Last block acquired.\n");
    m_Console.Format("%s", multiBlockFile);

    delete [] multiBlockFile;
}


//--------------------------------------------------------------------------------------
// GetUserFileBinaryUnverified
//
// Gets binary user file from the unverified storage, should fail on xbox.
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::GetUserFileBinaryUnverified()
{
    m_Console.Format("[TitleStorageSample] Getting %s from unverified storage\n", TEST_UNVERIFIED_BINARY_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_UNVERIFIED_BINARY_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfNoneMatch(TEST_UNVERIFIED_BINARY_FILE, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    HttpResponse response;
    m_REST.MakeWebRequest(header, fullUrl, "GET", NULL, m_eTag, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_OK)
    {
        if(strlen(m_eTag) != 0)
        {
            m_ETags.AddOrUpdateETag(TEST_UNVERIFIED_BINARY_FILE, m_eTag);
        }

        m_Console.Format( "%s\n", response.buffer);
    }
    else if(response.returnCode == HTTP_STATUS_FORBIDDEN)
    {
        m_Console.Format("This is expected. Trying to download unverified BINARY data to a verified platform (i.e. Xbox360 or Windows Phone) is not allowed. JSON would be allowed.");
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("Already have the latest file.");
    }
}


//--------------------------------------------------------------------------------------
// GetUserFileSelect
//
// Gets the message property out of the user JSON file using the 'select' query
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::GetUserFileSelect()
{
    m_Console.Format("[TitleStorageSample] Getting \"message\" from %s\n", TEST_JSON_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s%s", m_baseUrl, MakeUserURL, "?select=message");
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_JSON_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    HttpResponse response;
    MessageSelect* message = NULL;
    message = m_REST.MakeWebRequest<MessageSelect>(NULL, fullUrl, "GET", NULL, message, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("Message:%s", message->message);
    }
}


//--------------------------------------------------------------------------------------
// PutUserFileBinary
//
// Uploads a binary user file
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::PutUserFileBinary()
{
    m_Console.Format("[TitleStorageSample] Uploading %s\n", TEST_BINARY_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    CHAR TIME[DEFAULT_STRSIZE];
    GetCurrentTimeString(TIME);
    sprintf_s(url, URL_STRSIZE, "%s%s%s", m_baseUrl, MakeUserURL, "?clientFileTime=%s&displayName=Display%%20Name%%20Binary");
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_BINARY_FILE, TIME);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfMatch(TEST_BINARY_FILE, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    HttpResponse response;
    m_REST.MakeWebRequest(header, fullUrl, "PUT", TestUserBinaryFileContents, m_eTag, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_CREATED)
    {
        if(strlen(m_eTag) != 0)
        {
            m_ETags.AddOrUpdateETag(TEST_BINARY_FILE, m_eTag);
        }

        m_Console.Format("File Created.\n");
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("ETags did not match, They prevent overwiting newer files with older files. Get Latest file.\n");
    }
}


//--------------------------------------------------------------------------------------
// PutUserFileJSON
//
// Uploads a JSON user file
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::PutUserFileJSON()
{
    m_Console.Format("[TitleStorageSample] Uploading %s\n", TEST_JSON_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    CHAR TIME[DEFAULT_STRSIZE];
    GetCurrentTimeString(TIME);
    sprintf_s(url, URL_STRSIZE, "%s%s%s", m_baseUrl, MakeUserURL, "?clientFileTime=%s&displayName=Display%%20Name%%20JSON");
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_JSON_FILE, TIME);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfMatch(TEST_JSON_FILE, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    HttpResponse response;
    m_REST.MakeWebRequest(header, fullUrl, "PUT", TestUserJsonFileContents, m_eTag, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_CREATED)
    {
        if(strlen(m_eTag) != 0)
        {
            m_ETags.AddOrUpdateETag(TEST_JSON_FILE, m_eTag);
        }

        m_Console.Format("File Created.\n");
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("ETags did not match, They prevent overwiting newer files with older files. Get Latest file.\n");
    }
}


//--------------------------------------------------------------------------------------
// PutUserFileMultiBlock
//
// Uploads a binary user file in 3 blocks.
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::PutUserFileMultiBlock()
{
    m_Console.Format("[TitleStorageSample] Uploading %s {Multi-block}\n", TEST_MULTIBLOCK_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    CHAR TOKEN[DEFAULT_STRSIZE];
    CHAR TIME[DEFAULT_STRSIZE];
    GetCurrentTimeString(TIME);
    HttpResponse response;
    ContinuationToken* token = NULL;

    sprintf_s(url, URL_STRSIZE, "%s%s%s", m_baseUrl, MakeUserURL, "?finalBlock=false");
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_MULTIBLOCK_FILE);
    m_Console.Format("[TitleStorageSample] 1. %s\n", fullUrl);
    token = m_REST.MakeWebRequest<ContinuationToken>(NULL, fullUrl, "PUT", TestMultiBlockFileContentsBlock1, token, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);
    if(response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("First Block Uploaded. ContinuationToken=%s\n", token->continuationToken);
    }

    strcpy_s<DEFAULT_STRSIZE>(TOKEN, token->continuationToken);
    delete token;

    sprintf_s(url, URL_STRSIZE, "%s%s%s", m_baseUrl, MakeUserURL, "?continuationToken=%s&finalBlock=false");
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_MULTIBLOCK_FILE, TOKEN);
    m_Console.Format("[TitleStorageSample] 2. %s\n", fullUrl);
    token = m_REST.MakeWebRequest<ContinuationToken>(NULL, fullUrl, "PUT", TestMultiBlockFileContentsBlock2, token, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);
    if(response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("Second Block Uploaded. ContinuationToken=%s\n", token->continuationToken);
    }

    strcpy_s<DEFAULT_STRSIZE>(TOKEN, token->continuationToken);
    delete token;

    sprintf_s(url, URL_STRSIZE, "%s%s%s", m_baseUrl, MakeUserURL, "?continuationToken=%s&finalBlock=true&clientFileTime=%s&displayName=Display%%20Name%%20Multi-block");
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_MULTIBLOCK_FILE, TOKEN, TIME);
    m_Console.Format("[TitleStorageSample] 3. %s\n", fullUrl);
    m_REST.MakeWebRequest(NULL, fullUrl, "PUT", TestMultiBlockFileContentsBlock3, m_eTag, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);
    if(response.returnCode == HTTP_STATUS_CREATED)
    {
        if(strlen(m_eTag) != 0)
        {
            m_ETags.AddOrUpdateETag(TEST_MULTIBLOCK_FILE, m_eTag);
        }

        m_Console.Format("Third and Final Block Uploaded.\n");
    }
}


//--------------------------------------------------------------------------------------
// PutUserFileBinaryUnverified
//
// Uploads a binary user file to the unverified storage.
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::PutUserFileBinaryUnverified()
{
    m_Console.Format("[TitleStorageSample] Uploading %s to unverified storage\n", TEST_UNVERIFIED_BINARY_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    CHAR TIME[DEFAULT_STRSIZE];
    GetCurrentTimeString(TIME);
    sprintf_s(url, URL_STRSIZE, "%s%s%s", m_baseUrl, MakeUserURL, "?clientFileTime=%s&displayName=Display%%20Name%%20Unverified%%20Binary");
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_UNVERIFIED_BINARY_FILE, TIME);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfMatch(TEST_UNVERIFIED_BINARY_FILE, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    HttpResponse response;
    m_REST.MakeWebRequest(header, fullUrl, "PUT", TestUserBinaryFileContents, m_eTag, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_CREATED)
    {
        if(strlen(m_eTag) != 0)
        {
            m_ETags.AddOrUpdateETag(TEST_UNVERIFIED_BINARY_FILE, m_eTag);
        }

        m_Console.Format("File Created.\n");
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("ETags did not match, They prevent overwiting newer files with older files. Get Latest file.\n");
    }
}


//--------------------------------------------------------------------------------------
// DeleteUserFileBinary
//
// Deletes the binary user file
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::DeleteUserFileBinary()
{
    m_Console.Format("[TitleStorageSample] Deleting %s\n", TEST_BINARY_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_BINARY_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfMatch(TEST_BINARY_FILE, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    HttpResponse response;
    m_REST.MakeWebRequest(header, fullUrl, "DELETE", NULL, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("File Deleted.\n");
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("ETags did not match, This prevents deleteing newer files.\n");
        m_Console.Format("For this sample you must re-GET the latest file, inorder to delete it.\n");
        m_Console.Format("In your project you can omit the ETag header if you are certain you want to delete the file.\n");
    }
}


//--------------------------------------------------------------------------------------
// DeleteUserFileJSON
//
// deletes the JSON user file
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::DeleteUserFileJSON()
{
    m_Console.Format("[TitleStorageSample] Deleting %s\n", TEST_JSON_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_JSON_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfMatch(TEST_JSON_FILE, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    HttpResponse response;
    m_REST.MakeWebRequest(header, fullUrl, "DELETE", NULL, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("File Deleted.\n");
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("ETags did not match, This prevents deleteing newer files.\n");
        m_Console.Format("For this sample you must re-GET the latest file, inorder to delete it.\n");
        m_Console.Format("In your project you can omit the ETag header if you are certain you want to delete the file.\n");
    }
}


//--------------------------------------------------------------------------------------
// DeleteUserFileBinaryUnverified
//
// Deletes the binary user file in the unverified storage
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::DeleteUserFileBinaryUnverified()
{
    m_Console.Format("[TitleStorageSample] Deleting %s\n", TEST_UNVERIFIED_BINARY_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_UNVERIFIED_BINARY_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfMatch(TEST_UNVERIFIED_BINARY_FILE, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    HttpResponse response;
    m_REST.MakeWebRequest(header, fullUrl, "DELETE", NULL, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("File Deleted.\n");
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("ETags did not match, This prevents deleteing newer files.\n");
        m_Console.Format("For this sample you must re-GET the latest file, inorder to delete it.\n");
        m_Console.Format("In your project you can omit the ETag header if you are certain you want to delete the file.\n");
    }
}


//--------------------------------------------------------------------------------------
// DeleteUserFileMultiBlock
//
// Deletes the binary user file uploaded by PutUserFileMultiBlock
//--------------------------------------------------------------------------------------
VOID TitleStorageSample::DeleteUserFileMultiBlock()
{
    m_Console.Format("[TitleStorageSample] Deleting %s\n", TEST_MULTIBLOCK_FILE);

    CHAR url[URL_STRSIZE];
    CHAR fullUrl[URL_STRSIZE];
    sprintf_s(url, URL_STRSIZE, "%s%s", m_baseUrl, MakeUserURL);
    sprintf_s(fullUrl, URL_STRSIZE, url, m_User->xuid, TEST_MULTIBLOCK_FILE);
    m_Console.Format("[TitleStorageSample] %s\n", fullUrl);

    CHAR* header = NULL;
    if(GetHeaderIfMatch(TEST_MULTIBLOCK_FILE, m_Header) == TRUE)
    {
        header = &m_Header[0];
    }

    HttpResponse response;
    m_REST.MakeWebRequest(header, fullUrl, "DELETE", NULL, NULL, response);
    m_Console.Format("[TitleStorageSample] HTTP response %d\n", response.returnCode);

    if(response.returnCode == HTTP_STATUS_OK)
    {
        m_Console.Format("File Deleted.\n");
    }
    else if(response.returnCode == HTTP_STATUS_PRECOND_FAILED)
    {
        m_Console.Format("ETags did not match, This prevents deleteing newer files.\n");
        m_Console.Format("For this sample you must re-GET the latest file, inorder to delete it.\n");
        m_Console.Format("In your project you can omit the ETag header if you are certain you want to delete the file.\n");
    }
}


//--------------------------------------------------------------------------------------
// GetHeaderIfMatch
//
// Gets the non-default header for If-Match
//--------------------------------------------------------------------------------------
BOOL TitleStorageSample::GetHeaderIfMatch(CONST CHAR* fileName, CHAR* header)
{
    CHAR ETAG[ETAG_SIZE];
    CHAR ETAG_HEADER[DEFAULT_STRSIZE];
    if(m_ETags.GetETag(fileName, ETAG))
    {
        sprintf_s(ETAG_HEADER, DEFAULT_STRSIZE, IF_MATCH_HEADER, ETAG);
        sprintf_s(header, MAX_HEADER_LENGTH, DEFAULT_HEADER, ETAG_HEADER);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

//--------------------------------------------------------------------------------------
// GetHeaderIfNonMatch
//
// Gets the non-default header for If-None-Match
//--------------------------------------------------------------------------------------
BOOL TitleStorageSample::GetHeaderIfNoneMatch(CONST CHAR* fileName, CHAR* header)
{
    CHAR ETAG[ETAG_SIZE];
    CHAR ETAG_HEADER[DEFAULT_STRSIZE];
    if(m_ETags.GetETag(fileName, ETAG))
    {
        sprintf_s(ETAG_HEADER, DEFAULT_STRSIZE, IF_NONE_MATCH_HEADER, ETAG);
        sprintf_s(header, MAX_HEADER_LENGTH, DEFAULT_HEADER, ETAG_HEADER);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

VOID TitleStorageSample::GetCurrentTimeString(CHAR* timeStr)
{
    time_t now = time(0);
    tm localtm;
    localtime_s(&localtm, &now);

    sprintf_s(
        timeStr,
        DEFAULT_STRSIZE,
        "%04d-%02d-%02dT%02d%%3A%02d%%3A%02dZ",
        localtm.tm_year + 1900,
        localtm.tm_mon + 1, // localtime_s returns a month between 0 and 11
        localtm.tm_mday,
        localtm.tm_hour,
        localtm.tm_min,
        localtm.tm_sec);
}

//--------------------------------------------------------------------------------------
// main
//
// Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    TitleStorageSample atgApp;

    atgApp.Initialize();

    while(g_isRunning)
    {
        atgApp.Update();
    }

    atgApp.Uninitialize();
}