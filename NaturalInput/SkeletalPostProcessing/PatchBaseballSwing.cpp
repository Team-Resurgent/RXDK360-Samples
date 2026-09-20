//--------------------------------------------------------------------------------------
// PatchBaseballSwing.cpp
//
// Implements a filter that reposition the player's arms to look like the player has
// adopted a natural hitter pose.
// 
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "PatchBaseballSwing.h"


#define EPSILON 0.000001
#define FLOAT_EQ(x,v) (((v - EPSILON) < x) && (x <( v + EPSILON )))


//--------------------------------------------------------------------------------------
// Name: GetMasterXXXXIndex, GetSlaveXXXXIndex or GetOppXXXXIndex
// Desc: Family of helper functions that return a position index for skeleton joints 
//       depending on whether the player is batting from right or left.
//--------------------------------------------------------------------------------------
inline NUI_SKELETON_POSITION_INDEX GetMasterShoulderIndex( BOOL bIsBattingRight ) 
    { return bIsBattingRight ? NUI_SKELETON_POSITION_SHOULDER_LEFT : NUI_SKELETON_POSITION_SHOULDER_RIGHT; }

inline NUI_SKELETON_POSITION_INDEX GetMasterElbowIndex( BOOL bIsBattingRight ) 
    { return bIsBattingRight ? NUI_SKELETON_POSITION_ELBOW_LEFT : NUI_SKELETON_POSITION_ELBOW_RIGHT; }

inline NUI_SKELETON_POSITION_INDEX GetMasterWristIndex( BOOL bIsBattingRight ) 
    { return bIsBattingRight ? NUI_SKELETON_POSITION_WRIST_LEFT : NUI_SKELETON_POSITION_WRIST_RIGHT; }

inline NUI_SKELETON_POSITION_INDEX GetMasterHandIndex( BOOL bIsBattingRight ) 
    { return bIsBattingRight ? NUI_SKELETON_POSITION_HAND_LEFT : NUI_SKELETON_POSITION_HAND_RIGHT; }

inline NUI_SKELETON_POSITION_INDEX GetOppositeShoulderIndex( BOOL bIsBattingRight ) 
    { return bIsBattingRight ? NUI_SKELETON_POSITION_SHOULDER_RIGHT : NUI_SKELETON_POSITION_SHOULDER_LEFT; }

inline NUI_SKELETON_POSITION_INDEX GetSlaveElbowIndex( BOOL bIsBattingRight ) 
    { return bIsBattingRight ? NUI_SKELETON_POSITION_ELBOW_RIGHT : NUI_SKELETON_POSITION_ELBOW_LEFT; }

inline NUI_SKELETON_POSITION_INDEX GetSlaveWristIndex( BOOL bIsBattingRight ) 
    { return bIsBattingRight ? NUI_SKELETON_POSITION_WRIST_RIGHT : NUI_SKELETON_POSITION_WRIST_LEFT; }

inline NUI_SKELETON_POSITION_INDEX GetSlaveHandIndex( BOOL bIsBattingRight ) 
    { return bIsBattingRight ? NUI_SKELETON_POSITION_HAND_RIGHT : NUI_SKELETON_POSITION_HAND_LEFT; }


//--------------------------------------------------------------------------------------
// Name: XMVectorNearEqualBool
// Desc: Return whether two vectors are equal within a given range.
//--------------------------------------------------------------------------------------
inline BOOL XMVectorNearEqualBool( XMVECTOR vV1, XMVECTOR vV2, XMVECTOR vEpsilon )
{
    XMVECTOR vResult = XMVectorNearEqual( vV1, vV2, vEpsilon );

    if( XMVectorGetX( vResult ) == 0.0f || XMVectorGetY( vResult ) == 0.0f || XMVectorGetZ( vResult ) == 0.0f )
    {
        return FALSE;
    }

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: ComputeMatrixFromAxisToAxis
// Desc: Returns the transformation matrix needed to rotate a point on the line 
//       segment define by vVectorA and align it on the line segment defined by 
//       vVectorB.
//-----------------------------------------------------------------------------
XMMATRIX ComputeMatrixFromAxisToAxis( XMVECTOR vVectorA, XMVECTOR vVectorB )
{
    FLOAT fAngle = XMVectorGetX( XMVector3AngleBetweenVectors( vVectorA, vVectorB ) );

    XMVECTOR vAxis = XMVector3Normalize( XMVector3Cross( vVectorA, vVectorB ) );
    if( XMVectorNearEqualBool( vAxis, XMVectorZero(), XMVectorSplatEpsilon() ) )
    {
        return XMMatrixIdentity();
    }
    
    XMMATRIX mResult = XMMatrixRotationNormal( vAxis, fAngle );

    return mResult;
}


//-----------------------------------------------------------------------------
// Name: ComputeMatrixToOrientedWorldXZ
// Desc: Given three vecotrs, returns the transformation matrix to translate 
//       the first vector to the origin, align the second wit hthe positive X 
//       axis and the third one with the positive Z side of the plane formed X 
//       and Z axises.
//-----------------------------------------------------------------------------
XMMATRIX ComputeMatrixToOrientedWorldXZ( XMVECTOR vOrigin, XMVECTOR vXAxis, XMVECTOR vZPlane )
{
    XMMATRIX mToOrigin = XMMatrixTranslationFromVector( vOrigin * -1 );

    XMMATRIX mToXAxis = ComputeMatrixFromAxisToAxis( XMVector3Transform( vXAxis, mToOrigin ), 
                                                     XMVectorSet( 1.0f, 0.0f, 0.0f, 0.0f ) );

    XMVECTOR vToZPlane = XMVector3Transform( XMVector3Transform( vZPlane, mToOrigin ), mToXAxis );
    vToZPlane = XMVectorSet( 0.0f, XMVectorGetY( vToZPlane ), XMVectorGetZ( vToZPlane ), 0.0f );

    XMMATRIX mToZPlane;
    XMVECTOR vAngle = XMVector3AngleBetweenVectors( vToZPlane, XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f ) );
    if( XMVectorGetX( XMVectorIsNaN( vAngle ) ) || XMVectorGetX( XMVectorIsInfinite( vAngle ) ) )
    {
        mToZPlane = XMMatrixIdentity();
    }
    else
    {
        if( XMVectorGetY( vToZPlane ) < 0.0f  )
            mToZPlane = XMMatrixRotationX( XMVectorGetX( vAngle ) );
        else
            mToZPlane = XMMatrixRotationX( XMVectorGetX( vAngle ) * -1 );
    }

    return mToOrigin * mToXAxis * mToZPlane;
}


//-----------------------------------------------------------------------------
// Name: ComputeMatrixToOrientedWorldXY
// Desc: Given three vecotrs, returns the transformation matrix to translate 
//       the first vector to the origin, align the second wit hthe positive X 
//       axis and the third one with the positive Y side of the plane formed X 
//       and Y axises.
//-----------------------------------------------------------------------------
XMMATRIX ComputeMatrixToOrientedWorldXY( XMVECTOR vOrigin, XMVECTOR vXAxis, XMVECTOR vYPlane )
{
    XMMATRIX mToOrigin = XMMatrixTranslationFromVector( vOrigin * -1 );

    XMMATRIX mToXAxis = ComputeMatrixFromAxisToAxis( XMVector3Transform( vXAxis, mToOrigin ), 
                                                     XMVectorSet( 1.0f, 0.0f, 0.0f, 0.0f ) );

    XMVECTOR vToYPlane = XMVector3Transform( XMVector3Transform( vYPlane, mToOrigin ), mToXAxis );
    vToYPlane = XMVectorSet( 0.0f, XMVectorGetY( vToYPlane ), XMVectorGetZ( vToYPlane ), 0.0f );

    XMMATRIX mToYPlane;
    XMVECTOR vAngle = XMVector3AngleBetweenVectors( vToYPlane, XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ) );
    if( XMVectorGetX( XMVectorIsNaN( vAngle ) ) || XMVectorGetX( XMVectorIsInfinite( vAngle ) ) )
    {
        mToYPlane = XMMatrixIdentity();
    }
    else
    {
        if( XMVectorGetZ( vToYPlane ) < 0.0f  )
            mToYPlane = XMMatrixRotationX( XMVectorGetX( vAngle ) );
        else
            mToYPlane = XMMatrixRotationX( XMVectorGetX( vAngle ) * -1 );
    }

    return mToOrigin * mToXAxis * mToYPlane;
}


//-----------------------------------------------------------------------------
// Name: ComputeMatrixToAlignBatToYAxis
// Desc: Returns a transforation matrix that can be used to position the 
//       skeleton so that the attach point (half-way between nthe wrist and the 
//       hand joints) sits at the origina and titlted so the bat would extend 
//       along the positive Y axis.
//-----------------------------------------------------------------------------
XMMATRIX ComputeMatrixToAlignBatToYAxis( const NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{

    XMVECTOR vHandAnchor = ( pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ] - 
                             pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] ) / 2 + 
                           pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ];

    XMMATRIX mFromLockedSpace;

    XMVECTOR vWristToHand  = pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ] - 
                             pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ];
    XMVECTOR vWristToElbow = pSkeleton->SkeletonPositions[ GetMasterElbowIndex( bIsBattingRight ) ] - 
                             pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ];

    FLOAT fAngle = XMVectorGetX( XMVector3AngleBetweenVectors( vWristToElbow, vWristToHand ) );
    if( FLOAT_EQ( fAngle, 0.0f ) )
    {
        assert( false );
        return XMMatrixIdentity();
    }
    else if( XM_PI - abs( fAngle ) < 0.005 )
    {
        return ComputeMatrixToOrientedWorldXY( vHandAnchor, 
                                               pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ], 
                                               pSkeleton->SkeletonPositions[ GetMasterShoulderIndex( bIsBattingRight ) ] );
    }
    else
    {
        return ComputeMatrixToOrientedWorldXY( vHandAnchor, 
                                               pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ], 
                                               pSkeleton->SkeletonPositions[ GetMasterElbowIndex( bIsBattingRight ) ] ) *
               XMMatrixRotationZ( XM_PI );
    }
}


//-----------------------------------------------------------------------------
// Name: GetAngleFromWristToOppShoulder
// Desc: Returns the angle formed by the vectors going from the shoulder to the 
//       wrist and from the shoulder to the opposite shoulder.
//-----------------------------------------------------------------------------
FLOAT GetAngleFromWristToOppShoulder( const NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
    XMVECTOR vShoulderToWrist       = pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] - 
                                      pSkeleton->SkeletonPositions[ GetMasterShoulderIndex( bIsBattingRight ) ];
    XMVECTOR vShoulderToOppShoulder = pSkeleton->SkeletonPositions[ GetOppositeShoulderIndex( bIsBattingRight ) ] - 
                                      pSkeleton->SkeletonPositions[ GetMasterShoulderIndex( bIsBattingRight ) ];

    return XMVectorGetX( XMVector3AngleBetweenVectors( vShoulderToOppShoulder, vShoulderToWrist ) );
}


//-----------------------------------------------------------------------------
// Name: GetAngleFromWristToShoulder
// Desc: Returns the angle formed by the vectors going from the elbow to the 
//       wrist and from the elbow to the shoulder.
//-----------------------------------------------------------------------------
FLOAT GetAngleFromWristToShoulder( const NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
    XMVECTOR vElbowToWrist    = pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] - 
                                pSkeleton->SkeletonPositions[ GetMasterElbowIndex( bIsBattingRight ) ];
    XMVECTOR vElbowToShoulder = pSkeleton->SkeletonPositions[ GetMasterShoulderIndex( bIsBattingRight ) ] - 
                                pSkeleton->SkeletonPositions[ GetMasterElbowIndex( bIsBattingRight ) ];

    XMVECTOR vAngle = XMVector3AngleBetweenVectors( vElbowToShoulder, vElbowToWrist );

    return XMVectorGetX( vAngle );
}


//-----------------------------------------------------------------------------
// Name: ComputeWristRotationAxis
// Desc: Returns the axis along which the hand should be rotated. The axis is 
//       the tangent to the wrist on a circle that has the shoulder as its 
//       origin and lie on a plane formed with the player's shoulders and the 
//       wrist of the hand holding the bat.
//-----------------------------------------------------------------------------
XMVECTOR ComputeWristRotationAxis( const NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
    FLOAT fAngle = GetAngleFromWristToOppShoulder( pSkeleton, bIsBattingRight );

    DWORD dwCount = 0;
    while( fAngle > 45 * XM_PI / 180 )
    {
        fAngle -= 45 * XM_PI / 180;
        ++ dwCount;
    }

    // Get the tangent
    FLOAT fTangent = tan( fAngle );

    // Compute the rotation axis
    FLOAT x1 = cos( fAngle );
    FLOAT y1 = sin( fAngle );

    FLOAT x2 = x1 + sqrt( pow( fTangent, 2 ) - pow( y1, 2 ) );
    FLOAT y2 = 0;

    XMVECTOR v0 = XMVectorSet( x1, y1, 0.0f, 0.0f );
    XMVECTOR v1 = XMVectorSet( x2, y2, 0.0f, 0.0f );
    while( dwCount )
    {
        v0 = XMVector3Transform( v0, XMMatrixRotationZ( 45 * XM_PI / 180 ) );
        v1 = XMVector3Transform( v1, XMMatrixRotationZ( 45 * XM_PI / 180 ) );
        -- dwCount;
    }

    v1 = XMVector3Normalize( v1 - v0 );

    XMVECTOR vShoulderToWrist       = pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] - 
                                      pSkeleton->SkeletonPositions[ GetMasterShoulderIndex( bIsBattingRight ) ];

    XMVECTOR vShoulderToOppShoulder = pSkeleton->SkeletonPositions[ GetOppositeShoulderIndex( bIsBattingRight ) ] - 
                                      pSkeleton->SkeletonPositions[ GetMasterShoulderIndex( bIsBattingRight ) ];

    XMMATRIX mTo2D =  ComputeMatrixToOrientedWorldXY( XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f ), vShoulderToOppShoulder, vShoulderToWrist );

    XMVECTOR vDeterminant;
    
   //assert( XMVectorNearEqualBool( pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ], XMVector3Transform( XMVectorSet( x1, y1, 0.0f, 0.0f ), XMMatrixInverse( &vDeterminant, mTo2D ) ), XMVectorSplatEpsilon() ) );
    
    XMVECTOR vResult = XMVector3Normalize( XMVector3Transform( v1, XMMatrixInverse( &vDeterminant, mTo2D ) ) );
    return vResult;
}


//-----------------------------------------------------------------------------
// Name: ComputeHandRotationAngle
// Desc: Computes the angle at which the hand shoutld be rotated at the wrist.
//-----------------------------------------------------------------------------
FLOAT ComputeHandRotationAngle( const NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
    FLOAT fElbowAngle = GetAngleFromWristToShoulder( pSkeleton, bIsBattingRight );
    if( FLOAT_EQ( fElbowAngle, XM_PI ) )
    {
        return 67.0f * XM_PI / 180;
    }

    if( fElbowAngle <= 90.0f * XM_PI / 180 )
    {
        return 0.0f;
    }

    return ( fElbowAngle - 90 * XM_PI / 180 ) / ( XM_PI - 90 * XM_PI / 180 ) * 67.0f * XM_PI / 180;
}


//-----------------------------------------------------------------------------
// Name: PositionHandToNeutral
// Desc: Sets the hand at the same angle as the forearm so that is is aligned 
//       with it and appear to extends it.This is refered as the neutral pose 
//       for the purpose of this filter.
//-----------------------------------------------------------------------------
VOID PositionHandToNeutral( NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
    // Get rotation angle and axis for the forearm
    XMVECTOR vWrist = XMVector3Transform( pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ], 
                                          XMMatrixTranslationFromVector( -1 * pSkeleton->SkeletonPositions[ GetMasterElbowIndex( bIsBattingRight ) ] ) );
    FLOAT fAngle = XMVectorGetX( XMVector3AngleBetweenVectors( XMVectorSet( 1.0f, 0.0f, 0.0f, 1.0f ), vWrist ) );
    XMVECTOR vAxis = XMVector3Normalize( XMVector3Cross( XMVectorSet( 1.0f, 0.0f, 0.0f, 1.0f ), vWrist ) );

    // Position the hand at the end of the forearm with the same rotation angle, around the axis
    pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ] = XMVectorSet( XMVectorGetX( XMVector3Length( pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ] - 
                                                                                                                        pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] ) ), 0.0f, 0.0f, 0.0f );
    if( ! XMVectorNearEqualBool( vAxis, XMVectorZero(), XMVectorSplatEpsilon() ) )
    {
        pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ] = XMVector3Transform( pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ], 
                                                                                                    XMMatrixRotationNormal( vAxis, fAngle ) );
    }
    pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ] = XMVector3Transform( pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ], 
                                                                                                XMMatrixTranslationFromVector( pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] ) );
}


//-----------------------------------------------------------------------------
// Name: PositionHand
// Desc: Sets the hand in the position it is expected to be in order to hold a 
//       baseball bat.
//-----------------------------------------------------------------------------
VOID PositionHand( NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
    PositionHandToNeutral( pSkeleton, bIsBattingRight );

    FLOAT fAngle = ComputeHandRotationAngle( pSkeleton, bIsBattingRight );
    if( !bIsBattingRight )
    {
        fAngle *= -1;
    }

    if( ! FLOAT_EQ( fAngle, 0.0f ) )
    {
        pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ] = XMVector3Transform( pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ], 
                                                                                                    XMMatrixTranslationFromVector( -1 * pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] ) );
        pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ] = XMVector3Transform( pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ], 
                                                                                                    XMMatrixRotationNormal( ComputeWristRotationAxis( pSkeleton, bIsBattingRight ), fAngle * -1 ) );
        pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ] = XMVector3Transform( pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ], 
                                                                                                    XMMatrixTranslationFromVector( pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] ) );

    }

}


//-----------------------------------------------------------------------------
// Name: PositionSlaveHand
// Desc: Sets the slave hand on top of the master hand, along the bat vector.
//-----------------------------------------------------------------------------
VOID PositionSlaveHand( NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
    FLOAT fHalfHandLength = XMVectorGetX( XMVector3Length( ( pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ] - 
                                                             pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] ) / 2 ) );

    XMMATRIX mC = ComputeMatrixToAlignBatToYAxis( pSkeleton, bIsBattingRight );

    XMVECTOR vSlaveHand = XMVector3Transform( pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ], mC );
    XMVECTOR vSlaveWrist = XMVector3Transform( pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ], mC );

    XMMATRIX mD = XMMatrixTranslationFromVector( XMVectorSet( 0.0f, fHalfHandLength, 0.0f, 0.0f ) );

    vSlaveHand = XMVector3Transform( vSlaveHand, mD );
    vSlaveWrist = XMVector3Transform( vSlaveWrist, mD );

    XMVECTOR vOppShoulder = XMVector3Transform( pSkeleton->SkeletonPositions[ GetOppositeShoulderIndex( bIsBattingRight ) ], mC );
    vOppShoulder = XMVectorSet( XMVectorGetX( vOppShoulder ), 0.0f, XMVectorGetZ( vOppShoulder ), 0.0f );
    XMVECTOR vSlaveWrist2 = XMVectorSet( XMVectorGetX( vSlaveWrist ), 0.0f, XMVectorGetZ( vSlaveWrist ), 0.0f );

    FLOAT fAngle = XMVectorGetX( XMVector3AngleBetweenVectors( vSlaveWrist2, vOppShoulder ) );
    if( bIsBattingRight )
    {
        fAngle *= -1;
    }
    XMMATRIX mE = XMMatrixRotationY( fAngle );
    
    vSlaveHand  = XMVector3Transform( vSlaveHand, mE );
    vSlaveWrist = XMVector3Transform( vSlaveWrist, mE );

    XMVECTOR vDeterminant;
    pSkeleton->SkeletonPositions[ GetSlaveHandIndex( bIsBattingRight ) ]  = XMVector3Transform( vSlaveHand, 
                                                                                                XMMatrixInverse( &vDeterminant, mC ) );
    pSkeleton->SkeletonPositions[ GetSlaveWristIndex( bIsBattingRight ) ] = XMVector3Transform( vSlaveWrist, 
                                                                                                XMMatrixInverse( &vDeterminant, mC ) );
}


//-----------------------------------------------------------------------------
// Name: GetArmLength
// Desc: Returns to sum of the length of the arm and forearm.
//-----------------------------------------------------------------------------
FLOAT GetArmLength( const NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
    return XMVectorGetX( XMVector3Length( pSkeleton->SkeletonPositions[ GetMasterElbowIndex( bIsBattingRight ) ] - 
                                          pSkeleton->SkeletonPositions[ GetMasterShoulderIndex( bIsBattingRight ) ] ) ) + 
           XMVectorGetX( XMVector3Length( pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] - 
                                          pSkeleton->SkeletonPositions[ GetMasterElbowIndex( bIsBattingRight ) ] ) );
}


//-----------------------------------------------------------------------------
// Name: GetDistanceBetweenSlaveWristAndOppShoulder
// Desc: Returns the distance between the slave wrist and the corresponding 
//       shoulder on the skeleton.
//-----------------------------------------------------------------------------
FLOAT GetDistanceBetweenSlaveWristAndOppShoulder( const NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
    return XMVectorGetX( XMVector3Length( pSkeleton->SkeletonPositions[ GetOppositeShoulderIndex( bIsBattingRight ) ] - 
                                          pSkeleton->SkeletonPositions[ GetSlaveWristIndex( bIsBattingRight ) ] ) );
}


//-----------------------------------------------------------------------------
// Name: PositionSlaveElbow
// Desc: Position the slave elbow so that the slave arm assumes a natural 
//       looking pose (for a batting player).
//-----------------------------------------------------------------------------
VOID PositionSlaveElbow( NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
    XMMATRIX mD = ComputeMatrixToOrientedWorldXZ( pSkeleton->SkeletonPositions[ GetOppositeShoulderIndex( bIsBattingRight ) ], 
                                                  pSkeleton->SkeletonPositions[ GetSlaveWristIndex( bIsBattingRight ) ], 
                                                  pSkeleton->SkeletonPositions[ GetMasterShoulderIndex( bIsBattingRight ) ] );

    XMVECTOR vOppWrist = XMVector3Transform( pSkeleton->SkeletonPositions[ GetSlaveWristIndex( bIsBattingRight ) ], mD );

    XMVECTOR vOppElbow = XMVectorSet( XMVectorGetX( XMVector3Length( pSkeleton->SkeletonPositions[ GetMasterElbowIndex( bIsBattingRight ) ] - 
                                                                     pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] ) ), 0.0f, 0.0f, 0.0f );
    FLOAT a = XMVectorGetX( vOppElbow );
    FLOAT b = XMVectorGetX( XMVector3Length( pSkeleton->SkeletonPositions[ GetMasterElbowIndex( bIsBattingRight ) ] - 
                                             pSkeleton->SkeletonPositions[ GetMasterShoulderIndex( bIsBattingRight ) ] ) );
    FLOAT c = XMVectorGetX( vOppWrist );
    FLOAT valA = ( pow( b, 2 ) + pow( c, 2) - pow( a, 2 ) ) / ( 2 * b * c );
    valA = valA < -1.0f ? -1.0f : ( valA > 1.0f ? 1.0f : valA );

    FLOAT fRotAngleA = acos( valA );
    if( !bIsBattingRight )
    {
        fRotAngleA *= -1;
    }
    XMMATRIX mRotation = XMMatrixRotationY( fRotAngleA );
    vOppElbow = XMVector3Transform( vOppElbow, mRotation );

    XMVECTOR vDeterminant;
    XMMATRIX mE = XMMatrixInverse( &vDeterminant, mD );
    pSkeleton->SkeletonPositions[ GetSlaveElbowIndex( bIsBattingRight ) ] = XMVector3Transform( vOppElbow, mE );
}

        
//-----------------------------------------------------------------------------
// Name: PositionSlaveArm
// Desc: Position the slave arm so that it assumes a natural looking pose 
//       (for a batting player).
//-----------------------------------------------------------------------------
void PositionSlaveArm( NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
    if( GetDistanceBetweenSlaveWristAndOppShoulder( pSkeleton, bIsBattingRight ) >= 
        GetArmLength( pSkeleton, bIsBattingRight ) )
    {
        pSkeleton->SkeletonPositions[ GetSlaveElbowIndex( bIsBattingRight ) ] = ( pSkeleton->SkeletonPositions[ GetSlaveWristIndex( bIsBattingRight ) ] - pSkeleton->SkeletonPositions[ GetOppositeShoulderIndex( bIsBattingRight ) ] ) / 2 + 
                                                                                  pSkeleton->SkeletonPositions[ GetOppositeShoulderIndex( bIsBattingRight ) ];
    }
    else
    {
        PositionSlaveElbow( pSkeleton, bIsBattingRight );
    }
}


//-----------------------------------------------------------------------------
// Name: GetBatAttachPoint
// Desc: Return the point that should be attached to the player's hand. The 
//       returned value is the distance from the bat's origine to the attach 
//       point, along the bat's axis.
//-----------------------------------------------------------------------------
FLOAT GetBatAttachPoint(  const NUI_SKELETON_DATA* pSkeleton, BOOL bIsBattingRight )
{
   return XMVectorGetX( XMVector3Length( ( pSkeleton->SkeletonPositions[ GetMasterHandIndex( bIsBattingRight ) ] - 
                                           pSkeleton->SkeletonPositions[ GetMasterWristIndex( bIsBattingRight ) ] ) / 2 ) );
}


//-----------------------------------------------------------------------------
// Name: BattingSide::Update
// Desc: Determine the batting side for a player based on his or her stance.
//-----------------------------------------------------------------------------
VOID BattingSide::Update( const NUI_SKELETON_DATA* pSkeleton )
{
    if( abs( XMVectorGetZ( pSkeleton->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_LEFT ] ) - 
             XMVectorGetZ( pSkeleton->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ] ) ) > 0.2f ) //20cm
    {
        m_bIsBattingRight = XMVectorGetZ( pSkeleton->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_LEFT ] ) < 
                                          XMVectorGetZ( pSkeleton->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ] );
    }
}


//-----------------------------------------------------------------------------
// Name: Bat::Update
// Desc: Position the baseball bat in the player's hands.
//-----------------------------------------------------------------------------
VOID Bat::Update( const NUI_SKELETON_DATA* pSkeleton )
{
    static BattingSide battingSide;

    battingSide.Update( pSkeleton );

    FLOAT fBatLength = GetArmLength( pSkeleton, battingSide.IsBattingRight() );
    FLOAT fBatGrip   = GetBatAttachPoint( pSkeleton, battingSide.IsBattingRight() );

    XMVECTOR vDeterminant;
    XMMATRIX mFromLockedSpace = XMMatrixInverse( &vDeterminant, 
                                                 ComputeMatrixToAlignBatToYAxis( pSkeleton, battingSide.IsBattingRight() ) );


    m_vStart = XMVector3Transform( XMVectorSet( 0.0f, fBatGrip * -1, 0.0f, 0.0f ), mFromLockedSpace );
    m_vEnd   = XMVector3Transform( XMVectorSet( 0.0f, fBatLength - fBatGrip, 0.0f, 0.0f ), mFromLockedSpace );
}


//--------------------------------------------------------------------------------------
// Name: FilterBaseballSwing::Initialize
// Desc: Resets filter state to defaults.
//--------------------------------------------------------------------------------------
VOID FilterBaseballSwing::Initialize()
{
    ResetFilterState();
}


//--------------------------------------------------------------------------------------
// Name: FilterBaseballSwing::FilterSkeleton
// Desc: Implements the per-frame filter logic for the Baseball Swing filter.
//--------------------------------------------------------------------------------------
BOOL FilterBaseballSwing::FilterSkeleton( SkeletonFilterContext* pContext, 
                                          const NUI_SKELETON_DATA* pInputSkeleton, 
                                          NUI_SKELETON_DATA* pOutputSkeleton, 
                                          const FLOAT fDeltaNuiTime )
{
    if( pInputSkeleton->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_bActive = FALSE;
        return FALSE;
    }

    m_bActive = TRUE;
    m_battingSide.Update( pInputSkeleton );

    if( pInputSkeleton->eSkeletonPositionTrackingState[ GetMasterShoulderIndex( m_battingSide.IsBattingRight() ) ]   == NUI_SKELETON_POSITION_NOT_TRACKED ||
        pInputSkeleton->eSkeletonPositionTrackingState[ GetMasterElbowIndex( m_battingSide.IsBattingRight() ) ]      == NUI_SKELETON_POSITION_NOT_TRACKED ||
        pInputSkeleton->eSkeletonPositionTrackingState[ GetMasterWristIndex( m_battingSide.IsBattingRight() ) ]      == NUI_SKELETON_POSITION_NOT_TRACKED ||
        pInputSkeleton->eSkeletonPositionTrackingState[ GetOppositeShoulderIndex( m_battingSide.IsBattingRight() ) ] == NUI_SKELETON_POSITION_NOT_TRACKED    )
    {
        return FALSE;
    }

    XMemCpy( pOutputSkeleton, pInputSkeleton, sizeof( NUI_SKELETON_DATA ) );

    BOOL bProvideSupportForOppArm;
    if( abs( XMVectorGetZ( pInputSkeleton->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_LEFT ] ) - 
             XMVectorGetZ( pInputSkeleton->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ] ) ) > 0.2f ) //20cm
    {
        bProvideSupportForOppArm = TRUE;
    }
    else
    {
        bProvideSupportForOppArm = FALSE;
    }

    PositionHand( pOutputSkeleton, m_battingSide.IsBattingRight() );

    if( bProvideSupportForOppArm )
    {
        NUI_SKELETON_DATA backup;
        XMemCpy( &backup, pOutputSkeleton, sizeof( NUI_SKELETON_DATA ) );

        PositionSlaveHand( pOutputSkeleton, m_battingSide.IsBattingRight() );
        
        // If the slave arm cannot reach the bat, then we abandon any idea of position taht arm.
        // Allowing for the arm to stretch by up to 15% gives better result in this caes.
        if( GetDistanceBetweenSlaveWristAndOppShoulder( pOutputSkeleton, m_battingSide.IsBattingRight() ) > 
            GetArmLength( pOutputSkeleton, m_battingSide.IsBattingRight() ) * 1.15f )
        {
            XMemCpy( pOutputSkeleton, &backup, sizeof( NUI_SKELETON_DATA ) );
        }
        else
        {
            PositionSlaveArm( pOutputSkeleton, m_battingSide.IsBattingRight() );
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Name: FilterBaseballSwing::ResetFilterState
// Desc: Resets filter state to defaults.
//--------------------------------------------------------------------------------------
VOID FilterBaseballSwing::ResetFilterState()
{
    m_bActive = FALSE;
    
    m_battingSide.Reset();
}

//--------------------------------------------------------------------------------------
// Name: FilterBaseballSwing::DebugRenderUI
// Desc: Displays a series of debug values from the Baseball Swing filter logic.
//--------------------------------------------------------------------------------------
VOID FilterBaseballSwing::DebugRenderUI( IFilterDebugRenderer* pDebugRenderer )
{
}
