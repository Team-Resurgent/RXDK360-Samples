//--------------------------------------------------------------------------------------
// File: ScalarSkinning.cpp
//
// Desc: Contains scalar floating point implementations of matrix palette skinning
//       Note that the scalar implementation is included for comparison only.
//       Prefer the VMX version on the Xbox
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "skinning.h"
#include "ppcintrinsics.h"
#include "xboxmath.h"


//--------------------------------------------------------------------------------------
// Name: Vec3TransformCoordNoAsm
// Desc: FPU vec3 transform
//--------------------------------------------------------------------------------------
__forceinline D3DXVECTOR3* WINAPI Vec3TransformCoordNoAsm( D3DXVECTOR3* pOut,
                                                           const D3DXVECTOR3* pV,
                                                           const XMMATRIX* pM )
{
    pOut->x = pV->x * pM->_11 + pV->y * pM->_21 + pV->z * pM->_31 + pM->_41;
    pOut->y = pV->x * pM->_12 + pV->y * pM->_22 + pV->z * pM->_32 + pM->_42;
    pOut->z = pV->x * pM->_13 + pV->y * pM->_23 + pV->z * pM->_33 + pM->_43;

    return pOut;
}


//--------------------------------------------------------------------------------------
// Name: Vec3TransformNormalNoAsm
// Desc: FPU vec3 normal transform
//--------------------------------------------------------------------------------------
__forceinline D3DXVECTOR3* WINAPI Vec3TransformNormalNoAsm( D3DXVECTOR3* pOut,
                                                            const D3DXVECTOR3* pV,
                                                            const XMMATRIX* pM )
{
    pOut->x = pV->x * pM->_11 + pV->y * pM->_21 + pV->z * pM->_31;
    pOut->y = pV->x * pM->_12 + pV->y * pM->_22 + pV->z * pM->_32;
    pOut->z = pV->x * pM->_13 + pV->y * pM->_23 + pV->z * pM->_33;

    return pOut;
}


//--------------------------------------------------------------------------------------
// Name: Skin_C
// Desc: C matrix palette skinning routine
//--------------------------------------------------------------------------------------
VOID SkinC( const XMFLOAT3* __restrict pXMInData,
            DWORD dwNumVerts,
            const SkinInfo* __restrict pSkinInfo,
            const XMMATRIX* __restrict pPalette,
            DWORD dwNumPaletteMatrices, bool bPreCacheMatrices,
            XMFLOAT3* __restrict pXMOutData )
 {
    const D3DXVECTOR3* pInData = ( D3DXVECTOR3* )pXMInData;
    D3DXVECTOR3* pOutData = ( D3DXVECTOR3* )pXMOutData;

    // Make sure the in and out pointers don't overlap - the __restrict modifier
    // requires this. It is also illegal for pSkinInfo or pPalette to overlap,
    // but we don't explicitly check for that.
    assert( pOutData >= pInData + dwNumVerts * 2 || pInData >= pOutData + dwNumVerts * 2 );

    // precache matrix palette if requested
    if( bPreCacheMatrices )
 {
        PreCachePalette( pPalette, dwNumPaletteMatrices );
    }

    D3DXVECTOR3 Temp;
    D3DXVECTOR3 OutData;
    for( DWORD i = 0; i < dwNumVerts; ++i )
 {
    // transform vec by palette matrix * weight
        Vec3TransformCoordNoAsm( &Temp, pInData,
                pPalette + pSkinInfo[i].Indices[0] );
        Temp *= ( pSkinInfo[i].Weights[0] / 255.0f );
        OutData = Temp;

    // continue if no more palettes affect this vec
        if( pSkinInfo[i].Indices[1] == 0 )
            goto NORMALS;
        Vec3TransformCoordNoAsm( &Temp, pInData,
                pPalette + pSkinInfo[i].Indices[1] );
        Temp *= ( pSkinInfo[i].Weights[1] / 255.0f );
        OutData += Temp;

        if( pSkinInfo[i].Indices[2] == 0 )
            goto NORMALS;
        Vec3TransformCoordNoAsm( &Temp, pInData,
                pPalette + pSkinInfo[i].Indices[2] );
        Temp *= ( pSkinInfo[i].Weights[2] / 255.0f );
        OutData += Temp;

        if( pSkinInfo[i].Indices[3] == 0 )
            goto NORMALS;
        Vec3TransformCoordNoAsm( &Temp, pInData,
               pPalette + pSkinInfo[i].Indices[3] );
        Temp *= ( pSkinInfo[i].Weights[3] / 255.0f );
        OutData += Temp;

NORMALS:
    // Store the transformed coordinate.
        *pOutData = OutData;
        pInData++;
        pOutData++;

    // Transform normal by palette matrix * weight
        Vec3TransformNormalNoAsm( &Temp, pInData,
                pPalette + pSkinInfo[i].Indices[0] );
        Temp *= ( pSkinInfo[i].Weights[0] / 255.0f );
        OutData = Temp;

    // continue if no more palettes affect this normal
        if( pSkinInfo[i].Indices[1] == 0 )
            goto END;
        Vec3TransformNormalNoAsm( &Temp, pInData,
                pPalette + pSkinInfo[i].Indices[1] );
        Temp *= ( pSkinInfo[i].Weights[1] / 255.0f );
        OutData += Temp;

        if( pSkinInfo[i].Indices[2] == 0 )
            goto END;
        Vec3TransformNormalNoAsm( &Temp, pInData,
                pPalette + pSkinInfo[i].Indices[2] );
        Temp *= ( pSkinInfo[i].Weights[2] / 255.0f );
        OutData += Temp;

        if( pSkinInfo[i].Indices[3] == 0 )
            goto END;
        Vec3TransformNormalNoAsm( &Temp, pInData,
                pPalette + pSkinInfo[i].Indices[3] );
        Temp *= ( pSkinInfo[i].Weights[3] / 255.0f );
        OutData += Temp;

END:
        *pOutData = OutData;
        pInData++;
        pOutData++;
    }
}



//--------------------------------------------------------------------------------------
// Name: PreCachePalette
// Desc: moves a matrix palette into the cache
//--------------------------------------------------------------------------------------
VOID PreCachePalette( const XMMATRIX* pPalette,
                      DWORD dwNumPaletteMatrices )
{
    assert( pPalette != NULL && dwNumPaletteMatrices != 0 );

    // palette must be 16 byte aligned
    assert( ( DWORD( pPalette ) & 0xF ) == 0 );

    // Prefetch the palette so it will be in the cache when needed.
    DWORD dwTotalSize = dwNumPaletteMatrices * sizeof( *pPalette );
    for( DWORD i = 0; i < dwTotalSize; i += 128 )
        __dcbt( i, pPalette );
}
