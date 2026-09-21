//--------------------------------------------------------------------------------------
// SimpleXAuth
//
// Simple sample to demonstrate XHTTP and XAuth libraries.
//
// Currently open up a single secure endpoint and pass up a STS token 
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

#include "AtgInput.h"
#include "AtgUtil.h"
#include "AtgConsole.h"
#include "AtgHttp.h"
#include "AtgSignIn.h"
#include "SimpleXAuth.spa.h"


// this is the test endpoint for this sample
// these endpoints are in the NSAL for the sample titleID for testing
// NOTE: the xboxlive.com restful services require an XAuth token

//#define TEST_URL  "http://www.microsoft.com/en-us/default.aspx"
//#define TEST_URL  "https://services.part.xboxlive.com/matchmaking/help"
#define TEST_URL  "https://services.part.xboxlive.com/users/me/id"

#define HTTP_POLLING_INTERVAL  33

BOOL g_isRunning = TRUE;


//--------------------------------------------------------------------------------------
// class SimpleXAuth
//
// main class to run this application.
//--------------------------------------------------------------------------------------
class SimpleXAuth 
{
public:
    SimpleXAuth() 
    {
        m_hWorkerThread = NULL;
        m_State = APPSTATE_INITIALIZE;
        m_Endpoint = NULL;
    };

    HRESULT Initialize();
    VOID    Uninitialize();
    HRESULT Update();

private:

    // Application state items
    enum APPSTATE
    {
        APPSTATE_INITIALIZE,
        APPSTATE_RUN,
    };

    APPSTATE m_State;

    ATG::Console      m_Console;
    ATG::HTTP::AuthManager  m_Http;
    ATG::HTTP::AuthManager::AuthEndpoint *m_Endpoint;

    HANDLE m_hWorkerThread;
};


//--------------------------------------------------------------------------------------
// Initialize
//
// Create the console used to render and initialize signin
//--------------------------------------------------------------------------------------
HRESULT SimpleXAuth::Initialize()
{
    HRESULT hr = S_OK;

    // Initialize the console window
    hr = m_Console.Create("game:\\Media\\Fonts\\Arial_12.xpr", 0xFF1F005F, 0xFFFFFFFF);
    if (hr != S_OK)
    {
        ATG::FatalError("Failed to create console: %x\n", hr);
    }

    m_Console.Format("[XAUTHSAMPLE] Task started\n");
    m_Console.Format("[XAUTHSAMPLE] hit A to issue request or B to exit\n");


    ATG::SignIn::Initialize(1, 1, TRUE, 1);

    return hr;
}


//--------------------------------------------------------------------------------------
// Update
//
// Ensure user is signed in and if so run the sample update
//--------------------------------------------------------------------------------------
HRESULT SimpleXAuth::Update()
{
    HRESULT hr = S_OK;

    // Get gamepad state
    ATG::GAMEPAD* pInput = ATG::Input::GetMergedInput(); 

    // Update autosignin
    ATG::SignIn::Update();

    // If we're not signed in, wait until we are
    if( !ATG::SignIn::AreUsersSignedIn() )
    {
        return S_OK;
    }

    switch (m_State)
    {
    case APPSTATE_INITIALIZE:
        {
            hr = m_Http.Startup(NULL, FALSE, &m_hWorkerThread, TRUE, ATG::SignIn::GetSignedInUser()); // NOTE: currently only handle single player
            if (hr != S_OK)
            {
                ATG::FatalError("Failed to Initialize: %x\n", hr);
            }

            m_Endpoint = m_Http.CreateEndpoint(TEST_URL, TRUE);

            m_State = APPSTATE_RUN;
        }
        break;
    case APPSTATE_RUN:
        {
            if (pInput->wButtons & XINPUT_GAMEPAD_A)
            {
                hr = m_Endpoint->OpenRequest("GET", NULL, NULL, 0);
                if (hr != S_OK)
                {
                    ATG::FatalError("Failed to OpenRequest: %x\n", hr);
                }

                while (!m_Endpoint->RequestCompleted())
                {
                    // Here we are polling the endpoint waiting for the request to be completed
                    Sleep(HTTP_POLLING_INTERVAL);
                }

                if (m_Endpoint->GetHTTPStatusCode() != HTTP_STATUS_OK) 
                {
                    m_Console.Format("[XAUTHSAMPLE] HTTP response %d\n", m_Endpoint->GetHTTPStatusCode());
                }
                else
                {
                    m_Console.Format("[XAUTHSAMPLE] %s\n", m_Endpoint->GetReadBuffer());
                }

                m_Endpoint->CloseRequest();
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
// Uninitialize
//
// AtgHttp shutdown
//--------------------------------------------------------------------------------------
VOID SimpleXAuth::Uninitialize()
{
    m_Http.Shutdown();

    m_Console.Format("[XAUTHSAMPLE] Task completed!\n");
}


//--------------------------------------------------------------------------------------
// main
//
// Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    SimpleXAuth atgApp;

    atgApp.Initialize();

    while(g_isRunning)
    {
        atgApp.Update();
    }

    atgApp.Uninitialize();
}

