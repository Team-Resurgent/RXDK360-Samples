//--------------------------------------------------------------------------------------
// Water.h
//
// Water definition
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <xtl.h>
#include <fxl.h>
#include "camera.h"



//--------------------------------------------------------------------------------------
// Name: Water
// Desc: Water interface definition
//--------------------------------------------------------------------------------------
class Water
{
public:
    virtual         ~Water()
    {
    };

    virtual VOID    SetProjectionMatrix( XMMATRIX& pProjectionMatrix ) = 0;
    virtual VOID    SetWireframeEnable( bool bEnable ) = 0;
    virtual VOID    SetSimulationPause( bool bPause ) = 0;

    // Set up resouces to begin/end reflection pass
    virtual VOID    BeginReflection() = 0;
    virtual VOID    EndReflection() = 0;

    // Set up resouces to begin/end refraction pass
    virtual VOID    BeginRefraction() = 0;
    virtual VOID    EndRefraction() = 0;

    // Update water physics
    virtual VOID    Update( FLOAT fElapsedTime ) = 0;

    // Render water mesh
    virtual HRESULT Render() = 0;

    // Release resources
    VOID            Destroy()
    {
        delete this;
    }

public:
    static  Water* Create( Camera* pCamera,
                           LPDIRECT3DDEVICE9 pD3DDevice,
                           FXLEffectPool* pFXLPool );
};

