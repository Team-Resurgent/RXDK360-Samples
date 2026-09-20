//--------------------------------------------------------------------------------------
// RecordCommandBuffer.cpp
//
// A command line tool that loads content and renders it to command buffers.  The
// command buffers are then deconstructed and saved to disk.  The AdvancedCommandBuffers
// solution demonstrates loading these files and playing them back on the Xbox 360.
//
// If a title wished to incorporate precompiled command buffers into its art pipeline,
// a tool similar to this sample would be part of a content build process.
// Alternatively, code similar to this sample could be incorporated into the title
// itself, and then the title would be run in a mode that would generate the command
// buffer files for use by the final release build.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// unreferenced formal parameter
#pragma warning(disable:4100)
// __forceinline not inlined
#pragma warning(disable:4714)

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <xbdm.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <fxl.h>
#include <xboxmath.h>
#include <assert.h>
#include <xgraphics.h>
#include "XTLOnPC.h"
#include "AtgSceneAll.h"
#include "AtgUtil.h"

#include "CommandBufferFile.h"

//--------------------------------------------------------------------------------------
// Name: class FixupArray
// Desc: A vector of fixup descriptions.  Includes convenience methods for easily
//       generating fixup descs and adding them to the vector.
//--------------------------------------------------------------------------------------
class FixupArray
{
public:
    VOID    AddTextureFixup( D3DBaseTexture* pTexture, const WCHAR* strName )
    {
        // Creates a fixup for a texture and adds it to the vector.
        FIXUP_DESC Fixup = { 0 };
        Fixup.pResource = pTexture;
        Fixup.Type = FIXUP_TEXTURE;
        wcscpy_s( Fixup.strResourceName, strName );
        m_Fixups.push_back( Fixup );
    }
    VOID    AddVertexBufferFixup( D3DVertexBuffer* pVB, DWORD dwStreamIndex, const WCHAR* strName )
    {
        // Creates a fixup for a vertex buffer and adds it to the vector.
        FIXUP_DESC Fixup = { 0 };
        Fixup.pResource = pVB;
        Fixup.dwResourceExtraData = dwStreamIndex;
        Fixup.Type = FIXUP_VERTEXBUFFER;
        wcscpy_s( Fixup.strResourceName, strName );
        m_Fixups.push_back( Fixup );
    }
    VOID    AddIndexBufferFixup( D3DIndexBuffer* pIB, const WCHAR* strName )
    {
        // Creates a fixup for an index buffer and adds it to the vector.
        FIXUP_DESC Fixup = { 0 };
        Fixup.pResource = pIB;
        Fixup.Type = FIXUP_INDEXBUFFER;
        wcscpy_s( Fixup.strResourceName, strName );
        m_Fixups.push_back( Fixup );
    }
    VOID    AddVertexShaderFixup( D3DVertexShader* pVS, const WCHAR* strName )
    {
        // Creates a fixup for a vertex shader and adds it to the vector.
        FIXUP_DESC Fixup = { 0 };
        Fixup.pResource = pVS;
        Fixup.Type = FIXUP_VERTEXSHADER;
        wcscpy_s( Fixup.strResourceName, strName );
        m_Fixups.push_back( Fixup );
    }
    VOID    AddPixelShaderFixup( D3DPixelShader* pPS, const WCHAR* strName )
    {
        // Creates a fixup for a pixel shader and adds it to the vector.
        FIXUP_DESC Fixup = { 0 };
        Fixup.pResource = pPS;
        Fixup.Type = FIXUP_PIXELSHADER;
        wcscpy_s( Fixup.strResourceName, strName );
        m_Fixups.push_back( Fixup );
    }
    VOID    AddSurfacesFixup()
    {
        // Creates a fixup for a set of surfaces and adds it to the vector.
        FIXUP_DESC Fixup = { 0 };
        Fixup.Type = FIXUP_SURFACES;
        m_Fixups.push_back( Fixup );
    }
    VOID    AddViewportFixup()
    {
        // Creates a fixup for a viewport and adds it to the vector.
        FIXUP_DESC Fixup = { 0 };
        Fixup.Type = FIXUP_VIEWPORT;
        m_Fixups.push_back( Fixup );
    }
    VOID    AddClipRectFixup()
    {
        // Creates a fixup for a clip rect and adds it to the vector.
        FIXUP_DESC Fixup = { 0 };
        Fixup.Type = FIXUP_CLIPRECT;
        m_Fixups.push_back( Fixup );
    }
public:
    DWORD   Count() const
    {
        return m_Fixups.size();
    }
    std::vector <FIXUP_DESC> m_Fixups;
};

typedef std::vector <ATG::Model*>   ModelVector;

// Forward declarations for various rendering functions.
VOID EndianSwapFixupDesc( FIXUP_DESC* pFixup );
VOID CreateFixups( D3DCommandBuffer* pCommandBuffer, FixupArray& Fixups, DWORD dwFlags );
VOID SaveCommandBuffer( D3DCommandBuffer* pCommandBuffer, FixupArray& StaticFixups, FixupArray& DynamicFixups,
                        const WCHAR* strObjectName, BOOL bShadowRender );
VOID RenderRigidModel( ATG::Model* pModel, D3DCommandBuffer** ppCommandBuffer, FixupArray& StaticFixups,
                       FixupArray& DynamicFixups, BOOL bShadowRender );
VOID RenderSkinnedModel( ATG::Model* pModel, D3DCommandBuffer** ppCommandBuffer, FixupArray& StaticFixups,
                         FixupArray& DynamicFixups, BOOL bShadowRender );
VOID RenderEnvironment( ATG::Scene* pScene );
VOID RenderModelCommon( ATG::Model* pModel, FixupArray& StaticFixups, FixupArray& DynamicFixups, BOOL bShadowRender,
                        BOOL bRenderSolid = TRUE, BOOL bRenderOpaque = TRUE );
VOID RenderAABB( const ATG::AxisAlignedBox& AABB );
VOID MergeAABB( ATG::Bound& CurrentBound, const ATG::Bound& MergeBound );

// Global variables for rendering.
D3DDevice*                          g_pd3dDevice = NULL;
D3DVertexShader*                    g_pVertexShaderSkinningConstants = NULL;
D3DVertexShader*                    g_pVertexShaderTransform = NULL;
D3DVertexShader*                    g_pVertexShaderEnvironment = NULL;
D3DVertexShader*                    g_pVertexShaderQuery = NULL;
D3DVertexShader*                    g_pVertexShaderSkinningConstantsNullPShader = NULL;
D3DVertexShader*                    g_pVertexShaderTransformNullPShader = NULL;
D3DPixelShader*                     g_pPixelShaderNormalMapping = NULL;
D3DPixelShader*                     g_pPixelShaderEnvironment = NULL;
D3DVertexDeclaration*               g_pVertexDeclQuery = NULL;
D3DSurface*                         g_pRenderTarget = NULL;
D3DSurface*                         g_pDepthStencil = NULL;
D3DSurface*                         g_pDepthStencilShadow = NULL;

const D3DRECT*                      g_pTilingRects = NULL;
DWORD                               g_dwTilingRectCount = 0;

static const BOOL                   g_bRecordBuffersTiled = TRUE;

ATG::Scene*                         g_pScene = NULL;

//----------------------------------------------------------------------------
// Global operator overloads for new, delete, new[], and delete[].
// This ensures that all allocations on x86 targets are 16-byte aligned, since
// XMMATRIX or XMVECTOR member variables within sample framework classes
// require 16-byte alignment.
//----------------------------------------------------------------------------
#ifdef _M_IX86

VOID* operator new( size_t AllocationSize ) 
{ 
    return _aligned_malloc( AllocationSize, 16 ); 
}
VOID* operator new[]( size_t AllocationSize )
{
    return _aligned_malloc( AllocationSize, 16 ); 
}

VOID operator delete( VOID* pAllocation ) 
{ 
    _aligned_free( pAllocation ); 
}
VOID operator delete[]( VOID* pAllocation ) 
{ 
    _aligned_free( pAllocation ); 
}

#endif

//----------------------------------------------------------------------------
// Name: main
// Desc: Entry point for program
//----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
    HRESULT hr = Direct3D_CreateDevice( 0, D3DDEVTYPE_COMMAND_BUFFER, NULL, D3DCREATE_NO_SHADER_PATCHING,
                                        NULL, &g_pd3dDevice );
    if( FAILED( hr ) )
    {
        return 1;
    }

    // Load character scene.
    ATG::Scene* pCharacterScene = new ATG::Scene();
    ATG::ResourceDatabase* pRDB = pCharacterScene->GetResourceDatabase();
    pRDB->CreateDefaultResources();
    hr = ATG::SceneFileParser::LoadXATGFile( "media\\scenes\\SkinnedCharacter.xatg", pCharacterScene, NULL,
                                             ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load character scene." );
    g_pScene = pCharacterScene;

    // Extract models from the scene - there are four models comprising the character.
    ModelVector SkinnedModelList;
    ModelVector RigidModelList;
    {
        ATG::NameIndexedCollection::iterator i;
        for( i = pCharacterScene->GetInstanceList()->begin(); i != pCharacterScene->GetInstanceList()->end(); i++ )
        {
            // Look for models.
            if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
            {
                ATG::Model* pModel = ( ATG::Model* )( *i );
                if( pModel->GetNumMeshMappings() == 0 )
                    continue;
                ATG::MeshMapping& mm = pModel->GetMeshMapping( 0 );
                if( mm.pMesh->IsDerivedFrom( ATG::SkinnedMesh::TypeID ) )
                    SkinnedModelList.push_back( pModel );
                else
                    RigidModelList.push_back( pModel );
            }
        }
    }

    // Load shaders for the character.
    hr = ATG::LoadVertexShader( "media\\shaders\\SkinVSConstants.xvu", &g_pVertexShaderSkinningConstants );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader.\n" );
    hr = ATG::LoadVertexShader( "media\\shaders\\TransformVS.xvu", &g_pVertexShaderTransform );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader.\n" );
    hr = ATG::LoadVertexShader( "media\\shaders\\SkinVSConstants.xvu", &g_pVertexShaderSkinningConstantsNullPShader );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader.\n" );
    hr = ATG::LoadVertexShader( "media\\shaders\\TransformVS.xvu", &g_pVertexShaderTransformNullPShader );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader.\n" );
    hr = ATG::LoadPixelShader( "media\\shaders\\NormalMapPS.xpu", &g_pPixelShaderNormalMapping );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load pixel shader.\n" );

    // Load shaders for the environment.
    hr = ATG::LoadVertexShader( "media\\shaders\\VSBackground.xvu", &g_pVertexShaderEnvironment );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader.\n" );
    hr = ATG::LoadPixelShader( "media\\shaders\\PSBackground.xpu", &g_pPixelShaderEnvironment );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load pixel shader.\n" );

    // Load shaders for visibility queries.
    hr = ATG::LoadVertexShader( "media\\shaders\\VSQuery.xvu", &g_pVertexShaderQuery );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader.\n" );

    // Create vertex decl for visibility queries.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
        D3DDECL_END()
    };
    g_pVertexDeclQuery = D3DDevice_CreateVertexDeclaration( VertexElements );

    // Bind visibility vertex shader.
    DWORD dwStride = sizeof( XMFLOAT3 );
    g_pVertexShaderQuery->Bind( 0, g_pVertexDeclQuery, &dwStride, NULL );

    // Bind shader pair for rigid models.
    if( RigidModelList.size() > 0 )
    {
        ATG::Model* pModel = RigidModelList[0];
        ATG::BaseMesh* pMesh = pModel->GetMeshMapping( 0 ).pMesh;
        D3DVertexDeclaration* pDecl = pMesh->GetVertexData( 0 )->GetVertexDecl();
        DWORD dwStride = pMesh->GetVertexData( 0 )->GetVertexStream( 0 )->Stride;

        g_pVertexShaderTransform->Bind( 0, pDecl, &dwStride, g_pPixelShaderNormalMapping );
        // Create a bound vertex shader with a NULL pixel shader.
        g_pVertexShaderTransformNullPShader->Bind( 0, pDecl, &dwStride, NULL );
        // Bind the vertex and pixel shaders for the environment.
        g_pVertexShaderEnvironment->Bind( 0, pDecl, &dwStride, g_pPixelShaderEnvironment );
    }

    // Bind shader pair for skinned models.
    if( RigidModelList.size() > 0 )
    {
        ATG::Model* pModel = SkinnedModelList[0];
        ATG::BaseMesh* pMesh = pModel->GetMeshMapping( 0 ).pMesh;
        D3DVertexDeclaration* pDecl = pMesh->GetVertexData( 0 )->GetVertexDecl();
        DWORD dwStride = pMesh->GetVertexData( 0 )->GetVertexStream( 0 )->Stride;

        g_pVertexShaderSkinningConstants->Bind( 0, pDecl, &dwStride, g_pPixelShaderNormalMapping );
        // Create a bound vertex shader with a NULL pixel shader.
        g_pVertexShaderSkinningConstantsNullPShader->Bind( 0, pDecl, &dwStride, NULL );
    }

    // Create rendertargets.
    if( g_bRecordBuffersTiled )
    {
        g_pd3dDevice->CreateRenderTarget( 1280, 256, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_4_SAMPLES, 0,
                                          FALSE, &g_pRenderTarget, NULL );
        g_pd3dDevice->CreateDepthStencilSurface( 1280, 256, D3DFMT_D24S8, D3DMULTISAMPLE_4_SAMPLES, 0,
                                                 FALSE, &g_pDepthStencil, NULL );
    }
    else
    {
        g_pd3dDevice->CreateRenderTarget( 1280, 720, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &g_pRenderTarget,
                                          NULL );
        g_pd3dDevice->CreateDepthStencilSurface( 1280, 720, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0,
                                                 FALSE, &g_pDepthStencil, NULL );
    }

    // Create tiling rectangles.
    if( g_bRecordBuffersTiled )
    {
        static const D3DRECT TilingRects[] =
        {
            { 0,   0, 1280, 256 },
            { 0, 256, 1280, 512 },
            { 0, 512, 1280, 720 }
        };
        g_pTilingRects = TilingRects;
        g_dwTilingRectCount = ARRAYSIZE( TilingRects );
    }

    // Create shadow rendertarget at EDRAM offset 0 and at Hi-Z offset 0.
    D3DSURFACE_PARAMETERS ShadowSurfParams = { 0 };
    g_pd3dDevice->CreateDepthStencilSurface( 1024, 1024, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0,
                                             FALSE, &g_pDepthStencilShadow, &ShadowSurfParams );

    // Create command buffers for rigid models.
    DWORD dwModelCount = ( DWORD )RigidModelList.size();
    for( DWORD i = 0; i < dwModelCount; ++i )
    {
        ATG::Model* pModel = RigidModelList[i];

        // Create color rendering command buffer.
        {
            D3DCommandBuffer* pCommandBuffer = NULL;
            FixupArray StaticFixups;
            FixupArray DynamicFixups;
            RenderRigidModel( pModel, &pCommandBuffer, StaticFixups, DynamicFixups, FALSE );
            SaveCommandBuffer( pCommandBuffer, StaticFixups, DynamicFixups, pModel->GetName(), FALSE );
            pCommandBuffer->Release();
        }
        // Create shadow rendering command buffer.
        {
            D3DCommandBuffer* pCommandBuffer = NULL;
            FixupArray StaticFixups;
            FixupArray DynamicFixups;
            RenderRigidModel( pModel, &pCommandBuffer, StaticFixups, DynamicFixups, TRUE );
            SaveCommandBuffer( pCommandBuffer, StaticFixups, DynamicFixups, pModel->GetName(), TRUE );
            pCommandBuffer->Release();
        }
    }

    // Create command buffers for skinned models.
    dwModelCount = ( DWORD )SkinnedModelList.size();
    for( DWORD i = 0; i < dwModelCount; ++i )
    {
        ATG::Model* pModel = SkinnedModelList[i];

        // Create color rendering command buffer.
        {
            D3DCommandBuffer* pCommandBuffer = NULL;
            FixupArray StaticFixups;
            FixupArray DynamicFixups;
            RenderSkinnedModel( pModel, &pCommandBuffer, StaticFixups, DynamicFixups, FALSE );
            SaveCommandBuffer( pCommandBuffer, StaticFixups, DynamicFixups, pModel->GetName(), FALSE );
            pCommandBuffer->Release();
        }
        // Create shadow rendering command buffer.
        {
            D3DCommandBuffer* pCommandBuffer = NULL;
            FixupArray StaticFixups;
            FixupArray DynamicFixups;
            RenderSkinnedModel( pModel, &pCommandBuffer, StaticFixups, DynamicFixups, TRUE );
            SaveCommandBuffer( pCommandBuffer, StaticFixups, DynamicFixups, pModel->GetName(), TRUE );
            pCommandBuffer->Release();
        }
    }

    // Load construction site scene.
    ATG::Scene* pEnvironmentScene = new ATG::Scene();
    pRDB = pEnvironmentScene->GetResourceDatabase();
    pRDB->CreateDefaultResources();
    hr = ATG::SceneFileParser::LoadXATGFile( "media\\scenes\\ConstructionSite.xatg", pEnvironmentScene, NULL,
                                             ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load environment scene." );
    g_pScene = pEnvironmentScene;

    // Create command buffer for the construction site scene.
    RenderEnvironment( pEnvironmentScene );

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: RenderRigidModel
// Desc: Renders a rigid model to a command buffer.  All of the pertinent state setup
//       for the rigid model is included.
//--------------------------------------------------------------------------------------
VOID RenderRigidModel( ATG::Model* pModel, D3DCommandBuffer** ppCommandBuffer, FixupArray& StaticFixups,
                       FixupArray& DynamicFixups, BOOL bShadowRender )
{
    // Create command buffer.
    D3DCommandBuffer* pCommandBuffer = NULL;
    g_pd3dDevice->CreateCommandBuffer( 512 * 1024, 0, &pCommandBuffer );

    // Create inherit tags.  This data describes which pieces of GPU state will be inherited from the main D3D device when the command buffer is played back.
    D3DTAGCOLLECTION InheritTags = { 0 };
    // For rigid meshes, we need the first 8 vertex shader constants inherited.  This matches the TransformVS vertex shader.
    D3DTagCollection_SetVertexShaderConstantFTag( &InheritTags, 0, 8 );
    // We need the first 4 pixel shader constants inherited.  This matches the NormalMapPS pixel shader.
    D3DTagCollection_SetPixelShaderConstantFTag( &InheritTags, 0, 4 );

    // Start command buffer recording.
    if( g_bRecordBuffersTiled && !bShadowRender )
    {
        g_pd3dDevice->BeginCommandBuffer( pCommandBuffer, D3DBEGINCB_TILING_PREDICATE_COMPONENTS, &InheritTags, NULL,
                                          g_pTilingRects, g_dwTilingRectCount );
    }
    else
    {
        g_pd3dDevice->BeginCommandBuffer( pCommandBuffer, 0, &InheritTags, NULL, NULL, 0 );
    }

    // Set up rendertargets.
    if( bShadowRender )
    {
        g_pd3dDevice->SetRenderTarget( 0, NULL );
        g_pd3dDevice->SetDepthStencilSurface( g_pDepthStencilShadow );
    }
    else
    {
        g_pd3dDevice->SetRenderTarget( 0, g_pRenderTarget );
        g_pd3dDevice->SetDepthStencilSurface( g_pDepthStencil );
    }

    // In order to change the rendertargets at runtime, we must add static fixups for
    // three things: the surfaces themselves, the viewport, and the clip rect.
    StaticFixups.AddSurfacesFixup();
    StaticFixups.AddViewportFixup();
    // Clip rect fixups are not needed when we are recording a tiled command buffer.
    // Predicated tiling provides its own special clip rect configurations that do not need fixups.
    if( bShadowRender )
        StaticFixups.AddClipRectFixup();

    // Set up shaders.
    if( bShadowRender )
    {
        g_pd3dDevice->SetVertexShader( g_pVertexShaderTransformNullPShader );
        StaticFixups.AddVertexShaderFixup( g_pVertexShaderTransformNullPShader, L"TransformVS_NullPS" );
        g_pd3dDevice->SetPixelShader( NULL );
    }
    else
    {
        g_pd3dDevice->SetVertexShader( g_pVertexShaderTransform );
        StaticFixups.AddVertexShaderFixup( g_pVertexShaderTransform, L"TransformVS" );
        g_pd3dDevice->SetPixelShader( g_pPixelShaderNormalMapping );
        StaticFixups.AddPixelShaderFixup( g_pPixelShaderNormalMapping, L"NormalMapPS" );
    }

    RenderModelCommon( pModel, StaticFixups, DynamicFixups, bShadowRender );

    g_pd3dDevice->EndCommandBuffer();

    *ppCommandBuffer = pCommandBuffer;
}


//--------------------------------------------------------------------------------------
// Name: RenderSkinnedModel
// Desc: Renders a skinned model to a command buffer.  This method is very similar to
//       RenderRigidModel, but small changes in shader selection and shader constant
//       inheritance allow for character skinning.
//--------------------------------------------------------------------------------------
VOID RenderSkinnedModel( ATG::Model* pModel, D3DCommandBuffer** ppCommandBuffer, FixupArray& StaticFixups,
                         FixupArray& DynamicFixups, BOOL bShadowRender )
{
    // Create command buffer.
    D3DCommandBuffer* pCommandBuffer = NULL;
    g_pd3dDevice->CreateCommandBuffer( 512 * 1024, 0, &pCommandBuffer );

    // Create inherit tags.  This data describes which pieces of GPU state will be inherited from the main D3D device when the command buffer is played back.
    D3DTAGCOLLECTION InheritTags = { 0 };
    // For skinned meshes, we need all but the last 4 vertex shader constants inherited.  This matches the SkinVSConstants vertex shader.
    D3DTagCollection_SetVertexShaderConstantFTag( &InheritTags, 0, 252 );
    // We need the first 4 pixel shader constants inherited.  This matches the NormalMapPS pixel shader.
    D3DTagCollection_SetPixelShaderConstantFTag( &InheritTags, 0, 4 );

    // Start command buffer recording.
    if( g_bRecordBuffersTiled && !bShadowRender )
    {
        g_pd3dDevice->BeginCommandBuffer( pCommandBuffer, D3DBEGINCB_TILING_PREDICATE_COMPONENTS, &InheritTags, NULL,
                                          g_pTilingRects, g_dwTilingRectCount );
    }
    else
    {
        g_pd3dDevice->BeginCommandBuffer( pCommandBuffer, 0, &InheritTags, NULL, NULL, 0 );
    }

    // Set up rendertargets.
    if( bShadowRender )
    {
        g_pd3dDevice->SetRenderTarget( 0, NULL );
        g_pd3dDevice->SetDepthStencilSurface( g_pDepthStencilShadow );
    }
    else
    {
        g_pd3dDevice->SetRenderTarget( 0, g_pRenderTarget );
        g_pd3dDevice->SetDepthStencilSurface( g_pDepthStencil );
    }

    // In order to change the rendertargets at runtime, we must add static fixups for
    // three things: the surfaces themselves, the viewport, and the clip rect.
    StaticFixups.AddSurfacesFixup();
    StaticFixups.AddViewportFixup();
    // Clip rect fixups are not needed when we are recording a tiled command buffer.
    // Predicated tiling provides its own special clip rect configurations that do not need fixups.
    if( bShadowRender )
        StaticFixups.AddClipRectFixup();

    // Set up shaders.
    if( bShadowRender )
    {
        g_pd3dDevice->SetVertexShader( g_pVertexShaderSkinningConstantsNullPShader );
        StaticFixups.AddVertexShaderFixup( g_pVertexShaderSkinningConstantsNullPShader, L"SkinVSConstants_NullPS" );
        g_pd3dDevice->SetPixelShader( NULL );
    }
    else
    {
        g_pd3dDevice->SetVertexShader( g_pVertexShaderSkinningConstants );
        StaticFixups.AddVertexShaderFixup( g_pVertexShaderSkinningConstants, L"SkinVSConstants" );
        g_pd3dDevice->SetPixelShader( g_pPixelShaderNormalMapping );
        StaticFixups.AddPixelShaderFixup( g_pPixelShaderNormalMapping, L"NormalMapPS" );
    }

    RenderModelCommon( pModel, StaticFixups, DynamicFixups, bShadowRender );

    g_pd3dDevice->EndCommandBuffer();

    *ppCommandBuffer = pCommandBuffer;
}


//--------------------------------------------------------------------------------------
// Name: RenderEnvironment
// Desc: Renders a full scene to a command buffer.  This method first collects and sorts
//       all models in the scene by visibility group, and then renders each group
//       separately using conditional rendering.
//--------------------------------------------------------------------------------------
VOID RenderEnvironment( ATG::Scene* pScene )
{
    // Find the frames in the scene starting with "sector".
    // These frames indicate visibility groups.
    // Also find all of the models.
    std::vector <ATG::Frame*> SectorFrames;
    ModelVector AllModels;
    ATG::NameIndexedCollection::iterator i;
    for( i = pScene->GetInstanceList()->begin(); i != pScene->GetInstanceList()->end(); i++ )
    {
        // Look for models.
        if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );
            if( pModel->GetNumMeshMappings() == 0 )
                continue;
            AllModels.push_back( pModel );
        }
        // Look for frames.
        if( ( *i )->IsDerivedFrom( ATG::Frame::TypeID ) )
        {
            ATG::Frame* pFrame = ( ATG::Frame* )( *i );
            const WCHAR* strName = pFrame->GetName().GetSafeString();
            if( wcsstr( strName, L"sector" ) == strName )
            {
                if( strName[6] != L'S' )
                    SectorFrames.push_back( pFrame );
            }
        }
    }

    // Build an array of model vectors, one vector for each sector we found in the scene.
    std::vector <ModelVector> SectorModelVectors;
    DWORD dwSectorCount = ( DWORD )SectorFrames.size();
    ModelVector v;
    SectorModelVectors.resize( dwSectorCount, v );

    // Build an array of bounding volumes, one for each sector.
    std::vector <ATG::Bound> SectorBounds;
    ATG::Bound b;
    SectorBounds.resize( dwSectorCount, b );

    // For each model, determine which sector it is in, and add it to the proper model list.
    // Also, recompute the sector bounds.
    DWORD dwModelCount = ( DWORD )AllModels.size();
    for( DWORD i = 0; i < dwModelCount; ++i )
    {
        ATG::Model* pModel = AllModels[i];
        for( DWORD dwSector = 0; dwSector < dwSectorCount; ++dwSector )
        {
            ATG::Frame* pSectorFrame = SectorFrames[dwSector];
            if( pModel->IsAncestor( pSectorFrame ) )
            {
                SectorModelVectors[dwSector].push_back( pModel );
                MergeAABB( SectorBounds[dwSector], pModel->GetWorldBound() );
                break;
            }
        }
    }

    FixupArray StaticFixups;
    FixupArray DynamicFixups;

    // Create command buffer.
    D3DCommandBuffer* pCommandBuffer = NULL;
    g_pd3dDevice->CreateCommandBuffer( 1024 * 1024, 0, &pCommandBuffer );

    // Create inherit tags.  This data describes which pieces of GPU state will be inherited from the main D3D device when the command buffer is played back.
    D3DTAGCOLLECTION InheritTags = { 0 };
    // We need vertex shader constants 0-3 and 8-252 inherited.  This matches the VSBackground vertex shader.
    // Vertex shader constants 4-7 (the world matrix for each mesh) will be baked into the command buffer.
    D3DTagCollection_SetVertexShaderConstantFTag( &InheritTags, 0, 4 );
    D3DTagCollection_SetVertexShaderConstantFTag( &InheritTags, 8, 244 );
    // We need all of the pixel shader constants inherited except the last 4.  This matches the PSBackground pixel shader.
    D3DTagCollection_SetPixelShaderConstantFTag( &InheritTags, 0, 252 );

    // Start command buffer recording.
    if( g_bRecordBuffersTiled )
    {
        g_pd3dDevice->BeginCommandBuffer( pCommandBuffer, D3DBEGINCB_TILING_PREDICATE_COMPONENTS, &InheritTags, NULL,
                                          g_pTilingRects, g_dwTilingRectCount );
    }
    else
    {
        g_pd3dDevice->BeginCommandBuffer( pCommandBuffer, 0, &InheritTags, NULL, NULL, 0 );
    }

    // Set up rendertargets.
    g_pd3dDevice->SetRenderTarget( 0, g_pRenderTarget );
    g_pd3dDevice->SetDepthStencilSurface( g_pDepthStencil );

    // In order to change the rendertargets at runtime, we must add static fixups for
    // two things: the surfaces themselves and the viewport.  No clip rect fixup is
    // needed because we're recording the command buffer for predicated tiling, which
    // provides its own special clip rect configurations.
    StaticFixups.AddSurfacesFixup();
    StaticFixups.AddViewportFixup();

    // Add static fixups for shaders.
    StaticFixups.AddVertexShaderFixup( g_pVertexShaderEnvironment, L"VSBackground" );
    StaticFixups.AddPixelShaderFixup( g_pPixelShaderEnvironment, L"PSBackground" );
    StaticFixups.AddVertexShaderFixup( g_pVertexShaderQuery, L"VSQuery" );

    // Set renderstate for drawing survey geometry.
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    g_pd3dDevice->SetVertexDeclaration( NULL );
    g_pd3dDevice->SetVertexShader( g_pVertexShaderQuery );
    g_pd3dDevice->SetPixelShader( NULL );

    // First, render the sector bounding volumes as conditional surveys.
    for( DWORD dwSectorIndex = 0; dwSectorIndex < dwSectorCount; ++dwSectorIndex )
    {
        g_pd3dDevice->PixBeginNamedEvent( 0, "Environment Sector %d Survey", dwSectorIndex );

        // Render sector bounding box.
        g_pd3dDevice->BeginConditionalSurvey( dwSectorIndex, D3DSURVEYBEGIN_CULLGEOMETRY );
        RenderAABB( SectorBounds[dwSectorIndex].GetAabb() );
        g_pd3dDevice->EndConditionalSurvey( 0 );

        g_pd3dDevice->PixEndNamedEvent();
    }

    // Set shaders for scene rendering.
    g_pd3dDevice->SetVertexShader( g_pVertexShaderEnvironment );
    g_pd3dDevice->SetPixelShader( g_pPixelShaderEnvironment );

    // Set renderstate for opaque models.
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    // Next, render each sector's opaque geometry.
    for( DWORD dwSectorIndex = 0; dwSectorIndex < dwSectorCount; ++dwSectorIndex )
    {
        g_pd3dDevice->PixBeginNamedEvent( 0, "Environment Sector %d Opaque", dwSectorIndex );

        ModelVector& SectorModels = SectorModelVectors[dwSectorIndex];
        DWORD dwModelCount = ( DWORD )SectorModels.size();

        g_pd3dDevice->BeginConditionalRendering( dwSectorIndex );
        for( DWORD i = 0; i < dwModelCount; ++i )
        {
            ATG::Model* pModel = SectorModels[i];
            if( !pModel->ContainsOpaqueSubsets() )
                continue;
            XMMATRIX matTransform = pModel->GetWorldTransform();
            matTransform = XMMatrixTranspose( matTransform );
            g_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTransform, 4 );
            RenderModelCommon( pModel, StaticFixups, DynamicFixups, FALSE, TRUE, FALSE );
        }
        g_pd3dDevice->EndConditionalRendering();

        g_pd3dDevice->PixEndNamedEvent();
    }

    // Set renderstate for transparent models.
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 1 );
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Finally, render each sector's transparent geometry.
    // This is not quite correct since transparent geometry should ideally be sorted
    // back-to-front and drawn at runtime.
    for( DWORD dwSectorIndex = 0; dwSectorIndex < dwSectorCount; ++dwSectorIndex )
    {
        g_pd3dDevice->PixBeginNamedEvent( 0, "Environment Sector %d Alpha", dwSectorIndex );

        ModelVector& SectorModels = SectorModelVectors[dwSectorIndex];
        DWORD dwModelCount = ( DWORD )SectorModels.size();

        g_pd3dDevice->BeginConditionalRendering( dwSectorIndex );
        for( DWORD i = 0; i < dwModelCount; ++i )
        {
            ATG::Model* pModel = SectorModels[i];
            if( !pModel->ContainsTransparentSubsets() )
                continue;
            XMMATRIX matTransform = pModel->GetWorldTransform();
            matTransform = XMMatrixTranspose( matTransform );
            g_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTransform, 4 );
            RenderModelCommon( pModel, StaticFixups, DynamicFixups, FALSE, FALSE, TRUE );
        }
        g_pd3dDevice->EndConditionalRendering();

        g_pd3dDevice->PixEndNamedEvent();
    }

    g_pd3dDevice->EndCommandBuffer();

    SaveCommandBuffer( pCommandBuffer, StaticFixups, DynamicFixups, L"ConstructionSite", FALSE );

    pCommandBuffer->Release();
}


//--------------------------------------------------------------------------------------
// Name: RenderModelCommon
// Desc: Walks through a model's mesh subsets, sets material-specific textures and state,
//       and renders each subset.
//--------------------------------------------------------------------------------------
VOID RenderModelCommon( ATG::Model* pModel, FixupArray& StaticFixups, FixupArray& DynamicFixups, BOOL bShadowRender,
                        BOOL bRenderSolid, BOOL bRenderOpaque )
{
    // Set sampler state.
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    g_pd3dDevice->SetVertexDeclaration( NULL );

    ATG::Texture* pBlackTexture = g_pScene->GetResourceDatabase()->GetBlackTexture();
    ATG::Texture* pBlueTexture = g_pScene->GetResourceDatabase()->GetBlueTexture();

    // Loop over mesh mappings.
    DWORD dwMapCount = pModel->GetNumMeshMappings();
    for( DWORD dwMapIndex = 0; dwMapIndex < dwMapCount; ++dwMapIndex )
    {
        ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
        ATG::BaseMesh* pMesh = mm.pMesh;
        DWORD dwSubsetCount = pMesh->GetNumSubsets();
        for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
        {
            if( !bShadowRender )
            {
                ATG::MaterialInstance* pMaterial = mm.Materials[dwSubsetIndex];
                if( pMaterial->IsTransparent() && !bRenderOpaque )
                    continue;
                if( !pMaterial->IsTransparent() && !bRenderSolid )
                    continue;

                // Set default textures from the resource database.
                ATG::Texture* pFinalTextureResources[] =
                {
                    pBlackTexture, pBlueTexture, pBlackTexture
                };

                // Extract textures from the material parameters.
                ATG::Texture* pTextureResource = NULL;
                switch( pMaterial->GetRawParameterCount() )
                {
                    case 3:
                        // Specular map texture
                        pTextureResource = ( ATG::Texture* )pMaterial->GetRawParameter( 2 ).pValue;
                        if( pTextureResource != NULL )
                        {
                            pFinalTextureResources[2] = pTextureResource;
                        }
                    case 2:
                        // Normal map texture
                        pTextureResource = ( ATG::Texture* )pMaterial->GetRawParameter( 1 ).pValue;
                        if( pTextureResource != NULL )
                        {
                            pFinalTextureResources[1] = pTextureResource;
                        }
                    case 1:
                        // Diffuse map texture
                        pTextureResource = ( ATG::Texture* )pMaterial->GetRawParameter( 0 ).pValue;
                        if( pTextureResource != NULL )
                        {
                            pFinalTextureResources[0] = pTextureResource;
                            // Also add a dynamic fixup for the diffuse map, so we can change that at runtime.
                            DynamicFixups.AddTextureFixup( pTextureResource->GetD3DTexture(),
                                                           pTextureResource->GetName() );
                        }
                    default:
                        break;
                }

                // Create static fixups for the 3 surface material textures.
                for( DWORD i = 0; i < ARRAYSIZE( pFinalTextureResources ); ++i )
                {
                    ATG::Texture* pTextureResource = pFinalTextureResources[i];
                    g_pd3dDevice->SetTexture( i, pTextureResource->GetD3DTexture() );
                    StaticFixups.AddTextureFixup( pTextureResource->GetD3DTexture(), pTextureResource->GetName() );
                }
            }

            // Render subset.
            pMesh->RenderSubset( dwSubsetIndex, g_pd3dDevice, ATG::BaseMesh::NoVertexDecl );
        }

        // Create fixups for the vertex and index buffers used by this mesh.
        ATG::VertexData* pVD = pMesh->GetVertexData( 0 );
        for( DWORD i = 0; i < pVD->GetNumVertexStreams(); ++i )
        {
            const ATG::VertexStream* pVS = pVD->GetVertexStream( i );
            StaticFixups.AddVertexBufferFixup( pVS->pVertexBuffer, i, pMesh->GetName() );
        }
        StaticFixups.AddIndexBufferFixup( pMesh->GetIndexData( 0 )->GetIndexBuffer(), pMesh->GetName() );
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderAABB
// Desc: Renders an axis-aligned bounding box.
//--------------------------------------------------------------------------------------
VOID RenderAABB( const ATG::AxisAlignedBox& AABB )
{
    static const XMFLOAT3 Corners[] =
    {
        XMFLOAT3( -1, -1, -1 ),
        XMFLOAT3( 1, -1, -1 ),
        XMFLOAT3( 1, -1, 1 ),
        XMFLOAT3( -1, -1, 1 ),
        XMFLOAT3( -1, 1, -1 ),
        XMFLOAT3( 1, 1, -1 ),
        XMFLOAT3( 1, 1, 1 ),
        XMFLOAT3( -1, 1, 1 ),
    };

    static const WORD Indices[] =
    {
        4, 5, 1, 0,
        6, 7, 3, 2,
        5, 6, 2, 1,
        7, 4, 0, 3,
        0, 1, 2, 3,
        7, 6, 5, 4
    };

    XMFLOAT3* pVerts = NULL;
    // BeginIndexedVertices does not currently work properly within precompiled command
    // buffers, so as a workaround we're using BeginVertices.
    HRESULT hr = g_pd3dDevice->BeginVertices( D3DPT_QUADLIST, 24, sizeof( XMFLOAT3 ), ( VOID** )&pVerts );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not draw AABB." );

    const XMVECTOR vCenter = XMLoadFloat3( &AABB.Center );
    const XMVECTOR vExtents = XMLoadFloat3( &AABB.Extents );
    for( DWORD i = 0; i < 24; ++i )
    {
        WORD wIndex = Indices[i];
        XMVECTOR vCorner = XMLoadFloat3( &Corners[wIndex] );
        XMVECTOR vCornerWorld = vCenter + vExtents * vCorner;
        XMStoreFloat3( &pVerts[i], vCornerWorld );
    }
    g_pd3dDevice->EndVertices();
}


//--------------------------------------------------------------------------------------
// Name: CreateFixups
// Desc: Given a vector of fixup descriptions, this method calls the appropriate methods
//       on a D3DCommandBuffer object to create command buffer fixups of several
//       different types.
//--------------------------------------------------------------------------------------
VOID CreateFixups( D3DCommandBuffer* pCommandBuffer, FixupArray& Fixups, DWORD dwFlags )
{
    // Loop through the FIXUP_DESC structures and create a command buffer fixup for each one.
    DWORD dwFixupCount = Fixups.Count();
    for( DWORD i = 0; i < dwFixupCount; ++i )
    {
        FIXUP_DESC& Fixup = Fixups.m_Fixups[i];

        switch( Fixup.Type )
        {
            case FIXUP_TEXTURE:
                Fixup.dwFixupHandle = pCommandBuffer->CreateTextureFixup( dwFlags, ( D3DBaseTexture* )Fixup.pResource,
                                                                          0, 0 );
                break;
            case FIXUP_VERTEXSHADER:
                Fixup.dwFixupHandle = pCommandBuffer->CreateVertexShaderFixup( dwFlags,
                                                                               ( D3DVertexShader* )Fixup.pResource, 0,
                                                                               0 );
                break;
            case FIXUP_PIXELSHADER:
                Fixup.dwFixupHandle = pCommandBuffer->CreatePixelShaderFixup( dwFlags,
                                                                              ( D3DPixelShader* )Fixup.pResource, 0,
                                                                              0 );
                break;
            case FIXUP_SURFACES:
                Fixup.dwFixupHandle = pCommandBuffer->CreateSurfacesFixup( dwFlags, 0, 0 );
                break;
            case FIXUP_VIEWPORT:
                Fixup.dwFixupHandle = pCommandBuffer->CreateViewportFixup( dwFlags, 0, 0 );
                break;
            case FIXUP_CLIPRECT:
                Fixup.dwFixupHandle = pCommandBuffer->CreateClipRectFixup( dwFlags, 0, 0 );
                break;
            case FIXUP_VERTEXBUFFER:
                Fixup.dwFixupHandle = pCommandBuffer->CreateVertexBufferFixup( dwFlags,
                                                                               ( D3DVertexBuffer* )Fixup.pResource, 0,
                                                                               0 );
                break;
            case FIXUP_INDEXBUFFER:
                Fixup.dwFixupHandle = pCommandBuffer->CreateIndexBufferFixup( dwFlags,
                                                                              ( D3DIndexBuffer* )Fixup.pResource, 0,
                                                                              0 );
                break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: MergeAABB
// Desc: Merges an arbitrary bound into an existing AABB bound.
//--------------------------------------------------------------------------------------
VOID MergeAABB( ATG::Bound& CurrentBound, const ATG::Bound& MergeBound )
{
    // Convert the merge bound into an axis-aligned bounding box
    ATG::Bound AABBBound;
    if( MergeBound.GetType() == ATG::Bound::AABB_Bound )
    {
        AABBBound = MergeBound;
    }
    else
    {
        ATG::AxisAlignedBox AABB;
        AABB.Center = MergeBound.GetCenter();
        FLOAT fRadius = MergeBound.GetMaxRadius();
        AABB.Extents = XMFLOAT3( fRadius, fRadius, fRadius );
        AABBBound.SetAabb( AABB );
    }

    // Merge the AABB bound into the current bound
    if( CurrentBound.GetType() == ATG::Bound::No_Bound )
    {
        CurrentBound = AABBBound;
        return;
    }
    assert( CurrentBound.GetType() == ATG::Bound::AABB_Bound );
    XMVECTOR vCurrentMin = XMLoadFloat3( &CurrentBound.GetAabb().Center ) -
        XMLoadFloat3( &CurrentBound.GetAabb().Extents );
    XMVECTOR vCurrentMax = XMLoadFloat3( &CurrentBound.GetAabb().Center ) +
        XMLoadFloat3( &CurrentBound.GetAabb().Extents );
    XMVECTOR vMergeMin = XMLoadFloat3( &AABBBound.GetAabb().Center ) - XMLoadFloat3( &AABBBound.GetAabb().Extents );
    XMVECTOR vMergeMax = XMLoadFloat3( &AABBBound.GetAabb().Center ) + XMLoadFloat3( &AABBBound.GetAabb().Extents );

    XMVECTOR vNewMin = XMVectorMin( vCurrentMin, vMergeMin );
    XMVECTOR vNewMax = XMVectorMax( vCurrentMax, vMergeMax );

    XMVECTOR vExtents = ( vNewMax - vNewMin ) * 0.5f;
    XMVECTOR vCenter = vNewMin + vExtents;

    ATG::AxisAlignedBox AABB;
    XMStoreFloat3( &AABB.Center, vCenter );
    XMStoreFloat3( &AABB.Extents, vExtents );
    CurrentBound.SetAabb( AABB );
}


//--------------------------------------------------------------------------------------
// Name: SaveCommandBuffer
// Desc: Saves a command buffer and its associated fixup descs to a file.
//--------------------------------------------------------------------------------------
VOID SaveCommandBuffer( D3DCommandBuffer* pCommandBuffer, FixupArray& StaticFixups, FixupArray& DynamicFixups,
                        const WCHAR* strObjectName, BOOL bShadowRender )
{
    ATG::DebugSpew( "Writing command buffer %S%S.\n", strObjectName, bShadowRender ? L"_shadow" : L"" );

    pCommandBuffer->BeginFixupCreation();
    CreateFixups( pCommandBuffer, StaticFixups, 0 );
    CreateFixups( pCommandBuffer, DynamicFixups, D3DFIXUP_DYNAMIC );
    pCommandBuffer->EndFixupCreation();

    // Create extra data block that will be used to match this command buffer to the original content.
    const DWORD dwModelNameSize = 256;
    WCHAR strModelName[dwModelNameSize];
    ZeroMemory( strModelName, sizeof( strModelName ) );
    DWORD dwExtraDataSize = sizeof( strModelName );
    wcscpy_s( strModelName, strObjectName );

    // Create file header.
    COMMANDBUFFER_FILE_HEADER FileHeader;
    ZeroMemory( &FileHeader, sizeof( COMMANDBUFFER_FILE_HEADER ) );
    FileHeader.dwFileHeaderSize = sizeof( COMMANDBUFFER_FILE_HEADER ) + dwExtraDataSize;
    FileHeader.dwVersion = COMMANDBUFFER_FILE_VERSION;
    FileHeader.dwStaticFixupCount = StaticFixups.Count();
    FileHeader.dwStaticFixupDataSize = StaticFixups.Count() * sizeof( FIXUP_DESC );
    FileHeader.dwDynamicFixupCount = DynamicFixups.Count();
    FileHeader.dwDynamicFixupDataSize = DynamicFixups.Count() * sizeof( FIXUP_DESC );

    // Determine the size of the three command buffer deconstruction pieces.
    pCommandBuffer->Deconstruct( 0, NULL, &FileHeader.dwHeaderSize, NULL, &FileHeader.dwPhysicalSize,
                                 NULL, &FileHeader.dwInitializationSize );

    // Allocate memory buffers for the three command buffer pieces.
    BYTE* pHeaderBytes = ( BYTE* )malloc( FileHeader.dwHeaderSize );
    BYTE* pPhysicalBytes = ( BYTE* )malloc( FileHeader.dwPhysicalSize );
    BYTE* pInitializationBytes = ( BYTE* )malloc( FileHeader.dwInitializationSize );
    assert( pHeaderBytes != NULL && pPhysicalBytes != NULL && pInitializationBytes != NULL );

    // Deconstruct the command buffer into three pieces: the header, the command stream (physical data), and initialization data.
    pCommandBuffer->Deconstruct( 0, ( D3DCommandBuffer* )pHeaderBytes, &FileHeader.dwHeaderSize,
                                 pPhysicalBytes, &FileHeader.dwPhysicalSize,
                                 pInitializationBytes, &FileHeader.dwInitializationSize );

    // Create a filename for the command buffer file.
    WCHAR strFileName[MAX_PATH];
    swprintf_s( strFileName, L"%s%s.cmdbuffer", strObjectName, bShadowRender ? L"_shadow" : L"" );

    // Create output file.
    HANDLE hFile;
    SetFileAttributesW( strFileName, FILE_ATTRIBUTE_NORMAL );
    hFile = CreateFileW( strFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
    {
        ATG::FatalError( "Could not create output command buffer file." );
    }

    // Endian swap the file header and write to file.
    COMMANDBUFFER_FILE_HEADER FileHeaderEndianSwap;
    DWORD dwBytesWritten = 0;
    XGEndianSwapMemory( &FileHeaderEndianSwap, &FileHeader, XGENDIAN_8IN32, sizeof( DWORD ), sizeof
                        ( FileHeader ) / sizeof( DWORD ) );
    BOOL bResult = WriteFile( hFile, &FileHeaderEndianSwap, sizeof( COMMANDBUFFER_FILE_HEADER ), &dwBytesWritten,
                              NULL );
    assert( bResult != FALSE );

    // Write the extra data to file.
    XGEndianSwapMemory( strModelName, strModelName, XGENDIAN_8IN16, sizeof( WCHAR ), sizeof( strModelName ) / sizeof
                        ( WCHAR ) );
    bResult = WriteFile( hFile, strModelName, sizeof( strModelName ), &dwBytesWritten, NULL );
    assert( bResult != FALSE );

    // Write the three command buffer pieces to file (they are already endian swapped correctly).
    bResult = WriteFile( hFile, pHeaderBytes, FileHeader.dwHeaderSize, &dwBytesWritten, NULL );
    assert( bResult != FALSE );
    bResult = WriteFile( hFile, pPhysicalBytes, FileHeader.dwPhysicalSize, &dwBytesWritten, NULL );
    assert( bResult != FALSE );
    bResult = WriteFile( hFile, pInitializationBytes, FileHeader.dwInitializationSize, &dwBytesWritten, NULL );
    assert( bResult != FALSE );

    // Write the static fixup descs to file.  The command buffer loader on Xbox 360 will use them to apply the fixups.
    DWORD dwFixupCount = StaticFixups.Count();
    for( DWORD i = 0; i < dwFixupCount; ++i )
    {
        FIXUP_DESC& Fixup = StaticFixups.m_Fixups[i];
        Fixup.pResource = NULL;
        EndianSwapFixupDesc( &Fixup );
        bResult = WriteFile( hFile, &Fixup, sizeof( FIXUP_DESC ), &dwBytesWritten, NULL );
        assert( bResult != FALSE );
    }

    // Write the dynamic fixup descs to file.  These fixups can be applied at runtime on the Xbox 360.
    dwFixupCount = DynamicFixups.Count();
    for( DWORD i = 0; i < dwFixupCount; ++i )
    {
        FIXUP_DESC& Fixup = DynamicFixups.m_Fixups[i];
        Fixup.pResource = NULL;
        EndianSwapFixupDesc( &Fixup );
        bResult = WriteFile( hFile, &Fixup, sizeof( FIXUP_DESC ), &dwBytesWritten, NULL );
        assert( bResult != FALSE );
    }

    // Free the memory allocated for the command buffer pieces.
    free( pHeaderBytes );
    free( pPhysicalBytes );
    free( pInitializationBytes );

    CloseHandle( hFile );
}
