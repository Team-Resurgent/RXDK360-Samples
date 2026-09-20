//--------------------------------------------------------------------------------------
// File: Airplane.h
//
// Implements airplane flight physics and rendering.
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include <xnamath.h>
#include <d3d9.h>
#include <AtgSceneAll.h>
#include "ParameterPool.h"

class Airplane
{
public:
    enum FlightState
    {
        Taxi = 0,
        Flying,
        Crashed
    };

protected:
    XMFLOAT4X4A     m_matWorld;
    XMFLOAT4A       m_vLinearVelocity;

    FLOAT           m_fThrottle;
    FlightState     m_FlightState;
    FLOAT           m_fEnginePower;

    FLOAT           m_fControlThrottle;
    FLOAT           m_fControlYAxis;
    FLOAT           m_fControlXAxis;
    FLOAT           m_fBankingAngle;

    FLOAT           m_fPropellerAngle;

    static ATG::Scene*  s_pScene;
    static ATG::Model*  s_pBiplaneModels[2];
    static ATG::Model*  s_pPropellerModel;

public:
    static HRESULT LoadContent( ATG::BaseMaterial* pBaseMaterial );

    Airplane();

    VOID Reset();
    VOID Reset( XMVECTOR vStartingPos, FLOAT fThrottle, XMVECTOR vVelocity );

    VOID SetControls( FLOAT fThrottle, FLOAT fYAxis, FLOAT fXAxis );
    VOID Update( FLOAT fDeltaTime );
    VOID Render( IDirect3DDevice9* pd3dDevice, UbershaderParameterPool* pParameterPool, XMMATRIX matViewProjection, XMVECTOR vDirLightWorldDirection );

    FlightState GetFlightState() const { return m_FlightState; }
    XMMATRIX GetWorldTransform() const { return XMLoadFloat4x4A( &m_matWorld ); }
    FLOAT GetVelocity() const { return XMVectorGetX( XMVector3LengthEst( XMLoadFloat4A( &m_vLinearVelocity ) ) ); }
    FLOAT GetAltitude() const { return m_matWorld._42; }
    FLOAT GetThrottle() const { return m_fThrottle; }

protected:
    VOID DetectCollision( XMVECTOR& vPosition, XMVECTOR& vVelocity, const XMVECTOR vPlaneUp );
    VOID RenderModel( IDirect3DDevice9* pd3dDevice, UbershaderParameterPool* pParameterPool, ATG::Model* pModel, XMMATRIX matWorldViewProjection );
};
