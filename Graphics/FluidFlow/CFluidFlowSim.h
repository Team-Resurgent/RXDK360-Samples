//--------------------------------------------------------------------------------------
// CFluidFlowSim.h
//
// Fluid flow simulation based on Stam's "Stable Fluids" SIGGRAPH 99 paper  and Harris'
// flow implementation in OpenGL
//
// Authored by Pedro Sander, ATI Research
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------


// Number of injectors for the fluid flow system
#define NUM_INJECTORS 2


//--------------------------------------------------------------------------------------
// Name: class CFluidFlow
// Desc: 
//--------------------------------------------------------------------------------------
class CFluidFlow
{
public:
    DWORD m_dwFlowWidth;                       // Dimensions of the flow
    DWORD m_dwFlowHeight;

    FLOAT m_fTimeStep;
    FLOAT m_fRadius;

    DWORD m_dwNumPoissonSteps;
    FLOAT m_fLongevity;

    BOOL m_bImpulseToProcess;
    BOOL m_bMassToAdd;

    FLOAT m_fParticleResPower;

    // Intermediate textures and render targets to hold the fluid flow values
    LPDIRECT3DSURFACE9 m_pTextureRT;
    LPDIRECT3DTEXTURE9 m_pVelocityTexture;
    LPDIRECT3DTEXTURE9 m_pDensityTexture;
    LPDIRECT3DTEXTURE9 m_pDivergenceTexture;
    LPDIRECT3DTEXTURE9 m_pPressureTexture;
    LPDIRECT3DTEXTURE9 m_pRepelTexture;

    LPDIRECT3DSURFACE9 m_pParticleRT;
    LPDIRECT3DTEXTURE9 m_pPositionSourceTextureXY;

    D3DVertexBuffer m_ParticleVB;       // VertexBuffer wrapper for the position texture
    LPDIRECT3DVERTEXDECLARATION9 m_pParticleVtxDecl; // Vertex decl for the vertex buffer

    // Functions to run the fluid flow simulation
    VOID    AddImpulse();
    VOID    AdvectDensity();
    VOID    AdvectVelocity();
    VOID    Project();
    VOID    Update();
    VOID    AdvectParticles();
    VOID    RepelParticles();

    // Helper function
    VOID    RenderQuadToTexture( LPDIRECT3DSURFACE9 pRenderTarget,
                                 LPDIRECT3DTEXTURE9 pTexture,
                                 BOOL bRenderBoundaries = TRUE );

public:
            CFluidFlow();                                        // Constructor

    HRESULT Initialize( DWORD dwWidth, DWORD dwHeight ); // Initialize the fluid flow
    VOID    Reinit( FLOAT res );                         // Re-initialize
    VOID    Render();                                    // Render the fluid flow density
    VOID    RenderParticles();                           // Render particles in the fluid flow
};


//--------------------------------------------------------------------------------------
// Name: class CFluidFlowInjector
// Desc: Class to inject fluid into the fluid flow solver
//--------------------------------------------------------------------------------------
class CFluidFlowInjector
{
    XMVECTOR m_vPosition;
    XMVECTOR m_vPos1;
    XMVECTOR m_vPos2;
    XMVECTOR m_vPos3;
    XMVECTOR m_vPos4;
    FLOAT m_fAlpha;
    FLOAT m_fAlphaSpeed;
    BOOL m_bFirst;
    CFluidFlow* m_pFlow;

public:
    VOID    Inject( XMVECTOR* pvStrength, XMVECTOR* pvPosition, FLOAT* pfRadius );

            CFluidFlowInjector( CFluidFlow* pFlow=NULL )
            {
                m_pFlow = pFlow;
                m_bFirst = TRUE;
                m_fAlpha = 0.0f;
                m_fAlphaSpeed = 0.01f;
            }
};


//--------------------------------------------------------------------------------------
// Name: class CFluidFlowWithInjectors
// Desc: Class with injectors to inject fluid into the fluid flow solver
//--------------------------------------------------------------------------------------
class CFluidFlowWithInjectors : public CFluidFlow
{
public:
    CFluidFlowInjector  m_Inject[NUM_INJECTORS];

    VOID                Update();

                        CFluidFlowWithInjectors() : CFluidFlow()
                        {
                            for( DWORD i = 0; i < NUM_INJECTORS; i++ )
                            {
                                m_Inject[i] = CFluidFlowInjector( this );
                            }
                        }
};
