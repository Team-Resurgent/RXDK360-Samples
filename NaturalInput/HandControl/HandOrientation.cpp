//-------------------------------------------------------------------------------------
// HandOrientation.cpp
//  
// Tracks the orientation of the hand when the fist is facing the screen using image 
// moments.
//  
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "stdafx.h"
#include "HandOrientation.h"
#include "ATGNuiHandRefinement.h"
#include <math.h>
#include <nuiapi.h>

//--------------------------------------------------------------------------------------
// Calculate orientation for one hand
// 
//--------------------------------------------------------------------------------------

HRESULT HandOrientation::UpdateHand( 
        HandOrientationData* pHandOrientationData,  // The orientation specific data
        ATG::HandSpecificData *pHand,               // The hand from hand refinement
        ATG::OptionalRefinementData* pExtraData,       // Hand refinement must be told to calculate this optional data.
                                                    // We pass this in so that we don't have to calculate the hand mask
        ATG::OptionalHandSpecificData* pExtraHand,     // This is inside of ExtraRefinementData.  We pass it in so we don't 
                                                    // have to do left hand right hand tests
        XMVECTOR vElbow )
{
    static CONST FLOAT fAligned = 0.85f;
    pHandOrientationData->m_eHandState = HAND_STATE_UNKNOWN;
    
    XMVECTOR vElbowToHand;
    vElbowToHand = XMVector3Normalize( vElbow - pHand->m_vRefinedHand );

    FLOAT fElbowToHand[3]; 
    XMStoreFloat3( (XMFLOAT3*)&fElbowToHand[0], vElbowToHand );
    // We're basically doing a dot product with 0,0,1.  
    // that simplifies to just looking at the z component to deterimine if the hand is aligned.

    if ( fElbowToHand[2] < fAligned )
    {
        if ( fabsf( fElbowToHand[0] ) > fabsf ( fElbowToHand[1] ) )
        {
            if (fElbowToHand[0] > 0 )
                pHandOrientationData->m_eHandState = HAND_STATE_HAND_TOO_FAR_LEFT;
            else 
                pHandOrientationData->m_eHandState = HAND_STATE_HAND_TOO_FAR_RIGHT;
        }
        else
        {
            if (fElbowToHand[1] > 0 )
                pHandOrientationData->m_eHandState = HAND_STATE_HAND_TOO_LOW;
            else 
                pHandOrientationData->m_eHandState = HAND_STATE_HAND_TOO_HIGH;
        }
        
        return S_OK;
    }

    FLOAT fMoment00 = 0.0f;
    FLOAT fMoment11 = 0.0f;
    FLOAT fMoment22 = 0.0f;
    FLOAT fMoment02 = 0.0f;
    FLOAT fMoment20 = 0.0f;

    ATG::INT3Vector Mean( 0, 0, 0 );
    INT nValuesInAverage = 0;
    // We're finding the mean for the center of the hand print
    for ( INT y = 0; y < ATG::g_iNuiRefinementKernelSize320x240; ++y )
    {
        for ( INT x = 0; x < ATG::g_iNuiRefinementKernelSize320x240; ++x )
        {
            
            if ( pExtraHand->m_uHandDepthValues320x240[y*ATG::g_iNuiRefinementKernelSize320x240+x] != 0 ) //uDepth > uMin && uDepth < uMax && uSegmentationIndex == uTexelPlayerIndex )
            {
                Mean.iX +=x;
                Mean.iY +=y;
                Mean.iZ += pExtraHand->m_uHandDepthValues320x240[y*ATG::g_iNuiRefinementKernelSize320x240+x];
                ++nValuesInAverage;
            }
        }
    }

    if ( nValuesInAverage == 0 )
    {
        return E_FAIL;
    }


    pHandOrientationData->m_Mean.x = (FLOAT)Mean.iX / (FLOAT)nValuesInAverage;
    pHandOrientationData->m_Mean.y = (FLOAT)Mean.iY / (FLOAT)nValuesInAverage;
    pHandOrientationData->m_Mean.z = (FLOAT)Mean.iZ / (FLOAT)nValuesInAverage;

    // Here we're calculating the image moments
    FLOAT fY = 0;
    for ( INT y = 0; y < ATG::g_iNuiRefinementKernelSize320x240; ++y, fY+=1.0f)
    {
        FLOAT fX = 0.0f;
        for ( INT x = 0; x < ATG::g_iNuiRefinementKernelSize320x240; ++x, fX+=1.0f )
        {
            if ( pExtraHand->m_uHandDepthValues320x240[y*ATG::g_iNuiRefinementKernelSize320x240+x] != 0 ) //uDepth > uMin && uDepth < uMax && uSegmentationIndex == uTexelPlayerIndex )
            {

                FLOAT fxCentered = fX - pHandOrientationData->m_Mean.x;
                FLOAT fyCentered = fY - pHandOrientationData->m_Mean.y;
                FLOAT fxCentered2 = fxCentered * fxCentered;
                FLOAT fyCentered2 = fyCentered * fyCentered;
 
                fMoment11 += fxCentered * fyCentered;
                fMoment22 += fxCentered2 * fyCentered2;
                fMoment20 += fxCentered2;
                fMoment02 += fyCentered2;
            }
        }
    }

    fMoment00 = (FLOAT)nValuesInAverage;
    pHandOrientationData->m_fMoment00 = fMoment00;
    pHandOrientationData->m_fMoment11 = fMoment11;
    pHandOrientationData->m_fMoment22 = fMoment22;
    pHandOrientationData->m_fMoment20 = fMoment20;
    pHandOrientationData->m_fMoment02 = fMoment02;

    pHandOrientationData->m_fStandardDeviationInX = 
        sqrtf( pHandOrientationData->m_fMoment20 / pHandOrientationData->m_fMoment00 );
    pHandOrientationData->m_fStandardDeviationInY = 
        sqrtf( pHandOrientationData->m_fMoment02 / pHandOrientationData->m_fMoment00 );    
    
    // Calculate the second order central moments
    FLOAT fUPrime11 = pHandOrientationData->m_fMoment11 / pHandOrientationData->m_fMoment00;
    FLOAT fUPrime20 = pHandOrientationData->m_fMoment20 / pHandOrientationData->m_fMoment00;
    FLOAT fUPrime02 = pHandOrientationData->m_fMoment02 / pHandOrientationData->m_fMoment00;

    // Use the second order central moments to calculate the orientation
    // this is not a full 360 orienation it's just 180
    pHandOrientationData->m_fOrientation180 = 0.5f * atan2f( 2.0f * ( fUPrime11 ) , 
        ( fUPrime20 ) - ( fUPrime02 ) ) ;

    pHandOrientationData->m_fOrientation = pHandOrientationData->m_fOrientation180;   
    
    // Calculate the skew along the orientation axis to determine if we need to add 180 degrees
    // The sin/cos calculations can be moved out of the for loop
    FLOAT fCos = cosf( pHandOrientationData->m_fOrientation180 );
    FLOAT fSin = sinf( pHandOrientationData->m_fOrientation180 );
    FLOAT fRotatedMoment20 = 0;
    FLOAT fRotatedMoment30 = 0;
    fY = 0.0f;

    for ( INT y = 0; y < ATG::g_iNuiRefinementKernelSize320x240; ++y, fY+=1.0f)
    {
        FLOAT fX = 0.0f;
        for ( INT x = 0; x < ATG::g_iNuiRefinementKernelSize320x240; ++x, fX+=1.0f )
        {
            if ( pExtraHand->m_uHandDepthValues320x240[y*ATG::g_iNuiRefinementKernelSize320x240+x] != 0 ) //uDepth > uMin && uDepth < uMax && uSegmentationIndex == uTexelPlayerIndex )
            {
                // Rotate just the x component and calculate the moments needed for the standard deviation
                FLOAT fxCentered = fX - pHandOrientationData->m_Mean.x;
                FLOAT fyCentered = fY - pHandOrientationData->m_Mean.y;
                FLOAT fxRotated = fxCentered * fCos + fyCentered * fSin;
                FLOAT fxRotated2 = fxRotated * fxRotated;
                FLOAT fxRotated3 = fxRotated2 * fxRotated;   
                fRotatedMoment20 += fxRotated2;
                fRotatedMoment30 += fxRotated3;
            }
        }
    }

    FLOAT fStdDeviationRotated =  sqrtf( fRotatedMoment20 / pHandOrientationData->m_fMoment00 );

    pHandOrientationData->m_fOrientationBasedSkew = ( fRotatedMoment30 / pHandOrientationData->m_fMoment00 ) / 
            ( fStdDeviationRotated * pHandOrientationData->m_fStandardDeviationInX * pHandOrientationData->m_fStandardDeviationInX ); 
    if ( pHandOrientationData->m_fOrientationBasedSkew > 0.0f )
    {
        pHandOrientationData->m_fOrientation = XM_PI + pHandOrientationData->m_fOrientation;
    }
    // Rotate 90 degrees to account for texture orientation
    pHandOrientationData->m_fOrientation -= XM_PIDIV2;
    pHandOrientationData->m_eHandState = HAND_STATE_ORIENTATION_TRACKED;
    return S_OK;
}

//--------------------------------------------------------------------------------------
// This is called by the app when no-one is tracked
//--------------------------------------------------------------------------------------
VOID HandOrientation::Reset()
{
    m_RightHandData.Reset();
    m_LeftHandData.Reset();
}

//--------------------------------------------------------------------------------------
// Calculate orientation for 2 hands
//--------------------------------------------------------------------------------------
VOID HandOrientation::Update(  LPDIRECT3DTEXTURE9 pDepthAndSegmentationTexture320x240, 
                               INT iSkeletonDataSlot, 
                               const NUI_SKELETON_FRAME* pSkeletonFrame,
                               ATG::RefinementData* pRefinementData,
                               ATG::OptionalRefinementData* pExtraRefinementData )  
                    
{
    PIXBeginNamedEvent( 0, "Update Hand Orientation" );

    D3DLOCKED_RECT rect320x240;

    HRESULT hr = pDepthAndSegmentationTexture320x240->LockRect( 0, &rect320x240, NULL, D3DLOCK_READONLY );
    if ( FAILED ( hr ) ) return;

    
    PIXBeginNamedEvent( 0, "Update Right Hand" );
    UpdateHand( &m_RightHandData, &pRefinementData->m_RightHandData,
        pExtraRefinementData, &pExtraRefinementData->m_RightHand, 
        pSkeletonFrame->SkeletonData[iSkeletonDataSlot].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT] );
    PIXEndNamedEvent( );

    PIXBeginNamedEvent( 0, "Update Left Hand" );
    UpdateHand( &m_LeftHandData, &pRefinementData->m_LeftHandData,
        pExtraRefinementData, &pExtraRefinementData->m_LeftHand,
        pSkeletonFrame->SkeletonData[iSkeletonDataSlot].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT] );
    PIXEndNamedEvent( );

    pDepthAndSegmentationTexture320x240->UnlockRect( 0 );

    PIXEndNamedEvent();
}