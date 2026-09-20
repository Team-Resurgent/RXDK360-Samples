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
static const DWORD  g_dwMaxDirLightCount = 1;
static const DWORD  g_dwMaxSpotLightCount = 6;

class SampleParameterPool
{
public:
                SampleParameterPool()
                {
                    Terminate();
                }
    VOID        Initialize( FXLEffectPool* pEffectPool, FXLEffect* pShaderLibraryEffect );
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
        for( DWORD dwIndex = 0; dwIndex < g_dwMaxPointLightCount; ++dwIndex )
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
        for( DWORD dwIndex = 0; dwIndex < g_dwMaxSpotLightCount; ++dwIndex )
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
        const BOOL bTrue = TRUE;
        const BOOL bFalse = FALSE;
        for( DWORD dwIndex = 0; dwIndex < g_dwMaxDirLightCount; ++dwIndex )
        {
            if( dwIndex < dwCount )
                m_pEffectPool->SetScalarB( m_hDirLightSwitches[dwIndex], &bTrue );
            else
                m_pEffectPool->SetScalarB( m_hDirLightSwitches[dwIndex], &bFalse );
        }
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
    VOID        SetDirLightShadowDepthTexture( DWORD dwNumLight, D3DBaseTexture* pTextureTight,
                                               D3DBaseTexture* pTextureScene )
    {
        if( m_hDirLightShadowTexturesTight[dwNumLight] != NULL )
        {
            m_pEffectPool->SetSampler( m_hDirLightShadowTexturesTight[dwNumLight], pTextureTight );
            m_pEffectPool->SetSampler( m_hDirLightShadowTexturesScene[dwNumLight], pTextureScene );
        }
    }
    VOID        SetDirLightProjMatrix( DWORD dwNumLight, const XMMATRIX& matLightViewProjTight,
                                       const XMMATRIX& matLightViewProjScene )
    {
        if( m_hDirLightProjMatricesTight[dwNumLight] != NULL )
        {
            m_pEffectPool->SetMatrixF4x4A( m_hDirLightProjMatricesTight[dwNumLight],
                                           ( const FLOAT* )&matLightViewProjTight );
            m_pEffectPool->SetMatrixF4x4A( m_hDirLightProjMatricesScene[dwNumLight],
                                           ( const FLOAT* )&matLightViewProjScene );
        }
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

    FXLHANDLE   GetShaderLibraryHandle( DWORD dwDirLightCount, DWORD dwPointLightCount, DWORD dwSpotLightCount )
    {
        assert( dwDirLightCount <= g_dwMaxDirLightCount );
        assert( dwPointLightCount <= g_dwMaxPointLightCount );
        assert( dwSpotLightCount <= g_dwMaxSpotLightCount );
        return m_hShaderLibraryTechniques[dwDirLightCount][dwPointLightCount][dwSpotLightCount];
    }

private:
    VOID        SetupArrayHandles( const CHAR* strRootHandleName, FXLHANDLE* pElementHandles, DWORD dwElementCount );

private:
    FXLHANDLE m_hWorldViewProjMatrix;

    FXLHANDLE   m_hPointLightPos[ g_dwMaxPointLightCount ];
    FXLHANDLE   m_hPointLightColor[ g_dwMaxPointLightCount ];

    FXLHANDLE   m_hDirLightDirs[ g_dwMaxDirLightCount ];
    FXLHANDLE   m_hDirLightColor[ g_dwMaxDirLightCount ];
    FXLHANDLE   m_hDirLightShadowTexturesTight[ g_dwMaxDirLightCount ];
    FXLHANDLE   m_hDirLightProjMatricesTight[ g_dwMaxDirLightCount ];
    FXLHANDLE   m_hDirLightShadowTexturesScene[ g_dwMaxDirLightCount ];
    FXLHANDLE   m_hDirLightProjMatricesScene[ g_dwMaxDirLightCount ];

    FXLHANDLE m_hShadowedSpotLightCount;
    FXLHANDLE   m_hSpotLightPos[ g_dwMaxSpotLightCount ];
    FXLHANDLE   m_hSpotLightColor[ g_dwMaxSpotLightCount ];
    FXLHANDLE   m_hSpotLightDirs[ g_dwMaxSpotLightCount ];
    FXLHANDLE   m_hSpotLightAngles[ g_dwMaxSpotLightCount ];
    FXLHANDLE   m_hSpotLightShadowTextures[ g_dwMaxSpotLightCount ];
    FXLHANDLE   m_hSpotLightProjMatrices[ g_dwMaxSpotLightCount ];

    FXLHANDLE   m_hPointLightSwitches[ g_dwMaxPointLightCount ];
    FXLHANDLE   m_hSpotLightSwitches[ g_dwMaxSpotLightCount ];
    FXLHANDLE   m_hDirLightSwitches[ g_dwMaxDirLightCount ];

    FXLHANDLE   m_hShaderLibraryTechniques[g_dwMaxDirLightCount + 1][g_dwMaxPointLightCount +
        1][g_dwMaxSpotLightCount + 1];

    FXLHANDLE m_hObjViewDir;
    FXLHANDLE m_hObjViewPos;
    FXLHANDLE m_hAmbient;

    FXLHANDLE m_hShadowMapMethod;

    FXLEffectPool* m_pEffectPool;
};

class DeferredParameterPool
{
public:
                DeferredParameterPool()
                {
                    Terminate();
                }
    VOID        Initialize( FXLEffectPool* pEffectPool, FXLEffect* pEffect );
    VOID        Terminate()
    {
        ZeroMemory( this, sizeof( DeferredParameterPool ) );
    }
    VOID        SetWorldViewProjMatrix( const XMMATRIX& matWVP )
    {
        if( m_hWorldViewProjMatrix != NULL )
            m_pEffectPool->SetMatrixF4x4( m_hWorldViewProjMatrix, ( FLOAT* )&matWVP );
    }
    VOID        SetWorldMatrix( const XMMATRIX& matWorld )
    {
        if( m_hWorldMatrix != NULL )
            m_pEffectPool->SetMatrixF4x4( m_hWorldMatrix, ( FLOAT* )&matWorld );
    }
    VOID        SetInvViewProjMatrix( const XMMATRIX& matInvViewProj )
    {
        if( m_hInvViewProjMatrix != NULL )
            m_pEffectPool->SetMatrixF4x4( m_hInvViewProjMatrix, ( FLOAT* )&matInvViewProj );
    }
    VOID        SetWorldViewDirection( const XMVECTOR& vWorldViewDirection )
    {
        if( m_hWorldViewDirection != NULL )
            m_pEffectPool->SetVectorF( m_hWorldViewDirection, ( FLOAT* )&vWorldViewDirection );
    }
    FXLHANDLE   GetTechniqueBufferConstruction() const
    {
        return m_hTechniqueBufferConstruction;
    }
    FXLHANDLE   GetTechniquePointLight() const
    {
        return m_hTechniquePointLight;
    }
    FXLHANDLE   GetTechniqueSpotLight() const
    {
        return m_hTechniqueSpotLight;
    }
    FXLHANDLE   GetTechniqueDirLight() const
    {
        return m_hTechniqueDirLight;
    }
    FXLHANDLE   GetTechniqueAmbientLight() const
    {
        return m_hTechniqueAmbientLight;
    }

    VOID        SetLightWorldPosRange( XMVECTOR vPosRange )
    {
        if( m_hLightWorldPosRange != NULL )
            m_pEffectPool->SetVectorF( m_hLightWorldPosRange, ( FLOAT* )&vPosRange );
    }
    VOID        SetLightColor( XMVECTOR vColor )
    {
        if( m_hLightColor != NULL )
            m_pEffectPool->SetVectorF( m_hLightColor, ( FLOAT* )&vColor );
    }
    VOID        SetLightWorldDir( XMVECTOR vDir )
    {
        if( m_hLightWorldDir != NULL )
            m_pEffectPool->SetVectorF( m_hLightWorldDir, ( FLOAT* )&vDir );
    }
    VOID        SetSpotLightAngles( FLOAT fOuterAngle, FLOAT fInnerAngle )
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
        if( m_hSpotLightAngles != NULL )
            m_pEffectPool->SetVectorF( m_hSpotLightAngles, ( FLOAT* )&Angles );
    }

    VOID        SetDepthBufferTexture( D3DBaseTexture* pTexture )
    {
        if( m_hDepthBufferTexture != NULL )
            m_pEffectPool->SetSampler( m_hDepthBufferTexture, pTexture );
    }
    VOID        SetColorBufferTexture( D3DBaseTexture* pTexture )
    {
        if( m_hColorBufferTexture != NULL )
            m_pEffectPool->SetSampler( m_hColorBufferTexture, pTexture );
    }
    VOID        SetNormalBufferTexture( D3DBaseTexture* pTexture )
    {
        if( m_hNormalBufferTexture != NULL )
            m_pEffectPool->SetSampler( m_hNormalBufferTexture, pTexture );
    }
    VOID        SetShadowParameters( D3DBaseTexture* pTexture, const XMMATRIX& matLightWVPMatrix )
    {
        if( pTexture == NULL )
        {
            if( m_hShadowingEnabled != NULL )
            {
                BOOL bEnabled = FALSE;
                m_pEffectPool->SetScalarB( m_hShadowingEnabled, &bEnabled );
            }
        }
        else
        {
            if( m_hShadowingEnabled != NULL )
            {
                BOOL bEnabled = TRUE;
                m_pEffectPool->SetScalarB( m_hShadowingEnabled, &bEnabled );
            }
            if( m_hShadowBufferTexture != NULL )
            {
                m_pEffectPool->SetSampler( m_hShadowBufferTexture, pTexture );
            }
            if( m_hLightWVPMatrix != NULL )
            {
                m_pEffectPool->SetMatrixF4x4( m_hLightWVPMatrix, ( FLOAT* )&matLightWVPMatrix );
            }
        }
    }

    VOID        SetShadowParametersScene( D3DBaseTexture* pTextureScene, const XMMATRIX& matLightWVPMatrixScene )
    {
        if( m_hShadowBufferTextureScene != NULL )
        {
            m_pEffectPool->SetSampler( m_hShadowBufferTextureScene, pTextureScene );
        }
        if( m_hLightWVPMatrixScene != NULL )
        {
            m_pEffectPool->SetMatrixF4x4( m_hLightWVPMatrixScene, ( FLOAT* )&matLightWVPMatrixScene );
        }
    }

    VOID        SetShadowMapMethod( INT iMethod )
    {
        if( m_hShadowMapMethod != NULL )
            m_pEffectPool->SetScalarI( m_hShadowMapMethod, &iMethod );
    }
private:
    FXLEffectPool* m_pEffectPool;

    FXLHANDLE m_hTechniqueBufferConstruction;
    FXLHANDLE m_hTechniqueAmbientLight;
    FXLHANDLE m_hTechniquePointLight;
    FXLHANDLE m_hTechniqueSpotLight;
    FXLHANDLE m_hTechniqueDirLight;

    FXLHANDLE m_hWorldViewProjMatrix;
    FXLHANDLE m_hWorldMatrix;
    FXLHANDLE m_hWorldViewDirection;
    FXLHANDLE m_hInvViewProjMatrix;

    FXLHANDLE m_hLightColor;
    FXLHANDLE m_hLightWorldDir;
    FXLHANDLE m_hLightWorldPosRange;
    FXLHANDLE m_hSpotLightAngles;

    FXLHANDLE m_hShadowingEnabled;
    FXLHANDLE m_hLightWVPMatrix;
    FXLHANDLE m_hShadowBufferTexture;
    FXLHANDLE m_hLightWVPMatrixScene;
    FXLHANDLE m_hShadowBufferTextureScene;

    FXLHANDLE m_hShadowMapMethod;

    FXLHANDLE m_hColorBufferTexture;
    FXLHANDLE m_hNormalBufferTexture;
    FXLHANDLE m_hDepthBufferTexture;
};


class PassPerLightParameterPool
{
public:
                PassPerLightParameterPool()
                {
                    Terminate();
                }
    VOID        Initialize( FXLEffectPool* pEffectPool, FXLEffect* pEffect );
    VOID        Terminate()
    {
        ZeroMemory( this, sizeof( PassPerLightParameterPool ) );
    }
    VOID        SetWorldViewProjMatrix( const XMMATRIX& matWVP )
    {
        if( m_hWorldViewProjMatrix != NULL )
            m_pEffectPool->SetMatrixF4x4( m_hWorldViewProjMatrix, ( FLOAT* )&matWVP );
    }
    FXLHANDLE   GetTechniquePointLight() const
    {
        return m_hTechniquePointLight;
    }
    FXLHANDLE   GetTechniqueSpotLight() const
    {
        return m_hTechniqueSpotLight;
    }
    FXLHANDLE   GetTechniqueDirLight() const
    {
        return m_hTechniqueDirLight;
    }
    FXLHANDLE   GetTechniqueAmbientLight() const
    {
        return m_hTechniqueAmbientLight;
    }

    VOID        SetLightObjPosRange( XMVECTOR vPosRange )
    {
        if( m_hLightObjPosRange != NULL )
            m_pEffectPool->SetVectorF( m_hLightObjPosRange, ( FLOAT* )&vPosRange );
    }
    VOID        SetLightColor( XMVECTOR vColor )
    {
        if( m_hLightColor != NULL )
            m_pEffectPool->SetVectorF( m_hLightColor, ( FLOAT* )&vColor );
    }
    VOID        SetLightObjDir( XMVECTOR vDir )
    {
        if( m_hLightObjDir != NULL )
            m_pEffectPool->SetVectorF( m_hLightObjDir, ( FLOAT* )&vDir );
    }
    VOID        SetSpotLightAngles( FLOAT fOuterAngle, FLOAT fInnerAngle )
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
        if( m_hSpotLightAngles != NULL )
            m_pEffectPool->SetVectorF( m_hSpotLightAngles, ( FLOAT* )&Angles );
    }

    VOID        SetShadowParameters( D3DBaseTexture* pTexture, const XMMATRIX& matLightWVPMatrix )
    {
        if( pTexture == NULL )
        {
            if( m_hShadowingEnabled != NULL )
            {
                BOOL bEnabled = FALSE;
                m_pEffectPool->SetScalarB( m_hShadowingEnabled, &bEnabled );
            }
        }
        else
        {
            if( m_hShadowingEnabled != NULL )
            {
                BOOL bEnabled = TRUE;
                m_pEffectPool->SetScalarB( m_hShadowingEnabled, &bEnabled );
            }
            if( m_hShadowBufferTexture != NULL )
            {
                m_pEffectPool->SetSampler( m_hShadowBufferTexture, pTexture );
            }
            if( m_hLightWVPMatrix != NULL )
            {
                m_pEffectPool->SetMatrixF4x4( m_hLightWVPMatrix, ( FLOAT* )&matLightWVPMatrix );
            }
        }
    }

    VOID        SetShadowParametersScene( D3DBaseTexture* pTextureScene, const XMMATRIX& matLightWVPMatrixScene )
    {
        if( m_hShadowBufferTextureScene != NULL )
        {
            m_pEffectPool->SetSampler( m_hShadowBufferTextureScene, pTextureScene );
        }
        if( m_hLightWVPMatrixScene != NULL )
        {
            m_pEffectPool->SetMatrixF4x4( m_hLightWVPMatrixScene, ( FLOAT* )&matLightWVPMatrixScene );
        }
    }

    VOID        SetShadowMapMethod( INT iMethod )
    {
        if( m_hShadowMapMethod != NULL )
            m_pEffectPool->SetScalarI( m_hShadowMapMethod, &iMethod );
    }
    VOID        SetObjectViewDirection( XMVECTOR vDir )
    {
        if( m_hObjViewDir == NULL )
            return;
        m_pEffectPool->SetVectorF( m_hObjViewDir, ( FLOAT* )&vDir );
    }
private:
    FXLEffectPool* m_pEffectPool;

    FXLHANDLE m_hTechniqueAmbientLight;
    FXLHANDLE m_hTechniquePointLight;
    FXLHANDLE m_hTechniqueSpotLight;
    FXLHANDLE m_hTechniqueDirLight;

    FXLHANDLE m_hWorldViewProjMatrix;

    FXLHANDLE m_hLightColor;
    FXLHANDLE m_hLightObjDir;
    FXLHANDLE m_hLightObjPosRange;
    FXLHANDLE m_hSpotLightAngles;
    FXLHANDLE m_hObjViewDir;

    FXLHANDLE m_hShadowMapMethod;

    FXLHANDLE m_hShadowingEnabled;
    FXLHANDLE m_hLightWVPMatrix;
    FXLHANDLE m_hShadowBufferTexture;
    FXLHANDLE m_hLightWVPMatrixScene;
    FXLHANDLE m_hShadowBufferTextureScene;
};

class PostEffectParameterPool
{
public:
            PostEffectParameterPool()
            {
                Terminate();
            }
    VOID    Initialize( FXLEffect* pEffect );
    VOID    Terminate()
    {
        ZeroMemory( this, sizeof( PostEffectParameterPool ) );
    }

    VOID    SetBuffers( D3DBaseTexture* pColorTexture, D3DBaseTexture* pDepthTexture )
    {
        if( m_hColorBuffer != NULL )
        {
            m_pEffect->SetSampler( m_hColorBuffer, pColorTexture );
        }
        if( m_hDepthBuffer != NULL )
        {
            m_pEffect->SetSampler( m_hDepthBuffer, pDepthTexture );
        }
    }
    VOID    SetFocalSettings( FLOAT fFocalDepth, FLOAT fFocalAperture, FLOAT fFocalSlope, FLOAT fMaxCoC )
    {
        if( m_hFocalSettings != NULL )
        {
            XMFLOAT4 Settings = XMFLOAT4( fFocalDepth, fFocalAperture, fFocalSlope, fMaxCoC );
            m_pEffect->SetVectorF( m_hFocalSettings, ( FLOAT* )&Settings );
        }
    }
private:
    FXLEffect* m_pEffect;

    FXLHANDLE m_hColorBuffer;
    FXLHANDLE m_hDepthBuffer;

    FXLHANDLE m_hFocalSettings;
};

#endif
