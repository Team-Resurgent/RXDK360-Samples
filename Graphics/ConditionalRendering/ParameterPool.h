//--------------------------------------------------------------------------------------
// ParameterPool.h
//
// A class that tracks parameter handles for engine parameters used in shaders, such
// as the world-view-projection matrix, lighting parameters, and more.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef PARAMETERPOOL_H
#define PARAMETERPOOL_H

#include <xtl.h>
#include <fxl.h>

static const DWORD  g_dwMaxPointLightCount = 6;
static const DWORD  g_dwMaxDirLightCount = 6;
static const DWORD  g_dwMaxSpotLightCount = 6;

class UbershaderParameterPool
{
public:
                UbershaderParameterPool()
                {
                    Terminate();
                }
    VOID        Initialize( FXLEffectPool* pEffectPool );
    VOID        Terminate();

    VOID        SetWorldViewProjMatrix( XMMATRIX matWVP )
    {
        if( m_hWorldViewProjMatrix == NULL )
            return;
        m_pEffectPool->SetMatrixF4x4A( m_hWorldViewProjMatrix, ( FLOAT* )&matWVP );
    }

    VOID        SetPointLightCount( DWORD dwCount )
    {
        const BOOL bTrue = TRUE;
        const BOOL bFalse = FALSE;
        for( DWORD dwIndex = 0; dwIndex < 6; ++dwIndex )
        {
            if( dwIndex < dwCount )
                m_pEffectPool->SetScalarB( m_hPointLightSwitches[dwIndex], &bTrue );
            else
                m_pEffectPool->SetScalarB( m_hPointLightSwitches[dwIndex], &bFalse );
        }
    }
    VOID        SetPointLightPosition( DWORD dwNumLight, XMVECTOR vPos )
    {
        if( m_hPointLightPos[ dwNumLight ] == NULL )
            return;
        m_pEffectPool->SetVectorF( m_hPointLightPos[ dwNumLight ], ( FLOAT* )&vPos );
    }
    VOID        SetPointLightColor( DWORD dwNumLight, XMVECTOR vColor )
    {
        if( m_hPointLightColor[ dwNumLight ] == NULL )
            return;
        m_pEffectPool->SetVectorF( m_hPointLightColor[ dwNumLight ], ( FLOAT* )&vColor );
    }

    VOID        SetSpotLightCount( DWORD dwCount )
    {
        const BOOL bTrue = TRUE;
        const BOOL bFalse = FALSE;
        for( DWORD dwIndex = 0; dwIndex < 6; ++dwIndex )
        {
            if( dwIndex < dwCount )
                m_pEffectPool->SetScalarB( m_hSpotLightSwitches[dwIndex], &bTrue );
            else
                m_pEffectPool->SetScalarB( m_hSpotLightSwitches[dwIndex], &bFalse );
        }
    }
    VOID        SetShadowedSpotLightCount( DWORD dwCount )
    {
        if( m_hShadowedSpotLightCount != NULL )
            m_pEffectPool->SetScalarI( m_hShadowedSpotLightCount, ( INT* )&dwCount );
    }
    VOID        SetSpotLightColor( DWORD dwNumLight, XMVECTOR vColor )
    {
        if( m_hSpotLightColor[dwNumLight] != NULL )
            m_pEffectPool->SetVectorF( m_hSpotLightColor[dwNumLight], ( FLOAT* )&vColor );
    }
    VOID        SetSpotLightPosition( DWORD dwNumLight, XMVECTOR vPos )
    {
        if( m_hSpotLightPos[dwNumLight] != NULL )
            m_pEffectPool->SetVectorF( m_hSpotLightPos[dwNumLight], ( FLOAT* )&vPos );
    }
    VOID        SetSpotLightObjectDir( DWORD dwNumLight, XMVECTOR vDir )
    {
        if( m_hSpotLightDirs[dwNumLight] != NULL )
            m_pEffectPool->SetVectorF( m_hSpotLightDirs[dwNumLight], ( FLOAT* )&vDir );
    }
    VOID        SetSpotLightAngles( DWORD dwNumLight, FLOAT fOuterAngle, FLOAT fInnerAngle )
    {
        fOuterAngle = cosf( fOuterAngle * 0.5f );
        fInnerAngle = cosf( fInnerAngle * 0.5f );
        XMFLOAT4 Angles;
        Angles.x = fOuterAngle;
        Angles.y = fInnerAngle;
        if( fInnerAngle - fOuterAngle < 0.0001f )
            Angles.z = 1e6f;
        else
            Angles.z = fabs( 1.0f / ( fInnerAngle - fOuterAngle ) );
        Angles.w = 0.0f;
        if( m_hSpotLightAngles[dwNumLight] != NULL )
            m_pEffectPool->SetVectorF( m_hSpotLightAngles[dwNumLight], ( FLOAT* )&Angles );
    }
    VOID        SetSpotLightShadowDepthTexture( DWORD dwNumLight, D3DBaseTexture* pTexture )
    {
        if( m_hSpotLightShadowTextures[dwNumLight] != NULL )
        {
            m_pEffectPool->SetSampler( m_hSpotLightShadowTextures[dwNumLight], pTexture );
        }
    }
    VOID        SetSpotLightProjMatrix( DWORD dwNumLight, const XMMATRIX& matLightViewProj )
    {
        if( m_hSpotLightProjMatrices[dwNumLight] != NULL )
            m_pEffectPool->SetMatrixF4x4A( m_hSpotLightProjMatrices[dwNumLight],
                                           ( const FLOAT* )&matLightViewProj );
    }

    VOID        SetDirLightCount( DWORD dwCount )
    {
        if( m_hDirLightCount != NULL )
            m_pEffectPool->SetScalarI( m_hDirLightCount, ( INT* )&dwCount );
    }
    VOID        SetDirLightColor( DWORD dwNumLight, XMVECTOR vColor )
    {
        if( m_hDirLightColor[dwNumLight] != NULL )
            m_pEffectPool->SetVectorF( m_hDirLightColor[dwNumLight], ( FLOAT* )&vColor );
    }
    VOID        SetDirLightObjectDir( DWORD dwNumLight, XMVECTOR vDir )
    {
        if( m_hDirLightDirs[dwNumLight] != NULL )
            m_pEffectPool->SetVectorF( m_hDirLightDirs[dwNumLight], ( FLOAT* )&vDir );
    }

    VOID        SetAmbient( XMVECTOR vColor )
    {
        if( m_hAmbient == NULL )
            return;
        m_pEffectPool->SetVectorF( m_hAmbient, ( FLOAT* )&vColor );
    }
    VOID        SetObjectViewDirection( XMVECTOR vDir )
    {
        if( m_hObjViewDir == NULL )
            return;
        m_pEffectPool->SetVectorF( m_hObjViewDir, ( FLOAT* )&vDir );
    }
    VOID        SetObjectViewPosition( XMVECTOR vPos )
    {
        if( m_hObjViewPos == NULL )
            return;
        m_pEffectPool->SetVectorF( m_hObjViewPos, ( FLOAT* )&vPos );
    }

    VOID        SetShadowMapMethod( INT iMethod )
    {
        if( m_hShadowMapMethod != NULL )
            m_pEffectPool->SetScalarI( m_hShadowMapMethod, &iMethod );
    }

private:
    VOID        SetupArrayHandles( const CHAR* strRootHandleName, FXLHANDLE* pElementHandles, DWORD dwElementCount );

private:
    FXLHANDLE m_hWorldViewProjMatrix;

    FXLHANDLE m_hPointLightCount;
    FXLHANDLE   m_hPointLightPos[ g_dwMaxPointLightCount ];
    FXLHANDLE   m_hPointLightColor[ g_dwMaxPointLightCount ];

    FXLHANDLE m_hDirLightCount;
    FXLHANDLE   m_hDirLightDirs[ g_dwMaxDirLightCount ];
    FXLHANDLE   m_hDirLightColor[ g_dwMaxDirLightCount ];

    FXLHANDLE m_hSpotLightCount;
    FXLHANDLE m_hShadowedSpotLightCount;
    FXLHANDLE   m_hSpotLightPos[ g_dwMaxSpotLightCount ];
    FXLHANDLE   m_hSpotLightColor[ g_dwMaxSpotLightCount ];
    FXLHANDLE   m_hSpotLightDirs[ g_dwMaxSpotLightCount ];
    FXLHANDLE   m_hSpotLightAngles[ g_dwMaxSpotLightCount ];
    FXLHANDLE   m_hSpotLightShadowTextures[ g_dwMaxSpotLightCount ];
    FXLHANDLE   m_hSpotLightProjMatrices[ g_dwMaxSpotLightCount ];

    FXLHANDLE   m_hPointLightSwitches[ g_dwMaxPointLightCount ];
    FXLHANDLE   m_hSpotLightSwitches[ g_dwMaxSpotLightCount ];

    FXLHANDLE m_hObjViewDir;
    FXLHANDLE m_hObjViewPos;
    FXLHANDLE m_hAmbient;

    FXLHANDLE m_hShadowMapMethod;

    FXLEffectPool* m_pEffectPool;
};

#endif
