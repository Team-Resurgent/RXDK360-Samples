//--------------------------------------------------------------------------------------
// CReflectiveShadowMap.h
//
// A helper class for rendering reflective shadowmaps (RSM) that can be used for
// calculating indirect lighting.
//
// This class is derived from CVarianceShadowMap, but could just as well have been
// derived from CShadowMap. RSM has no dependancy on VSM. The sample just looks much
// nicer with VSM than regular shadowmaps.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef CREFLECTIVESHADOWMAP_H
#define CREFLECTIVESHADOWMAP_H

#include "AdvancedLighting.h"
#include "CVarianceShadowMap.h"


//--------------------------------------------------------------------------------------
// Name: class CReflectiveShadowMap
// Desc: A helper class for rendering reflective shadowmaps (RSM).
//--------------------------------------------------------------------------------------
class CReflectiveShadowMap : public CVarianceShadowMap
{
public:
                    CReflectiveShadowMap();

    HRESULT         Initialize( ATG::PostProcess* pPostProcess, const DWORD dwShadowMapSize = 512 );
    virtual HRESULT PreRender();
    virtual HRESULT PostRender();

    D3DTexture* GetWorldSpacePositionsTexture() const
    {
        return m_pWorldSpacePositionsTexture;
    }
    D3DTexture* GetLightDirectionTexture() const
    {
        return m_pLightDirectionTexture;
    }
    D3DTexture* GetFluxTexture() const
    {
        return m_pFluxTexture;
    }
    D3DTexture* GetWorldSpacePositionsSmallTexture() const
    {
        return m_pWorldSpacePositionsSmallTexture;
    }
    D3DTexture* GetLightDirectionSmallTexture() const
    {
        return m_pLightDirectionSmallTexture;
    }
    D3DTexture* GetFluxSmallTexture() const
    {
        return m_pFluxSmallTexture;
    }
    D3DTexture* GetTexCoordTexture() const
    {
        return m_pTexCoordTexture;
    }

    FLOAT* GetLinearSampling() const
    {
        return ( FLOAT* )&m_fLinearSampling;
    }

    VOID            RenderSamplePoints( const FLOAT fWidth, const FLOAT fHeight );

protected:
    virtual HRESULT CreateTextures();
    virtual HRESULT CreateRenderTargets();

protected:
    // Shaders
    IDirect3DVertexShader9* m_pReflectiveShadowmapVS;           // RSM vertex shader
    IDirect3DPixelShader9* m_pReflectiveShadowmapPS;           // RSM pixel shader
    IDirect3DPixelShader9* m_pReflectiveShadowmapDebugPS;      // Debug shader for RSM
    IDirect3DPixelShader9* m_pImportanceSamplingPS;            // Shader for importance sampling
    IDirect3DPixelShader9* m_pGenerateSmallRSMTexturesPS;      // Shader for generating 1D textures

    // Textures
    D3DTexture* m_pWorldSpacePositionsTexture;      // RSM world space positions
    D3DTexture* m_pLightDirectionTexture;           // RSM light direction
    D3DTexture* m_pFluxTexture;                     // RSM flux

    D3DTexture* m_pWorldSpacePositionsSmallTexture; // 1D texture with world space positions
    D3DTexture* m_pLightDirectionSmallTexture;      // 1D texture with light directions
    D3DTexture* m_pFluxSmallTexture;                // 1D texture with flux
    D3DTexture* m_pTexCoordTexture;                 // 1D texture with texture coordinates

    D3DTexture* m_pHammersleySamplingTexture;       // Hammersley distribution sampling points
    D3DTexture* m_pImportanceSamplingTexture[ 2 ];  // Warped Hammersley points as importance sampling points

    // Rendertargets
    D3DSurface* m_pWorldSpacePositionsRT;
    D3DSurface* m_pLightDirectionRT;
    D3DSurface* m_pFluxRT;
    D3DSurface* m_pTexCoordRT;
    D3DSurface* m_pImportanceSamplingRT;

private:
    HRESULT         GenerateHammersleySamplingTexture();
    HRESULT         ImportanceSampling();
    HRESULT         GenerateSmallRSMTextures();

private:
    DWORD m_dwDoubleBufferCounter;
    FLOAT           m_fHammersleySampling[RSM_NUMSAMPLES ][ 4 ];
    FLOAT           m_fImportanceSampling[RSM_NUMSAMPLES ][ 4 ];
    FLOAT           m_fLinearSampling[RSM_NUMSAMPLES ][ 4 ];

};


#endif  // CREFLECTIVESHADOWMAP_H
