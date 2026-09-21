//--------------------------------------------------------------------------------------
// RESTfulObjects
//
// Simple sample to demonstrate REST calls using the ATGFramework
//
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

#include "AtgRest.h"
#include "AtgInput.h"
#include "AtgUtil.h"
#include "AtgConsole.h"
#include "AtgHttp.h"
#include "AtgSignIn.h"
#include "RESTfulObjects.spa.h"

using namespace ATG::REST;

// this is the test endpoint for this sample
// this endpoint is in the NSAL for the sample titleID for testing
// NOTE: the xboxlive.com restful services require an XAuth token
//#define TEST_URL  "https://services.part.xboxlive.com/users/me/id"

#define TITLE_ID "4294903764"  //title id of this xex
#define GROUP_ID ""            //used if spanning multiple title id's

BOOL g_isRunning = TRUE;



//--------------------------------------------------------------------------------------
// class RESTfulObjects
//
// main class to run this application.
//--------------------------------------------------------------------------------------
class RESTfulObjects 
{
public:
    RESTfulObjects() 
    {
        m_State = APPSTATE_INITIALIZE;
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

    ATG::Console       m_Console;
    ATG::REST::AtgREST m_REST;
};


//--------------------------------------------------------------------------------------
// Initialize
//
// Create the console used to render and initialize signin
//--------------------------------------------------------------------------------------
HRESULT RESTfulObjects::Initialize()
{
    HRESULT hr = S_OK;

    // Initialize the console window
    hr = m_Console.Create("game:\\Media\\Fonts\\Arial_12.xpr", 0xFF1F005F, 0xFFFFFFFF);
    if (hr != S_OK)
    {
        ATG::FatalError("Failed to create console: %x\n", hr);
    }

    m_Console.Format("[RESTSAMPLE] Task started\n");
    m_Console.Format("[RESTSAMPLE] hit A to issue request or B to exit\n");


    ATG::SignIn::Initialize(1, 1, TRUE, 1);

    return hr;
}


//--------------------------------------------------------------------------------------
// Update
//
// Ensure user is signed in and if so run the sample update
//--------------------------------------------------------------------------------------
HRESULT RESTfulObjects::Update()
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
            hr = m_REST.Initialize(TITLE_ID, GROUP_ID, ATG::SignIn::GetSignedInUser()); // NOTE: currently only handle single player
            if (hr != S_OK)
            {
                ATG::FatalError("Failed to Initialize: %x\n", hr);
            }

            m_State = APPSTATE_RUN;
        }
        break;
    case APPSTATE_RUN:
        {
            if (pInput->wButtons & XINPUT_GAMEPAD_A)
            {
                HttpResponse response;
                // hit the REST endpoint /users/me/id via wrapper
                UsersMe *me = m_REST.GetMe(response); // NOTE: synchronous call to REST framework, handles JSON, HTTP, and STS tokens
                if (me != NULL)
                {
                    m_Console.Format("[RESTSAMPLE] gamertag:%s  xuid:%I64d\n", me->gamerTag, me->xuid);
                    delete me;
                }

                m_Console.Format("[RESTSAMPLE] HTTP response %d\n", response.returnCode);

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
// REST shutdown
//--------------------------------------------------------------------------------------
VOID RESTfulObjects::Uninitialize()
{
    m_REST.Shutdown();

    m_Console.Format("[RESTSAMPLE] Task completed!\n");
}


//--------------------------------------------------------------------------------------
// main
//
// Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    RESTfulObjects atgApp;

    atgApp.Initialize();

    while(g_isRunning)
    {
        atgApp.Update();
    }

    atgApp.Uninitialize();
}

