//--------------------------------------------------------------------------------------
// XGUI.h
//
// Xbox 360 GUI primitive drawing component declarations for TrueSkill(TM) viewing
// sample.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#ifndef XGUI_H
#define XGUI_H

#include <xtl.h>
#include <AtgApp.h>
 
namespace XGUI
{
    bool Initialise( LPDIRECT3DDEVICE9 pd3dDevice );

    // draw primitive functions
    void OutlinedRectangle( LPDIRECT3DDEVICE9 pd3dDevice, FLOAT fX, FLOAT fY,
                            FLOAT fWidth, FLOAT fHeight, DWORD dwColour );
    void SolidRectangle( LPDIRECT3DDEVICE9 pd3dDevice, FLOAT fX, FLOAT fY,
                         FLOAT fWidth, FLOAT fHeight, DWORD dwColour,
                         FLOAT fGradient = 0.0f );
    void MuSigmaRectangle( LPDIRECT3DDEVICE9 pd3dDevice, FLOAT fX, FLOAT fY,
                           FLOAT fWidth, FLOAT fHeight,
                           DWORD dwColour1, DWORD dwColour2, DWORD dwColour3,
                           FLOAT fMu, FLOAT fSigma, FLOAT fGradient = 0.0f );
    void DrawLine( LPDIRECT3DDEVICE9 pd3dDevice, FLOAT fX1, FLOAT fY1,
                   FLOAT fX2, FLOAT fY2, DWORD dwColour );
}

#endif // XGUI_H