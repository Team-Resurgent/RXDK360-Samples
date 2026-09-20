//--------------------------------------------------------------------------------------
// SubDMesh.h
//
// Structs and classes that represent and manipulate a Catmull-Clark subdivision mesh 
// and its corresponding set of Approximate Catmull-Clark 2 (ACC2) patches.
//
// Microsoft XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xboxmath.h>
#include <xgraphics.h>
#include <d3dx9.h>
#include <vector>
#include "AtgSceneAll.h"
#include "AtgDevice.h"

using namespace ATG;

// MAX_POINTS is the number of points in a quad and its 1-ring neighborhood.
#define MAX_POINTS 32

// MAX_VALENCE is the maximum valence (number of neighbors) for a vertex in a quad mesh.
#define MAX_VALENCE 16

// ACC2_PATCH_STRIDE is the number of control points (vectors) in an ACC2 patch.
#define ACC2_PATCH_STRIDE 20

// MESH_VERTEX_SIZE is the size in bytes of a mesh vertex in the control mesh.
#define MESH_VERTEX_SIZE 32

// Sample defaults to 32-bit floats for patch elements.
// It runs faster with 16-bit floats, but precision is lost and minor creasing is sometimes visible in adjacent patches.
// Switch the following commented #define statements to change the precision.

//#define PATCHELEMENT_FLOAT16
#define PATCHELEMENT_FLOAT32

#if defined( PATCHELEMENT_FLOAT32 )

// Vertex declaration for an ACC2 patch using single-precision floats.
typedef XMFLOAT4 PATCHELEMENT;
#define PATCHDECLTYPE D3DDECLTYPE_FLOAT4
#define PATCHDECLSIZE 16

#elif defined( PATCHELEMENT_FLOAT16 )

// Vertex declaration for an ACC2 patch using half-precision floats.
typedef XMHALF4 PATCHELEMENT;
#define PATCHDECLTYPE D3DDECLTYPE_FLOAT16_4
#define PATCHDECLSIZE 8

#else

#error Either PATCHELEMENT_FLOAT32 or PATCHELEMENT_FLOAT16 must be defined.

#endif

#define ACC2END (ACC2_PATCH_STRIDE*PATCHDECLSIZE)
static const D3DVERTEXELEMENT9 PatchVertexElements[] =
{
    // 20 vector elements for the ACC2 patch
    { 0,  0 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  0 },
    { 0,  1 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  1 },
    { 0,  2 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  2 },
    { 0,  3 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  3 },
    { 0,  4 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  4 },
    { 0,  5 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  5 },
    { 0,  6 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  6 },
    { 0,  7 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  7 },
    { 0,  8 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  8 },
    { 0,  9 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  9 },
    { 0, 10 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 10 },
    { 0, 11 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 11 },
    { 0, 12 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 12 },
    { 0, 13 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 13 },
    { 0, 14 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 14 },
    { 0, 15 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 15 },
    { 0, 16 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 12 },
    { 0, 17 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 13 },
    { 0, 18 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 14 },
    { 0, 19 * PATCHDECLSIZE, PATCHDECLTYPE, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 15 },
    
    // Pairs of texture coordinates for the four quad corners
    { 0, ACC2END     , D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, ACC2END +  8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },

    // Texture tangent space vectors for each quad corner
    { 0, ACC2END + 16, D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
    { 0, ACC2END + 20, D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 1 },
    { 0, ACC2END + 24, D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 2 },
    { 0, ACC2END + 28, D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 3 },

    D3DDECL_END()
};

//--------------------------------------------------------------------------------------
// Name: struct ACC2PATCH
// Desc: Represents a single ACC2 patch.
//--------------------------------------------------------------------------------------
struct ACC2PATCH
{
    PATCHELEMENT m_vPoints[ACC2_PATCH_STRIDE];
    XMHALF2 m_vTexCoords[4];
    XMDECN4 m_vTangents[4];
};

class SubDMesh;
class PolyMesh;

typedef std::vector<SubDMesh*> SubDMeshVector;
typedef std::vector<PolyMesh*> PolyMeshVector;

class PolyMesh
{
    friend class SubDMesh;

protected:
    XMFLOAT3            m_vMeshCenter;
    FLOAT               m_fMeshRadius;

    D3DVertexBuffer*    m_pMeshVB;
    D3DIndexBuffer*     m_pMeshIB;

    Model*              m_pModel;
    StaticMesh*         m_pMesh;
    Scene*              m_pScene;

    ATG::SkeletonInstance*   m_pSkeletonInstance;
    DWORD               m_dwSkinnedMeshBinding;
    INT                 m_iSkeletonBoneIndex;

public:
    PolyMesh();

    D3DVertexBuffer* GetMeshVB()
    {
        return m_pMeshVB;
    }
    D3DIndexBuffer* GetMeshIB()
    {
        return m_pMeshIB;
    }
    XMVECTOR GetMeshCenter() const
    {
        return XMLoadFloat3( &m_vMeshCenter );
    }
    FLOAT GetMeshRadius() const
    {
        return m_fMeshRadius;
    }

    BOOL IsSkinned() const
    {
        return m_pMesh->IsDerivedFrom( SkinnedMesh::TypeID );
    }
    Model* GetModel()
    {
        return m_pModel;
    }
    SkinnedMesh* GetSkinnedMesh()
    {
        assert( IsSkinned() );
        return (SkinnedMesh*)m_pMesh;
    }
    XMMATRIX GetWorldTransform()
    {
        if( m_pSkeletonInstance == NULL || m_iSkeletonBoneIndex < 0 )
        {
            return XMMatrixIdentity();
        }
        return m_pSkeletonInstance->m_WorldPose.LoadTransform( (DWORD)m_iSkeletonBoneIndex );
    }
};

//--------------------------------------------------------------------------------------
// Name: class SubDMesh
// Desc: Represents a Catmull-Clark subdivision mesh.  Handles loading, basis
//       conversion, patch storage, and mesh storage.
//--------------------------------------------------------------------------------------
class SubDMesh : public PolyMesh
{
protected:
    D3DVertexBuffer*    m_pACC2PatchVB[2];
    D3DVertexBuffer*    m_pCurrentACC2PatchVB;

    D3DVertexBuffer*    m_pQuadArrayVB;
    D3DIndexBuffer*     m_pQuadNeighborhoodIB;

    XMFLOAT3*           m_pSkinnedMeshBuffer;

    DWORD               m_dwPatchCount;
    DWORD               m_dwVertexCount;
    DWORD               m_dwMeshVertexStride;

public:
    SubDMesh();

    static HRESULT LoadFromXATG( ATG::D3DDevice* pd3dDevice, const CHAR* strFileName, const WCHAR* strRootBoneName, SubDMeshVector& OutputSubDMeshes, PolyMeshVector& OutputPolyMeshes, ATG::SkeletonInstance** ppSkeletonInstance );

    VOID        GenPatches( INT iIsolatePatch = -1 );

    DWORD       GetNumPatches() const
    {
        return m_dwPatchCount;
    }
    DWORD       GetNumVertices() const
    {
        return m_dwVertexCount;
    }
    D3DVertexBuffer* GetQuadArrayVB()
    {
        return m_pQuadArrayVB;
    }
    D3DVertexBuffer* GetACC2PatchVB()
    {
        return m_pCurrentACC2PatchVB;
    }

private:
    VOID        CreateBuffers( ATG::D3DDevice* pd3dDevice );

    VOID        SkinMesh( const BYTE* pSrcVertexData );

    VOID        GenACC2PatchVMX( const BYTE* pPositions, const DWORD dwPositionStride, 
                                 const BYTE* pTangents, const DWORD dwTangentStride,
                                 const BYTE* pTexCoords, const DWORD dwTexCoordStride,
                                 const UINT Vert[MAX_POINTS], const BYTE Val[4],
                                 const BYTE Pref[4], ACC2PATCH* pOutputACC2Patch );
};
