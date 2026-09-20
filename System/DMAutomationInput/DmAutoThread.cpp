//-------------------------------------------------------------------------------------
// DmAutoThread.cpp
//
// Microsoft XNA Game Platform Extensions Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "stdafx.h"

#define UPDATES_PER_SECOND 120
#define DM_SAMPLE_TIMEOUT   5000
//-------------------------------------------------------------------------------------
// Name: DmAutoThread
//-------------------------------------------------------------------------------------
DWORD WINAPI DmAutoThread( LPVOID lpParameter )
{
    UNREFERENCED_PARAMETER( lpParameter );

    HRESULT hr = DmSetConnectionTimeout( DM_SAMPLE_TIMEOUT, DM_SAMPLE_TIMEOUT );
    if( FAILED( hr ) )
        return ( DWORD )hr;

    hr = DmAutomationBindController( 0, 0 );

    if( FAILED( hr ) )
        return ( DWORD )hr;

    // Read the controller connected to the PC
    XINPUT_STATE inputState = { 0 };

    int iUserIndex = 0;
    for( int iUserIndex = 0; iUserIndex < 3; ++iUserIndex )
    {
        hr = XInputGetState( iUserIndex, &inputState );
        if( hr == ERROR_SUCCESS )
            break;
    }

    if( hr != ERROR_SUCCESS )
        return( ( DWORD )hr );

    DWORD dwSleepTime = 1000 / UPDATES_PER_SECOND;

    while( !g_bDone )
    {
        DWORD dwEnterTime = GetTickCount();

        hr = XInputGetState( iUserIndex, &inputState );

        if( hr != ERROR_SUCCESS )
            return( ( DWORD )hr );

        hr = DmAutomationSetGamepadState( 0,
                                          ( PDM_XINPUT_GAMEPAD )&inputState.Gamepad );

        if( FAILED( hr ) )
            return( ( DWORD )hr );

        DWORD dwElapsedTime = GetTickCount() - dwEnterTime;
        if( dwElapsedTime < dwSleepTime )
        {
            // Sleep to simulate the proper poll rate
            Sleep( dwSleepTime - dwElapsedTime );
        }
    }

    DmAutomationUnbindController( 0 );

    return 0;
}
