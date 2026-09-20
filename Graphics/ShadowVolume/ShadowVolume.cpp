//--------------------------------------------------------------------------------------
// ShadowVolume.cpp
//
// Sample code showing how to use stencil buffers to implement shadow volumes. The
// shadow volumes closed and are drawn using the "zfail" method and depth clamping so
// that they are fully robust. Two techniques for computing the shadow volume are shown:
// one using a simple fast CPU based algorithm and one using the GPU. Two techniques for
// drawing the shadow volume are shown: the typical two-pass solution and a one-pass
// solution.
//
// In the two-pass case, the geometry must be transformed twice, but the rasterization
// cost is slightly less. Since rendering shadow volumes tends to be fill-bound (in
// other words, the transform cost is masked by the fill cost), the two-pass case
// usually works out to be faster.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "ShadowMesh.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_1, L"Move airplane" },
    { ATG::HELP_RIGHTSTICK,   ATG::HELP_PLACEMENT_1, L"Move light" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"GPU silhouette\ngeneration" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Show\nshadow volume" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\nhi stencil" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// External definitions and prototypes
//--------------------------------------------------------------------------------------
#define FOG_COLOR 0xff0000ff

XMMATRIX    g_matView;
XMMATRIX    g_matProj;
XMVECTOR    g_vLightPos;

const DWORD VSCONT_matWorldViewProj = 0;  // World-view-projection matrix
const DWORD VSCONT_vLightPos = 4;  // Light position (may be infinite: w=0)
const DWORD VSCONT_fOffsetScale = 20;  // Offset scale
const DWORD VSCONT_fOffsetScale2 = 21;  // Offset scale for back-facing polys
const DWORD PSCONT_vConstantColor = 0;  // Constant diffuse color
const DWORD PSCONT_vDiffuseColor = 8;  // Diffuse color


//--------------------------------------------------------------------------------------
// Name: class CAirplaneMesh
// Desc: Derived from ATG::Mesh class to support selecting an appropriate pixel shader
//       based on the current subset's material settings
//--------------------------------------------------------------------------------------
class CAirplaneMesh : public ATG::Mesh
{
public:
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    LPDIRECT3DPIXELSHADER9 m_pTexturedPS;
    LPDIRECT3DPIXELSHADER9 m_pNonTexturedPS;

public:
    virtual BOOL RenderCallback( DWORD dwSubset, const ATG::MESH_SUBSET* pSubset, DWORD dwFlags )
    {
        if( pSubset->pTexture != NULL )
        {
            m_pd3dDevice->SetPixelShader( m_pTexturedPS );
        }
        else
        {
            m_pd3dDevice->SetPixelShader( m_pNonTexturedPS );
            m_pd3dDevice->SetPixelShaderConstantF( PSCONT_vDiffuseColor, ( FLOAT* )&pSubset->mtrl.Diffuse, 1 );
        }

        return TRUE;
    }
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::PackedResource m_xprResource;         // Packed resources for the app
    ATG::Font m_Font;                // Font class
    ATG::Help m_Help;                // Help class
    BOOL m_bDrawHelp;           // Whether to draw help

    ATG::Mesh m_TerrainObject;       // Terrain geometry
    XMMATRIX m_matTerrainMatrix;

    CAirplaneMesh m_AirplaneObject; // Object to render and cast shadows
    XMMATRIX m_matAirplaneMatrix;

    CShadowMeshCPU m_ShadowVolumeCPU;     // Shadow-casting object (using CPU to compute shadow volume)
    CShadowMeshGPU m_ShadowVolumeGPU;     // Shadow-casting object (using GPU to compute shadow volume)
    CShadowMesh* m_pShadowVolume;       // Currently active shadow volume (of the above two)

    BOOL m_bDrawUsingGPU;       // Use the GPU to create the volume.
    BOOL m_bShowShadowVolume;   // Draw the shadow volume (for debugging)
    BOOL m_bDrawUsingHiS;       // Use hi-stencil.
    BOOL m_bDrawSilhouette;     // For debugging, draw the shadow caster's silhouette

    LPDIRECT3DVERTEXSHADER9 m_pMeshVS;
    LPDIRECT3DPIXELSHADER9 m_pTexModDiffusePS;
    LPDIRECT3DPIXELSHADER9 m_pDiffuseOnlyPS;

    LPDIRECT3DVERTEXSHADER9 m_pScreenspaceVS;
    LPDIRECT3DPIXELSHADER9 m_pConstantColorPS;

    HRESULT RenderShadow();
    HRESULT RenderSilhouette();
    HRESULT DrawShadow();

private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the app
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    m_bDrawUsingGPU = TRUE;
    m_bShowShadowVolume = FALSE;
    m_bDrawUsingHiS = TRUE;
    m_bDrawSilhouette = FALSE;
    m_bDrawHelp = FALSE;
    m_matTerrainMatrix = XMMatrixIdentity();
    m_pShadowVolume = &m_ShadowVolumeCPU;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Load some terrain
    if( FAILED( m_TerrainObject.Create( "game:\\Media\\Meshes\\ShadowVolume_Terrain.xbg", &m_xprResource ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Load an object to cast the shadow
    if( FAILED( m_AirplaneObject.Create( "game:\\Media\\Meshes\\BiPlaneShadowProxy.xbg", &m_xprResource ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Generate data for quick shadow rendering for the mesh.
    if( FAILED( m_ShadowVolumeCPU.Create( &m_AirplaneObject ) ) )
        return E_FAIL;
    if( FAILED( m_ShadowVolumeGPU.Create( &m_AirplaneObject ) ) )
        return E_FAIL;

    // Create vertex shader for the CPU-based shadow algorithms
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
            D3DDECL_END()
        };

        m_pd3dDevice->CreateVertexDeclaration( decl, &m_ShadowVolumeCPU.m_pShadowVolumeVertexDecl );

        if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadowVolumeCpuVS.xvu",
                                                &m_ShadowVolumeCPU.m_pShadowVolumeVertexShader ) ) )
            return hr;
    }

    // Create vertex shader for the GPU-based shadow algorithms
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
            D3DDECL_END()
        };
        m_pd3dDevice->CreateVertexDeclaration( decl, &m_ShadowVolumeGPU.m_pShadowVolumeVertexDecl );

        if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadowVolumeGpuVS.xvu",
                                                &m_ShadowVolumeGPU.m_pShadowVolumeVertexShader ) ) )
            return hr;
    }

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 10.0f, -20.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    g_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    g_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 100.0f );

    // Load shaders for rendering meshes and visualizations
    ATG::LoadVertexShader( "game:\\Media\\Shaders\\MeshVS.xvu", &m_pMeshVS );
    ATG::LoadVertexShader( "game:\\Media\\Shaders\\ScreenspaceVS.xvu", &m_pScreenspaceVS );
    ATG::LoadPixelShader( "game:\\Media\\Shaders\\TexModDiffusePS.xpu", &m_pTexModDiffusePS );
    ATG::LoadPixelShader( "game:\\Media\\Shaders\\DiffuseOnlyPS.xpu", &m_pDiffuseOnlyPS );
    ATG::LoadPixelShader( "game:\\Media\\Shaders\\ConstantColorPS.xpu", &m_pConstantColorPS );

    // The airplane mesh has to switch between shaders based on whether or not a texture
    // is specified for each subset, so tell it which shaders to use:
    m_AirplaneObject.m_pd3dDevice = m_pd3dDevice;
    m_AirplaneObject.m_pTexturedPS = m_pTexModDiffusePS;
    m_AirplaneObject.m_pNonTexturedPS = m_pDiffuseOnlyPS;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle pure GPU generation of the silhouette.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bDrawUsingGPU = !m_bDrawUsingGPU;

        if( m_bDrawUsingGPU )
            m_pShadowVolume = &m_ShadowVolumeGPU;
        else
            m_pShadowVolume = &m_ShadowVolumeCPU;
    }

    // Toggle whether or not to show the shadowvolume
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bShowShadowVolume = !m_bShowShadowVolume;

    // Toggle use of hi-stencil.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bDrawUsingHiS = !m_bDrawUsingHiS;
    }

    // Setup viewing position from Gamepad
    static FLOAT fRotateX1 = 0.0f;
    static FLOAT fRotateY1 = 0.0f;
    fRotateX1 += pGamepad->fX1 * fElapsedTime * XM_PI * 0.5f;
    fRotateY1 += pGamepad->fY1 * fElapsedTime * XM_PI * 0.5f;
    m_matAirplaneMatrix = XMMatrixRotationRollPitchYaw( -fRotateY1, -fRotateX1, 0.0f );

    // Setup light position from Gamepad
    static FLOAT Lx = 0.0f;
    static FLOAT Ly = 1.0f;
    static FLOAT Lz = 0.0f;
    Lx = pGamepad->fX2;
    Lz = pGamepad->fY2;

    // Set the light position. For CPU shadow volumes, this will trigger a fair
    // amount of computation to re-build the shadow volume. For GPU meshes, this
    // function is trivial, as the real work is done in the vertex shader
    g_vLightPos = XMVectorSet( Lx, Ly, Lz, 0.0f );
    m_pShadowVolume->SetLightPos( g_vLightPos );

    m_pShadowVolume->m_matView = g_matView;
    m_pShadowVolume->m_matProj = g_matProj;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderShadow()
// Desc: Renders the shadow volume.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderShadow()
{
    // Disable z-buffer writes (note: z-testing still occurs)
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    // Set up stencil compare function, reference value, and masks.
    // Stencil test passes if ((ref & mask) cmpfn (stencil & mask)) is true.

    // Make sure that no pixels get drawn to the frame buffer
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );
    m_pd3dDevice->SetPixelShader( NULL );

    // With 2-sided stencil, we can avoid rendering twice:
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_TWOSIDEDSTENCILMODE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILMASK, 0xffffffff );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILWRITEMASK, 0xffffffff );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILREF, 0x00000001 );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCR );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
    m_pd3dDevice->SetRenderState( D3DRS_CCW_STENCILMASK, 0xffffffff );
    m_pd3dDevice->SetRenderState( D3DRS_CCW_STENCILWRITEMASK, 0xffffffff );
    m_pd3dDevice->SetRenderState( D3DRS_CCW_STENCILREF, 0x00000001 );
    m_pd3dDevice->SetRenderState( D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS );
    m_pd3dDevice->SetRenderState( D3DRS_CCW_STENCILPASS, D3DSTENCILOP_DECR );
    m_pd3dDevice->SetRenderState( D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_KEEP );
    m_pd3dDevice->SetRenderState( D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Setup the hi-stencil so that only tiles where all stencil values are zero 
    // will be set to "cull" when rendering the shadow.
    m_pd3dDevice->SetRenderState( D3DRS_HISTENCILWRITEENABLE, m_bDrawUsingHiS );
    m_pd3dDevice->SetRenderState( D3DRS_HISTENCILFUNC, D3DHSCMP_EQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_HISTENCILREF, 0 );

    // Draw both sides of shadow volume in stencil/z only
    m_pShadowVolume->m_matWorld = m_matAirplaneMatrix;
    m_pShadowVolume->RenderVolume();

    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_TWOSIDEDSTENCILMODE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    if( m_bShowShadowVolume )
    {
        m_pd3dDevice->SetPixelShader( m_pConstantColorPS );
        static XMFLOAT4 vColor( 0.0f, 0.0f, 0.25f, 1.0f );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONT_vConstantColor, ( FLOAT* )&vColor, 1 );

        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

        // Draw the volume for visualization purposes
        m_pShadowVolume->m_matWorld = m_matAirplaneMatrix;
        m_pShadowVolume->RenderVolume();
    }

    // Restore render states
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawShadow()
// Desc: Draws a big gray polygon over the scene according to the stencil mask.
//--------------------------------------------------------------------------------------
HRESULT Sample::DrawShadow()
{
    // Set up stencil test
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILREF, 0x00000000 );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );

    // Enable hi-stencil culling.    
    m_pd3dDevice->SetRenderState( D3DRS_HISTENCILENABLE, m_bDrawUsingHiS );

    // Set renderstates (disable z-buffering and turn on alphablending)
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    // Set the hardware to draw black, alpha-blended pixels
    m_pd3dDevice->SetPixelShader( m_pConstantColorPS );
    if( m_bShowShadowVolume )
    {
        static XMFLOAT4 vShadowColor( 1.0f, 0.0f, 0.0f, 0.5f );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONT_vConstantColor, ( FLOAT* )&vShadowColor, 1 );
    }
    else
    {
        static XMFLOAT4 vShadowColor( 0.0f, 0.0f, 0.0f, 0.5f );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONT_vConstantColor, ( FLOAT* )&vShadowColor, 1 );
    }

    // Draw the big, darkening square
    static XMFLOAT4 v[4];
    v[0] = XMFLOAT4( 0 - 0.5f, 0 - 0.5f, 0.0f, 1.0f );
    v[1] = XMFLOAT4( m_d3dpp.BackBufferWidth - 0.5f, 0 - 0.5f, 0.0f, 1.0f );
    v[2] = XMFLOAT4( m_d3dpp.BackBufferWidth - 0.5f, m_d3dpp.BackBufferHeight - 0.5f, 0.0f, 1.0f );
    v[3] = XMFLOAT4( 0 - 0.5f, m_d3dpp.BackBufferHeight - 0.5f, 0.0f, 1.0f );

    m_pd3dDevice->SetFVF( D3DFVF_XYZ );
    m_pd3dDevice->SetVertexShader( m_pScreenspaceVS );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );

    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, v, sizeof( v[0] ) );

    // Restore render states
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderSilhouette()
// Desc: For debugging, renders the silhouette of the shadow caster.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderSilhouette()
{
    m_pd3dDevice->SetPixelShader( m_pConstantColorPS );
    static XMFLOAT4 vColor( 1.0f, 1.0f, 1.0f, 1.0f );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONT_vConstantColor, ( FLOAT* )&vColor, 1 );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );

    // Draw the volume.
    m_pShadowVolume->m_matWorld = m_matAirplaneMatrix;
    m_pShadowVolume->RenderVolume();

    // Restore state
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d rendering.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Clear the viewport, zbuffer, and stencil buffer, and set hi-stencil to cull
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL | D3DCLEAR_HISTENCIL_CULL,
                         FOG_COLOR, 1.0f, 0L );

    // Set state
    {
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    }

    // Draw the terrain
    {
        m_pd3dDevice->SetVertexShader( m_pMeshVS );

        // World * View * Projection transform for terrain
        XMMATRIX matWVP = m_matTerrainMatrix * g_matView * g_matProj;
        matWVP = XMMatrixTranspose( matWVP );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONT_matWorldViewProj, ( FLOAT* )&matWVP, 4 );

        // Transform the light direction to local coordinates for terrain
        XMVECTOR vLocalLightDir = XMVectorSet( g_vLightPos.x, g_vLightPos.y, g_vLightPos.z, 0.0f );
        XMVECTOR vDeterminant;
        XMMATRIX matWorldInv;
        matWorldInv = XMMatrixInverse( &vDeterminant, m_matTerrainMatrix );
        vLocalLightDir = XMVector4Transform( vLocalLightDir, matWorldInv );
        vLocalLightDir = XMVector4Normalize( vLocalLightDir );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONT_vLightPos, ( FLOAT* )&vLocalLightDir, 1 );

        m_pd3dDevice->SetPixelShader( m_pTexModDiffusePS );
        m_TerrainObject.m_matWorld = m_matTerrainMatrix;
        m_TerrainObject.Render();
    }

    // Draw the airplane
    {
        XMMATRIX matWVP = m_matAirplaneMatrix * g_matView * g_matProj;
        matWVP = XMMatrixTranspose( matWVP );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONT_matWorldViewProj, ( FLOAT* )&matWVP, 4 );

        XMVECTOR vLocalLightDir = XMVectorSet( g_vLightPos.x, g_vLightPos.y, g_vLightPos.z, 0.0f );
        XMVECTOR vDeterminant;
        XMMATRIX matWorldInv;
        matWorldInv = XMMatrixInverse( &vDeterminant, m_matAirplaneMatrix );
        vLocalLightDir = XMVector4Transform( vLocalLightDir, matWorldInv );
        vLocalLightDir = XMVector4Normalize( vLocalLightDir );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONT_vLightPos, ( FLOAT* )&vLocalLightDir, 1 );

        m_AirplaneObject.m_matWorld = m_matAirplaneMatrix;
        m_AirplaneObject.Render();
    }

    // Render the shadow
    {
        RenderShadow();
    }

    // Our hi-stencil culling is not conservative because everything is initially set to 
    // cull and the previous rendering just changed to pass in places where we want 
    // to draw. So we have to call FlushHiZStencil to make sure the stencil results have 
    // been updated.
    if( m_bDrawUsingHiS )
    {
        m_pd3dDevice->FlushHiZStencil( D3DFHZS_SYNCHRONOUS );
    }

    // Draw a shadow using the stencil buffer as a mask
    {
        DrawShadow();
    }

    // For debuging, draw the silhouette used to build the shadow volume
    if( m_bDrawSilhouette )
        RenderSilhouette();

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"ShadowVolume" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.DrawText( 16, 40, 0xffffffff, L"Computing silhouette: " );
        m_Font.DrawText( 0xffffff00, m_bDrawUsingGPU ? L"On GPU" : L"On CPU" );

        m_Font.DrawText( 16, 60, 0xffffffff, L"Hi stencil: " );
        m_Font.DrawText( 0xffffff00, m_bDrawUsingHiS ? L"Yes" : L"No" );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


