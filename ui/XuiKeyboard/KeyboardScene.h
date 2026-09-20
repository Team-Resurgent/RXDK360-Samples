//--------------------------------------------------------------------------------------
// KeyboardScene.h
//
// Declarations for the keyboard UI sample.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------



//--------------------------------------------------------------------------------------
// Declarations
//--------------------------------------------------------------------------------------

HRESULT InitUI( LPDIRECT3DDEVICE9 pd3dDevice, D3DPRESENT_PARAMETERS* pd3dpp );
VOID DispatchUIKeystroke( XINPUT_KEYSTROKE* pKeystroke );
VOID RenderUI( LPDIRECT3DDEVICE9 pDevice, UINT dwWidth, UINT dwHeight, float fDeltaTime );
VOID UninitUI();
