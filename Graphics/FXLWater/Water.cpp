//--------------------------------------------------------------------------------------
// Water.cpp
//
// Water implementation
// Water is rendered via the "Projected Grid" algorithm.
// 
// http://graphics.cs.lth.se/theses/projects/projgrid/
// 
// Vertices are generated in screen space, and projected onto the water plane.
// A water simulation runs on the GPU, with in turn drives displacement mapping when the
// water vertices are rendered.
// This particular sample employs simple sin waves to generate the water displacements
// on the GPU.  A more advanced technique would be to implement an IFFT on the GPU to
// drive the simulation.
// 
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "water.h"
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <vector>
#include <cassert>
#include "camera.h"


using std::vector;

#define WATER_Y_PLANE 0.f
#define TESSELLATION


// A simple vertex containing only position
typedef struct _GRID_VERTEX
{
    FLOAT x;
    FLOAT y;
    FLOAT z;
} GRID_VERTEX;


//--------------------------------------------------------------------------------------
// Name: Grid
// Desc: A small class that emits vertices in a grid pattern
//--------------------------------------------------------------------------------------
class Grid
{
public:
                Grid( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2 );
    Grid& operator=( const Grid& rhs )
    {
        if( this == &rhs )
            return *this;
        m_fX1 = rhs.m_fX1;
        m_fY1 = rhs.m_fY1;
        m_fX2 = rhs.m_fX2;
        m_fY2 = rhs.m_fY2;
        return *this;
    }
    VOID        EmitVertices( LPVOID pDest, INT nStride, INT nTotalSize );
    inline INT  getPrimtiveCount()
    {
        return m_nPrimitives;
    }
protected:
    const INT m_nX;
    const INT m_nY;
    const INT m_nVertices;
    const INT m_nPrimitives;
    FLOAT m_fX1;
    FLOAT m_fX2;
    FLOAT m_fY1;
    FLOAT m_fY2;
};


//--------------------------------------------------------------------------------------
// Name: Grid
// Desc: Grid constructor - allows the caller to retrieve mesh parameters after
//       the instance is instantiated.
//--------------------------------------------------------------------------------------
Grid::Grid( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2 ) : m_nX( 20 ),
                                                       m_nY( 20 ),
                                                       m_nPrimitives( m_nX * m_nY * 2 ),
                                                       m_nVertices( m_nPrimitives * 3 )
{
    m_fX1 = x1;
    m_fX2 = x2;
    m_fY1 = y1;
    m_fY2 = y2;
}


//--------------------------------------------------------------------------------------
// Name: EmitVertices
// Desc: Generate and write vertices describing the grid.
//--------------------------------------------------------------------------------------
VOID Grid::EmitVertices( LPVOID pDest, INT nStride, INT nTotalSize )
{
    assert( nTotalSize >= nStride * m_nVertices );

    GRID_VERTEX* p = ( GRID_VERTEX* )pDest;
    FLOAT dx = ( m_fX2 - m_fX1 ) / m_nX;
    FLOAT dy = ( m_fY2 - m_fY1 ) / m_nY;

    // Table describing dx and dy offsets to make a 2 triangle quad cell
    static INT offsetTable[] = { 0, 0,  1, 0,  0, 1,  0, 1,  1, 0,  1, 1 };

    // Loop through the entire grid, each vertex
    for( INT y = 0; y < m_nY; ++y )
    {
        for( INT x = 0; x < m_nX; ++x )
        {
            for( INT v = 0; v < 6; ++v )
            {
                p->x = m_fX1 + ( x + offsetTable[2 * v + 0] ) * dx;
                p->y = m_fY1 + ( y + offsetTable[2 * v + 1] ) * dy;
                p->z = 0.f;

                p = ( GRID_VERTEX* )( ( char* )p + nStride );
            }
        }
    }
}



//--------------------------------------------------------------------------------------
// Name: XWater()
// Desc: 
//--------------------------------------------------------------------------------------
class XWater : public Water
{
public:
                    XWater( Camera* pCamera, LPDIRECT3DDEVICE9 pD3DDevice, FXLEffectPool* pFXLPool );
                    ~XWater();
    virtual HRESULT Initialize();
    virtual VOID    SetProjectionMatrix( XMMATRIX& pProjectionMatrix );
    virtual VOID    Update( FLOAT fElapsedTime );
    virtual HRESULT Render();
    virtual VOID    SetWireframeEnable( bool bEnable )
    {
        m_bWireframe = bEnable;
    }
    virtual VOID    SetSimulationPause( bool bPause )
    {
        m_bSimulationPaused = bPause;
    }
    virtual VOID    BeginReflection();
    virtual VOID    EndReflection();
    virtual VOID    BeginRefraction();
    virtual VOID    EndRefraction();
protected:
    Camera* m_pCamera;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMMATRIX m_matProjector;
    XMMATRIX m_matViewProjINV;
    XMVECTOR m_vCameraPosition;

    XMVECTOR m_vUpperPlane;
    XMVECTOR m_vLowerPlane;
    XMVECTOR m_vPlane;
    XMVECTOR m_vNormal;

    LPDIRECT3DDEVICE9 m_pD3DDevice;
    LPDIRECT3DVERTEXDECLARATION9 m_pDecl;
    LPDIRECT3DVERTEXBUFFER9 m_pVB;
    LPDIRECT3DTEXTURE9 m_pReflMap;        // Reflection map
    LPDIRECT3DTEXTURE9 m_pRefrMap;        // Refraction map
    LPDIRECT3DSURFACE9 m_pReflRT;         // Reflection/Refration RT

    FXLEffectPool* m_pFXLPool;
    FXLEffect* m_pHeightmapComputer;
    FXLHANDLE m_hFXLHeightmapTime;

    FXLEffect* m_pNormalComputer;
    FXLHANDLE m_hFXLComputeSampler;
    LPDIRECT3DVERTEXDECLARATION9 m_pNormalDecl;
    LPDIRECT3DSURFACE9 m_pNormalRT;
    LPDIRECT3DSURFACE9 m_pBackbuffer;

    LPDIRECT3DTEXTURE9 m_pHeightMap;               // Height map
    LPDIRECT3DTEXTURE9 m_pNormalMap;               // Normal map
    LPDIRECT3DLINETEXTURE9 m_pFresnelMap;          // Precomputed Fresnel Term

    FXLEffect* m_pFXLWater;
    FXLHANDLE m_hFXLWaterEnvMap;
    FXLHANDLE m_hFXLWaterHeightMap;
    FXLHANDLE m_hFXLWaterEyePosition;
    FXLHANDLE m_hFXLWaterNormalMap;
    FXLHANDLE m_hFXLWaterFresnelMap;
    FXLHANDLE m_hFXLWaterReflMap;
    FXLHANDLE m_hFXLWaterRefrMap;

    UINT m_nPrimitives;

    FLOAT m_fWaveAmplitudeMax;

    bool m_bSimulationPaused;
    bool m_bWireframe;
protected:
    bool            ComputeRangeMatrix( XMMATRIX& matRange );

    virtual HRESULT ComputeNormalMap( LPDIRECT3DTEXTURE9 pNormal,
                                      LPDIRECT3DTEXTURE9 pHeight,
                                      XMVECTOR Scalings );
    virtual HRESULT ComputeHeightMap( LPDIRECT3DTEXTURE9 pHeight,
                                      XMVECTOR Scalings );
    virtual HRESULT Render( XMMATRIX& matRange );
};


//--------------------------------------------------------------------------------------
// Name: XWater()
// Desc: 
//--------------------------------------------------------------------------------------
XWater::XWater( Camera* pCamera, LPDIRECT3DDEVICE9 pD3DDevice, FXLEffectPool* pFXLPool ) : m_fWaveAmplitudeMax( 2.f ),
                                                                                           m_pCamera( pCamera ),
                                                                                           m_pFXLPool( pFXLPool ),
                                                                                           m_pD3DDevice( pD3DDevice )
{
    m_pD3DDevice->AddRef();
    m_pFXLPool->AddRef();

    m_bWireframe = false;
    m_bSimulationPaused = false;

    m_matView = XMMatrixIdentity();
    m_matProj = XMMatrixIdentity();

    m_vNormal = XMVectorSet( 0.f, 1.f, 0.f, 0.f );
    m_vPlane = XMPlaneFromPointNormal( XMVectorSet( 0.f, 0.f, 0.f, 1.f ), m_vNormal );
    m_vUpperPlane = XMPlaneFromPointNormal(
        XMVectorSet( 0.f, +m_fWaveAmplitudeMax, 0.f, 1.f ),
        m_vNormal
        );
    m_vLowerPlane = XMPlaneFromPointNormal(
        XMVectorSet( 0.f, -m_fWaveAmplitudeMax, 0.f, 1.f ),
        m_vNormal
        );
}


//--------------------------------------------------------------------------------------
// Name: ~XWater()
// Desc: destructor
//--------------------------------------------------------------------------------------
XWater::~XWater()
{
    if( m_pDecl )    m_pDecl->Release();
    if( m_pVB )      m_pVB->Release();
    if( m_pFXLPool ) m_pFXLPool->Release();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT XWater::Initialize()
{
    HRESULT hr;

    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0,  8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    // Create the vertex declaration for the height-map to normal-map converter
    hr = m_pD3DDevice->CreateVertexDeclaration( VertexElements, &m_pNormalDecl );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create heightmap vertex declaration\n" );
        return hr;
    }

    // Use custom EDRAM allocation to create the rendertargets.
    // The color rendertarget is placed at address 0 in EDRAM.
    D3DSURFACE_PARAMETERS SurfaceParams;
    memset( &SurfaceParams, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    SurfaceParams.Base = 0;

    // Render Target used in height/normal-map generation
    hr = m_pD3DDevice->CreateRenderTarget( 512, 512,
                                           D3DFMT_A16B16G16R16F,
                                           D3DMULTISAMPLE_NONE,
                                           0,
                                           FALSE,
                                           &m_pNormalRT,
                                           &SurfaceParams );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create 16F render target\n" );
        return hr;
    }

    // Render Target used in reflection/refractin map rendering
    hr = m_pD3DDevice->CreateRenderTarget( 512, 512,
                                           ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                           D3DMULTISAMPLE_NONE,
                                           0,
                                           FALSE,
                                           &m_pReflRT,
                                           &SurfaceParams );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create 8888 render target\n" );
        return hr;
    }

    if( FAILED( hr = m_pD3DDevice->GetBackBuffer( 0, 0, 0, &m_pBackbuffer ) ) )
    {
        ATG_PrintError( "Could not retrieve backbuffer\n" );
        return hr;
    }

    // Create the Water FXLite effect
    // This effect is used to render the water surface.
    // The effect is precompiled on the PC-side into a binary file.
    // The binary file is then loaded, and instantiated via FXLCreateEffect
    VOID* pCode;
    DWORD dwSize;
    hr = ATG::LoadFile( "game:\\Media\\Effects\\water.fxobj", &pCode, &dwSize );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not load FXL Effect (water)\n" );
        return hr;
    }
    hr = FXLCreateEffect( m_pD3DDevice, pCode, m_pFXLPool, &m_pFXLWater );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create FXL Effect (water)\n" );
        return hr;
    }

    ATG::UnloadFile( pCode );

    // Create the ComputeNormals FXLite effect
    // This effect is used compute normals from a heightmap
    hr = ATG::LoadFile( "game:\\Media\\Effects\\ComputeNormals.fxobj", &pCode, &dwSize );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not load FXL Effect (ComputeNormals)\n" );
        return hr;
    }

    hr = FXLCreateEffect( m_pD3DDevice, pCode, NULL, &m_pNormalComputer );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create FXL Effect (ComputeNormals)\n" );
        return hr;
    }

    ATG::UnloadFile( pCode );

    // Create the ComputeHeightmap effect
    // This effect is used compute a heightmap representing the water surface
    hr = ATG::LoadFile( "game:\\Media\\Effects\\ComputeHeightmap.fxobj", &pCode, &dwSize );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not load FXL Effect (ComputeHeightmap)\n" );
        return hr;
    }

    hr = FXLCreateEffect( m_pD3DDevice, pCode, NULL, &m_pHeightmapComputer );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create FXL Effect (ComputeHeightmap)\n" );
        return hr;
    }

    ATG::UnloadFile( pCode );

    // Cache Effect Parameter handles
    m_hFXLWaterEyePosition = m_pFXLWater->GetParameterHandle( "eye_W" );
    m_hFXLWaterFresnelMap = m_pFXLWater->GetParameterHandle( "fresnel_sampler" );
    m_hFXLWaterEnvMap = m_pFXLWater->GetParameterHandle( "envmap_sampler" );
    m_hFXLWaterHeightMap = m_pFXLWater->GetParameterHandle( "heightmap_sampler" );
    m_hFXLWaterNormalMap = m_pFXLWater->GetParameterHandle( "normal_sampler" );
    m_hFXLWaterReflMap = m_pFXLWater->GetParameterHandle( "refl_sampler" );
    m_hFXLWaterRefrMap = m_pFXLWater->GetParameterHandle( "refr_sampler" );
    m_hFXLHeightmapTime = m_pHeightmapComputer->GetParameterHandle( "fTime" );
    m_hFXLComputeSampler = m_pNormalComputer->GetParameterHandle( "heightmap_sampler" );

    // Water grid mesh declaration
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
#ifdef TESSELLATION
        { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
        { 0, 24, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 2 },
#endif
        D3DDECL_END()
    };

    if( FAILED( hr = m_pD3DDevice->CreateVertexDeclaration( decl, &m_pDecl ) ) )
    {
        ATG_PrintError( "Could not create water vertex declaration\n" );
        return hr;
    }


    // Instantiate a grid to be applied in clip space
    // This grid will be projected onto the water surface to generate vertices
    Grid grid( -1.f, +1.f, 1.f, -1.f );

    m_nPrimitives = grid.getPrimtiveCount();

    // Create a vertex buffer of sufficient size to hold the water vertices
    hr = m_pD3DDevice->CreateVertexBuffer( 3 * m_nPrimitives * sizeof( GRID_VERTEX ),
                                           0,
                                           0,
                                           D3DPOOL_DEFAULT,
                                           &m_pVB,
                                           NULL );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create water vertex buffer\n" );
        return hr;
    }

    LPVOID pVerts;
    if( FAILED( hr = m_pVB->Lock( 0, 0, &pVerts, 0 ) ) )
    {
        ATG_PrintError( "Could not lock water vertex buffer\n" );
        return hr;
    }

    // Load the vertex buffer with clip space positions
    grid.EmitVertices( pVerts,
                       sizeof( GRID_VERTEX ),
                       3 * m_nPrimitives * sizeof( GRID_VERTEX ) );

    m_pVB->Unlock();

    // The fresnel map is stored in a 1-D texture
    hr = m_pD3DDevice->CreateLineTexture( 256,
                                          1,
                                          0,
                                          D3DFMT_LIN_A8,
                                          D3DPOOL_DEFAULT,
                                          &m_pFresnelMap,
                                          NULL );

    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create fresnel map texture\n" );
        return hr;
    }

    // Load the fresnel map with precomputed terms
    D3DLOCKED_RECT lr;
    if( FAILED( hr = m_pFresnelMap->LockRect( 0, &lr, 0, 0 ) ) )
    {
        ATG_PrintError( "Could not lock fresnel map texture\n" );
        return hr;
    }

    BYTE* b = ( BYTE* )lr.pBits;
    for( INT i = 0; i < 256; ++i )
    {
        *b++ = ( BYTE )( 255.5f * D3DXFresnelTerm( ( FLOAT )i / 255.f, 1.33 ) );
    }

    m_pFresnelMap->UnlockRect( 0 );

    // Create the Normal map texture
    hr = m_pD3DDevice->CreateTexture( 256,
                                      256,
                                      1,
                                      0,
                                      D3DFMT_A16B16G16R16F_EXPAND,
                                      D3DPOOL_DEFAULT,
                                      &m_pNormalMap,
                                      NULL );

    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create water normal map\n" );
        return hr;
    }

    // Create the height map texture
    hr = m_pD3DDevice->CreateTexture( 256,
                                      256,
                                      1,
                                      0,
                                      D3DFMT_R16F,
                                      D3DPOOL_DEFAULT,
                                      &m_pHeightMap,
                                      NULL );

    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create water height map\n" );
        return hr;
    }

    // Reflection Map texture
    hr = m_pD3DDevice->CreateTexture( 512,
                                      512,
                                      1,
                                      0,
                                      ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                      D3DPOOL_DEFAULT,
                                      &m_pReflMap,
                                      NULL );

    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create water reflection map\n" );
        return hr;
    }

    // Refraction Map texture
    hr = m_pD3DDevice->CreateTexture( 512,
                                      512,
                                      1,
                                      0,
                                      ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                      D3DPOOL_DEFAULT,
                                      &m_pRefrMap,
                                      NULL );

    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create water refraction map\n" );
        return hr;
    }

    // Bind the textures to the water technique samplers
    m_pFXLWater->SetSampler( m_hFXLWaterHeightMap, m_pNormalMap );
    m_pFXLWater->SetSampler( m_hFXLWaterNormalMap, m_pNormalMap );
    m_pFXLWater->SetSampler( m_hFXLWaterFresnelMap, m_pFresnelMap );
    m_pFXLWater->SetSampler( m_hFXLWaterRefrMap, m_pRefrMap );
    m_pFXLWater->SetSampler( m_hFXLWaterReflMap, m_pReflMap );

    m_pNormalComputer->SetSampler( m_hFXLComputeSampler, m_pHeightMap );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetProjectionMatrix()
// Desc: 
//--------------------------------------------------------------------------------------
VOID XWater::SetProjectionMatrix( XMMATRIX& matProjection )
{
    m_matProj = matProjection;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: 
//--------------------------------------------------------------------------------------
VOID XWater::Update( FLOAT fElapsedTime )
{
    // NOTE: Optimization opportunity:
    //       If no water is within frustum the water simulation could be skipped
    static FLOAT fAnimationTime = 0.f;

    if( m_bSimulationPaused )
    {
        return;
    }

    fAnimationTime += fElapsedTime;
    if( fAnimationTime > 2 * D3DX_PI )
    {
        fAnimationTime -= 2 * D3DX_PI;
    }

    m_pHeightmapComputer->SetFloat( m_hFXLHeightmapTime, fAnimationTime );

    ComputeHeightMap( m_pHeightMap, XMVectorSet( 50.f, 0.5f, 50.f, 0.f ) );
    ComputeNormalMap( m_pNormalMap, m_pHeightMap, XMVectorSet( 50.f, 0.5f, 50.f, 0.f ) );
}



//--------------------------------------------------------------------------------------
// Name: ComputeHeightMap()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT XWater::ComputeHeightMap( LPDIRECT3DTEXTURE9 pHeight,
                                  XMVECTOR Scalings )
{
    FXLTECHNIQUE_DESC fxl_desc;
    FXLHANDLE hTechnique;

    static FLOAT verts[] =
    {
        -1.f, +1.f,  0.f, 0.f,
        +1.f, +1.f,  1.f, 0.f,
        -1.f, -1.f,  0.f, 1.f,
        +1.f, -1.f,  1.f, 1.f
    };

    m_pD3DDevice->SetRenderTarget( 0, m_pNormalRT );

    // Set effect parameters
    hTechnique = m_pHeightmapComputer->GetTechniqueHandleFromIndex( 0 );

    m_pHeightmapComputer->GetTechniqueDesc( hTechnique, &fxl_desc );
    m_pHeightmapComputer->BeginTechnique( hTechnique, FXL_RESTORE_DEFAULT_RENDER_STATE );
    m_pD3DDevice->SetVertexDeclaration( m_pNormalDecl );

    for( UINT i = 0; i < fxl_desc.Passes; ++i )
    {
        m_pHeightmapComputer->BeginPassFromIndex( i );
        m_pHeightmapComputer->Commit();
        m_pD3DDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, verts, 4 * sizeof( FLOAT ) );
        m_pHeightmapComputer->EndPass();
    }
    m_pHeightmapComputer->EndTechnique();

    m_pD3DDevice->Resolve( D3DRESOLVE_RENDERTARGET0,
                           NULL,
                           m_pHeightMap,
                           NULL,
                           0,
                           0,
                           NULL,
                           0.f,
                           0,
                           NULL );
    m_pD3DDevice->SetRenderTarget( 0, m_pBackbuffer );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ComputeNormalMap()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT XWater::ComputeNormalMap( LPDIRECT3DTEXTURE9 pNormal,
                                  LPDIRECT3DTEXTURE9 pHeight,
                                  XMVECTOR Scalings )
{
    FXLTECHNIQUE_DESC fxl_desc;
    FXLHANDLE hTechnique;

    static FLOAT verts[] =
    {
        -1.f, +1.f,  0.f, 0.f,
        +1.f, +1.f,  1.f, 0.f,
        -1.f, -1.f,  0.f, 1.f,
        +1.f, -1.f,  1.f, 1.f
    };

    m_pD3DDevice->SetRenderTarget( 0, m_pNormalRT );

    hTechnique = m_pNormalComputer->GetTechniqueHandleFromIndex( 0 );

    m_pNormalComputer->GetTechniqueDesc( hTechnique, &fxl_desc );
    m_pNormalComputer->BeginTechnique( hTechnique, FXL_RESTORE_DEFAULT_RENDER_STATE );
    m_pD3DDevice->SetVertexDeclaration( m_pNormalDecl );

    for( UINT i = 0; i < fxl_desc.Passes; ++i )
    {
        m_pNormalComputer->BeginPassFromIndex( i );
        m_pNormalComputer->Commit();
        m_pD3DDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, verts, 4 * sizeof( FLOAT ) );
        m_pNormalComputer->EndPass();
    }
    m_pNormalComputer->EndTechnique();

    m_pD3DDevice->Resolve( D3DRESOLVE_RENDERTARGET0,
                           NULL,
                           m_pNormalMap,
                           NULL,
                           0,
                           0,
                           NULL,
                           0.f,
                           0,
                           NULL );
    m_pD3DDevice->SetRenderTarget( 0, m_pBackbuffer );

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT XWater::Render()
{
    m_pCamera->GetViewMatrix( m_matView );
    m_pCamera->GetPosition( m_vCameraPosition );

    XMMATRIX matRange;
    if( ComputeRangeMatrix( matRange ) )
    {
        return Render( matRange );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ComputeRangeMatrix()
// Desc: Calculate the matrix that defines the minimum rectangle in which the frustrum
//       is located.
//       Returns true if any part of the water surface may be visible within the view
//       frustum, false otherwise.
//--------------------------------------------------------------------------------------
bool XWater::ComputeRangeMatrix( XMMATRIX& matRange )
{
    XMVECTOR UNUSED;
    XMMATRIX matViewProjINV = XMMatrixInverse( &UNUSED, m_matView * m_matProj );

    XMVECTOR vLocalCameraAhead = XMVectorSet( 0.f, 0.f, 1.f, 0.f );

    // Assume orthogonal and no scaling
    XMVECTOR vCameraAhead = XMVector3TransformNormal( vLocalCameraAhead,
                                                      XMMatrixTranspose( m_matView ) );

    XMVECTOR vFrustum[8];

    // Table describing which frustum points are connected together
    static INT cube[] =
    {
        0,1,    0,2,    2,3,    1,3,
        0,4,    2,6,    3,7,    1,5,
        4,6,    4,5,    5,7,    6,7
    };

    // Transform frustum points to worldspace
    vFrustum[0] = XMVector3TransformCoord( XMVectorSet( -1, -1, 0, +1 ), matViewProjINV );
    vFrustum[1] = XMVector3TransformCoord( XMVectorSet( +1, -1, 0, +1 ), matViewProjINV );
    vFrustum[2] = XMVector3TransformCoord( XMVectorSet( -1, +1, 0, +1 ), matViewProjINV );
    vFrustum[3] = XMVector3TransformCoord( XMVectorSet( +1, +1, 0, +1 ), matViewProjINV );
    vFrustum[4] = XMVector3TransformCoord( XMVectorSet( -1, -1, +1, +1 ), matViewProjINV );
    vFrustum[5] = XMVector3TransformCoord( XMVectorSet( +1, -1, +1, +1 ), matViewProjINV );
    vFrustum[6] = XMVector3TransformCoord( XMVectorSet( -1, +1, +1, +1 ), matViewProjINV );
    vFrustum[7] = XMVector3TransformCoord( XMVectorSet( +1, +1, +1, +1 ), matViewProjINV );


    // Check intersections with m_vUpperPlane and m_vLowerPlane 
    vector <XMVECTOR> vecvProjPoints;
    XMVECTOR v;
    FLOAT f;
    for( INT i = 0; i < 12; ++i )
    {
        INT src = cube[i * 2 + 0];
        INT dst = cube[i * 2 + 1];

        f = XMVector4Dot( m_vUpperPlane, vFrustum[src] ).x
            * XMVector4Dot( m_vUpperPlane, vFrustum[dst] ).x;
        if( f < 0 )
        {
            v = XMPlaneIntersectLine( m_vUpperPlane, vFrustum[src], vFrustum[dst] );
            vecvProjPoints.push_back( v );
        }

        f = XMVector4Dot( m_vLowerPlane, vFrustum[src] ).x
            * XMVector4Dot( m_vLowerPlane, vFrustum[dst] ).x;
        if( f < 0 )
        {
            v = XMPlaneIntersectLine( m_vLowerPlane, vFrustum[src], vFrustum[dst] );
            vecvProjPoints.push_back( v );
        }
    }

    // Check if any of the frustums vertices lie between the m_vUpperPlane and
    // m_vLowerPlane planes
    for( INT i = 0; i < 8; ++i )
    {
        f = XMVector4Dot( m_vUpperPlane, vFrustum[i] ).x
            * XMVector4Dot( m_vLowerPlane, vFrustum[i] ).x;
        if( f < 0 )
        {
            vecvProjPoints.push_back( vFrustum[i] );
        }
    }


    // Make sure the camera isn't too close to the plane
    FLOAT fHeightInPlane = XMVector3Dot( m_vLowerPlane, m_vCameraPosition ).x;

    XMVECTOR vAimpoint;
    XMVECTOR vAimpoint2;
    XMVECTOR vSurfaceNormal = m_vLowerPlane;

    // ASSUMPTION - water base plane is Y=0.
    vSurfaceNormal.w = 0.f;

    const FLOAT fUdge = 5.f;
    if( fHeightInPlane < fUdge )
    {
        m_vCameraPosition += vSurfaceNormal * ( fUdge - fHeightInPlane );
    }

    // Aim the projector at the point where the camera view-vector intersects the plane
    // if the camera is aimed away from the plane, mirror it's view-vector against the
    // plane
    v = vCameraAhead;
    f = XMPlaneDotNormal( m_vPlane, vCameraAhead ).x
        * XMPlaneDotCoord( m_vPlane, m_vCameraPosition ).x;
    if( f >= 0.f )
    {
        // Reflect the view vector
        v = vCameraAhead - 2 * m_vNormal * XMVector3Dot( vCameraAhead, m_vNormal );
    }
    vAimpoint = XMPlaneIntersectLine( m_vPlane,
                                      m_vCameraPosition,
                                      m_vCameraPosition + v );

    // Force the point the camera is looking at in a plane, and have the projector look
    // at it works well against horizon, even when camera is looking upwards doesn't
    // work straight down/up
    FLOAT af = fabs( XMPlaneDotNormal( m_vPlane, vCameraAhead ).x );

    vAimpoint2 = m_vCameraPosition + 10.f * vCameraAhead;
    vAimpoint2 = vAimpoint2 - m_vNormal * XMVector3Dot( vAimpoint2, m_vNormal );

    XMVECTOR infinite = XMVectorIsNaN( vAimpoint );
    if( XMVector3Dot( infinite, infinite ).x )
    {
        af = 1.f;
        vAimpoint = XMVectorZero();
    }

    // Fade between vAimpoint & vAimpoint2 depending on view angle
    vAimpoint = XMVectorLerp( vAimpoint, vAimpoint2, af );

    XMMATRIX matProjCameraView = XMMatrixLookAtLH( m_vCameraPosition,
                                                   vAimpoint,
                                                   XMVectorSet( 0.f, 1.f, 0.f, 0.f ) );

    for( vector <XMVECTOR>::iterator it = vecvProjPoints.begin();
         it != vecvProjPoints.end();
         ++it )
    {
        // project the point onto the surface plane
        *it = *it - m_vNormal * XMVector3Dot( *it, m_vNormal );

        *it = XMVector3TransformCoord( *it, matProjCameraView );

        *it = XMVector3TransformCoord( *it, m_matProj );
    }

    // get max/min x & y-values to determine how big the "projection window" must be
    if( !vecvProjPoints.empty() )
    {
        vector <XMVECTOR>::iterator it = vecvProjPoints.begin();
        FLOAT fXmin = ( *it ).x;
        FLOAT fXmax = ( *it ).x;
        FLOAT fYmin = ( *it ).y;
        FLOAT fYmax = ( *it ).y;
        for( ++it; it != vecvProjPoints.end(); ++it )
        {
            if( ( *it ).x > fXmax ) fXmax = ( *it ).x;
            if( ( *it ).x < fXmin ) fXmin = ( *it ).x;
            if( ( *it ).y > fYmax ) fYmax = ( *it ).y;
            if( ( *it ).y < fYmin ) fYmin = ( *it ).y;
        }

        // build the packing matrix that spreads the grid across the "projection window"
        XMMATRIX matPack( ( fXmax - fXmin ) / 2,               0,        0, ( fXmin + fXmax ) / 2,
                          0, ( fYmax - fYmin ) / 2,        0, ( fYmin + fYmax ) / 2,
                          0,               0,        1,               0,
                          0,               0,        0,               1 );
        matPack = XMMatrixTranspose( matPack );
        matRange = matPack * XMMatrixInverse( &UNUSED, matProjCameraView * m_matProj );

        return true;
    }
    return false;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT XWater::Render( XMMATRIX& matRange )
{
    FXLTECHNIQUE_DESC fxl_desc;
    FXLHANDLE hTechnique;

    m_pFXLWater->SetVector( m_hFXLWaterEyePosition, m_vCameraPosition );

    hTechnique = m_pFXLWater->GetTechniqueHandleFromIndex( 0 );
    m_pFXLWater->GetTechniqueDesc( hTechnique, &fxl_desc );
    m_pFXLWater->BeginTechnique( hTechnique, FXL_RESTORE_DEFAULT_RENDER_STATE );

    XMMATRIX r = XMMatrixTranspose( matRange );
    m_pD3DDevice->SetVertexDeclaration( m_pDecl );

    for( UINT i = 0; i < fxl_desc.Passes; ++i )
    {
        m_pFXLWater->BeginPassFromIndex( i );
        m_pFXLWater->Commit();

        // render the surface
        m_pD3DDevice->SetVertexDeclaration( m_pDecl );
        m_pD3DDevice->SetVertexShaderConstantF( 24, ( FLOAT* )&r, 4 );
#ifdef TESSELLATION
        m_pD3DDevice->SetStreamSource( 0, m_pVB, 0, 3 * sizeof( GRID_VERTEX ) );
        m_pD3DDevice->DrawTessellatedPrimitive( D3DTPT_TRIPATCH, 0, m_nPrimitives );
#else
        m_pD3DDevice->SetStreamSource( 0, m_pVB, 0, sizeof( GRID_VERTEX ) );
        m_pD3DDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, m_nPrimitives );
#endif
        m_pFXLWater->EndPass();

        // The second pass renders the same mesh in reduced resolution wireframe black
        if( !m_bWireframe )
        {
            break;
        }
    }
    m_pFXLWater->EndTechnique();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: BeginReflection()
// Desc: 
//--------------------------------------------------------------------------------------
VOID XWater::BeginReflection()
{
    m_pD3DDevice->SetRenderTarget( 0, m_pReflRT );

    // Note: It is important to clear the alpha value to 0 here.
    //       When the water surface is rendered, the global reflection is used where
    //       alpha is 0, and the local reflections from the reflection map (rendered
    //       in this pass) are used where alpha is 1.
    m_pD3DDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0 );
}


//--------------------------------------------------------------------------------------
// Name: EndReflection()
// Desc: 
//--------------------------------------------------------------------------------------
VOID XWater::EndReflection()
{
    m_pD3DDevice->Resolve( D3DRESOLVE_RENDERTARGET0,
                           NULL,
                           m_pReflMap,
                           NULL,
                           0,
                           0,
                           NULL,
                           0.f,
                           0,
                           NULL );

    m_pD3DDevice->SetRenderTarget( 0, m_pBackbuffer );
}


//--------------------------------------------------------------------------------------
// Name: BeginRefraction()
// Desc: 
//--------------------------------------------------------------------------------------
VOID XWater::BeginRefraction()
{
    m_pD3DDevice->SetRenderTarget( 0, m_pReflRT );
    m_pD3DDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff2b4442, 1.0f, 0 );
}


//--------------------------------------------------------------------------------------
// Name: EndRefraction()
// Desc: 
//--------------------------------------------------------------------------------------
VOID XWater::EndRefraction()
{
    m_pD3DDevice->Resolve( D3DRESOLVE_RENDERTARGET0,
                           NULL,
                           m_pRefrMap,
                           NULL,
                           0,
                           0,
                           NULL,
                           0.f,
                           0,
                           NULL );

    m_pD3DDevice->SetRenderTarget( 0, m_pBackbuffer );
}



//--------------------------------------------------------------------------------------
// Name: Create()
// Desc: Creates a new water object
//--------------------------------------------------------------------------------------
Water* Water::Create( Camera* pCamera,
                      LPDIRECT3DDEVICE9 pD3DDevice,
                      FXLEffectPool* pFXLPool )
{
    XWater* pWater = new XWater( pCamera, pD3DDevice, pFXLPool );

    if( FAILED( pWater->Initialize() ) )
    {
        delete pWater;
        return NULL;
    }

    return ( Water* )pWater;
}

