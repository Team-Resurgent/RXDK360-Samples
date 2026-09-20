//--------------------------------------------------------------------------------------
// Dolphin.h
//
// Dolphin definition
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <xtl.h>
#include <fxl.h>
#include <xboxmath.h>
#include <AtgResource.h>


//--------------------------------------------------------------------------------------
// Name: Dolphin
// Desc: Interface Definition for a class describing 1 instance of a dolphin
//--------------------------------------------------------------------------------------
class Dolphin
{
public:

    // Sets the position this instance will swim towards
    virtual VOID    SetTargetPosition( const XMVECTOR& vTarget ) = 0;
    // Retrieves the current position of the instance
    virtual VOID    GetPosition( XMVECTOR& vPosition ) = 0;
    VOID            Destroy()
    {
        delete this;
    }

protected:

    virtual         ~Dolphin()
    {
    };
};


//--------------------------------------------------------------------------------------
// Name: Dolphin
// Desc: Interface Definition for a Container/Generator of Dolphin Instances
//--------------------------------------------------------------------------------------
class DolphinSchool
{
public:
    // Objects are rendered in three passes - into a refraction map, a reflection map,
    // and normally.
    typedef enum RENDERPASS
    {
        PASS_REFRACTION = 0,
        PASS_REFLECTION = 1,
        PASS_NORMAL     = 2
    };

    // Adds a dolphin to the school
    virtual Dolphin* AddDolphin( const XMVECTOR& vTarget, FLOAT fPitch, FLOAT fYaw ) =0;

    // Renders the school
    virtual HRESULT Render( RENDERPASS rpPass ) = 0;

    // Updates the scene
    virtual VOID    Update( FLOAT fElapsedTime ) = 0;

public:

    // Creates a Dolphin School
    static DolphinSchool* Create( LPDIRECT3DDEVICE9 pD3DDevice,
                                  FXLEffectPool* pFXLPool,
                                  ATG::PackedResource* pResource );
};
