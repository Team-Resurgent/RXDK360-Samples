//--------------------------------------------------------------------------------------
// Visualization.cpp
//
// Declares functions used to visualize the output of the turn detection filter
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgUtil.h>
#include <d3d9types.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>

#include "Visualization.h"

//--------------------------------------------------------------------------------------
// Constants and global variables
//--------------------------------------------------------------------------------------

// World scale
const FLOAT c_fScale            = 1.0f;
const UINT c_uNumHistoryEntries = 60;       // Running at 60fps, so keep 1 second worths of history
const UINT c_uNumSearchEntries  = 60;
const UINT c_uNumFramesToFade   = 120;      // Take two seconds to fade out

// A ring buffer to keep track of rotation values
FLOAT g_fRotationHistory[ c_uNumHistoryEntries ];
UINT g_uRotationHistoryIndex = 0;

// An array to keep track of angles where we searched for a flip
FLOAT g_fSearchHistory[ c_uNumSearchEntries ];
UINT g_uSearchHistoryIndex = 0;

// Some variables to keep track of when we started searching and found a new flip
BOOL g_bSearchForFlip   = FALSE;
BOOL g_bStopSearch      = FALSE;
BOOL g_bFoundFlip       = FALSE;
FLOAT g_fFlipAngle      = 0.0f;
UINT g_uNumFramesToFade = 0;


//--------------------------------------------------------------------------------------
// Name: InitVisualization()
// Desc: Initialized the visualization
//--------------------------------------------------------------------------------------

VOID InitVisualization()
{
    // Clear history
    ZeroMemory( g_fRotationHistory, ARRAYSIZE( g_fRotationHistory ) );
    ZeroMemory( g_fSearchHistory, ARRAYSIZE( g_fSearchHistory ) );

    // Reset the indices and states
    g_uRotationHistoryIndex = 0;
    g_uSearchHistoryIndex = 0;
    g_bSearchForFlip = FALSE;
    g_bStopSearch = FALSE;
    g_bFoundFlip = FALSE;
    g_fFlipAngle = 0.0f;
    g_uNumFramesToFade = 0;

    // Set the view matrix
    XMVECTOR vCameraPosition = XMVectorSet( 0.0f, 1.0, -2.0, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    XMMATRIX matView = XMMatrixLookAtLH( vCameraPosition, XMVectorZero(), vUp );

    // Set up projection matrix
    const FLOAT fZNear = 0.1f;
    const FLOAT fZFar = 10.0f;
    UINT uWidth, uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );
    FLOAT fAspectRatio = (FLOAT)uWidth / (FLOAT)uHeight;
    XMMATRIX m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 2.5f, fAspectRatio, fZNear, fZFar );

    // Set the world view projection matrix into the debug draw system.
    XMMATRIX matWVP = matView * m_matProj;
    ATG::DebugDraw::SetViewProjection( matWVP );
}


//--------------------------------------------------------------------------------------
// Name: UpdateVisualization()
// Desc: Updates the states of the visualization with new data from the turn detection filter
//--------------------------------------------------------------------------------------

VOID UpdateVisualization( const FLOAT fTurnAngleInDegrees, const BOOL bSearchForFlip, const BOOL bFoundFlip )
{
    // See if we're starting, stopping or still busy with a search
    if ( !g_bSearchForFlip && bSearchForFlip )
    {
        ZeroMemory( g_fSearchHistory, ARRAYSIZE( g_fSearchHistory ) );
        g_uSearchHistoryIndex   = 0;
        g_uNumFramesToFade      = 0;
        g_bStopSearch           = FALSE;
        g_bFoundFlip            = FALSE;
    }
    else if ( g_bSearchForFlip && !bSearchForFlip )
    {        
        g_uNumFramesToFade  = 0;
        g_bStopSearch       = TRUE;
    }
    else if ( g_uNumFramesToFade >= c_uNumFramesToFade )
    {
        ZeroMemory( g_fSearchHistory, ARRAYSIZE( g_fSearchHistory ) );
        g_uSearchHistoryIndex   = 0;
        g_uNumFramesToFade      = 0;
        g_bFoundFlip            = FALSE;
    }

    // Update the rotation angle. Convert to radians and rotate 90 degrees
    g_fRotationHistory[ g_uRotationHistoryIndex ] = XMConvertToRadians( fTurnAngleInDegrees + 90.0f );

    if ( bSearchForFlip )
    {
        // Record the first flip found in the search
        if ( bFoundFlip && !g_bFoundFlip )
        {
            g_fFlipAngle = g_fRotationHistory[ g_uRotationHistoryIndex ];
        }

        // Record all the search angles
        if ( g_uSearchHistoryIndex < c_uNumSearchEntries )
        {
            g_fSearchHistory[ g_uSearchHistoryIndex ] = g_fRotationHistory[ g_uRotationHistoryIndex ];
            g_uSearchHistoryIndex++;
        }
    }

    if ( g_bStopSearch )
    {
        g_uNumFramesToFade++;
    }

    g_uRotationHistoryIndex++;
    g_uRotationHistoryIndex %= c_uNumHistoryEntries;

    g_bSearchForFlip = bSearchForFlip;

    if ( !g_bFoundFlip )
    {
        g_bFoundFlip = bFoundFlip;
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderVisualization()
// Desc: Renders something interesting with the output from the turn detection filter
//--------------------------------------------------------------------------------------

VOID RenderVisualization( D3DDevice* pD3DDevice )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Render grid lines
    ATG::DebugDraw::DrawGrid( XMFLOAT3( c_fScale * 5.0f, 0, 0 ), XMFLOAT3( 0, 0, c_fScale * 2.0f ), XMFLOAT3( 0, 0, 0 ), 50, 20, 0xFF808080 );

    // Render sphere in center
    ATG::DebugDraw::DrawSphere( XMFLOAT3( 0.0f, 0.0f, 0.0f ), c_fScale * 0.01f, 0xFFAAAAAA );

    // Render the history of turn angles, oldest to newest, with newest as the largest sphere
    const FLOAT fScale = 1.0f / c_uNumHistoryEntries;
    for ( UINT i = 0; i < c_uNumHistoryEntries; i++ )
    {
        UINT uIndex = ( g_uRotationHistoryIndex + i ) % c_uNumHistoryEntries;
        FLOAT fTheta = g_fRotationHistory[ uIndex ];
        FLOAT fRadius = (FLOAT)(i + 1) * fScale * c_fScale * 0.025f;
        XMFLOAT3 vPosition = XMFLOAT3( c_fScale * cosf( fTheta ), 0.0f, c_fScale * sinf( fTheta ) );
        ATG::DebugDraw::DrawSphere( vPosition, fRadius, 0xFF00FF00 );
    }
 
    // Render radial search for flip angles
    const XMFLOAT3 vStart = XMFLOAT3( 0.0f, 0.0f, 0.0f );

    if ( g_bSearchForFlip || g_bStopSearch )
    {
        FLOAT fFadeScale = (FLOAT)( c_uNumFramesToFade - g_uNumFramesToFade ) / (FLOAT)c_uNumFramesToFade;
        FLOAT fRadius = c_fScale * fFadeScale * 0.6f;

        // Render search results
        for ( UINT i = 0; i < g_uSearchHistoryIndex; i++ )
        {
            FLOAT fDepthBias = 0.01f;
            FLOAT fTheta = g_fSearchHistory[ i ];

            XMFLOAT3 vEnd = XMFLOAT3( fRadius * cosf( fTheta ), fDepthBias, fRadius * sinf( fTheta ) );
            ATG::DebugDraw::DrawLineSegment( vStart, vEnd, 0xFF0000FF );
        }

        // Render the found flip angle
        if ( g_bFoundFlip )
        {
            FLOAT fDepthBias = 0.02f;
            FLOAT fTheta = g_fFlipAngle;

            XMFLOAT3 vPos = XMFLOAT3( fRadius * 1.2f * cosf( fTheta ), fDepthBias, fRadius * 1.2f * sinf( fTheta ) );
            XMFLOAT3 vEnd = XMFLOAT3( fRadius * 1.5f * cosf( fTheta ), fDepthBias, fRadius * 1.5f * sinf( fTheta ) );
            XMFLOAT3 vAxis = XMFLOAT3( vEnd.x - vPos.x, vEnd.y - vPos.y, vEnd.z - vPos.z );
            ATG::DebugDraw::DrawConeWireframe( vPos, vAxis, 0.02f, 0.0f, 0xFFFF0000 );
        }
    }

    PIXEndNamedEvent();
}