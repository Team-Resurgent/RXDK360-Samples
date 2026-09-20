//--------------------------------------------------------------------------------------
// File: RingLayout.h
//
// Implements a series of rings in 3D space, and tracks the player's progress through
// the series of rings.
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include <xnamath.h>
#include <vector>
#include <AtgSceneAll.h>

#include "ParameterPool.h"

struct RingPlacement
{
    XMFLOAT4X4  m_matWorld;
    FLOAT       m_fActive;
};

class RingLayout
{
protected:
    static ATG::Scene*          s_pScene;
    static ATG::Model*          s_pRingModel;
    static ATG::Model*          s_pConeModel;

    std::vector<RingPlacement*> m_Rings;
    DWORD                       m_dwCurrentObjective;
    FLOAT                       m_fRaceTime;
    FLOAT                       m_fTime;
    DWORD                       m_dwScore;
    WCHAR                       m_strMessage[200];

public:
    static HRESULT LoadContent( ATG::BaseMaterial* pBaseMaterial );

    RingLayout();
    ~RingLayout();

    HRESULT CreateCourse( DWORD dwLevelIndex );

    VOID Reset();
    VOID Update( FLOAT fDeltaTime, const XMMATRIX& matAirplaneWorld );
    VOID Render( IDirect3DDevice9* pd3dDevice, UbershaderParameterPool* pParameterPool, XMMATRIX matViewProjection, XMVECTOR vDirLightWorldDirection );

    const WCHAR* GetMessage() const { return m_strMessage; }
    VOID NewCourseMessage( const WCHAR* strTitle );
    
protected:
    VOID Clear();
    VOID AddRing( CXMMATRIX matWorld );
    VOID AddRing( FXMVECTOR vPosition, FXMVECTOR vFacingDirection );
    VOID AddRingCurve( FXMVECTOR vStartPosition, FXMVECTOR vStartFacingDirection, FXMVECTOR vEndPosition, CXMVECTOR vEndFacingDirection, DWORD dwNumRings );

    VOID ObjectiveAttained( BOOL bThrough, BOOL bCorrectDirection );
    VOID UpdateMessage( DWORD dwEvent );

    VOID RenderModel( IDirect3DDevice9* pd3dDevice, UbershaderParameterPool* pParameterPool, ATG::Model* pModel, XMMATRIX matWorldViewProjection );
};
