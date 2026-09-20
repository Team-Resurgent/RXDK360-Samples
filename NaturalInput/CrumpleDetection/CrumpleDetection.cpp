//--------------------------------------------------------------------------------------
// CrumpleDetectionApp.cpp
//
// This sample demonstrates the use of a tool that scores the quality of 
// skeletons tracked by ST.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <AtgNuiCommon.h>
#include <AtgDebugDraw.h>
#include "CrumpleDetection.h"

FLOAT C2DLineFitter::findLine( INT *pPoints2D , INT iNumPoints )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    assert( iNumPoints>=0);
    assert(pPoints2D);

    if ( iNumPoints <= 1 ) return 0;

    // compute centroid
    FLOAT centroidX = 0;
    FLOAT centroidY = 0;

    for(INT i=0; i< iNumPoints;i++)
    {
        centroidX += pPoints2D[2*i];
        centroidY += pPoints2D[2*i+1];
    }

    centroidX /= iNumPoints;
    centroidY /= iNumPoints;

    // compute moments (covarrience-matrix);
    FLOAT mXX=0, mXY=0, mYY=0;

    for(INT i=0; i< iNumPoints;i++)
    {
        FLOAT dx = pPoints2D[2*i] - centroidX;
        FLOAT dy = pPoints2D[2*i+1] - centroidY;

        mXX += dx*dx;
        mXY += dx*dy;
        mYY += dy*dy;
    }

    // compute the eigen values, which hold the spread of the points,
    // along its moments
    FLOAT e1,e2;
    eigvalues( mXX, mXY, mXY, mYY, e1,e2);
    assert( e1 >= 0 );
    assert( e2 >= 0 );


    PIXEndNamedEvent();


    // the minimum eigen-value holds the fitting error of
    // a line to the set of points.
    return sqrt( min(e1,e2) );
}


FLOAT Sigmoid( FLOAT fX, FLOAT fLowerCutoff, FLOAT fUpperCutoff )
{
    FLOAT widthOfTransationPart = fUpperCutoff - fLowerCutoff;
    assert( widthOfTransationPart > 0 );

    FLOAT scale = widthOfTransationPart * .125f;

    FLOAT center = fLowerCutoff  + widthOfTransationPart *.5f;


    return 1.0f/ ( 1.0f + exp( ( -fX + center ) / scale   ) );
}


void CDepthSkelChangeScore::ComputeChange( const NUI_SKELETON_FRAME& skeleton, const USHORT* pDepthImageCurr )
{        
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Compute change in depth image;

    const USHORT *depthLastP =  m_DepthImageLast[m_CurrentBuffer];
    const USHORT * depthImageCurrP = pDepthImageCurr;

    FLOAT diffThresh = DEPTH_SKEL_CHANGE_SKEL_DEPTH_CHANGE_THRESHOLD;
    FLOAT numPixelsDifferent[NUI_SKELETON_COUNT];
    FLOAT numPixels[NUI_SKELETON_COUNT];

    for(INT i=0; i < NUI_SKELETON_COUNT;i++)
    {
        numPixelsDifferent[i]=0;
        numPixels[i]=0;
    }

    // for each pixel, compute the difference the current depth map
    // and the previous depth map
    for(INT i=0; i < DEPTH_IMAGE_HEIGHT; i++)
    {
        for( INT j=0 ; j < DEPTH_IMAGE_WIDTH; j++ )
        {                     
            FLOAT depthLast = GETDEPTH( depthImageCurrP[j] );
            FLOAT depthCurr = GETDEPTH( depthLastP[j] );

            INT labelCurr =  PLAYERINDEX( depthImageCurrP[j] );
            INT labelLast =  PLAYERINDEX( depthLastP[j] );

            FLOAT weightCurr = 1;
            FLOAT weightLast = 1;

            if ( labelLast == labelCurr &&  labelCurr != 0)
            {
                FLOAT diff = fabs( depthCurr - depthLast );                
                numPixelsDifferent[labelCurr-1] +=  ( (diff < diffThresh ) ? 0 : 1.0f )  * weightLast * weightCurr;
                numPixels[labelCurr-1] +=  weightLast * weightCurr;

            }
            else
            {
                if ( labelCurr != 0 )
                {
                    numPixelsDifferent[labelCurr-1] += weightCurr;   
                    numPixels[labelCurr-1]+=weightCurr;
                }

                if ( labelLast != 0)
                {
                    numPixelsDifferent[labelLast-1] += weightLast;   
                    numPixels[labelLast-1]+=weightLast;
                }
            }
        }        

        depthLastP += DEPTH_IMAGE_WIDTH;
        depthImageCurrP += DEPTH_IMAGE_WIDTH;
    }


    // keep track of the depth map
    memcpy( m_DepthImageLast[m_CurrentBuffer], pDepthImageCurr,  DEPTH_IMAGE_HEIGHT*DEPTH_IMAGE_WIDTH * sizeof( USHORT ) );        

    for(INT i=0; i < NUI_SKELETON_COUNT; i++)
    {
        // compute the number of pixels changed;
        m_fNumPixelsChangedWeighted[i] = (FLOAT)( ( numPixelsDifferent[i] )/( numPixels[i]  + DELTA) );
    }

    // compute change in skeleton.
    // change in skeleton is taken as the maximum difference between the current skeleton
    // and any skeleton in the window.
    for(INT s=0;s < NUI_SKELETON_COUNT; s++)
    {
        FLOAT jointDiffMax = 0;
        if ( skeleton.SkeletonData[s].eTrackingState == NUI_SKELETON_TRACKED ) 
        {

            for(INT b =0; b < CHANGE_MEASURE_BUFFER_SIZE; b++)
            {
                FLOAT jointDiff = 0;
                FLOAT totalWeight =0;
                for(INT i=0;i < NUI_SKELETON_POSITION_COUNT; i++)
                {
                    // put more weight on the head and shoulders
                    FLOAT weight = 1;
                    switch(  i )
                    {
                    case NUI_SKELETON_POSITION_HEAD:                      
                    case NUI_SKELETON_POSITION_SHOULDER_CENTER:
                    case NUI_SKELETON_POSITION_SHOULDER_LEFT:
                    case NUI_SKELETON_POSITION_SHOULDER_RIGHT:
                        weight = 9; 
                        break;
                    case NUI_SKELETON_POSITION_HIP_CENTER:
                        weight = 3; 
                        break;
                    };

                    // look at projection;
                    XMVECTOR ptA = Project( m_SkeletonHistory[m_CurrentBuffer].SkeletonData[s].SkeletonPositions[i] );
                    XMVECTOR ptB = Project( skeleton.SkeletonData[s].SkeletonPositions[i] );

                    jointDiff += weight * XMVectorGetX( XMVector2Length( ptA - ptB ) );
                    totalWeight += weight;
                }

                jointDiff /= ( totalWeight );

                if ( jointDiff > jointDiffMax ) jointDiffMax = jointDiff;
            }   
        }

        m_fJointDiff[s] = jointDiffMax;
    }


    // keep track of skeleton
    m_SkeletonHistory[m_CurrentBuffer] = skeleton;

    // addvance current buffer
    m_CurrentBuffer = (m_CurrentBuffer+1) % CHANGE_MEASURE_BUFFER_SIZE;

    for(INT i=0;i < NUI_SKELETON_COUNT; i++)
    {
        m_fProbBlobChanging[i] = 
            Sigmoid( m_fNumPixelsChangedWeighted[i], 
            DEPTH_SKEL_CHANGE_BLOB_CHANGE_SIGMOID_LOWER, 
            DEPTH_SKEL_CHANGE_BLOB_CHANGE_SIGMOID_UPPER );

        m_fProbSkeletonChanging[i] = 
            Sigmoid( m_fJointDiff[i], 
            DEPTH_SKEL_CHANGE_SKEL_CHANGE_SIGMOID_LOWER,
            DEPTH_SKEL_CHANGE_SKEL_CHANGE_SIGMOID_UPPER );    
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------
// Name: AboveFloor:
// Desc: TRUE if point is above the floor 
//--------------------------------------------------------------------
BOOL AboveFloor( XMVECTOR floor, INT iX, INT iY, FLOAT iDepth )
{                    
    XMVECTOR point3d = Unproject( iX, iY, iDepth  );
    BOOL aboveFloor = XMVectorGetX( XMVector4Dot( floor, point3d ) ) > FLOOR_CLIP_HEIGHT_THRESH;
    return aboveFloor;
}


void CBlobOverlapDetector::BlobVerticalOcclusionScores( XMVECTOR floor,
                                                       const USHORT* pDepthImage, INT iPerimeterWidth ) 
{        
    for( INT i = 0; i < NUI_SKELETON_COUNT ; i++)
    {
        m_NumPerimeterPointsVertical[i] = 0;
        m_NumOccludingPointsVertical[i]  = 0;
        m_NumPixelsVertical[i]=0;         
    }

    // look at occlusions for each interior pixel.
    const USHORT* depthP = pDepthImage + 1 + DEPTH_IMAGE_WIDTH;        

    INT nextPixelStep = DEPTH_IMAGE_WIDTH;
    for(INT col=1 ; col < DEPTH_IMAGE_WIDTH -1; col++ )
    {
        // look down each column
        BOOL polarity = TRUE;
        for(INT i=1 ; i < DEPTH_IMAGE_HEIGHT - 1; i++ )
        {
            INT index = i*nextPixelStep;
            ASSERT_IN_DEPTHIMAGE(index);
            UINT8 label =  ( depthP[index] & 0x7 );

            if ( label != 0 )   
            {

                USHORT nextLabel = 0;

                // keep track of the number of pixels in each blob             
                m_NumPixelsVertical[label-1]++;               

                // look at the next pixel along the column;
                if ( polarity )
                {
                    ASSERT_IN_DEPTHIMAGE( index-nextPixelStep );
                    nextLabel = (*(depthP+index-nextPixelStep) ) & 0x7;     
                }
                else
                {
                    ASSERT_IN_DEPTHIMAGE( index+nextPixelStep );
                    nextLabel =  (*(depthP+index+nextPixelStep)) & 0x7;
                }            

                ASSERT_IN_DEPTHIMAGE( index );
                INT currDepth = depthP[index] >> 3;

                BOOL isEdgeDepthPixel = label != nextLabel;
                if ( isEdgeDepthPixel )
                {         
                    // if the next pixel has a different label,
                    // then its an edge.  If the edge is in the direction
                    // we are looking in (according to polarity), we can look along
                    // the span of perimterWidth pixels and count the number of depth
                    // pixels in front  (and potentially occluding ).
                    //
                    // polarity == FALSE corresponds to the downward facing edge
                    // here we only look for occlusion below the blob.
                    if ( polarity == FALSE )
                    {
                        INT offset=0;
                        for( INT ii=0; ii < iPerimeterWidth; ii++ )
                        {
                            int y = i+offset;  

                            if ( y < 0 || y >= DEPTH_IMAGE_HEIGHT )
                            {
                                break;
                            }

                            INT indexY = y *nextPixelStep;
                            ASSERT_IN_DEPTHIMAGE( indexY );                                
                            USHORT depthInBoundPel = depthP[indexY];
                            BOOL isOccluded = FALSE;                                

                            if ( ( depthInBoundPel & 0x7 ) != label  )
                            {
                                INT depthInBound = depthInBoundPel >> 3;
                                m_NumPerimeterPointsVertical[label-1]++;
                                if ( depthInBoundPel != 0  && 
                                    AboveFloor( floor, col, y, depthInBound * .001f ) &&
                                    depthInBound < currDepth)
                                {
                                    isOccluded = TRUE;                              
                                    m_NumOccludingPointsVertical[label-1]++;
                                }
                                AddBlobPerimeterPoint( col,y, isOccluded );                          
                            }

                            offset += polarity? -1 : 1;
                        }
                    }
                    polarity = !polarity;               
                }
            }   
        }
        depthP ++;
    }
}


void CBlobOverlapDetector::BlobOcclusionScores( XMVECTOR floor, const USHORT* pDepthImage, INT iPerimeterWidth ) 
{
    m_NumPerimeterPointsDisplay = 0;

    for( INT i = 0; i < NUI_SKELETON_COUNT ; i++)
    {
        m_NumPerimeterPoints[i] = 0;
        m_NumOccludingPoints[i]  = 0;
        m_NumEdgePoints[i] = 0;
        m_NumPixels[i]=0;
    }
    const USHORT* depthP = pDepthImage + 1 + DEPTH_IMAGE_WIDTH;

    // for each pixel in interror of depth image;
    for(INT i=1 ; i < DEPTH_IMAGE_HEIGHT - 1; i++ )
    {
        BOOL polarity = TRUE;

        // look across rows.
        for(INT j=1 ; j < DEPTH_IMAGE_WIDTH -1; j++ )
        {
            ASSERT_IN_DEPTHIMAGE(j);
            UINT8 label =  ( depthP[j] & 0x7 );

            if ( label != 0 )
            {
                USHORT nextLabel = 0;
                m_NumPixels[label-1]++;

                if ( polarity)
                {
                    ASSERT_IN_DEPTHIMAGE(j-1);
                    nextLabel = (*(depthP+j-1) ) & 0x7;     
                }
                else
                {
                    ASSERT_IN_DEPTHIMAGE(j+1);
                    nextLabel =  (*(depthP+j+1)) & 0x7;
                }            

                ASSERT_IN_DEPTHIMAGE(j);
                INT currDepth = depthP[j] >> 3;
                BOOL isEdgeDepthPixel = label != nextLabel;
                if ( isEdgeDepthPixel )
                {          
                    m_NumEdgePoints[label-1]++;

                    // if the next pixel has a different label,
                    // then its an edge. we can look along
                    // the span of perimterWidth pixels and count the number of depth
                    // pixels in front  (and potentially occluding ).
                    INT offset=0;
                    for( INT ii=0; ii < iPerimeterWidth; ii++ )
                    {
                        int x = j+offset;  

                        if ( x < 0 || x >= DEPTH_IMAGE_WIDTH )
                        {
                            break;
                        }

                        ASSERT_IN_DEPTHIMAGE(x);
                        INT depthInBoundPel = depthP[x];
                        BOOL isOccluded = FALSE;

                        if ( ( depthInBoundPel & 0x7 ) != label  )
                        {
                            ASSERT_IN_DEPTHIMAGE(x);
                            INT depthInBound = depthP[x] >> 3;
                            m_NumPerimeterPoints[label-1]++;
                            if ( depthInBoundPel != 0  && 
                                AboveFloor( floor, x, i, depthInBound * .001f ) &&
                                depthInBound < currDepth )
                            {
                                isOccluded = TRUE;                              
                                m_NumOccludingPoints[label-1]++;
                            }

                            AddBlobPerimeterPoint( x,i, isOccluded );                          
                        }
                        offset += polarity? -1 : 1;
                    }
                    polarity = !polarity;               
                }
            }            
        }
        depthP += DEPTH_IMAGE_WIDTH;
    }
}


void CBlobOverlapDetector::RenderOcclusionScores(  const USHORT* pDepthImage,   UINT8* pColorDebugBuffer )
{    
    const UINT8 colorLabels[][3] =
    {  { 0   , 0   , 0 },
    { 255 , 0   , 0 },
    { 0   , 255 , 0 },
    { 0   , 0   , 255 },
    { 0   , 255 , 255 },
    { 255 , 0   , 255 },
    { 255 , 255 , 0 },
    { 128 , 128 , 64 } };

    // render the labels;
    UINT8 *colorP = pColorDebugBuffer;
    const USHORT* depthP = pDepthImage;

    for(INT i=0 ; i < DEPTH_IMAGE_HEIGHT; i++ )
    {
        for(INT j=0 ; j < DEPTH_IMAGE_WIDTH; j++ )
        {
            UINT8 label =  ( depthP[j] & 0x7 );
            const UINT8 *color = colorLabels[label];
            colorP[j*3] = color[0];
            colorP[j*3+1] = color[1];
            colorP[j*3+2] = color[2];
        }

        depthP += DEPTH_IMAGE_WIDTH;
        colorP += DEPTH_IMAGE_WIDTH*3;
    }

    // render boundary points as gray;
    for( INT i=0; i < m_NumPerimeterPointsDisplay; i++)
    {
        INT x = m_PerimeterPoints[i*3  ];
        INT y = m_PerimeterPoints[i*3+1];
        INT occl = m_PerimeterPoints[i*3+2];
        INT pel = x + y * DEPTH_IMAGE_WIDTH ;

        if (  !occl )
        {
            pColorDebugBuffer[ pel*3     ] = 128;
            pColorDebugBuffer[ pel*3 + 1 ] = 128;
            pColorDebugBuffer[ pel*3 + 2 ] = 128;
        }
        else
        {
            pColorDebugBuffer[ pel*3     ] = 255;
            pColorDebugBuffer[ pel*3 + 1 ] = 255;
            pColorDebugBuffer[ pel*3 + 2 ] = 255;
        }
    }
}


FLOAT CClipedFromTopDetector::VerticalClip( INT iSkeletonIndex , XMVECTOR head, XMVECTOR waist , const USHORT* pDepthImage)
{          
    XMVECTOR waist2d = Project( waist );
    // compute y position of the waist;
    INT botY = (INT) XMVectorGetY( waist2d );

    // compute the height of this blob, as measured from the depth
    // image and relative to the waist.  (this will allow 
    // a skeleton to sit )
    ComputeVerticalSpan( iSkeletonIndex, pDepthImage, botY  ); 

    if ( m_Recapture[iSkeletonIndex] )
    {
        // this is a new blob, so the height
        // is computed
        SetBlobVerticalExtents( iSkeletonIndex ,head, waist);
        m_Recapture[iSkeletonIndex] = FALSE;            
    }

    // compute predicted vertical pixel span,
    // this is based on using the current depth and 
    // the measured height (in 3D ) of the blob.
    XMVECTOR top = Project( waist + XMVectorSet(0, m_f3DBlobHeight[iSkeletonIndex], 0 ,0 ) );
    XMVECTOR bot = Project( waist );

    m_fPredictedPixelSpan[iSkeletonIndex] = XMVectorGetX(XMVector2Length(top - bot));
#ifdef CRUMPLE_VERBOSE_DEBUG
    printf("\nwaist == %f %f %f\n", waist.x, waist.y, waist.z );
    printf("  height == %f  \n" , m_f3DBlobHeight[iSkeletonIndex] );
    printf("  span == %d \n", m_BlobBotLastFrame[iSkeletonIndex] - m_BlobTopLastFrame[iSkeletonIndex] );
    printf("  pspan == %lf \n",m_fPredictedPixelSpan[iSkeletonIndex]);
#endif

    // the score is based on how well the predicted verticel span
    // lines up with the measured vertical span.
    FLOAT score = ( m_BlobBotLastFrame[iSkeletonIndex] - m_BlobTopLastFrame[iSkeletonIndex]  )  /  (  m_fPredictedPixelSpan[iSkeletonIndex] + .000001f );

    // if the expected pixel span is smaller then the observed pixel span
    // re acquire the height of the blob.
    if ( score >= 1 )
    {
        SetBlobVerticalExtents( iSkeletonIndex , head, waist);
        score = 1.0;
    }

    return score ;
}   


void CClipedFromTopDetector::ComputeVerticalSpan( INT iSkeletonIndex, const USHORT* pDepthImage, INT iBotY ) 
{
    assert( iSkeletonIndex >=0 && iSkeletonIndex < NUI_SKELETON_COUNT );

    m_BlobTopLastFrame[iSkeletonIndex] = iBotY;               
    m_BlobBotLastFrame[iSkeletonIndex] = iBotY;

    const USHORT* depthP = pDepthImage;

    // for each pixel;
    for(INT i=0;i < DEPTH_IMAGE_HEIGHT; i++ )
    {
        for( INT j=0 ; j < DEPTH_IMAGE_WIDTH; j++)
        {
            ASSERT_IN_DEPTHIMAGE(j);
            UINT8 label =  ( depthP[j] & 0x7 ) -1;

            if ( label == iSkeletonIndex )
            {
                // found the top most point in the blob
                INT top = i;
                if ( top > iBotY ) top = iBotY;           
                m_BlobBotLastFrame[iSkeletonIndex] = iBotY; 
                m_BlobTopLastFrame[iSkeletonIndex] = top;
                m_BlobSpanXPos[iSkeletonIndex] = j;
                return;
            }
        }
        depthP+= DEPTH_IMAGE_WIDTH;
    }  
}


FLOAT CCrumpleScoringTerms::CrumpleScore( BOOL bUseHeadShape, BOOL bFirst,
                                          BOOL bBlobIsMoving, 
                                          const NUI_SKELETON_DATA& skeleton, INT iDepthLabel,  const USHORT* pDepthImage )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // if joints are not tracked then the arm is occluced
    m_bLeftArmOccluded = !( skeleton.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ELBOW_LEFT] == NUI_SKELETON_POSITION_TRACKED  &&
        skeleton.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_LEFT] == NUI_SKELETON_POSITION_TRACKED  && 
        skeleton.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_WRIST_LEFT] == NUI_SKELETON_POSITION_TRACKED   );

    m_bRightArmOccluded = !( skeleton.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ELBOW_RIGHT] == NUI_SKELETON_POSITION_TRACKED  &&
        skeleton.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_RIGHT] == NUI_SKELETON_POSITION_TRACKED  && 
        skeleton.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_WRIST_RIGHT] == NUI_SKELETON_POSITION_TRACKED   );

    m_NumEdgePoints =0;

    m_Scores.m_fGeometryOfSkeleton = CrumpleScoreGeom(skeleton,   iDepthLabel,  pDepthImage);

    if ( bFirst )
    {  
        // compute initial head score
        m_Scores.m_fHeadShoulderEdgeShape = TestHeadShoulderShape(skeleton, iDepthLabel, pDepthImage);

#ifdef CRUMPLE_VERBOSE_DEBUG  
        printf("init socre == %f", m_Scores.m_fHeadShoulderEdgeShape );
#endif

        // if initial head score is bad, keep checking head score;
        if (  m_Scores.m_fHeadShoulderEdgeShape  < CRUMPLE_SCORE_TERMS_INITIAL_HEAD_SCORE_THRESH )
        {
            m_bBadInitialHead = TRUE;
        }
        else
        {
            m_bBadInitialHead =FALSE;
        }
    }

    // compute head score if it was initially low
    // or signaled to compute it.
    if ( bUseHeadShape || m_bBadInitialHead == TRUE )
    {
        m_Scores.m_fHeadShoulderEdgeShape = TestHeadShoulderShape(skeleton, iDepthLabel, pDepthImage);
    }
    else
    {
        m_Scores.m_fHeadShoulderEdgeShape  = 1;
    }

    m_Scores.m_fClippedByImageBoundary = BorderScore(skeleton) ;

    // if blobs are not moving then compute are shape scores.
    // they are more reliable when blobs are stationary, and if a blob is moving
    // its likely to be a human.
    if ( !bBlobIsMoving )
    {
        m_Scores.m_fOccupancyAlongBone = LimbOccupancyScore(  skeleton, iDepthLabel, pDepthImage, m_bLeftArmOccluded, m_bRightArmOccluded  );
        m_Scores.m_fEdgePresenceAroundLimb= LimbEdgePresenceScore( skeleton, iDepthLabel, pDepthImage,  m_bLeftArmOccluded, m_bRightArmOccluded);
    }
    else
    {
        m_Scores.m_fOccupancyAlongBone  = 1.0f;
        m_Scores.m_fEdgePresenceAroundLimb  = 1.0f;
    }

    // iff both arms are occluded penalize overal score;
    FLOAT occlScore = 1;
    if ( m_bRightArmOccluded && m_bLeftArmOccluded )
    {
        occlScore = .5;
    }

    FLOAT overalScore = 
        m_Scores.m_fGeometryOfSkeleton *
        m_Scores.m_fHeadShoulderEdgeShape*
        m_Scores.m_fClippedByImageBoundary*
        m_Scores.m_fOccupancyAlongBone *
        m_Scores.m_fEdgePresenceAroundLimb;             

    overalScore /= CRUMPLE_SCORE_TERMS_OVERALLSCORE_RESCALE;
    if ( overalScore > 1 ) overalScore  = 1;     

    PIXEndNamedEvent();

    return overalScore;
}   


FLOAT CCrumpleScoringTerms::CrumpleScoreGeom(  const NUI_SKELETON_DATA& skeleton, INT iDepthLabel, const USHORT* pDepthImage )
{

    FLOAT scaleOfskeleton = XMVectorGetX( 
        XMVector3Length( skeleton.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER] - 
        skeleton.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER] ) );

    XMVECTOR upperArmL = skeleton.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT] - skeleton.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT] ;
    XMVECTOR upperArmR = skeleton.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT] - skeleton.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT] ;

    XMVECTOR torsoDir = skeleton.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER] - skeleton.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER] ;

    // compute limb length
    FLOAT lengthUAL = XMVectorGetX( XMVector3Length( upperArmL ) );
    FLOAT lengthUAR = XMVectorGetX( XMVector3Length( upperArmR  ) ) ;
    FLOAT lengthLAL = XMVectorGetX( XMVector3Length( skeleton.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT] -
        skeleton.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT] ));
    FLOAT lengthLAR = XMVectorGetX( XMVector3Length( skeleton.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT] -
        skeleton.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT] ));
    FLOAT lengthULL = XMVectorGetX( XMVector3Length( skeleton.SkeletonPositions[NUI_SKELETON_POSITION_HIP_LEFT] -
        skeleton.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT] ));
    FLOAT lengthULR = XMVectorGetX( XMVector3Length( skeleton.SkeletonPositions[NUI_SKELETON_POSITION_HIP_RIGHT] -
        skeleton.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT] ));
    FLOAT lengthLLL = XMVectorGetX( XMVector3Length( skeleton.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT] -
        skeleton.SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_LEFT] ));
    FLOAT lengthLLR = XMVectorGetX( XMVector3Length( skeleton.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT] -
        skeleton.SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_RIGHT] ));

    FLOAT asymetry = sqr( lengthUAL - lengthUAR ) + sqr( lengthLAL - lengthLAR ) + sqr( lengthULL - lengthULR ) + sqr( lengthLLL - lengthLLR ) ;
    FLOAT symetryScore = exp( -asymetry / scaleOfskeleton / scaleOfskeleton );

    XMVECTOR down = XMVectorSet(0,-1,0,0);
    XMVECTOR torsoDirNorm=  XMVector3Normalize( torsoDir );

    FLOAT downScore = XMVectorGetX( XMVector3Dot(down, torsoDirNorm  ) );       

    if ( downScore <  0 )
    {
        downScore = 0;
    }

    FLOAT probFallen = exp( -downScore*3 );

    return symetryScore * probFallen;
}


FLOAT CCrumpleScoringTerms::TestHeadShoulderShape( const NUI_SKELETON_DATA& skeleton, INT iDepthLabel,  const USHORT* pDepthImage )
{
    // compute box around the projected head

    // project the head and shoulders into the image plane
    XMVECTOR headPosition = Project( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_HEAD]  );
    XMVECTOR leftShoulder = Project( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_SHOULDER_LEFT]  );;
    XMVECTOR rightShoulder = Project( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_SHOULDER_RIGHT]  );;
    XMVECTOR centerShoulder = Project( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_SHOULDER_CENTER]  );;

    // find a bounding box around the projecte head and shoulder joints;
    INT left,right,top,bot;
    left  = (INT) min( XMVectorGetX(headPosition), min( XMVectorGetX(leftShoulder), min( XMVectorGetX(rightShoulder),  XMVectorGetX( centerShoulder))));
    right = (INT) max( XMVectorGetX(headPosition), max( XMVectorGetX(leftShoulder) , max( XMVectorGetX(rightShoulder),  XMVectorGetX( centerShoulder ))));
    top   = (INT) min( XMVectorGetY(headPosition), min( XMVectorGetY(leftShoulder), min( XMVectorGetY(rightShoulder),  XMVectorGetY( centerShoulder ))));
    bot   = (INT) max( XMVectorGetY(headPosition), max( XMVectorGetY(leftShoulder), max( XMVectorGetY(rightShoulder),  XMVectorGetY( centerShoulder ))));

    // dilate
    top -= CRUMPLE_SCORE_TERMS_HEAD_SHAPE_BOX_DILATION_SIZE;
    left -= CRUMPLE_SCORE_TERMS_HEAD_SHAPE_BOX_DILATION_SIZE/2;
    right += CRUMPLE_SCORE_TERMS_HEAD_SHAPE_BOX_DILATION_SIZE/2;

    // clip this bounding box against the viewing area;
    if ( left < 1 )    left = 1;
    if ( left > DEPTH_IMAGE_WIDTH - 2 )  left = DEPTH_IMAGE_WIDTH - 2;
    if ( right < 1 )   right = 1;
    if ( right > DEPTH_IMAGE_WIDTH - 1 ) right = DEPTH_IMAGE_WIDTH - 1;
    if ( top <  1  )   top = 1;
    if ( top > DEPTH_IMAGE_HEIGHT - 2 )   top = DEPTH_IMAGE_HEIGHT - 2;
    if ( bot <  1 )    bot =1;
    if ( bot > DEPTH_IMAGE_HEIGHT - 1 )   bot = DEPTH_IMAGE_HEIGHT - 1;

    // look at the depth image in this bounding box and look for depth edges 
    // on the foreground mask for this player;
    m_NumEdgePoints =0;

    INT minShoulderWidth = CRUMPLE_SCORE_TERMS_HEAD_SHAPE_BOX_DILATION_SIZE * 2 ;

    if ( right - left < minShoulderWidth )
    {
        INT mid = ( left + right ) / 2;
        right = mid + minShoulderWidth/2;
        left  = mid -minShoulderWidth /2;
    }

    for( INT i=top; i < bot ; i++)
    {
        for(INT j=left; j < right; j++)
        {
            if ( IsEdgePoint( pDepthImage + i*DEPTH_IMAGE_WIDTH + j,iDepthLabel,  DEPTH_IMAGE_WIDTH ) )
            {
                AddEdgePoint( j,i );          
                if ( m_NumEdgePoints == CRUMPLE_DETECTOR_MAX_EDGE_POINT) break;
            }
        }
        if ( m_NumEdgePoints == CRUMPLE_DETECTOR_MAX_EDGE_POINT) break;
    }

    // do these edge points form a straight line?
    // if these edges form a staright line, they dont form a good head shape.  a head shape 
    FLOAT score = 1 - exp( - C2DLineFitter().findLine(m_pEdgePoints, m_NumEdgePoints) / ( 10  * IMAGE_SCALE )  );
    return score;       
}


FLOAT CCrumpleScoringTerms::LimbEdgePresenceScore( const NUI_SKELETON_DATA& skeleton, 
                                                   INT iDepthLabel,  const USHORT* pDepthImage ,
                                                   BOOL bLeftOccluded, BOOL bRightOccluded)
{       
    FLOAT numEdgesR=0,numEdgesL=0;

    // find the number of edges around each arm
    if ( !bRightOccluded )
    {
        numEdgesR =     LimbEdgePresenceScore
            ( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_ELBOW_RIGHT],
            skeleton.SkeletonPositions [NUI_SKELETON_POSITION_WRIST_RIGHT], CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB,
            iDepthLabel, pDepthImage );
    }            

    if ( !bLeftOccluded )
    {
        numEdgesL =     LimbEdgePresenceScore
            ( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_ELBOW_LEFT],
            skeleton.SkeletonPositions [NUI_SKELETON_POSITION_WRIST_LEFT], CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB,
            iDepthLabel, pDepthImage );
    }

    FLOAT probLeft = 1;
    FLOAT probRight = 1;

    // score is based on the number of found edges.
    if ( !bLeftOccluded )
    {
        probLeft = numEdgesL / (FLOAT ) CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB;
    }
    else
    {
        probLeft = 1;
    }

    if ( !bRightOccluded )
    {
        probRight = numEdgesR / (FLOAT ) CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB;
    }
    else
    {
        probRight = 1;
    }

    return probRight * probLeft;
}


static 
FLOAT ArmPointingAlongLineOfSightScore( XMVECTOR wrist, XMVECTOR elbow )
{
    FLOAT angle = XMVectorGetX( XMVector3AngleBetweenVectors( wrist-elbow, -elbow ) );
    FLOAT score = Sigmoid( angle, CRUMPLE_SCORE_TERMS_POINTING_ANGLE1, 
                           CRUMPLE_SCORE_TERMS_POINTING_ANGLE2 );

    return score;
}

FLOAT HandInFrontOfFace( XMVECTOR head, XMVECTOR neck, XMVECTOR handL, XMVECTOR handR )
{
    XMVECTOR head2d = Project( head );
    XMVECTOR neck2d = Project( neck );
    XMVECTOR handL2d = Project( handL );
    XMVECTOR handR2d = Project( handR );

    
    XMVECTOR dir = XMVector2Normalize( head2d-neck2d );
    FLOAT minDist = fabs(  XMVectorGetX( XMVector2Cross( dir , handR2d-neck2d ) ) );
    minDist = min( minDist, fabs(XMVectorGetX( XMVector2Cross( dir , handL2d-neck2d ) ) ) );
    minDist = min( minDist, XMVectorGetX( XMVector2Length( head2d - handR2d ) ) );
    minDist = min( minDist, XMVectorGetX( XMVector2Length( head2d - handL2d ) ) );
    minDist = min( minDist, XMVectorGetX( XMVector2Length( neck2d - handR2d ) ) );
    minDist = min( minDist, XMVectorGetX( XMVector2Length( neck2d - handL2d ) ) );

    FLOAT normDist = minDist / XMVectorGetX(  XMVector2Length( neck2d - head2d ) );

    FLOAT score = Sigmoid( normDist, CRUMPLE_SCORE_TERMS_FACE_OCLUDE1, 
                                     CRUMPLE_SCORE_TERMS_FACE_OCLUDE2 );

    return score;   
    
}


FLOAT CCrumpleScoringTerms::LimbOccupancyScore( const NUI_SKELETON_DATA& skeleton, 
                                                INT iDepthLabel,  const USHORT* pDepthImage ,
                                                BOOL bLeftOccluded, BOOL bRightOccluded)
{
    INT numLabeledL=0;
    INT numLabeledC=0;
    INT numLabeledR=0;

    FLOAT errorR=0;
    FLOAT errorL=0;
    FLOAT errorC=0;


    // compute the quality of this term for each arm
    // limb occupancy is only reliable when the fore arm is not pointing directly at the line of sight.
    // As the arm points towards the line of sight of the camera, this term becomes disabled 
    // (i.e. it gets pulled to 1 )

    FLOAT armAlongLineOfSightL = ArmPointingAlongLineOfSightScore( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_WRIST_LEFT],
                                                                   skeleton.SkeletonPositions [NUI_SKELETON_POSITION_ELBOW_LEFT] );

    FLOAT armAlongLineOfSightR = ArmPointingAlongLineOfSightScore( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_WRIST_RIGHT],
                                                                   skeleton.SkeletonPositions [NUI_SKELETON_POSITION_ELBOW_RIGHT] );

    FLOAT faceOccludedScore = HandInFrontOfFace( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_HEAD],
                                            skeleton.SkeletonPositions [NUI_SKELETON_POSITION_SHOULDER_CENTER],
                                            skeleton.SkeletonPositions [NUI_SKELETON_POSITION_WRIST_LEFT],
                                            skeleton.SkeletonPositions [NUI_SKELETON_POSITION_WRIST_RIGHT] );

    
    // compute occupancy of left arm
    if (! bLeftOccluded )
    {
        LimbScore( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_ELBOW_LEFT],
            skeleton.SkeletonPositions [NUI_SKELETON_POSITION_WRIST_LEFT], CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB,
            iDepthLabel, pDepthImage, numLabeledL, errorL );

        LimbScore( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_ELBOW_LEFT],
            skeleton.SkeletonPositions [NUI_SKELETON_POSITION_SHOULDER_LEFT], CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB,
            iDepthLabel, pDepthImage,numLabeledL, errorL );
    }

    // compute occupancy of right arm
    if (! bRightOccluded )
    {
        LimbScore( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_ELBOW_RIGHT],
            skeleton.SkeletonPositions [NUI_SKELETON_POSITION_WRIST_RIGHT], CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB,
            iDepthLabel, pDepthImage, numLabeledR, errorR );

        LimbScore( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_ELBOW_RIGHT],
            skeleton.SkeletonPositions [NUI_SKELETON_POSITION_SHOULDER_RIGHT], CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB,
            iDepthLabel, pDepthImage,numLabeledR, errorR );
    }

    // compute occupancy of head and torso
    LimbScore( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_HEAD],
        skeleton.SkeletonPositions [NUI_SKELETON_POSITION_SHOULDER_CENTER], CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB,
        iDepthLabel, pDepthImage, numLabeledC, errorC );


    LimbScore( skeleton.SkeletonPositions [NUI_SKELETON_POSITION_HIP_CENTER],
        skeleton.SkeletonPositions [NUI_SKELETON_POSITION_SHOULDER_CENTER], CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB,
        iDepthLabel, pDepthImage, numLabeledC, errorC );

    FLOAT probL = 1, probR =1, probC=1;

    if ( !bLeftOccluded )
    { 
        probL = numLabeledL/ (FLOAT ) (CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB *2 );
    }

    if ( !bRightOccluded )
    {
        probR = numLabeledR/ (FLOAT ) (CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB *2 );
    }

    probC = numLabeledC / (FLOAT) (CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB *2 );

    FLOAT errorScore  = exp( -( errorR*errorR*2*armAlongLineOfSightR  + errorL*errorL*2*armAlongLineOfSightL  + errorC*errorC*faceOccludedScore ) );

    return errorScore ;
}


void CCrumpleScoringTerms::LimbScore( XMVECTOR ptA, XMVECTOR ptB, INT iNumSamples, INT iDepthLabel, const USHORT* pDepthImage,
                                      INT& iNumLabeledOut, FLOAT& fAverageErrorOut ) const
{
    assert( iNumSamples >1 );
    XMVECTOR delta = ( ptB-ptA ) * 1.0 / (FLOAT) ( iNumSamples -1 );

    INT numLabeled = 0;
    FLOAT error =0;

    for( INT i=0;i < iNumSamples; i++)
    {
        // project the point
        XMVECTOR pt2d = Project( ptA );

        INT x =(INT) XMVectorGetX( pt2d );
        INT y =(INT) XMVectorGetY( pt2d );

        if ( IsInsideImage(x,y) )
        {
            // find the depth point;
            USHORT pixel = pDepthImage[ y*DEPTH_IMAGE_WIDTH + x ];

            // look at label;
            if ( ( pixel & 0x7 ) == iDepthLabel)
            {
                numLabeled++;
                FLOAT depth = ( pixel >> 3 ) * 1e-3f;
                error += fabs( depth - XMVectorGetZ( ptA ) );
            }
        }
        ptA += delta;
    }

    error /=  numLabeled + .00000001f;

    fAverageErrorOut += error;
    iNumLabeledOut += numLabeled;
}


BOOL CCrumpleScoringTerms::IsEdgePresent( INT iDepthLabel, const USHORT* pDepthImage, 
                                          INT iXposition, INT iYposition, INT iXrange, INT iYrange )
{ 
    if ( iXrange == 0)
    {
        // seach for edge in column,

        int step = (iYrange) < 0 ? -1 : 1;
        iYrange  = abs(iYrange);

        FLOAT depthLast = GetDepth(iXposition,iYposition, iDepthLabel, pDepthImage);

        for(INT i=0; i < iYrange; i++)
        {
            AddEdgePoint( iXposition, iYposition );

            FLOAT depthCurr = GetDepth(iXposition,iYposition,iDepthLabel, pDepthImage);

            if ( depthCurr > depthLast ) 
            {
                // edge test consist of a large drop from the center point
                if ( fabs( depthCurr - depthLast  ) >  CRUMPLE_SCORE_TERMS_DEPTH_EDGE  )
                {
                    return TRUE;
                }
            }
            iYposition+=step;
        }
    }
    else
    {
        // seach for edge in row,
        int step = (iXrange) < 0 ? -1 : 1;
        iXrange  = abs(iXrange);

        FLOAT depthLast = GetDepth(iXposition,iYposition, iDepthLabel,pDepthImage);

        for(INT i=0; i < iXrange; i++)
        {
            AddEdgePoint( iXposition,iYposition);         
            FLOAT depthCurr = GetDepth(iXposition,iYposition,iDepthLabel, pDepthImage);

            if ( depthCurr > depthLast ) 
            {
                // edge test consist of a large drop from the center point
                if ( fabs( depthCurr - depthLast  ) >  CRUMPLE_SCORE_TERMS_DEPTH_EDGE  )
                {
                    return TRUE;
                }
            }
            iXposition+=step;
        }
    }

    return FALSE;   
}


BOOL CCrumpleScoringTerms::HasTwoEdgesPresent( INT iDepthLabel, const USHORT* pDepthImage, 
                                               INT iXposition, INT iYposition, INT iXrange, INT iYrange )
{
    BOOL depthEdgeFromBkgroundToFgd =TRUE;  

    if ( iXrange == 0)
    {
        // search in column
        int step = (iYrange) < 0 ? -1 : 1;
        iYrange  = abs(iYrange);

        FLOAT depthLast = GetDepth(iXposition,iYposition, iDepthLabel, pDepthImage);      

        for(INT i=0; i < iYrange; i++)
        {
            if ( !depthEdgeFromBkgroundToFgd )
            {
                AddEdgePoint( iXposition, iYposition);
            }

            FLOAT depthCurr = GetDepth(iXposition,iYposition,iDepthLabel, pDepthImage);

            // search for a transition from bkground to foreground
            if ( depthEdgeFromBkgroundToFgd )
            {
                if ( depthCurr < depthLast ) 
                {
                    if ( fabs( depthCurr - depthLast  ) >  CRUMPLE_SCORE_TERMS_DEPTH_EDGE_MAG_THRESH  )
                    {                     
                        depthEdgeFromBkgroundToFgd = FALSE;
                    }
                }
            }
            else if ( !depthEdgeFromBkgroundToFgd )
            {
                // search for transition from foreground back to background
                if ( depthCurr > depthLast ) 
                {
                    if ( fabs( depthCurr - depthLast  ) >  CRUMPLE_SCORE_TERMS_DEPTH_EDGE_MAG_THRESH  )
                    {
                        return TRUE;
                    }
                }
            }                        
            depthLast = depthCurr;
            iYposition+=step;
        }
    }
    else
    {
        // search in row
        int step = (iXrange) < 0 ? -1 : 1;
        iXrange  = abs(iXrange);

        FLOAT depthLast = GetDepth(iXposition,iYposition, iDepthLabel,pDepthImage);

        for(INT i=0; i < iXrange; i++)
        {               
            if ( !depthEdgeFromBkgroundToFgd )
            {
                AddEdgePoint( iXposition,iYposition);
            }

            FLOAT depthCurr = GetDepth(iXposition,iYposition,iDepthLabel, pDepthImage);

            // search for a transition from bkground to foreground
            if ( depthEdgeFromBkgroundToFgd )
            {
                if ( depthCurr < depthLast ) 
                {
                    if ( fabs( depthCurr - depthLast  ) >  CRUMPLE_SCORE_TERMS_DEPTH_EDGE_MAG_THRESH  )
                    {                     
                        depthEdgeFromBkgroundToFgd = FALSE;
                    }
                }
            }
            else if ( !depthEdgeFromBkgroundToFgd )
            {
                // search for transiion from foreground back to background
                if ( depthCurr > depthLast ) 
                {
                    if ( fabs( depthCurr - depthLast  ) >  CRUMPLE_SCORE_TERMS_DEPTH_EDGE_MAG_THRESH  )
                    {
                        return TRUE;
                    }
                }
            }                        
            depthLast = depthCurr;
            iXposition+=step;
        }
    }

    return FALSE;     
}


FLOAT CCrumpleScoringTerms::LimbEdgePresenceScore( XMVECTOR ptA, 
                                                   XMVECTOR ptB, INT iNumSamples, 
                                                   INT iDepthLabel, const USHORT* pDepthImage, BOOL bBoth ) 
{
    assert( iNumSamples >1 );
    XMVECTOR delta = ( ptB-ptA ) * 1.0 / (FLOAT) ( iNumSamples -1 );

    // determ a left vector;
    XMVECTOR right= XMVectorSet( XMVectorGetY(delta), -XMVectorGetX(delta), 0,0  );
    right = XMVector3Normalize( right );

    // displacement in X dir to get outline;
    // compute thee contrast scores;
    INT numEdgesA = 0;
    INT numEdgesB =0;


    // accumuluate scores from samples along segment
    for( INT i=0;i < iNumSamples; i++)
    {
        // project the point
        XMVECTOR ptEdgeA = ptA + right * CRUMPLE_SCORE_TERMS_MAX_LIMB_WIDTH; 
        XMVECTOR ptEdgeB = ptA - right * CRUMPLE_SCORE_TERMS_MAX_LIMB_WIDTH; 

        XMVECTOR ptLimb2D = Project( ptA );
        XMVECTOR ptEdgeA2D = Project( ptEdgeA );
        XMVECTOR ptEdgeB2D = Project( ptEdgeB );

        // the center point
        INT xC =(INT) XMVectorGetX( ptLimb2D );
        INT yC =(INT) XMVectorGetY( ptLimb2D );

        // the search direction on one side of the arm
        INT xEA =(INT) XMVectorGetX( ptEdgeA2D );
        INT yEA =(INT) XMVectorGetY( ptEdgeA2D );

        // the search direction on the other side
        INT xEB =(INT) XMVectorGetX( ptEdgeB2D );
        INT yEB =(INT) XMVectorGetY( ptEdgeB2D );

        INT dx1 = xC - xEA, dy1 = yC - yEA;
        INT dx2 = xC - xEB, dy2 = yC - yEB;

        if ( xC < 0 || yC < 0 || xC >= DEPTH_IMAGE_WIDTH  || yC >= DEPTH_IMAGE_HEIGHT )
        {
            continue;
        }


        // snap each search range to be axis aligned.
        if ( abs( dx1 ) < abs( dy1 ) )
        {
            dx1 = 0;           
            // keep search range on image
            if ( dy1 + yC > DEPTH_IMAGE_HEIGHT ) dy1 = DEPTH_IMAGE_HEIGHT - yC;
            if ( dy1 + yC < 0 ) dy1 = -yC;
        }
        else
        {
            dy1 =0;
            // keep search range on image
            if ( dx1 + xC > DEPTH_IMAGE_WIDTH ) dx1 = DEPTH_IMAGE_WIDTH-xC;
            if ( dx1 + xC < 0 ) dx1 = -xC;

        }

        // snap to farther
        if ( abs( dx2 ) < abs( dy2 ) )
        {
            dx2 = 0;
            // keep search range on image
            if ( dy2 + yC > DEPTH_IMAGE_HEIGHT ) dy2 = DEPTH_IMAGE_HEIGHT - yC;
            if ( dy2 + yC < 0 ) dy2 = -yC;
        }
        else
        {
            dy2 =0;
            // keep search range on image
            if ( dx2 + xC > DEPTH_IMAGE_WIDTH ) dx2 = DEPTH_IMAGE_WIDTH-xC;
            if ( dx2 + xC < 0 ) dx2 = -xC ;
        }

        //
        // the center should be on the foreground shape.  if it is 
        // we can look for the presence of the edge on either side of bone
        //
        if (   (pDepthImage[ yC*DEPTH_IMAGE_WIDTH + xC ] & 0x7 ) == iDepthLabel )
        {
            //if ( IsInsideImage(xC+dx1, yC+dy1  ) )
            {
                AddEdgePoint( xC + dx1, yC + dy1 );
                if ( IsEdgePresent(iDepthLabel, pDepthImage, xC,yC, dx1,dy1 ) )
                {
                    numEdgesA++;
                }
            }

            //if ( IsInsideImage(xC+dx2, yC+dy2  ) )
            {
                AddEdgePoint( xC + dx2, yC + dy2 );

                if ( IsEdgePresent(iDepthLabel, pDepthImage, xC,yC, dx2,dy2 ) )
                {
                    numEdgesB++;
                }
            }
        }
        else
        {
            // if the point on the bone does NOT overlap foreground it could be due to some lag
            // in the bone tracker.. in which case we need to look for 2 depth edges on the same side of the bone.
            if ( HasTwoEdgesPresent(iDepthLabel, pDepthImage, xC,yC, dx1,dy1 ) )
            {
                numEdgesB++;
                numEdgesA++;
            }
            else if ( HasTwoEdgesPresent(iDepthLabel, pDepthImage, xC,yC, dx2,dy2 ) )
            {
                numEdgesB++;
                numEdgesA++;
            }
        }
        ptA += delta;
    }
    return (FLOAT) max(numEdgesA, numEdgesB);
}


FLOAT CCrumpleScoringTerms::BorderScore(  const NUI_SKELETON_DATA& skeleton  )
{
    // project each joint and accumulate the distances to the perimeter;       
    double dist2 =0;
    for(INT i=0;i < NUI_SKELETON_POSITION_COUNT; i++)
    {
        XMVECTOR pt2d = Project( skeleton.SkeletonPositions [i]  );
        FLOAT x  = XMVectorGetX(pt2d);
        FLOAT y  = XMVectorGetY(pt2d);

        // find the edge its closest to;
        FLOAT distClosest = FLT_MAX;
        distClosest = min (  distClosest, abs(x) );
        distClosest = min (  distClosest, abs(y) );
        distClosest = min (  distClosest, abs(DEPTH_IMAGE_WIDTH-x) );
        distClosest = min (  distClosest, abs(DEPTH_IMAGE_HEIGHT-y) );

        dist2 += distClosest*distClosest;            
    }

    return (FLOAT) (  1 - exp(-dist2/ CRUMPLE_DETECTOR_MARGIN_IN_PIXELS /  CRUMPLE_DETECTOR_MARGIN_IN_PIXELS ) );             
}


void CCrumpleDetector::EvaluteSkeleton( const NUI_SKELETON_FRAME& skel, USHORT* pDepthBuffer )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );


    m_BlobOverlap.SetDepth( pDepthBuffer, CRUMPLE_DETECTOR_BLOB_OVERLAP_PERIMETER_SIZE, skel.vFloorClipPlane );   

#ifdef USECHANGE
    m_DepthChangeMeasure.ComputeChange( skel, pDepthBuffer  );
#endif

    for(INT i=0 ;i < NUI_SKELETON_COUNT; i++)
    {
        if ( skel.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED  )
        {

            //is this a new blob?
            BOOL first =   !m_ClippedFromTop.BlobIsTracked(i);

            // compute occlusion scores horizontaly and vertically
            FLOAT oclScore =  m_BlobOverlap.ProbBoundaryOccluded( i );
            FLOAT oclScoreVert =  m_BlobOverlap.ProbBoundaryOccludedVertical( i );

            oclScore = max( oclScore, oclScoreVert );
            m_fOcclScore[i] = 1.0f - min(1.0f, oclScore/ CRUMPLE_DETECTOR_OCCLUSION_SCORE_RESCALE );

            // head cliped score
            m_fHeadClip[i] =   m_ClippedFromTop.VerticalClip
                ( i, 
                  skel.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD],
                  skel.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER],
                  pDepthBuffer);                


#ifdef USETWITCH
            m_fDepthTwitchScore[i] = m_DepthChangeMeasure.likelihoodOfTwitch(i);
#endif
            BOOL farOrLow = m_ClippedFromTop.FarAndAway( i, pDepthBuffer ) > 
                CRUMPLE_DETECTOR_OCCLUSION_LOW_AND_FAR_THRESH;

            if ( farOrLow )
            {
                m_fHeadClip[i]  = 1.0f;
            }

#if CRUMPLE_VERBOSE_DEBUG
            printf("farOrLow == %d, m_fHeadClip[i]==%lf\n", farOrLow, m_fHeadClip[i] );
#endif

            // 
            m_fCrumpleScore[i] =     m_CrumpleDetector[i].CrumpleScore
                (  m_fHeadClip[i] > CRUMPLE_DETECTOR_OCCLUSION_HEAD_CLIP_THRESH  || farOrLow,    // use head detector when head is not clipped 
                // or when skel is low or far
                first,
                m_DepthChangeMeasure.probBlobChanging(i) > .9,  
                skel.SkeletonData[i],i+1, pDepthBuffer  );

            FLOAT compactNess = m_BlobOverlap.Compactness( i );
            FLOAT compactNessScore = 1 - Sigmoid( compactNess , CRUMPLE_DETECTOR_COMPACTNESS_SIGMOID_LOW, CRUMPLE_DETECTOR_COMPACTNESS_SIGMOID_HIGH );

#ifdef CRUMPLE_VERBOSE_DEBUG
            printf("compact == %f \n ", compactNess );
#endif

            // use compactness of blob as a crumple measure as well.
            m_fCrumpleScore[i] *= compactNessScore;
                

            m_fOveralScore[i] = m_fCrumpleScore[i]  *  m_fOcclScore[i]  * 
                Sigmoid( m_fHeadClip[i], 
                CRUMPLE_DETECTOR_HEAD_CLIP_SIGMOID_LOW, CRUMPLE_DETECTOR_HEAD_CLIP_SIGMOID_HIGH )  
#ifdef USETWICH
                *  m_fDepthTwitchScore[i]
#endif
            ;


#if 0
            if ( m_fOveralScore[i] < .7 )
            {
                printf("crumplie!!!");
            }
#endif




        }
        else
        {
            m_fOveralScore[i] = -1;
            m_ClippedFromTop.LostTrack(i);

        }
    }

    PIXEndNamedEvent();
}


void CCrumpleDetector::DrawScores( ATG::Font& font, FLOAT fXpos, FLOAT fYpos, BOOL bDrawDetails )

{
    DWORD colors[8];
    colors[0] = D3DCOLOR_RGBA(255,0   ,0   ,255);
    colors[1] = D3DCOLOR_RGBA(0  ,255 ,0   ,255);
    colors[2] = D3DCOLOR_RGBA(0  ,0   ,255 ,255);

    colors[3] = D3DCOLOR_RGBA(0  ,255   ,255 ,255);
    colors[4] = D3DCOLOR_RGBA(255  ,0 ,255   ,255);
    colors[5] = D3DCOLOR_RGBA(255,255   ,0   ,255);
    colors[6] = D3DCOLOR_RGBA(128  ,128   ,128 ,255);


    WCHAR msg[80];        
    FLOAT currYDetails = fYpos;

    for(INT i=0 ;i < NUI_SKELETON_COUNT; i++)
    {
        if (m_fOveralScore[i] >= 0  )
        {                
            swprintf_s( msg,L"skel %ld, overall-score = %f", i, m_fOveralScore[i] );
            font.DrawText( 100,fYpos,  colors[i] ,    msg );
            fYpos+=20;

#ifdef USETWICH
            swprintf_s( msg,L"skel %ld, probTwich = %f,  jointDiff = %f, depthDiff = %f", i, m_fDepthTwitchScore[i],
                m_DepthChangeMeasure.m_fJointDiff[i],
                m_DepthChangeMeasure.m_fNumPixelsChangedWeighted[i]                
            );
            font.DrawText( 100,fYpos,  colors[i] ,    msg );
            fYpos+=20;
#endif

            swprintf_s( msg,L"skel %ld, occlusion-score = %f", i,  m_fOcclScore[i]  );
            font.DrawText( 100,fYpos,  colors[i] ,    msg );
            fYpos+=20;

            swprintf_s( msg,L"skel %ld, head-clip-score = %f", i,  m_fHeadClip[i]  );
            font.DrawText( 100,fYpos,  colors[i] ,    msg );
            fYpos+=20;

            swprintf_s( msg,L"skel %ld, other-terms = %f", i, m_fCrumpleScore[i] );
            font.DrawText( 100,fYpos,  colors[i] ,    msg );
            fYpos+=20;


            if ( bDrawDetails)
            {
                m_CrumpleDetector[i].Trace( font , fXpos + 650, currYDetails );
                currYDetails += 20;
                fYpos = max(fYpos, currYDetails ); // line up the text to the end of this row
            }
        }
    }
}


void CCrumpleDetector::RenderPoints ( D3DDevice* pd3dDevice, const USHORT* pDepthBuffer,  INT* pPoints, INT iNumPoints, INT iStep )
{
    COLORED_VERTEX colorVertexBuffer[ colorVertexBufferSize ];
    INT numVertsInColorBuffer=0;

    for( INT i=0; i < iNumPoints; i++)
    {
        INT x =pPoints[i*iStep];
        INT y = pPoints[i*iStep+1];

        INT occl = 0;

        if ( iStep == 3 )
        {
            occl = pPoints[i*iStep+2 ];
        }

        if ( x < 0 || x >= 320 || y < 0 || y >= 240 ) continue;

        USHORT depthpixel = pDepthBuffer[y * DEPTH_IMAGE_WIDTH  + x];
        if(  ! ISZERO( depthpixel ) )
        {
            COLORED_VERTEX vert;
            XMVECTOR point3d;

            FLOAT depthVal = GETDEPTH( depthpixel );
            point3d =  Unproject( x, y, depthVal );

            vert.position.x =  XMVectorGetX( point3d );
            vert.position.y =  XMVectorGetY( point3d );
            vert.position.z =  XMVectorGetZ( point3d );
            vert.position.w =  1;

            if ( iStep == 2 )
            {
                vert.color = XMVectorSet(0,0,1,1);;
            }
            else
            {             
                if ( occl )
                {
                    vert.color = XMVectorSet(0,1,.5,1);;
                }
                else
                {
                    vert.color = XMVectorSet(.5,0,.5,1);;
                }
            }
            colorVertexBuffer[numVertsInColorBuffer++] = vert;

            if ( numVertsInColorBuffer == colorVertexBufferSize )
            {
                pd3dDevice->DrawPrimitiveUP(D3DPT_POINTLIST, numVertsInColorBuffer, colorVertexBuffer, sizeof(COLORED_VERTEX));
                numVertsInColorBuffer = 0;
            }
        }
    }

    if ( numVertsInColorBuffer > 0 )
    {
        pd3dDevice->DrawPrimitiveUP(D3DPT_POINTLIST, numVertsInColorBuffer, colorVertexBuffer, sizeof(COLORED_VERTEX));
        numVertsInColorBuffer = 0;
    }
}
