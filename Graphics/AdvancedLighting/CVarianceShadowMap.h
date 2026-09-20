//--------------------------------------------------------------------------------------
// CVarianceShadowMap.h
//
// A helper class for rendering variance shadowmaps (VSM).
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef CVARIANCESHADOWMAP_H
#define CVARIANCESHADOWMAP_H

#include <AtgPostProcess.h>
#include "CShadowMap.h"


//--------------------------------------------------------------------------------------
// Name: class CVarianceShadowMap
// Desc: A helper class for rendering variance shadowmaps (VSM).
//--------------------------------------------------------------------------------------
class CVarianceShadowMap : public CShadowMap
{
public:
                    CVarianceShadowMap();

    HRESULT         Initialize( ATG::PostProcess* pPostProcess, const DWORD dwShadowMapSize = 512 );
    virtual HRESULT PreRender();
    virtual HRESULT PostRender();

    virtual D3DTexture* GetShadowMapTexture() const
    {
        return m_pVarianceShadowMapTexture;
    }

protected:
    virtual HRESULT CreateTextures();
    VOID            BlurShadowMap();
    VOID            MipMapShadowMap();

protected:
    IDirect3DPixelShader9* m_pCopyDepthToVariancePS;           // Shader for depth to variance conversion
    IDirect3DPixelShader9* m_pHorizontalBlurDepthToVariancePS; // Shader for blurring variance horizontally
    IDirect3DPixelShader9* m_pVerticalBlurDepthToVariancePS;   // Shader for blurring variance vertically

    D3DTexture* m_pVarianceShadowMapTexture;        // Depth texture for the variance shadowmap

    ATG::PostProcess* m_pPostProcess;                     // ATG postprocessing helper class


};

#endif  // CVARIANCESHADOWMAP_H
