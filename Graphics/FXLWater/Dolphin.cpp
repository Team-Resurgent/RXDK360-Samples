//--------------------------------------------------------------------------------------
// Dolphin.cpp
//
// Dolphin implementation
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "dolphin.h"
#pragma warning(push)
// Disable these warnings:
// \Include\Xbox\list(1143) : warning C4127: conditional expression is constant
// \Include\Xbox\list(1164) : warning C4127: conditional expression is constant
#pragma warning(disable : 4127)
#include <list>
#pragma warning(pop)
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgUtil.h>

using std::list;
class XDolphinSchool;


//--------------------------------------------------------------------------------------
// Name: XDolphin
// Desc: Definition for a class describing 1 instance of a dolphin
//--------------------------------------------------------------------------------------
class XDolphin : public Dolphin
{
public:
                    XDolphin( XDolphinSchool* pSchool,
                              const XMVECTOR& vecPosition,
                              FLOAT fPitch,
                              FLOAT fYaw );
    virtual VOID    SetTargetPosition( const XMVECTOR& vTarget );
    virtual VOID    Update( FLOAT fElapsedTime );
    virtual VOID    GetPosition( XMVECTOR& vPosition );
    inline const XMMATRIX& GetWorldMatrix()
    {
        return m_matWorld;
    }
    inline const XMVECTOR& GetBlendWeights()
    {
        return m_vBlendWeights;
    }
protected:
    virtual         ~XDolphin();
    XDolphinSchool* m_pSchool;
    XMVECTOR m_vPosition;
    XMVECTOR m_vTargetPosition;
    FLOAT m_fPitch;
    FLOAT m_fYaw;
    XMVECTOR m_vBlendWeights;
    XMMATRIX m_matWorld;
    FLOAT m_fKickPhase;
};


//--------------------------------------------------------------------------------------
// Name: XDolphinSchool
// Desc: Definition for a Container/Generator of Dolphin Instances
//--------------------------------------------------------------------------------------
class XDolphinSchool : public DolphinSchool
{
public:
                    XDolphinSchool( LPDIRECT3DDEVICE9 pD3DDevice,
                                    FXLEffectPool* pFXLPool,
                                    ATG::PackedResource* pResource );
    virtual HRESULT Initialize();
    virtual HRESULT Render( RENDERPASS rpPass );
    virtual VOID    Update( FLOAT fElapsedTime );
    virtual Dolphin* AddDolphin( const XMVECTOR& vTarget, FLOAT fPitch, FLOAT fYaw );
    virtual VOID    RemoveInstance( Dolphin* pDolphin );
protected:
    list <XDolphin*> m_DolphinList;
    ATG::PackedResource* m_pResource;
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    ATG::Mesh2 m_DolphinMesh1;
    ATG::Mesh2 m_DolphinMesh2;
    ATG::Mesh2 m_DolphinMesh3;
    FXLEffect* m_pFXLDolphin;
    FXLEffectPool* m_pFXLPool;
    FXLHANDLE m_hFXLDolphinWorld;
    FXLHANDLE m_hFXLDolphinBlendWeights;
    LPDIRECT3DTEXTURE9 m_pDolphinTexture;
    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB1;
    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB2;
    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB3;
    LPDIRECT3DINDEXBUFFER9 m_pDolphinIB;
    D3DPRIMITIVETYPE m_dwDolphinPrimType;
    DWORD m_dwDolphinVertexSize;
    DWORD m_dwNumDolphinVertices;
    DWORD m_dwNumDolphinPrimitives;
    LPDIRECT3DVERTEXDECLARATION9 m_pDolphinVertexDeclaration;
};


//--------------------------------------------------------------------------------------
// Name: XDolphin
// Desc: c-tor
//--------------------------------------------------------------------------------------
XDolphin::XDolphin( XDolphinSchool* pSchool,
                    const XMVECTOR& vecPosition,
                    FLOAT fPitch,
                    FLOAT fYaw ) : m_pSchool( pSchool ),
                                   m_vPosition( vecPosition ),
                                   m_fPitch( fPitch ),
                                   m_fYaw( fYaw )
{
    m_matWorld = XMMatrixIdentity();
    m_vBlendWeights = XMVectorSet( 1.f, 0.f, 0.f, 0.f );
    m_fKickPhase = 0.f;
}


//--------------------------------------------------------------------------------------
// Name: XDolphin
// Desc: Destructor, notifies container that this instance is being destroyed
//--------------------------------------------------------------------------------------
XDolphin::~XDolphin()
{
    m_pSchool->RemoveInstance( this );
}


//--------------------------------------------------------------------------------------
// Name: SetTargetPosition
// Desc: Dolphins are controlled by sending them to a position, not by directly
//       setting their position.
//--------------------------------------------------------------------------------------
VOID XDolphin::SetTargetPosition( const XMVECTOR& vTarget )
{
    m_vTargetPosition = vTarget;
}


//--------------------------------------------------------------------------------------
// Name: GetPosition
// Desc: Retrieve the position of this dolphin instance
//--------------------------------------------------------------------------------------
VOID XDolphin::GetPosition( XMVECTOR& vPosition )
{
    vPosition = m_vPosition;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Updates this instance of a dolphin
//       This includes directing the dolphin's travel, updating matrices, and
//       setting animation blend weights.
//--------------------------------------------------------------------------------------
VOID XDolphin::Update( FLOAT fElapsedTime )
{
    const FLOAT fMaxSpeed = 20.f;       // ft/sec
    const FLOAT fMaxRotation = 1.0;     // radians/sec

    // The positional delta is used to direct dolphin travel
    XMVECTOR vToTarget = m_vTargetPosition - m_vPosition;

    // Dolphins slow down as they approach their target
    FLOAT fDistance = XMVector3Length( vToTarget ).x;

    FLOAT fSpeed = fDistance / 50.f * fMaxSpeed;
    fSpeed = __min( fSpeed, fMaxSpeed );

    // The slower a dolpin swims, the less steering control is exercised.
    // Note: This results in always "missing" their target as they close in on it.
    //       The net effect is a "milling about" action once reaching their target.
    FLOAT fAngularSpeed = fDistance / 5.f * fMaxRotation;
    fAngularSpeed = __min( fAngularSpeed, fMaxRotation );

    // Compute the local coordinate system so the AI/physics inputs can be applied
    XMMATRIX matRot = XMMatrixRotationY( m_fYaw );

    XMVECTOR vAhead = XMVectorSet( 0.f, 0.f, 1.f, 0.f );
    vAhead = XMVector3TransformNormal( vAhead, matRot );

    // Turn the dolphin left or right, depending on where the target lies
    if( vToTarget.x * vAhead.z - vToTarget.z * vAhead.x > 0.f )
    {
        m_fYaw += fAngularSpeed * fElapsedTime;
    }
    else
    {
        m_fYaw -= fAngularSpeed * fElapsedTime;
    }

    vAhead = XMVectorSet( 0.f, 0.f, 1.f, 0.f );
    vAhead = XMVector3TransformNormal( vAhead, matRot );

    m_vPosition += fSpeed * vAhead * fElapsedTime;

    m_matWorld = XMMatrixScaling( 0.01f, 0.01f, 0.01f )
        * XMMatrixRotationY( +D3DX_PI / 2 );

    // Raise the dolphin in the water column (y axis) as it swims along
    // This effect lets the viewer see reflections more clearly over time.
    m_matWorld *= matRot
        * XMMatrixTranslation( m_vPosition.x,
                               m_vPosition.y
                               + 0.25f * sinf( m_fKickPhase * 0.05f ),
                               m_vPosition.z );

    // The shaders tween vertex positions based on blend weights, so here we transform
    // the kick phase into blend weights the shaders will understand.
    m_fKickPhase += fSpeed * fElapsedTime;

    FLOAT fBlendWeight = sinf( 0.7f * m_fKickPhase );
    FLOAT fWeight1;
    FLOAT fWeight2;
    FLOAT fWeight3;

    if( fBlendWeight > 0.0f )
    {
        fWeight1 = fabsf( fBlendWeight );
        fWeight2 = 1.0f - fabsf( fBlendWeight );
        fWeight3 = 0.0f;
    }
    else
    {
        fWeight1 = 0.0f;
        fWeight2 = 1.0f - fabsf( fBlendWeight );
        fWeight3 = fabsf( fBlendWeight );
    }
    m_vBlendWeights = XMVectorSet( fWeight1, fWeight2, fWeight3, 0.0f );
}


//--------------------------------------------------------------------------------------
// Name: 
// Desc: 
//--------------------------------------------------------------------------------------
XDolphinSchool::XDolphinSchool( LPDIRECT3DDEVICE9 pD3DDevice,
                                FXLEffectPool* pFXLPool,
                                ATG::PackedResource* pResource ) : m_pd3dDevice( pD3DDevice ),
                                                                   m_pFXLPool( pFXLPool ),
                                                                   m_pResource( pResource )
{
    m_pd3dDevice->AddRef();
    m_pFXLPool->AddRef();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initializes the resources used for rendering the dolphin instances.
//       These resources include Meshes for tweening, shaders, and textures.
//--------------------------------------------------------------------------------------
HRESULT XDolphinSchool::Initialize()
{
    HRESULT hr;

    m_pDolphinTexture = m_pResource->GetTexture( "DolphinTexture" );

    if( FAILED( hr = m_DolphinMesh1.Create( "game:\\Media\\Meshes\\Dolphin1.xbg" ) ) )
    {
        ATG_PrintError( "Could not create Dolphin1 mesh\n" );
        return hr;
    }
    if( FAILED( hr = m_DolphinMesh2.Create( "game:\\Media\\Meshes\\Dolphin2.xbg" ) ) )
    {
        ATG_PrintError( "Could not create Dolphin2 mesh\n" );
        return hr;
    }
    if( FAILED( hr = m_DolphinMesh3.Create( "game:\\Media\\Meshes\\Dolphin3.xbg" ) ) )
    {
        ATG_PrintError( "Could not create Dolphin3 mesh\n" );
        return hr;
    }

    m_pDolphinVB1 = &m_DolphinMesh1.GetMesh()->m_VB;
    m_pDolphinVB2 = &m_DolphinMesh2.GetMesh()->m_VB;
    m_pDolphinVB3 = &m_DolphinMesh3.GetMesh()->m_VB;
    m_pDolphinIB = &m_DolphinMesh1.GetMesh()->m_IB;

    m_dwDolphinPrimType = m_DolphinMesh1.GetMesh()->m_dwPrimType;
    m_dwNumDolphinVertices = m_DolphinMesh1.GetMesh()->m_pSubsets[0].dwVertexCount;
    m_dwNumDolphinPrimitives = m_DolphinMesh1.GetMesh()->m_pSubsets[0].dwPrimitiveCount;
    m_dwDolphinVertexSize = m_DolphinMesh1.GetMesh()->m_dwVertexSize;

    // Build the vertex declaration for the dolphin
    D3DVERTEXELEMENT9 declDolphin[MAXD3DDECLLENGTH] = { 0 };
    ATG::AppendVertexElements(
        declDolphin, 0, m_DolphinMesh1.GetMesh()->m_VertexElements, 0
        );
    ATG::AppendVertexElements(
        declDolphin, 1, m_DolphinMesh2.GetMesh()->m_VertexElements, 1
        );
    ATG::AppendVertexElements(
        declDolphin, 2, m_DolphinMesh3.GetMesh()->m_VertexElements, 2
        );

    // Create vertex declaration for the dolphin
    hr = m_pd3dDevice->CreateVertexDeclaration( declDolphin,
                                                &m_pDolphinVertexDeclaration );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't create vertex declaration\n" );
        return hr;
    }

    // Create the Dolphin FXLite effect
    // The effect is precompiled on the PC-side into a binary file.
    // The binary file is then loaded, and instantiated via FXLCreateEffect
    VOID* pCode;
    DWORD dwSize;

    hr = ATG::LoadFile( "game:\\Media\\Effects\\Dolphin.fxobj", &pCode, &dwSize );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not load FXL Effect (Dolphin)\n" );
        return hr;
    }

    hr = FXLCreateEffect( m_pd3dDevice, pCode, m_pFXLPool, &m_pFXLDolphin );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create FXL Effect (Dolphin)\n" );
        return hr;
    }
    ATG::UnloadFile( pCode );

    m_hFXLDolphinWorld = m_pFXLDolphin->GetParameterHandle( "matWorld" );
    m_hFXLDolphinBlendWeights = m_pFXLDolphin->GetParameterHandle( "vBlendWeights" );

    FXLHANDLE hFXLDolphinSampler = m_pFXLDolphin->GetParameterHandle( "base_sampler" );
    m_pFXLDolphin->SetSampler( hFXLDolphinSampler, m_pDolphinTexture );

    // Set the blend weights for manual register updates, via CommitU
    // Performance may sometimes be improved via manual register updates - your
    // own code may often make optimizations that FXLite cannot
    m_pFXLDolphin->SetParameterRegisterUpdate( m_hFXLDolphinBlendWeights, FXLREGUPDATE_MANUAL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: AddDolphin
// Desc: Associate a dolphin instance with the school
//--------------------------------------------------------------------------------------
Dolphin* XDolphinSchool::AddDolphin( const XMVECTOR& vTarget, FLOAT fPitch, FLOAT fYaw )
{
    m_DolphinList.push_back( new XDolphin( this, vTarget, fPitch, fYaw ) );
    return m_DolphinList.back();
}


//--------------------------------------------------------------------------------------
// Name: RemoveInstance
// Desc: Disassociate an instance from the school
//--------------------------------------------------------------------------------------
VOID XDolphinSchool::RemoveInstance( Dolphin* pDolphin )
{
    m_DolphinList.remove( ( XDolphin* )pDolphin );
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Allows each dolphin instance to update
//--------------------------------------------------------------------------------------
VOID XDolphinSchool::Update( FLOAT fElapsedTime )
{
    for( list <XDolphin*>::iterator it = m_DolphinList.begin();
         it != m_DolphinList.end();
         ++it )
    {
        ( *it )->Update( fElapsedTime );
    }
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Renders each instance in the school of dolphins.
//--------------------------------------------------------------------------------------
HRESULT XDolphinSchool::Render( RENDERPASS rpPass )
{
    FXLHANDLE hTechnique;
    FXLHANDLE hPass;

    hTechnique = m_pFXLDolphin->GetTechniqueHandleFromIndex( 0 );
    m_pFXLDolphin->BeginTechnique( hTechnique, FXL_RESTORE_DEFAULT_RENDER_STATE );
    hPass = m_pFXLDolphin->GetPassHandleFromIndex( hTechnique, ( INT )rpPass );

    UINT nBlendWeightIndex;
    UINT nBlendWeightCount;
    m_pFXLDolphin->GetParameterRegister( hPass,
                                         m_hFXLDolphinBlendWeights,
                                         FXLPCONTEXT_VERTEXSHADERCONSTANTF,
                                         &nBlendWeightIndex,
                                         &nBlendWeightCount );

    // The dolphin effects contain three passes - refration, reflection, and normal
    m_pFXLDolphin->BeginPass( hPass );

    m_pd3dDevice->SetVertexDeclaration( m_pDolphinVertexDeclaration );
    m_pd3dDevice->SetStreamSource( 0, m_pDolphinVB1, 0, m_dwDolphinVertexSize );
    m_pd3dDevice->SetStreamSource( 1, m_pDolphinVB2, 0, m_dwDolphinVertexSize );
    m_pd3dDevice->SetStreamSource( 2, m_pDolphinVB3, 0, m_dwDolphinVertexSize );
    m_pd3dDevice->SetIndices( m_pDolphinIB );

    for( list <XDolphin*>::iterator it = m_DolphinList.begin();
         it != m_DolphinList.end();
         ++it )
    {
        m_pFXLDolphin->SetMatrix( m_hFXLDolphinWorld,
                                  ( FXLMATRIX* )&( *it )->GetWorldMatrix() );

        // The blend weights are set to manual update, and will not be updated in CommitU.
        // This is where we would otherwise update them:
        // 
        // m_pFXLDolphin->SetVector( m_hFXLDolphinBlendWeights,
        //                          (FXLVECTOR)(*it)->GetBlendWeights() );

        m_pFXLDolphin->CommitU();

        // Update the blend weights manually
        m_pd3dDevice->SetVertexShaderConstantF( nBlendWeightIndex,
                                                ( FLOAT* )&( *it )->GetBlendWeights(),
                                                1 );

        m_pd3dDevice->DrawIndexedPrimitive( m_dwDolphinPrimType, 0,
                                            0, m_dwNumDolphinVertices,
                                            0, m_dwNumDolphinPrimitives );
    }

    m_pFXLDolphin->EndPass();

    m_pFXLDolphin->EndTechnique();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Create
// Desc: Creates a school of dolphins.  This is used to create individual dolphin
//       instances. 
//--------------------------------------------------------------------------------------
DolphinSchool* DolphinSchool::Create( LPDIRECT3DDEVICE9 pD3DDevice,
                                      FXLEffectPool* pFXLPool,
                                      ATG::PackedResource* pResource )
{
    XDolphinSchool* pSchool = new XDolphinSchool( pD3DDevice, pFXLPool, pResource );

    if( FAILED( pSchool->Initialize() ) )
    {
        delete pSchool;
        return NULL;
    }

    return ( DolphinSchool* )pSchool;
}

