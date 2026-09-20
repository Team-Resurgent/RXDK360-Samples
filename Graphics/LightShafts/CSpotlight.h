//--------------------------------------------------------------------------------------
// CSpotLight.h
//
// Spotlight Class for Light Shaft rendering
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------
#ifndef CSPOTLIGHT_H
#define CSPOTLIGHT_H


//--------------------------------------------------------------------------------------
// Name: class CSpotlight
// Desc: Spotlight Class
//--------------------------------------------------------------------------------------
class CSpotlight
{
    // Specification of the spotlight frustum
    XMVECTOR m_vEyePt;
    XMVECTOR m_vLookAtPt;
    XMVECTOR m_vUpVec;
    FLOAT m_fFOV;
    FLOAT m_fAspect;

    FLOAT               m_Pad0[2];
    XMVECTOR            m_FrustumVerts[8]; // Light space positions of frustum vertices

    XMVECTOR            m_FrustumPlanes[6];
    BOOL m_bFarPlaneIsGroundPlane;

    FLOAT               m_Pad1[3];
    XMVECTOR m_vSceneLightPos;

    // Spotlight parameters
    FLOAT m_fWidth;
    FLOAT m_fHeight;

    FLOAT m_fNearWidth;
    FLOAT m_fNearHeight;
    FLOAT m_fVerticalFOV;
    FLOAT m_fFarWidth;
    FLOAT m_fFarHeight;

    // Light source
    UINT m_dwNumFrustumIndices;
    UINT m_dwNumFrustumVertices;
    LPDIRECT3DINDEXBUFFER9 m_pFrustumIB;
    LPDIRECT3DVERTEXBUFFER9 m_pFrustumVB;

    // Wireframe
    UINT m_dwNumWireFrustumIndices;
    UINT m_dwNumWireFrustumVertices;
    LPDIRECT3DINDEXBUFFER9 m_pWireFrustumIB;
    LPDIRECT3DVERTEXBUFFER9 m_pWireFrustumVB;

    // Transforms
    FLOAT               m_Pad2[1];
    XMMATRIX m_matView;
    XMMATRIX m_matProjection;

    // Textures and Surfaces
    LPDIRECT3DTEXTURE9 m_pCookie;

    HRESULT             RegenerateWireFrustum();

public:
                        CSpotlight();

    HRESULT             Initialize();

    HRESULT             Update( XMVECTOR vPos, XMVECTOR vLight, BOOL bCalcCaustics );

    // Setters
    HRESULT             SetView( XMVECTOR vEyePt, XMVECTOR vLookAtPt, XMVECTOR vUpVec,
                                 FLOAT fWidth, FLOAT fHeight, FLOAT fZNear, FLOAT fZFar );
    HRESULT             SetSceneLightPos( XMVECTOR vSceneLightPos );
    HRESULT             SetCookie( LPDIRECT3DTEXTURE9 pCookie );
    HRESULT             SetGroundPlaneClip( BOOL bFarPlaneIsGroundPlane );
    HRESULT             SetWidth( FLOAT fWidth );
    HRESULT             SetHeight( FLOAT fHeight );

    // Getters
    XMMATRIX            GetViewMatrix()
    {
        return m_matView;
    }
    XMMATRIX            GetProjectionMatrix()
    {
        return m_matProjection;
    }
    LPDIRECT3DTEXTURE9  GetCookie()
    {
        return m_pCookie;
    }
    FLOAT               GetWidth()
    {
        return m_fWidth;
    }
    FLOAT               GetHeight()
    {
        return m_fHeight;
    }
    HRESULT             GetViewSpaceBounds( XMVECTOR& vMinBounds, XMVECTOR& vMaxBounds );
    HRESULT             GetWorldSpaceFrustumPlanes( XMVECTOR* pWorldSpaceFrustumPlanes );

    // Drawing routines
    HRESULT             Draw();
    HRESULT             DrawClippingFrustum();
};


#endif // CSPOTLIGHT_H
