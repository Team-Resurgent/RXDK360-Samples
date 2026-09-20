//--------------------------------------------------------------------------------------
// GamepadMesh.h
//
// Code to render a gamepad controller
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "AtgMesh.h"


// Structures for animating, highlighting, and texturing the gamepad controls.
extern BOOL*    g_ControlActive;

extern DWORD    CONTROL_UNKNOWN;
extern DWORD    CONTROL_BODY;
extern DWORD    CONTROL_LEFT_THUMBSTICK;
extern DWORD    CONTROL_RIGHT_THUMBSTICK;
extern DWORD    CONTROL_DPAD;
extern DWORD    CONTROL_BACK_BUTTON;
extern DWORD    CONTROL_START_BUTTON;
extern DWORD    CONTROL_A_BUTTON;
extern DWORD    CONTROL_B_BUTTON;
extern DWORD    CONTROL_X_BUTTON;
extern DWORD    CONTROL_Y_BUTTON;
extern DWORD    CONTROL_LEFT_TRIGGER;
extern DWORD    CONTROL_RIGHT_TRIGGER;
extern DWORD    CONTROL_LEFT_SHOULDER;
extern DWORD    CONTROL_RIGHT_SHOULDER;


//--------------------------------------------------------------------------------------
// Name: class GamepadMesh
// Desc: The gamepad mesh. This is overridden from the base class so that we can
//       provide a custom RenderCallback() function.
//--------------------------------------------------------------------------------------
class GamepadMesh : public ATG::Mesh
{
    LPDIRECT3DVERTEXSHADER9 m_pVS;
    LPDIRECT3DPIXELSHADER9 m_pPS;

public:
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    // Overridable callback function (called before each subset is rendered). 
    virtual BOOL    RenderCallback( DWORD dwSubset, const ATG::MESH_SUBSET* pSubset, DWORD dwFlags );

    virtual HRESULT Create( const CHAR* strFilename, const ATG::PackedResource* pResource );
    virtual HRESULT Update( ATG::GAMEPAD* pGamepad );
    virtual HRESULT RenderGamepad();

                    GamepadMesh();
    virtual         ~GamepadMesh();
};

