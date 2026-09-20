//--------------------------------------------------------------------------------------
// SubDMesh.cpp
//
// Microsoft XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <fstream>
#include <assert.h>

#include "SubDMesh.h"

#include "AtgUtil.h"
#include "AtgSceneAll.h"

using namespace std;
using namespace ATG;

// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" that we won't run out of memory in this sample
#pragma warning ( disable : 6211 )

// Lookup tables for the basis conversion algorithm in GenACC2PatchVMX.
static XMVECTOR g_pTangentStencils[MAX_VALENCE][64] = { 0 };
static XMVECTOR g_pCornerWeightTable1[MAX_VALENCE] = { 0 };
static XMVECTOR g_pCornerWeightTable2[MAX_VALENCE] = { 0 };
static XMVECTOR g_pInnerWeightTable[MAX_VALENCE] = { 0 };
#define P_STENCIL(IValue) (g_pTangentStencils[Valence][((IValue)<<1)])
#define Q_STENCIL(IValue) (g_pTangentStencils[Valence][((IValue)<<1) + 1])

// Blend weights are at offset 12 in the mesh vertex struct.
static const DWORD g_dwBlendWeightOffset = 12;

// Blend indices are at offset 16 in the mesh vertex struct.
static const DWORD g_dwBlendIndicesOffset = 16;

// Tangent vectors are at offset 28 in the mesh vertex struct.
static const DWORD g_dwTangentOffset = 28;

// Texture coordinates are at offset 24 in the mesh vertex struct.
static const DWORD g_dwTexCoordOffset = 24;

PolyMesh::PolyMesh()
{
    m_pMeshIB = NULL;
    m_pMeshVB = NULL;
    m_pScene = NULL;
    m_pModel = NULL;
    m_pMesh = NULL;
    m_pSkeletonInstance = NULL;
    m_dwSkinnedMeshBinding = 0;
    m_iSkeletonBoneIndex = -1;
}

SubDMesh::SubDMesh()
{
    m_pQuadArrayVB = NULL;
    m_pCurrentACC2PatchVB = NULL;
    m_pACC2PatchVB[0] = NULL;
    m_pACC2PatchVB[1] = NULL;
    m_pQuadNeighborhoodIB = NULL;
    m_dwPatchCount = 0;
    m_dwVertexCount = 0;
    m_dwMeshVertexStride = 0;
}

HRESULT SubDMesh::LoadFromXATG( ATG::D3DDevice* pd3dDevice, const CHAR* strFileName, const WCHAR* strRootBoneName, SubDMeshVector& OutputSubDMeshes, PolyMeshVector& OutputPolyMeshes, ATG::SkeletonInstance** ppSkeletonInstance )
{
    // Create scene object and load XATG file
    Scene* pScene = new Scene();
    pScene->GetResourceDatabase()->CreateDefaultResources();
    HRESULT hr = SceneFileParser::LoadXATGFile( strFileName, pScene, NULL, XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );

    if( FAILED(hr) )
    {
        FatalError( "Could not load XATG scene file \"%s\"", strFileName );
    }

    Animation* pAnimation = NULL;
    DWORD dwSkinnedMeshCount = 0;

    SubDMeshVector NewSubDMeshes;
    PolyMeshVector NewPolyMeshes;

    // Find all meshes and animations within the file
    NameIndexedCollection::iterator iter = pScene->GetInstanceList()->begin();
    NameIndexedCollection::iterator end = pScene->GetInstanceList()->end();
    while( iter != end )
    {
        NamedTypedObject* pNTO = *iter;
        iter++;
        if( pNTO->IsDerivedFrom( Animation::TypeID ) )
        {
            pAnimation = (Animation*)pNTO;
        }
        else if( pNTO->IsDerivedFrom( Model::TypeID ) )
        {
            Model* pModel = (Model*)pNTO;
            if( pModel->GetNumMeshMappings() == 0 )
            {
                continue;
            }

            StaticMesh* pMesh = (StaticMesh*)pModel->GetMeshMapping( 0 ).pMesh;
            if( pMesh->IsDerivedFrom( SkinnedMesh::TypeID ) )
            {
                ++dwSkinnedMeshCount;
            }

            XMFLOAT3 vCenter = pModel->GetWorldBound().GetCenter();
            FLOAT fRadius = pModel->GetWorldBound().GetMaxRadius();

            VertexData* pVData = pMesh->GetVertexData( 0 );
            if( pVData->GetNumVertexStreams() == 2 )
            {
                // Create a SubDMesh struct used for runtime manipulation of a preprocessed Catmull-Clark surface.
                DWORD dwPatchCount = pMesh->GetIndexData( 0 )->GetNumIndices() / MAX_POINTS;

                SubDMesh* pSubDMesh = new SubDMesh();

                pSubDMesh->m_pScene = pScene;
                pSubDMesh->m_pModel = pModel;
                pSubDMesh->m_pMesh = pMesh;
                pSubDMesh->m_dwPatchCount = dwPatchCount;
                pSubDMesh->m_pMeshVB = pVData->GetVertexStream( 0 )->pVertexBuffer;
                pSubDMesh->m_pQuadArrayVB = pVData->GetVertexStream( 1 )->pVertexBuffer;
                pSubDMesh->m_pQuadNeighborhoodIB = pMesh->GetIndexData( 0 )->GetIndexBuffer();

                pSubDMesh->m_dwVertexCount = pMesh->GetNumVertices();
                pSubDMesh->m_dwMeshVertexStride = pVData->GetVertexStream( 0 )->Stride;
                assert( pSubDMesh->m_dwMeshVertexStride == MESH_VERTEX_SIZE );
                pSubDMesh->CreateBuffers( pd3dDevice );

                pSubDMesh->m_vMeshCenter = vCenter;
                pSubDMesh->m_fMeshRadius = fRadius;

                NewSubDMeshes.push_back( pSubDMesh );
            }
            else if( pVData->GetNumVertexStreams() == 1 )
            {
                // Create a poly mesh struct.  Note: Poly meshes are not currently rendered
                PolyMesh* pPolyMesh = new PolyMesh();

                pPolyMesh->m_pScene = pScene;
                pPolyMesh->m_pModel = pModel;
                pPolyMesh->m_pMesh = pMesh;
                pPolyMesh->m_pMeshIB = pMesh->GetIndexData( 0 )->GetIndexBuffer();
                pPolyMesh->m_pMeshVB = pVData->GetVertexStream( 0 )->pVertexBuffer;
                assert( pVData->GetVertexStream( 0 )->Stride == MESH_VERTEX_SIZE );

                pPolyMesh->m_vMeshCenter = vCenter;
                pPolyMesh->m_fMeshRadius = fRadius;

                NewPolyMeshes.push_back( pPolyMesh );
            }
        }
    }

    // Set up skeleton and animation structures
    if( pAnimation != NULL )
    {
        Frame* pRootFrame = (Frame*)pScene->FindObjectOfType( strRootBoneName, Frame::TypeID );
        if( pRootFrame == NULL )
        {
            FatalError( "Could not find root bone for skeleton." );
        }

        // Create skeleton and skeleton instance
        ATG::Skeleton* pSkeleton = new ATG::Skeleton();
        pSkeleton->Initialize( pRootFrame );
        ATG::SkeletonInstance* pSkelInst = new ATG::SkeletonInstance();
        pSkelInst->Initialize( pSkeleton, dwSkinnedMeshCount );

        // Bind animation to skeleton instance
        pSkelInst->CreateAnimationBinding( pAnimation );

        // Bind skinned meshes to skeleton instance
        DWORD dwSkinIndex = 0;
        for( DWORD i = 0; i < NewSubDMeshes.size(); ++i )
        {
            SubDMesh* pSubDMesh = NewSubDMeshes[i];
            if( pSubDMesh->IsSkinned() )
            {
                pSkelInst->BindSkinnedMesh( dwSkinIndex, pSubDMesh->GetSkinnedMesh() );
                pSubDMesh->m_dwSkinnedMeshBinding = dwSkinIndex;
                ++dwSkinIndex;
            }
            pSubDMesh->m_pSkeletonInstance = pSkelInst;
            pSubDMesh->m_iSkeletonBoneIndex = pSkeleton->FindBone( pSubDMesh->GetModel()->GetName() );
        }
        for( DWORD i = 0; i < NewPolyMeshes.size(); ++i )
        {
            PolyMesh* pPolyMesh = NewPolyMeshes[i];
            if( pPolyMesh->IsSkinned() )
            {
                pSkelInst->BindSkinnedMesh( dwSkinIndex, pPolyMesh->GetSkinnedMesh() );
                pPolyMesh->m_dwSkinnedMeshBinding = dwSkinIndex;
                ++dwSkinIndex;
            }
            pPolyMesh->m_pSkeletonInstance = pSkelInst;
            pPolyMesh->m_iSkeletonBoneIndex = pSkeleton->FindBone( pPolyMesh->GetModel()->GetName() );
        }
        assert( dwSkinIndex == dwSkinnedMeshCount );

        *ppSkeletonInstance = pSkelInst;
    }

    // Merge mesh vectors
    for( DWORD i = 0; i < NewSubDMeshes.size(); ++i )
    {
        OutputSubDMeshes.push_back( NewSubDMeshes[i] );
    }
    for( DWORD i = 0; i < NewPolyMeshes.size(); ++i )
    {
        OutputPolyMeshes.push_back( NewPolyMeshes[i] );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GenPatches
// Desc: Walks through the quads in the mesh, and generates an ACC2 patch for each one.
//       Can optionally isolate one patch and zero out all of the rest.
//--------------------------------------------------------------------------------------
VOID SubDMesh::GenPatches( INT iIsolatePatch )
{
    DWORD dwPatchCount = GetNumPatches();

    // Select the current frame's buffer.
    static DWORD dwFrameCount = 0;
    DWORD dwIndex = ( dwFrameCount++ ) % ARRAYSIZE( m_pACC2PatchVB );
    m_pCurrentACC2PatchVB = m_pACC2PatchVB[dwIndex];

    // Set up source data pointers and strides.
    const BYTE* __restrict pPositionData = NULL;
    m_pMeshVB->Lock( 0, 0, (VOID**)&pPositionData, D3DLOCK_READONLY );

    const BYTE* __restrict pTangentData = pPositionData + g_dwTangentOffset;
    const BYTE* __restrict pTexCoordData = pPositionData + g_dwTexCoordOffset;

    DWORD dwPositionStride = m_dwMeshVertexStride;
    DWORD dwTangentStride = m_dwMeshVertexStride;
    DWORD dwTexCoordStride = m_dwMeshVertexStride;

    // If the mesh is skinned, perform CPU skinning and redirect source data pointers.
    if( IsSkinned() )
    {
        SkinMesh( pPositionData );
        pPositionData = (const BYTE*)m_pSkinnedMeshBuffer;
        pTangentData = pPositionData + sizeof(XMFLOAT3);
        dwPositionStride = sizeof(XMFLOAT3) * 2;
        dwTangentStride = dwPositionStride;
    }

    const BYTE* pValencePrefixData = NULL;
    m_pQuadArrayVB->Lock( 0, 0, (VOID**)&pValencePrefixData, D3DLOCK_READONLY );

    const UINT* pVertexIndices = NULL;
    m_pQuadNeighborhoodIB->Lock( 0, 0, (VOID**)&pVertexIndices, D3DLOCK_READONLY );

    ACC2PATCH* pACC2Patches = NULL;
    m_pCurrentACC2PatchVB->Lock( 0, 0, ( VOID** )&pACC2Patches, 0 );

    // This loop can be parallelized across multiple threads.
    // Each patch's computation is completely independent of other patches.
    for( INT i = 0; i < ( INT )dwPatchCount; ++i )
    {
        const UINT* pPatchVertexIndices = &pVertexIndices[ i * MAX_POINTS ];
        const BYTE* pValence = &pValencePrefixData[ i * 8 ];
        const BYTE* pPrefix = &pValencePrefixData[ i * 8 + 4 ];

        GenACC2PatchVMX( pPositionData, dwPositionStride,
                         pTangentData, dwTangentStride,
                         pTexCoordData, dwTexCoordStride,
                         pPatchVertexIndices, 
                         pValence, 
                         pPrefix, 
                         &pACC2Patches[i] );
    }

    m_pCurrentACC2PatchVB->Unlock();
    m_pQuadNeighborhoodIB->Unlock();
    m_pQuadArrayVB->Unlock();
    m_pMeshVB->Unlock();
}


//--------------------------------------------------------------------------------------
// Name: XMVector3TransformCoordTransposed()
// Desc: Multiplies a position by a transposed XMMATRIX.
//--------------------------------------------------------------------------------------
__forceinline XMVECTOR XMVector3TransformCoordTransposed( const XMVECTOR V0, const XMMATRIX matTransform )
{
    static const XMVECTOR vSelectYZW = XMVectorSelectControl( 0, 1, 1, 1 );
    static const XMVECTOR vSelectZW = XMVectorSelectControl( 0, 0, 1, 1 );

    XMVECTOR R0 = XMVector4Dot( V0, matTransform.r[0] );
    XMVECTOR R1 = XMVector4Dot( V0, matTransform.r[1] );
    XMVECTOR R2 = XMVector4Dot( V0, matTransform.r[2] );

    XMVECTOR Rxy = XMVectorSelect( R0, R1, vSelectYZW );
    XMVECTOR Result = XMVectorSelect( Rxy, R2, vSelectZW );

    return Result;
}


//--------------------------------------------------------------------------------------
// Name: XMVector3TransformTransposed()
// Desc: Multiplies a vector by a transposed XMMATRIX.
//--------------------------------------------------------------------------------------
__forceinline XMVECTOR XMVector3TransformTransposed( const XMVECTOR V0, const XMMATRIX matTransform )
{
    static const XMVECTOR vSelectYZW = XMVectorSelectControl( 0, 1, 1, 1 );
    static const XMVECTOR vSelectZW = XMVectorSelectControl( 0, 0, 1, 1 );

    XMVECTOR R0 = XMVector3Dot( V0, matTransform.r[0] );
    XMVECTOR R1 = XMVector3Dot( V0, matTransform.r[1] );
    XMVECTOR R2 = XMVector3Dot( V0, matTransform.r[2] );

    XMVECTOR Rxy = XMVectorSelect( R0, R1, vSelectYZW );
    XMVECTOR Result = XMVectorSelect( Rxy, R2, vSelectZW );

    return Result;
}


//--------------------------------------------------------------------------------------
// Name: SkinMesh()
// Desc: Using source vertex data in a specific layout, this method skins the position
//       and tangent components into a destination buffer.
//--------------------------------------------------------------------------------------
VOID SubDMesh::SkinMesh( const BYTE* __restrict pSrcVertexData )
{
    assert( m_pSkinnedMeshBuffer != NULL );
    assert( m_pSkeletonInstance != NULL );

    // Set up some useful constants.
    static const XMVECTOR vSelectW = XMVectorSelectControl( 0, 0, 0, 1 );
    static const XMVECTOR vOne = XMVectorReplicate( 1.0f );

    static XMFLOAT4A BoneBuffer[ 80 * 3 ];
    m_pSkeletonInstance->CreateBonePalette( m_dwSkinnedMeshBinding, &BoneBuffer[0], FALSE );

    // Obtain the bone palette from the skeleton instance.
    const ATG::Pose3x4& BoneMatrixPalette = m_pSkeletonInstance->m_pSkinnedMeshBindings[ m_dwSkinnedMeshBinding ].m_BoneMatrixPalette;

    const BYTE* pCurrentVertex = pSrcVertexData;
    XMFLOAT3* pDestVertex = m_pSkinnedMeshBuffer;
    for( DWORD i = 0; i < m_dwVertexCount; ++i )
    {
        const XMFLOAT3* pSrcPosition = (const XMFLOAT3*)pCurrentVertex;
        XMVECTOR vPos = XMLoadFloat3( pSrcPosition );
        XMVECTOR vTangent = XMLoadDecN4( (const XMDECN4*)( pCurrentVertex + g_dwTangentOffset ) );

        // Make sure the W component of the position is 1.
        vPos = XMVectorSelect( vPos, vOne, vSelectW );

        // Load the bone weights and bone indices.
        const XMVECTOR vBoneWeights = XMLoadUByteN4( ( XMUBYTEN4* )( pCurrentVertex + g_dwBlendWeightOffset ) );
        const XMVECTOR vBoneWeightsX = XMVectorSplatX( vBoneWeights );
        const XMVECTOR vBoneWeightsY = XMVectorSplatY( vBoneWeights );
        const XMVECTOR vBoneWeightsZ = XMVectorSplatZ( vBoneWeights );
        const XMVECTOR vBoneWeightsW = XMVectorSplatW( vBoneWeights );

        // Load the bone matrices from the bone matrix palette.
        const BYTE* pBoneIndices = pCurrentVertex + g_dwBlendIndicesOffset;
        XMMATRIX matBone0 = BoneMatrixPalette.LoadTransform( ( INT )pBoneIndices[0] );
        XMMATRIX matBone1 = BoneMatrixPalette.LoadTransform( ( INT )pBoneIndices[1] );
        XMMATRIX matBone2 = BoneMatrixPalette.LoadTransform( ( INT )pBoneIndices[2] );
        XMMATRIX matBone3 = BoneMatrixPalette.LoadTransform( ( INT )pBoneIndices[3] );

        // Transform position and blend.
        XMVECTOR vTransformedPos;
        XMVECTOR vPos0 = XMVector3TransformCoordTransposed( vPos, matBone0 );
        XMVECTOR vPos1 = XMVector3TransformCoordTransposed( vPos, matBone1 );
        XMVECTOR vPos2 = XMVector3TransformCoordTransposed( vPos, matBone2 );
        XMVECTOR vPos3 = XMVector3TransformCoordTransposed( vPos, matBone3 );
        vTransformedPos = XMVectorMultiply( vPos0, vBoneWeightsX );
        vTransformedPos = XMVectorMultiplyAdd( vPos1, vBoneWeightsY, vTransformedPos );
        vTransformedPos = XMVectorMultiplyAdd( vPos2, vBoneWeightsZ, vTransformedPos );
        vTransformedPos = XMVectorMultiplyAdd( vPos3, vBoneWeightsW, vTransformedPos );

        // Transform tangent and blend.
        XMVECTOR vTransformedTangent;
        XMVECTOR vTangent0 = XMVector3TransformTransposed( vTangent, matBone0 );
        XMVECTOR vTangent1 = XMVector3TransformTransposed( vTangent, matBone1 );
        XMVECTOR vTangent2 = XMVector3TransformTransposed( vTangent, matBone2 );
        XMVECTOR vTangent3 = XMVector3TransformTransposed( vTangent, matBone3 );
        vTransformedTangent = XMVectorMultiply( vTangent0, vBoneWeightsX );
        vTransformedTangent = XMVectorMultiplyAdd( vTangent1, vBoneWeightsY, vTransformedTangent );
        vTransformedTangent = XMVectorMultiplyAdd( vTangent2, vBoneWeightsZ, vTransformedTangent );
        vTransformedTangent = XMVectorMultiplyAdd( vTangent3, vBoneWeightsW, vTransformedTangent );

        XMStoreFloat3( pDestVertex++, vTransformedPos );
        XMStoreFloat3( pDestVertex++, vTransformedTangent );

        pCurrentVertex += m_dwMeshVertexStride;
    }
}


//--------------------------------------------------------------------------------------
// Name: GenerateBasisConversionStencils
// Desc: Fills in lookup tables of values used during basis conversion.
//--------------------------------------------------------------------------------------
VOID GenerateBasisConversionStencils()
{
    for( UINT Valence = 1; Valence < MAX_VALENCE; ++Valence )
    {
        const FLOAT fValence = ( FLOAT )Valence;

        const FLOAT fCosTerm = cosf( D3DX_PI / fValence );
        const FLOAT fDenominator = 3.0f * fValence * sqrtf( 4.0f + fCosTerm * fCosTerm );

        const FLOAT fAlpha = ( 1.0f / ( fValence * 3.0f ) ) + fCosTerm / fDenominator;
        const FLOAT fBeta = 1.0f / fDenominator;

        for( UINT Item = 0; Item < 32; ++Item )
        {
            FLOAT fItem = ( FLOAT )Item;
            FLOAT fPTerm = fAlpha * cosf( ( 2.0f * D3DX_PI * fItem ) / fValence );
            FLOAT fQTerm = fBeta * cosf( ( 2.0f * D3DX_PI * fItem + D3DX_PI ) / fValence );

            P_STENCIL(Item) = XMVectorReplicate( fPTerm );
            Q_STENCIL(Item) = XMVectorReplicate( fQTerm );
        }

        FLOAT fValenceSquared = fValence * fValence;
        FLOAT fCornerWeight = 1.0f / ( fValenceSquared + 5.0f * fValence );
        g_pCornerWeightTable1[Valence] = XMVectorReplicate( fValenceSquared * fCornerWeight );
        g_pCornerWeightTable2[Valence] = XMVectorReplicate( fCornerWeight );
        FLOAT fInnerWeight = cosf( ( 2.0f * D3DX_PI ) / fValence );
        g_pInnerWeightTable[Valence] = XMVectorReplicate( fInnerWeight );
    }
}


//--------------------------------------------------------------------------------------
// Name: GenACC2PatchVMX
// Desc: Given the description of a quad in the mesh, and its adjacency information,
//       generate the corresponding ACC2 patch for the quad.  This is the basis
//       conversion algorithm.
//--------------------------------------------------------------------------------------
#define VERT(a) ( XMLoadFloat3( (XMFLOAT3*)( pPositions + Vert[(a)] * dwPositionStride ) ) )
VOID SubDMesh::GenACC2PatchVMX( const BYTE* __restrict pPositions, const DWORD dwPositionStride,
                                const BYTE* __restrict pTangents, const DWORD dwTangentStride,
                                const BYTE* __restrict pTexCoords, const DWORD dwTexCoordStride,
                                const UINT Vert[MAX_POINTS],
                                const BYTE Val[4],
                                const BYTE Pref[4],
                                ACC2PATCH* __restrict pOutputACC2Patch )
{
    assert( Val[0] >= 3 && Val[1] >= 3 && Val[2] >= 3 && Val[3] >= 3 );
    assert( Pref[0] >= 4 && Pref[1] >= 4 && Pref[2] >= 4 && Pref[3] >= 4 );

    // Prefetch the output memory, to avoid the L2 miss when we're storing the final results.
    // If pOutputACC2Patch were cache aligned, we could use __dcbz for even more speed.
    __dcbt( 0, pOutputACC2Patch );
    __dcbt( 128, pOutputACC2Patch );
#ifdef PATCHELEMENT_FLOAT32
    __dcbt( 256, pOutputACC2Patch );
#endif

    // Define the temporary struct for the ACC2 patch.
    // Hopefully this will stay in registers.
    XMVECTOR ACC2Patch[ACC2_PATCH_STRIDE];

    // On the first run, generate the tangent stencil lookup table.
    static BOOL bStencilsInitialized = FALSE;
    if( !bStencilsInitialized )
    {
        GenerateBasisConversionStencils();
        bStencilsInitialized = TRUE;
    }

    // Define some useful constants for the vertex walk.
    const INT MOD4[8] = {0,1,2,3,0,1,2,3};
    const UINT cCorners[] = {0,12,15,3};    // b00
    const UINT cEdge0b10s[] = {4,13,11,2};   // b10,i
    const UINT cEdge0b20s[] = {8,14,7,1};  // b20,i
    const UINT cEdge1b10s[] = {1,8,14,7};  // b10,i+1
    const UINT cEdge1b20s[] = {2,4,13,11};   // b20,i+1
    const UINT cCenterUs[] = {16,9,18,6};    // b11u,i
    const UINT cCenterVs[] = {5,17,10,19}; // b11v,i

    // Define constants for weights in the vertex summations.
    const XMVECTOR vOppositeWeight = XMVectorReplicate( 1.0f );
    const XMVECTOR vEdgeWeight = XMVectorReplicate( 4.0f );

    // Process each corner of the quad separately.
    // Each corner generates 3 patch control points: the corner point (b00),
    // the edge 0 neighbor control point (b10,i), and the edge 1 neighbor
    // control point (b20,i).
    for( UINT CornerIndex = 0; CornerIndex < 4; ++CornerIndex )
    {
        const UINT Valence = ( UINT )Val[CornerIndex];

        const XMVECTOR vCornerWeight1 = g_pCornerWeightTable1[Valence];
        const XMVECTOR vCornerWeight2 = g_pCornerWeightTable2[Valence];

        // Compute the corner point value from the corner vertex
        // Vertex v
        XMVECTOR CornerPoint = VERT(CornerIndex) * vCornerWeight1;

        // Initialize the accumulator value
        XMVECTOR Accumulator = XMVectorZero();
        XMVECTOR Edge0Accumulator = XMVectorZero();
        XMVECTOR Edge1Accumulator = XMVectorZero();

        // Figure out the start and end index for neighbor walking
        UINT StartIndex = 4;
        if( CornerIndex > 0 )
            StartIndex = Pref[ CornerIndex - 1 ];
        UINT EndIndex = Pref[ CornerIndex ] - 1;

        // Add in the verts in the quad
        // p(i), q(i), p(i+1)
        Accumulator += VERT( MOD4[CornerIndex+1] ) * vEdgeWeight;
        Edge0Accumulator += VERT( MOD4[CornerIndex+1] ) * P_STENCIL(0);
        Edge1Accumulator += VERT( MOD4[CornerIndex+1] ) * P_STENCIL(Valence - 1);

        Accumulator += VERT( MOD4[CornerIndex+2] ) * vOppositeWeight;
        Edge0Accumulator += VERT( MOD4[CornerIndex+2] ) * Q_STENCIL(0);
        Edge1Accumulator += VERT( MOD4[CornerIndex+2] ) * Q_STENCIL(Valence - 1);

        Accumulator += VERT( MOD4[CornerIndex+3] ) * vEdgeWeight;
        Edge0Accumulator += VERT( MOD4[CornerIndex+3] ) * P_STENCIL(1);
        Edge1Accumulator += VERT( MOD4[CornerIndex+3] ) * P_STENCIL(0);

        // Add in the vertex before the neighbor walk
        if( CornerIndex > 0 )
        {
            Accumulator += VERT( Pref[CornerIndex - 1] - 1 ) * vOppositeWeight;
            Edge0Accumulator += VERT( Pref[CornerIndex - 1] - 1 ) * Q_STENCIL(1);
            Edge1Accumulator += VERT( Pref[CornerIndex - 1] - 1 ) * Q_STENCIL(0);
        }
        else
        {
            Accumulator += VERT( Pref[3] - 1 ) * vOppositeWeight;
            Edge0Accumulator += VERT( Pref[3] - 1 ) * Q_STENCIL(1);
            Edge1Accumulator += VERT( Pref[3] - 1 ) * Q_STENCIL(0);
        }

        // Add in the start vertex
        Accumulator += VERT(StartIndex) * vEdgeWeight;
        Edge0Accumulator += VERT(StartIndex) * P_STENCIL(2);
        Edge1Accumulator += VERT(StartIndex) * P_STENCIL(1);

        // Walk through the 1-ring neighbors
        UINT StencilIndex = 2;
        for( UINT CurrentIndex = StartIndex + 1; CurrentIndex < EndIndex; CurrentIndex += 2 )
        {
            Accumulator += VERT(CurrentIndex) * vOppositeWeight;
            Edge0Accumulator += VERT(CurrentIndex) * Q_STENCIL(StencilIndex);
            Edge1Accumulator += VERT(CurrentIndex) * Q_STENCIL(StencilIndex - 1);

            Accumulator += VERT(CurrentIndex + 1) * vEdgeWeight;
            Edge0Accumulator += VERT(CurrentIndex + 1) * P_STENCIL(StencilIndex + 1);
            Edge1Accumulator += VERT(CurrentIndex + 1) * P_STENCIL(StencilIndex);

            ++StencilIndex;
        }
        assert( StencilIndex == ( Valence - 1 ) );

        // Add in the vertex after the neighbor walk
        if( CornerIndex == 3 )
        {
            Accumulator += VERT(4) * vOppositeWeight;
            Edge0Accumulator += VERT(4) * Q_STENCIL(Valence - 1);
            Edge1Accumulator += VERT(4) * Q_STENCIL(Valence - 2);
        }
        else
        {
            Accumulator += VERT(EndIndex+1) * vOppositeWeight;
            Edge0Accumulator += VERT(EndIndex+1) * Q_STENCIL(Valence - 1);
            Edge1Accumulator += VERT(EndIndex+1) * Q_STENCIL(Valence - 2);
        }

        // Accumulate the neighbor contribution into the corner point value
        CornerPoint += ( Accumulator * vCornerWeight2 );

        // Store the corner point value in the patch
        ACC2Patch[cCorners[CornerIndex]] = CornerPoint;

        // Store the neighbor control point values in the patch
        ACC2Patch[cEdge0b10s[CornerIndex]] = CornerPoint + Edge0Accumulator;
        ACC2Patch[cEdge1b10s[CornerIndex]] = CornerPoint + Edge1Accumulator;
    }

    // Now that the 12 corner and edge control points are computed, construct
    // the inner 8 control points.
    for( UINT CornerIndex = 0; CornerIndex < 4; ++CornerIndex )
    {
        UINT StartIndex = 4;
        if( CornerIndex > 0 )
            StartIndex = Pref[ CornerIndex - 1 ];
        UINT EndIndex = Pref[ CornerIndex ] - 1;

        XMVECTOR CornerPos = ACC2Patch[cCorners[CornerIndex]];

        // Load some constants from a lookup table.
        const XMVECTOR vConstantV = g_pInnerWeightTable[Val[CornerIndex]];
        const XMVECTOR vConstantI0 = g_pInnerWeightTable[Val[MOD4[CornerIndex+1]]];
        const XMVECTOR vConstantI1 = g_pInnerWeightTable[Val[MOD4[CornerIndex+3]]];

        // Define some useful math constants.
        static const XMVECTOR vTwo = XMVectorReplicate( 2.0f );
        static const XMVECTOR vThree = XMVectorReplicate( 3.0f );
        static const XMVECTOR vOneThird = XMVectorReplicate( 1.0f / 3.0f );
        static const XMVECTOR vTwoThirds = XMVectorReplicate( 2.0f / 3.0f );
        static const XMVECTOR vOneEighteenth = XMVectorReplicate( 1.0f / 18.0f );

        // Compute the V tangent point for this corner.
        XMVECTOR ConstantX = ( vConstantI0 * vOneThird ) * CornerPos
            + ( ( vThree - vTwo * vConstantV - vConstantI0 ) * vOneThird ) * ACC2Patch[cEdge0b10s[CornerIndex]]
        + ( vConstantV * vTwoThirds ) * ACC2Patch[cEdge0b20s[CornerIndex]];

        XMVECTOR VertQIMinusOne = VERT(4);
        if( CornerIndex != 3 )
            VertQIMinusOne = VERT(EndIndex + 1);
        XMVECTOR ConstantY = vOneEighteenth * ( vTwo * ( VERT(MOD4[CornerIndex+3]) - VERT(EndIndex) ) + VERT(MOD4[CornerIndex+2]) - VertQIMinusOne );

        // Store the V tangent point.
        ACC2Patch[cCenterVs[CornerIndex]] = ConstantX + ConstantY;

        // Compute the U tangent point for this corner.
        XMVECTOR ConstantX1 = ( vConstantI1 * vOneThird ) * CornerPos
            + ( ( vThree - vTwo * vConstantV - vConstantI1 ) * vOneThird ) * ACC2Patch[cEdge1b10s[CornerIndex]]
        + ( vConstantV * vTwoThirds ) * ACC2Patch[cEdge1b20s[CornerIndex]];

        XMVECTOR VertQIPlusOne = VERT( Pref[3] - 1 );
        if( CornerIndex > 0 )
            VertQIPlusOne = VERT( Pref[CornerIndex - 1] - 1 );

        XMVECTOR ConstantY1 = vOneEighteenth * ( vTwo * ( VERT(StartIndex) - VERT(MOD4[CornerIndex+1]) ) + VertQIPlusOne - VERT(MOD4[CornerIndex+2]) );

        // Store the U tangent point.
        ACC2Patch[cCenterUs[CornerIndex]] = ConstantX1 - ConstantY1;
    }

    // Write all of the patch control points back to system memory.
#ifdef PATCHELEMENT_FLOAT16
    for( UINT i = 0; i < ARRAYSIZE(ACC2Patch); ++i )
    {
        XMStoreHalf4( &pOutputACC2Patch->m_vPoints[i], ACC2Patch[i] );
    }
#elif defined(PATCHELEMENT_FLOAT32)
    for( UINT i = 0; i < ARRAYSIZE( ACC2Patch ); ++i )
    {
        XMStoreFloat4( &pOutputACC2Patch->m_vPoints[i], ACC2Patch[i] );
    }
#endif

    // Copy tangents and texcoords into the ACC2 patch.  This will eliminate the need
    // for a dependent fetch in the vertex shader.
    for( UINT i = 0; i < 4; ++i )
    {
        UINT Index = Vert[i];
        XMVECTOR vTangent = XMLoadFloat3( (const XMFLOAT3*)( pTangents + Index * dwTangentStride ) );
        XMStoreDecN4( &pOutputACC2Patch->m_vTangents[i], vTangent );

        pOutputACC2Patch->m_vTexCoords[i] = *(const XMHALF2*)( pTexCoords + Index * dwTexCoordStride );
    }
}


//--------------------------------------------------------------------------------------
// Name: CreatePatchBuffers
// Desc: Allocates and fills vertex and index buffers corresponding to the ACC2 patches.
//--------------------------------------------------------------------------------------
VOID SubDMesh::CreateBuffers( ATG::D3DDevice* pd3dDevice )
{
    DWORD dwPatchCount = GetNumPatches();

    // Create double-buffered buffers to hold the patch control points.
    for( DWORD i = 0; i < ARRAYSIZE( m_pACC2PatchVB ); ++i )
    {
        pd3dDevice->CreateVertexBuffer( dwPatchCount * sizeof(ACC2PATCH),
                                        D3DUSAGE_CPU_CACHED_MEMORY, 
                                        0, 
                                        D3DPOOL_DEFAULT, 
                                        &m_pACC2PatchVB[i], 
                                        NULL );
    }
    m_pCurrentACC2PatchVB = m_pACC2PatchVB[0];

    m_pSkinnedMeshBuffer = new XMFLOAT3[m_dwVertexCount * 2];
}
