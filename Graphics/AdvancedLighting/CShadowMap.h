//--------------------------------------------------------------------------------------
// CShadowMap.h
//
// A helper class for rendering shadowmaps.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef CSHADOWMAP_H
#define CSHADOWMAP_H


//--------------------------------------------------------------------------------------
// Name: class CShadowMap
// Desc: A helper class for rendering shadowmaps.
//--------------------------------------------------------------------------------------
class CShadowMap
{
public:
                    CShadowMap();

    HRESULT         Initialize( const DWORD dwShadowMapSize = 512 );
    virtual HRESULT PreRender();
    virtual HRESULT PostRender();

    virtual D3DTexture* GetShadowMapTexture() const
    {
        return m_pShadowMapTexture;
    }
    DWORD           GetShadowMapSize() const
    {
        return m_dwShadowMapSize;
    }

protected:
    virtual HRESULT CreateTextures();
    virtual HRESULT CreateRenderTargets();

protected:
    IDirect3DVertexShader9* m_pDepthOnlyVS;               // Vertex shader for writing depth to shadowmap

    D3DSurface* m_pShadowMapDepthSurface;     // Depth-stencil render surface for the shadow map
    D3DTexture* m_pShadowMapTexture;          // Depth texture for shadow map

    D3DSurface* m_pCurrentBackBufferSurface;  // Save back buffer so we can restore it
    D3DSurface* m_pCurrentDepthBufferSurface; // Save depth buffer so we can restore it

    DWORD m_dwShadowMapSize;            // Dimension of shadow map

};


#endif  // CSHADOWMAP_H
