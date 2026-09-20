//--------------------------------------------------------------------
// CrumpleDetection.h
//
// This class creates a detector that measures the likelihood of a skeleton being 
// crumpled.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <nuiapi.h>
#include <xnamath.h>
#include <math.h>

// for additional debug printing
//#define CRUMPLE_VERBOSE_DEBUG

// Image Dimmensions 
const int  DEPTH_IMAGE_FULL_WIDTH = 320;
const int  DEPTH_IMAGE_FULL_HEIGHT = 240;
const int  DEPTH_IMAGE_WIDTH = DEPTH_IMAGE_FULL_WIDTH/4;
const int  DEPTH_IMAGE_HEIGHT = DEPTH_IMAGE_FULL_HEIGHT/4;
const FLOAT IMAGE_SCALE = .25f;
const FLOAT IMAGE_SCALE_INV = 4.0f;


//Depth camera field of view
const FLOAT DEPTH_SCALE_FACTOR		= NUI_CAMERA_DEPTH_NOMINAL_FOCAL_LENGTH_IN_PIXELS / (FLOAT) DEPTH_IMAGE_FULL_WIDTH;

//Depth camera resolution
const int DEPTH_CAMERA_RESOLUTION_X = DEPTH_IMAGE_WIDTH;
const int DEPTH_CAMERA_RESOLUTION_Y = DEPTH_IMAGE_HEIGHT;

//Derived values for convenience
const FLOAT DEPTH_CAMERA_CENTER_X = DEPTH_CAMERA_RESOLUTION_X * 0.5f;
const FLOAT DEPTH_CAMERA_CENTER_Y = DEPTH_CAMERA_RESOLUTION_Y * 0.5f;
const FLOAT DEPTH_SCALE_FACTOR_SCREEN	= DEPTH_SCALE_FACTOR * DEPTH_CAMERA_RESOLUTION_X;
const FLOAT DEPTH_SCALE_FACTOR_SCREEN_INV = 1.0f / DEPTH_SCALE_FACTOR_SCREEN;

const XMVECTOR g_vDepthCameraCenter = XMVectorSet( DEPTH_CAMERA_CENTER_X, DEPTH_CAMERA_CENTER_Y, 0.0f, 0.0f );
const XMVECTOR g_vDepthCameraFOVScreen = XMVectorSet( DEPTH_SCALE_FACTOR_SCREEN, -DEPTH_SCALE_FACTOR_SCREEN, 1.0f, 1.0f );

const FLOAT MAX_DEPTH = ((2<<13)-1)   *  1e-3;;

const FLOAT DEPTH_CAMERA_FOV		= 286.62960f / (FLOAT) 320;
const FLOAT DEPTH_CAMERA_FOV_SCREEN	= DEPTH_CAMERA_FOV * DEPTH_CAMERA_RESOLUTION_X;

const FLOAT DELTA = 1e-8;

//--------------------------------------------------------------------------------------
// Name: Project
// Desc: Project point into depth camera projection plane:
//       p2d = FOVScreen * ( Px/Pz, Py/Pz ) + cameraCenter
//--------------------------------------------------------------------------------------
inline XMVECTOR Project( XMVECTOR p )
{
    XMVECTOR vProject   = XMVectorSplatZ( p );            
    vProject            = XMVectorReciprocal( vProject ); 
    XMVECTOR vResult    = XMVectorMultiplyAdd( p, g_vDepthCameraFOVScreen * vProject, g_vDepthCameraCenter );
    return vResult;
}


//--------------------------------------------------------------------------------------
// Name: Unproject
// Desc: Calculate 3D vPosition from vPosition in depth camera projection plane and depth
//--------------------------------------------------------------------------------------
template< class T1, class T2>
inline XMVECTOR Unproject( T1 x, T2 y, FLOAT depth )
{    
    FLOAT scale = depth * DEPTH_SCALE_FACTOR_SCREEN_INV;
    return XMVectorSet( (x - DEPTH_CAMERA_CENTER_X )*scale, ( DEPTH_CAMERA_CENTER_Y - y )*scale, depth, 1 ); 
}


#define PLAYERINDEX(iDepth) ( ( iDepth ) & 0x7 )
#define GETDEPTH(iDepth) ( ( (iDepth ) >> 3  ) * .001f )


//--------------------------------------------------------------------
// Name: C2DLineFitter
// Desc: computes how well a line can be fit to a set of points
//--------------------------------------------------------------------
class C2DLineFitter
{
public:

    //--------------------------------------------------------------------
    // Name: findLine
    // Desc: Computes how well a line can be fit to a set of point.
    //       points2D hold numPoints in an array of point coordiantes (x0,y0, x1,y1,...)
    //       The return value is the RMS distance of the points to the fitted line
    //--------------------------------------------------------------------
    FLOAT findLine( INT *pPoints2D , INT iNumPoints );
  
private:

    //--------------------------------------------------------------------
    // Name: eigvalues
    // Desc: computes the eigen-values of the matrix
    //       [ a b,
    //         c d].
    //       This assumes the matrix has real eigen-values
    //       which is the case for covarrience matricies.
    //--------------------------------------------------------------------
    void eigvalues( FLOAT fA, FLOAT fB, FLOAT fC, FLOAT fD,  FLOAT& fE1, FLOAT& fE2 )
    {
        FLOAT det = 4*fB*fC  + (fA-fD)*(fA-fD);
        assert( det >=  0 );
        det = sqrt(det)*.5f;
        FLOAT v1 = (fA+fD)*.5f;

        fE1 = v1 + det;
        fE2 = v1 - det;
    }   
};


// Crumple detection parameters.

// size of debug graphic buffers
const INT CRUMPLE_DETECTOR_MAX_EDGE_POINT      = (DEPTH_IMAGE_WIDTH * DEPTH_IMAGE_HEIGHT);
const INT CRUMPLE_DETECTOR_MAX_OCCLUDING_POINT = (DEPTH_IMAGE_WIDTH * DEPTH_IMAGE_HEIGHT);

const FLOAT CRUMPLE_DETECTOR_MARGIN_IN_PIXELS  =  100 * ( IMAGE_SCALE ) ;
                                    // size of the margin along the perimter of the field 
                                    // of view in which a skeloton is likely 
                                    // to crumple.

const INT CRUMPLE_DETECTOR_NUM_SAMPLES_PER_LIMB = 10;    
                                    // number of samples along the forearms
                                    // used in validation

const INT CHANGE_MEASURE_BUFFER_SIZE = 5 ;  // number of frames to keep track of, in change estimation


//--------------------------------------------------------------------
// Name: Sigmoid
// Desc: a smooth ramp-like function that ranges from 0 to 1.  
//       lowerCutoff and upperCutoff specifiy the width of this transation.
//       Sigmoid returns a value that is close to zero at lowerCutoff (~.02 )
//       Sigmoid returns close to 1 at upperCutOff ( ~.98 )
//--------------------------------------------------------------------
FLOAT Sigmoid( FLOAT fX, FLOAT fLowerCutoff, FLOAT fUpperCutoff );


// Depth/Skeleton Change constants
const FLOAT DEPTH_SKEL_CHANGE_BLOB_CHANGE_SIGMOID_LOWER = .2f;
const FLOAT DEPTH_SKEL_CHANGE_BLOB_CHANGE_SIGMOID_UPPER = .4f;

const FLOAT DEPTH_SKEL_CHANGE_SKEL_CHANGE_SIGMOID_LOWER = .1f;
const FLOAT DEPTH_SKEL_CHANGE_SKEL_CHANGE_SIGMOID_UPPER = .3f;
const FLOAT DEPTH_SKEL_CHANGE_SKEL_DEPTH_CHANGE_THRESHOLD = 0.005f;

//--------------------------------------------------------------------
// Name: CDepthSkelChangeScore
// Desc: determines the likelihood of regions in the depth mask
//--------------------------------------------------------------------
class  CDepthSkelChangeScore
{
public:

    CDepthSkelChangeScore()
    {
        m_CurrentBuffer = 0;
        for(INT i =0 ; i < CHANGE_MEASURE_BUFFER_SIZE; i++)
        {
            m_DepthImageLast[i] = new USHORT[DEPTH_IMAGE_WIDTH*DEPTH_IMAGE_HEIGHT];
            memset( m_DepthImageLast[i], 0, DEPTH_IMAGE_WIDTH*DEPTH_IMAGE_HEIGHT *sizeof(*m_DepthImageLast[i]) );
        }    
    }
    
    ~CDepthSkelChangeScore()
    {
        for(INT i =0 ; i < CHANGE_MEASURE_BUFFER_SIZE; i++)
        {
            delete[] m_DepthImageLast[i];
        }
    }

    //--------------------------------------------------------------------
    // Name: ComputeChange
    // Desc: computes the amount of change for the given skeleton and its
    //       corresponding depth pixels.
    //--------------------------------------------------------------------
    void ComputeChange( const NUI_SKELETON_FRAME& skeleton, const USHORT* pDepthImageCurr );


    //--------------------------------------------------------------------
    // Name: likelihoodOfTwitch
    // Desc: likilihood of the depth mask not-changing, while the skeleton is.
    //--------------------------------------------------------------------
    FLOAT likelihoodOfTwitch(INT iSkeletonIndex) const
    {
        return 1 - ( ( 1 - m_fProbBlobChanging[iSkeletonIndex] ) * m_fProbSkeletonChanging[iSkeletonIndex] );
    }


    //--------------------------------------------------------------------
    // Name: probBlobChanging
    // Desc: likelihood of depth pixels changing
    //--------------------------------------------------------------------
    FLOAT probBlobChanging(INT iSkeletonIndex) const
    {
        return m_fProbBlobChanging[iSkeletonIndex];
    }

private:

    CDepthSkelChangeScore( const CDepthSkelChangeScore& rhs  );
    const CDepthSkelChangeScore& operator=( const CDepthSkelChangeScore& rhs  );

    
    // history of depth buffers and skeletons.
    INT m_CurrentBuffer;
    USHORT* m_DepthImageLast[CHANGE_MEASURE_BUFFER_SIZE];
    NUI_SKELETON_FRAME  m_SkeletonHistory[CHANGE_MEASURE_BUFFER_SIZE];

    FLOAT m_fNumPixelsChangedWeighted[NUI_SKELETON_COUNT];  // number of depth pixels that have changed
    FLOAT m_fJointDiff[NUI_SKELETON_COUNT];                 // difference between joints
    FLOAT m_fProbBlobChanging[NUI_SKELETON_COUNT];          // likelihood of depth pixels changing
    FLOAT m_fProbSkeletonChanging[NUI_SKELETON_COUNT];      // likilihood of skeleton changing

};


 const FLOAT FLOOR_CLIP_HEIGHT_THRESH = .05f;  // height above floor which depth points are considered occluding


#define ASSERT_IN_DEPTHIMAGE(index) assert( index >=0 && index <= DEPTH_IMAGE_HEIGHT*DEPTH_IMAGE_WIDTH)

//--------------------------------------------------------------------
// Name: CBlobOverlapDetector
// Desc: keeps track of blobs overlapping each other, or being occluded by background
//--------------------------------------------------------------------
class CBlobOverlapDetector
{
public:
   
    CBlobOverlapDetector() :  m_NumPerimeterPointsDisplay(0)
    {
        m_PerimeterPoints = new INT[CRUMPLE_DETECTOR_MAX_OCCLUDING_POINT*3];
        assert(m_PerimeterPoints != NULL);
    }
    CBlobOverlapDetector( const CBlobOverlapDetector& rhs ) :  m_NumPerimeterPointsDisplay(0)
    {
        m_PerimeterPoints = new INT[CRUMPLE_DETECTOR_MAX_OCCLUDING_POINT*3];
        assert(m_PerimeterPoints);
    }

    CBlobOverlapDetector& operator=( const CBlobOverlapDetector& rhs ) 
    {
        if ( &rhs != this )
        {
            memcpy( m_PerimeterPoints, rhs.m_PerimeterPoints, sizeof(*m_PerimeterPoints) * rhs.m_NumPerimeterPointsDisplay*3 );  
        }
    }

    ~CBlobOverlapDetector() 
    {
        SAFE_DELETE_ARRAY(m_PerimeterPoints);        
    }

    //--------------------------------------------------------------------
    // Name: ProbBoundaryOccluded
    // Desc: the likelihood of a blob being occluded as measured,
    //       by looking along the rows of the depth image.
    //--------------------------------------------------------------------  
    FLOAT ProbBoundaryOccluded( INT iSkeletonIndex ) const
    {
        return m_NumOccludingPoints[iSkeletonIndex] / (FLOAT) (   m_NumOccludingPoints[iSkeletonIndex] + m_NumPixels[iSkeletonIndex] + 1 );
    }

    //--------------------------------------------------------------------
    // Name: ProbBoundaryOccludedVertical
    // Desc: the likelihood of a blob being occluded as measured by looking down
    //       the columns of the depth image.
    //--------------------------------------------------------------------  
    FLOAT ProbBoundaryOccludedVertical( INT iSkeletonIndex ) const
    {     
        return m_NumOccludingPointsVertical[iSkeletonIndex] / (FLOAT) (   m_NumOccludingPointsVertical[iSkeletonIndex] + m_NumPixelsVertical[iSkeletonIndex] + 1 );
    }

    //--------------------------------------------------------------------
    // Name: SetDepth
    // Desc: computes blob occlusions
    //--------------------------------------------------------------------
    void SetDepth( const USHORT* pDepthImage, INT iPerimeterWidth,  XMVECTOR floor ) 
    {
        m_NumPerimeterPointsDisplay = 0;
        BlobOcclusionScores( floor, pDepthImage, iPerimeterWidth );
        BlobVerticalOcclusionScores( floor, pDepthImage, iPerimeterWidth );
    }


    //--------------------------------------------------------------------
    // Name: Compactness
    // Desc: returns the compactness of a blob.  Compactness is the 
    //       ratio of the number of perimeter pixels of the blob  squared to the
    //       number of total pixels  (i.e. area ).
    //--------------------------------------------------------------------
    FLOAT Compactness( INT iSkeletonIndex ) const 
    {
        return  m_NumEdgePoints[iSkeletonIndex] * m_NumEdgePoints[iSkeletonIndex] / ( (FLOAT) m_NumPixels[iSkeletonIndex] + 1);   
    }

    //--------------------------------------------------------------------
    // Name: NumPixels
    // Desc: returns the number of pixels in specified skeleton blob
    //--------------------------------------------------------------------
    INT NumPixels( INT iSkeletonIndex ) const 
    {
        return m_NumPixels[iSkeletonIndex];   
    }

private:

    //--------------------------------------------------------------------
    // Name: BlobVerticalOcclusionScores
    // Desc: finds depth points that might be occluded blobs
    //       by looking down the columns
    //--------------------------------------------------------------------
    void BlobVerticalOcclusionScores( XMVECTOR floor,
                                      const USHORT* pDepthImage, INT iPerimeterWidth );


    //--------------------------------------------------------------------
    // Name: BlobOcclusionScores
    // Desc: finds potentially occluding points by looking across rows
    //--------------------------------------------------------------------
    void BlobOcclusionScores( XMVECTOR floor, const USHORT* pDepthImage, INT iPerimeterWidth );


private:

    //--------------------------------------------------------------------
    // Name: AddBlobPerimterPoint
    // Desc: add a perimeter to the buffer if there is space
    //--------------------------------------------------------------------
    void AddBlobPerimeterPoint( INT iX, INT iY, INT iIsOccluded )
    {
        if ( m_NumPerimeterPointsDisplay < CRUMPLE_DETECTOR_MAX_EDGE_POINT )
        {
            m_PerimeterPoints[m_NumPerimeterPointsDisplay*3]   = iX;
            m_PerimeterPoints[m_NumPerimeterPointsDisplay*3+1] = iY;                      
            m_PerimeterPoints[m_NumPerimeterPointsDisplay*3+2]  = iIsOccluded;
            m_NumPerimeterPointsDisplay++;
        }
    }
   
    INT m_NumEdgePoints[NUI_SKELETON_COUNT];
    INT m_NumPerimeterPoints[NUI_SKELETON_COUNT];
    INT m_NumOccludingPoints[NUI_SKELETON_COUNT];
    INT m_NumPixels[NUI_SKELETON_COUNT];

    INT m_NumPerimeterPointsVertical[NUI_SKELETON_COUNT];
    INT m_NumOccludingPointsVertical[NUI_SKELETON_COUNT];
    INT m_NumPixelsVertical[NUI_SKELETON_COUNT];

public:
    INT *m_PerimeterPoints;
    INT  m_NumPerimeterPointsDisplay ;


    //--------------------------------------------------------------------
    // Name: RenderOcclusionScores
    // Desc: a visualization of the label foreground blobs and their computed
    //       boundaries
    //--------------------------------------------------------------------
    void RenderOcclusionScores(  const USHORT* pDepthImage, UINT8* pColorDebugBuffer );
};


const FLOAT CLIPPED_FROM_TOP_DETECTOR_NEAR_PLANE = 2;
const FLOAT CLIPPED_FROM_TOP_DETECTOR_FAR_PLANE = 3;

const FLOAT CLIPPED_FROM_TOP_DETECTOR_LOW_IN_DEPTH_IMAGE =  DEPTH_IMAGE_HEIGHT / 20.0f;


//--------------------------------------------------------------------
// Name: CClipedFromTopDetector
// Desc: Determines when a skeleton is clipped from the top plane of the viewing
//       frustum.
//--------------------------------------------------------------------
class CClipedFromTopDetector
{
public:
    CClipedFromTopDetector()
    {
        for( INT i=0; i < NUI_SKELETON_COUNT  ;i++)
        {
            m_Recapture[i] = TRUE;
        }
    }

    //--------------------------------------------------------------------
    // Name: BlobIsTracked
    // Desc: keeps track of blobs overlapping each other, or being occluded by background
    //--------------------------------------------------------------------
    BOOL BlobIsTracked( INT iSkeletonIndex )
    {
        return m_Recapture[iSkeletonIndex] == FALSE;
    }

    //--------------------------------------------------------------------
    // Name: LostTrack
    // Desc: set when skeleton is lost, so this metric can recomputed 
    //       initial statistics
    //--------------------------------------------------------------------
    void LostTrack( INT iSkeletonIndex )
    {
        m_Recapture[iSkeletonIndex] = TRUE;
    }

    //--------------------------------------------------------------------
    // Name: VerticalClip
    // Desc: returns the likelihood of a blob being clipped by the top 
    //       plane of the frustum. 
    //--------------------------------------------------------------------  
    FLOAT VerticalClip( INT iSkeletonIndex ,  XMVECTOR head, XMVECTOR waist , const USHORT* pDepthImage);   

    //--------------------------------------------------------------------
    // Name: ExpectedPixelSpan
    // Desc: the expected height of the foreground blob
    //--------------------------------------------------------------------
    FLOAT ExpectedPixelSpan( INT iSkeletonIndex ) const
    {
      return m_fPredictedPixelSpan[iSkeletonIndex];
    }


    //--------------------------------------------------------------------
    // Name: FarAndAway
    // Desc: the likelihood that a blob is far enough from the camera
    //       or low enough in the view frustum to be clipped by
    //       the top plane of the view frustum.
    //--------------------------------------------------------------------  
    FLOAT FarAndAway( INT iSkeletonIndex , const USHORT* pDepthImage) const
    {
        INT idx = m_BlobSpanXPos[iSkeletonIndex] + m_BlobTopLastFrame[iSkeletonIndex] * DEPTH_IMAGE_WIDTH;
        XMVECTOR headPointIn3D = 
            Unproject( m_BlobSpanXPos[iSkeletonIndex] , 
                       m_BlobTopLastFrame[iSkeletonIndex], 
                       ( pDepthImage[idx] >> 3 ) *.001f );

        FLOAT probFar = Sigmoid( XMVectorGetZ( headPointIn3D), 
            CLIPPED_FROM_TOP_DETECTOR_NEAR_PLANE, 
            CLIPPED_FROM_TOP_DETECTOR_FAR_PLANE );

        FLOAT probLow = Sigmoid ( (FLOAT) m_BlobTopLastFrame[iSkeletonIndex],  0, CLIPPED_FROM_TOP_DETECTOR_LOW_IN_DEPTH_IMAGE );
#ifdef CRUMPLE_VERBOSE_DEBUG
        printf("pf:  %f = s(%f) , pl : %f = s(%d )", probFar, XMVectorGetZ( headPointIn3D), probLow,  m_BlobTopLastFrame[iSkeletonIndex] );
#endif
        return probFar +  probLow;
    }

private:
     //--------------------------------------------------------------------
    // Name: SetBlobVerticalExtents
    // Desc: compute the height of a blob from the depth image
    //--------------------------------------------------------------------
    void SetBlobVerticalExtents(  INT iSkeletonIndex, XMVECTOR head, XMVECTOR waist )
    {
        m_f3DBlobHeight[iSkeletonIndex] = XMVectorGetX( XMVector3Length( head - waist  ) ); 
    }

    //--------------------------------------------------------------------
    // Name: ComputeVerticalSpan
    // Desc: find the height of a blob (relative to botY)
    //--------------------------------------------------------------------
    void ComputeVerticalSpan( INT iSkeletonIndex, const USHORT* pDepthImage, INT iBotY );

private:
    FLOAT m_f3DBlobHeight[NUI_SKELETON_COUNT];  // Length of blob in 3D 

    INT   m_BlobSpanXPos[NUI_SKELETON_COUNT];            // x position of the blob
    INT   m_BlobTopLastFrame[NUI_SKELETON_COUNT];        // y position of the top
    INT   m_BlobBotLastFrame[NUI_SKELETON_COUNT];        // y position of the bottom

    FLOAT m_fPredictedPixelSpan[NUI_SKELETON_COUNT];     // predicted height of blob

    BOOL  m_Recapture[NUI_SKELETON_COUNT] ;                  // signal to recompute blob heights



};

const FLOAT CRUMPLE_SCORE_TERMS_INITIAL_HEAD_SCORE_THRESH = .5f;
const FLOAT CRUMPLE_SCORE_TERMS_OVERALLSCORE_RESCALE = .7f;
const INT CRUMPLE_SCORE_TERMS_HEAD_SHAPE_BOX_DILATION_SIZE = (INT)( 20  * IMAGE_SCALE);
const FLOAT CRUMPLE_SCORE_TERMS_MAX_LIMB_WIDTH = .4f;
const FLOAT CRUMPLE_SCORE_TERMS_DEPTH_EDGE = .025f;
const FLOAT CRUMPLE_SCORE_TERMS_DEPTH_EDGE_MAG_THRESH = .1f;

const FLOAT CRUMPLE_SCORE_TERMS_POINTING_ANGLE1 = 45.0f / 180.0f * 3.1415f;
const FLOAT CRUMPLE_SCORE_TERMS_POINTING_ANGLE2 = 50.0f / 180.0f * 3.1415f;

const FLOAT CRUMPLE_SCORE_TERMS_FACE_OCLUDE1 = .4;
const FLOAT CRUMPLE_SCORE_TERMS_FACE_OCLUDE2 = .6;

//--------------------------------------------------------------------
// Name: CCrumpleScoringTerms
// Desc: various heuristics in evaluating how good a skeleton and its
//       associated depth mask look.
//--------------------------------------------------------------------
class CCrumpleScoringTerms
{
public:

    //--------------------------------------------------------------------
    // Name: CrumpleScore
    // Desc: computes the crumple score based on limb length symmetry,
    //       head shape, and forearm appearence.
    //--------------------------------------------------------------------
    FLOAT CrumpleScore( BOOL bUseHeadShape, BOOL bFirst,
                        BOOL bBlobIsMoving, 
                        const NUI_SKELETON_DATA& skeleton, INT iDepthLabel,  const USHORT* pDepthImage );
   
    ~CCrumpleScoringTerms()
    {
        SAFE_DELETE_ARRAY(m_pEdgePoints);           
    }

    CCrumpleScoringTerms() : m_NumEdgePoints(0)
    {
        m_pEdgePoints = new INT[CRUMPLE_DETECTOR_MAX_EDGE_POINT*2];                         
        assert( m_pEdgePoints );
    }

    CCrumpleScoringTerms( const CCrumpleScoringTerms& rhs );

    const CCrumpleScoringTerms& operator=( const CCrumpleScoringTerms& rhs );

private:

   //--------------------------------------------------------------------
    // Name: CrumpleScoreGeom
    // Desc: scores based on the shape of the skeleton
    //       looks for symmetry of bone lengths, and skeletons being upright.
    //--------------------------------------------------------------------
    FLOAT CrumpleScoreGeom(  const NUI_SKELETON_DATA& skeleton, INT iDepthLabel, const USHORT* pDepthImage );
 

    //--------------------------------------------------------------------
    // Name: TestHeadShoulderShape
    // Desc: score based on the head shape.  The more like a straight line
    //       the outline around the head is, the lower the score.
    //--------------------------------------------------------------------
    FLOAT TestHeadShoulderShape( const NUI_SKELETON_DATA& skeleton, INT iDepthLabel,  const USHORT* pDepthImage );


    //--------------------------------------------------------------------
    // Name: LimbEdgePresenceScore
    // Desc: a score based on the presence of depth edges around
    //       the forarms.
    //--------------------------------------------------------------------
    FLOAT LimbEdgePresenceScore( const NUI_SKELETON_DATA& skeleton, 
                                 INT iDepthLabel,  const USHORT* pDepthImage ,
                                 BOOL bLeftOccluded, BOOL bRightOccluded);


    //--------------------------------------------------------------------
    // Name: LimbOccupancyScore
    // Desc: a score based on the depth values under the arm bones and head/torso.
    //--------------------------------------------------------------------
    FLOAT LimbOccupancyScore( const NUI_SKELETON_DATA& skeleton, 
                              INT iDepthLabel,  const USHORT* pDepthImage ,
                              BOOL bLeftOccluded, BOOL bRightOccluded);


    //--------------------------------------------------------------------
    // Name: IsInsideImage
    // Desc: TRUE if the point is within the depth image.
    //--------------------------------------------------------------------
    BOOL IsInsideImage(INT iX, INT iY ) const
    {
        return iX > 0 &&  iY > 0 && iX <  DEPTH_IMAGE_WIDTH && iY < DEPTH_IMAGE_HEIGHT ;
    }

    //--------------------------------------------------------------------
    // Name: LimbScore
    // Desc: computes the average error under the bone segement givenin 
    //       ptA, ptB.  as well as the number of depth pixels under the bone
    //       that have depthLabel.
    //--------------------------------------------------------------------
    void LimbScore( XMVECTOR ptA, XMVECTOR ptB, INT iNumSamples, INT iDepthLabel, const USHORT* pDepthImage,
                    INT& iNumLabeledOut, FLOAT& fAverageErrorOut ) const;


private:

    //--------------------------------------------------------------------
    // Name: GetDepth
    // Desc: returns the depth at point (x,y) in depth and the label.  If the pixel is 
    //       0 or out of bounds the maximum depth value is returned.
    //--------------------------------------------------------------------
    void GetDepth(INT iX,INT iY, const USHORT* pDepth, FLOAT& fDepthP , INT& iLabel )
    {
        USHORT pixel=0;
        if ( !IsInsideImage(iX,iY))
        {         
            fDepthP = MAX_DEPTH;
            iLabel = 0;
        }
        else
        {
            fDepthP = ( pDepth[ iY*DEPTH_IMAGE_WIDTH + iX ] >> 3 ) *.001f;         
            iLabel = pixel & 0x7;  
            if (iLabel == 0 )
            {
                fDepthP = MAX_DEPTH;
            }
        }
    }

    //--------------------------------------------------------------------
    // Name: GetDepth
    // Desc: returns the depth at point (x,y) in depth.  If the depth doesnt
    //       have the right label, the maximum depth value is returned.
    //--------------------------------------------------------------------
    FLOAT GetDepth(INT iX, INT iY, INT iDepthLabel, const USHORT* pDepth )
    {
        USHORT pixel;
        if ( IsInsideImage(iX,iY))
        {
            USHORT label = pDepth[ iY*DEPTH_IMAGE_WIDTH + iX ] & 0x7;

            if ( label == iDepthLabel )
            {
                // find the depth point;
                pixel = pDepth[ iY*DEPTH_IMAGE_WIDTH + iX ] >> 3;;
            }
            else
            {
                return  MAX_DEPTH;
            }
        }
        else
        {
            return  MAX_DEPTH;
        }

        if ( pixel == 0  )
        {
            return MAX_DEPTH;
        }

        return pixel * .001f;
    }


    //--------------------------------------------------------------------
    // Name: IsEdgePresent
    // Desc: return true if an edge is present between (iXposition,iYposition) and 
    //       (iXposition+iXrange,iYposition+iYrange)
    //       (iXrange,iYrange) is axis aligned.
    //--------------------------------------------------------------------
    BOOL IsEdgePresent( INT iDepthLabel, const USHORT* pDepthImage, 
                        INT iXposition, INT iYposition, INT iXrange, INT iYrange );


    //--------------------------------------------------------------------
    // Name: HasTwoEdgesPresent
    // Desc: looks for the presence of two edges between (iXposition,iYposition) and 
    //       (iXposition+iXrange,iYposition+iYrange)
    //       (iXrange,iYrange) is axis aligned.
    //--------------------------------------------------------------------
    BOOL HasTwoEdgesPresent( INT iDepthLabel, const USHORT* pDepthImage, 
                             INT iXposition, INT iYposition, INT iXrange, INT iYrange );


    //--------------------------------------------------------------------
    // Name: LimbEdgePresenceScore
    // Desc: looks for edge presence around the bone (ptA,ptB)
    //--------------------------------------------------------------------
    FLOAT LimbEdgePresenceScore( XMVECTOR ptA, 
                                 XMVECTOR ptB, INT iNumSamples, 
                                 INT iDepthLabel, const USHORT* pDepthImage, BOOL bBoth = FALSE );
   

	FLOAT sqr( FLOAT a ) const
	{
		return a*a;
    }


    //--------------------------------------------------------------------
    // Name: BorderScore
    // Desc: penalizes a skeleton that is snug against an edge of the
    //       depth plane (usually its crumpled in this case )
    //--------------------------------------------------------------------
    FLOAT BorderScore(  const NUI_SKELETON_DATA& skeleton  );


    //--------------------------------------------------------------------
    // Name: IsEdgePoint
    // Desc: tells whether a depth pel is in an edge based on labels
    //--------------------------------------------------------------------
    BOOL IsEdgePoint(const USHORT* pDepthPel, INT iDepthLabel ,int iRowWidth)
    {
        USHORT currentPixel = (*pDepthPel) & 0x7;
        USHORT leftPixel = (*(pDepthPel+1)) & 0x7;
        USHORT rightPixel = (*(pDepthPel-1)) & 0x7;
        USHORT topPixel = (*(pDepthPel-iRowWidth)) & 0x7;
        USHORT botPixel = (*(pDepthPel+iRowWidth)) & 0x7;

        if (  currentPixel == iDepthLabel  &&
             ( leftPixel != iDepthLabel ||
               rightPixel != iDepthLabel ||
               topPixel != iDepthLabel ||
               botPixel != iDepthLabel ) )
        {
            return TRUE;
        }

        return FALSE;
    }

    BOOL m_bBadInitialHead;         // true if initial head score was low

    // individual terms of score
    class CCrumpleScores
    {
    public:

        FLOAT m_fGeometryOfSkeleton;
        FLOAT m_fHeadShoulderEdgeShape;
        FLOAT m_fClippedByImageBoundary;
        FLOAT m_fOccupancyAlongBone;
        FLOAT m_fEdgePresenceAroundLimb;
    };

    CCrumpleScores m_Scores;           

    BOOL  m_bLeftArmOccluded;
    BOOL  m_bRightArmOccluded;

 public:
     INT *m_pEdgePoints;              // debug buffer
     INT m_NumEdgePoints ;           // number of points in buffer


     //--------------------------------------------------------------------
     // Name: AddEdgePoint
     // Desc: add a point to the debug-graphics buffer
     //--------------------------------------------------------------------    
     void AddEdgePoint( INT iX, INT iY )
     {
         if ( m_NumEdgePoints < CRUMPLE_DETECTOR_MAX_EDGE_POINT )
         {
             m_pEdgePoints[m_NumEdgePoints*2  ] = iX;
             m_pEdgePoints[m_NumEdgePoints*2+1] = iY;
             m_NumEdgePoints++;
         }
     }


     //--------------------------------------------------------------------
     // Name: PrintScores
     // Desc: prints scores
     //--------------------------------------------------------------------
     void PrintScores(  ) const
     {
#define PRINTF(term) { printf( #term " = %f\n",  term );  }
#define PRINTB(term) { printf( #term " = %d\n",  term );  }

         PRINTF( m_Scores.m_fGeometryOfSkeleton );
         PRINTF( m_Scores.m_fHeadShoulderEdgeShape);
         PRINTF( m_Scores.m_fClippedByImageBoundary);
         PRINTF( m_Scores.m_fOccupancyAlongBone);
         PRINTF( m_Scores.m_fEdgePresenceAroundLimb);
         PRINTB( m_bLeftArmOccluded );
         PRINTB( m_bRightArmOccluded );
     }

#ifdef _XBOX
     //--------------------------------------------------------------------
     // Name: DrawTerm
     // Desc: prints score to a font
     //--------------------------------------------------------------------
     void DrawTerm( ATG::Font& font, WCHAR* pszName, FLOAT fVal, FLOAT fXpos, FLOAT& fYpos ) const
     {
         WCHAR msg[80];
         swprintf_s( msg, L"%s = %f",  pszName, fVal );
         font.DrawText( fXpos, fYpos,  D3DCOLOR_RGBA(255,255   ,0   ,255) ,    msg );      
         fYpos+=20;                                                                         
     }


     //--------------------------------------------------------------------
     // Name: Trace
     // Desc: prints individual terms of score.
     //--------------------------------------------------------------------
     void Trace( ATG::Font& font,FLOAT fXpos, FLOAT& fYpos ) const
     {
         font.Begin();
         font.SetScaleFactors(1.0,1.0);

         DrawTerm( font, L"skeleton-shape score", m_Scores.m_fGeometryOfSkeleton , fXpos, fYpos);
         DrawTerm( font, L"head-shoulder-shape score",   m_Scores.m_fHeadShoulderEdgeShape, fXpos, fYpos );
         DrawTerm( font, L"view-frustum clip score ", m_Scores.m_fClippedByImageBoundary , fXpos, fYpos);
         DrawTerm( font, L"bone-occupancy ", m_Scores.m_fOccupancyAlongBone , fXpos, fYpos);
         DrawTerm( font, L"edge-presence around forearms ", m_Scores.m_fEdgePresenceAroundLimb , fXpos, fYpos);
         DrawTerm( font, L"left arm occluded ", m_bLeftArmOccluded ? 1.0f : 0 , fXpos, fYpos);
         DrawTerm( font, L"right arm occluded ", m_bRightArmOccluded ? 1.0f : 0 , fXpos, fYpos);

         font.End();

     }
#endif

};



typedef struct _COLORED_VERTEX {
    XMVECTOR position;
    XMVECTOR color;
} COLORED_VERTEX;


#define USECHANGE

//default size is 512k, we only do half that to be conservative perhaps should do 1/4th?
#define  colorVertexBufferSize  ( 512 * 1024  / ( sizeof(COLORED_VERTEX)  * DEPTH_IMAGE_WIDTH ) )

#define ISZERO(fVal)  ( (fVal>>3)== 0 )
#define GETPLAYERID(fVal) ( ( fVal ) & 0x7 )



const INT   CRUMPLE_DETECTOR_BLOB_OVERLAP_PERIMETER_SIZE = (INT)(  16 * IMAGE_SCALE );
                                            // width of border along image boundary, in which
                                            // a skelton starts to crumple

const FLOAT CRUMPLE_DETECTOR_OCCLUSION_SCORE_RESCALE = .3f;
                                            // occlusion scores are rescaled by this factor


const FLOAT CRUMPLE_DETECTOR_OCCLUSION_HEAD_CLIP_THRESH = .9f;
                                            // threshold for the head clip decgtor beyond which the head shape term 
                                            // can be used 


const FLOAT CRUMPLE_DETECTOR_OCCLUSION_LOW_AND_FAR_THRESH = .9f;
                                            // threshold beyond which the head shape term 
                                            // can be used 

// Sigmoid for the head clip score.
const FLOAT CRUMPLE_DETECTOR_HEAD_CLIP_SIGMOID_LOW = .5f;
const FLOAT CRUMPLE_DETECTOR_HEAD_CLIP_SIGMOID_HIGH= .6f;                            

// Sigmoid for compactness scores
const FLOAT CRUMPLE_DETECTOR_COMPACTNESS_SIGMOID_LOW = 45.0f;
const FLOAT CRUMPLE_DETECTOR_COMPACTNESS_SIGMOID_HIGH= 50.0f;                            


//--------------------------------------------------------------------
// Name: CCrumpleDetector
// Desc: determines whether a skeleton is crumpled
//--------------------------------------------------------------------
class CCrumpleDetector
{
public:

    //--------------------------------------------------------------------
    // Name: EvaluteSkeleton
    // Desc: Evaluate skeletons based on skeleton and depth buffer.
    //       Depth pixels below the ground plane are zero'd out
    //--------------------------------------------------------------------
    void EvaluteSkeleton( const NUI_SKELETON_FRAME& skel, USHORT* pDepthBuffer );


    //--------------------------------------------------------------------
    // Name: overalScore
    // Desc: overall score for skeleton.  This returns a value between zero and 1.
    //        a scores closer to zero signify crumpled skeletons and scores closer to 1
    //        signify uncrumpled skeletons.
    //--------------------------------------------------------------------
    FLOAT OveralScore( INT iSkeletonIndex )
    {
        return m_fOveralScore[iSkeletonIndex];
    }


    //--------------------------------------------------------------------
    // Name: DrawDebugGraphics
    // Desc: debug graphics,
    //       renders occluding points, and edge points.
    //--------------------------------------------------------------------
    void DrawDebugGraphics( D3DDevice* pd3dDevice, USHORT* pDepthBuffer )
    {
        RenderPoints(pd3dDevice, pDepthBuffer, m_BlobOverlap.m_PerimeterPoints, m_BlobOverlap.m_NumPerimeterPointsDisplay,3 );

        for(INT k=0; k < NUI_SKELETON_COUNT; k++)
        {         
            RenderPoints(pd3dDevice, pDepthBuffer, m_CrumpleDetector[k].m_pEdgePoints, m_CrumpleDetector[k].m_NumEdgePoints,2 );         
        }
    }

    //--------------------------------------------------------------------
    // Name: DrawScores
    // Desc: draws score via a font
    //--------------------------------------------------------------------
    void DrawScores( ATG::Font& font, FLOAT fXpos, FLOAT fYpos, BOOL bDrawDetails );


private:

    //--------------------------------------------------------------------
    // Name: RenderPoints
    // Desc: Draws points in points that are step a part.
    //       Points holds the 2d positions (x0 ,y0 ,x1 ,y1 ...) when step == 2
    //       or the 2d position and the visibility of the point when step == 3,
    //          (x0,y0, occluded0, x1,y1,occluded1,.... )
    //--------------------------------------------------------------------
    void RenderPoints( D3DDevice* pd3dDevice, const USHORT* pDepthBuffer,  INT* pPoints, INT iNumPoints, INT iStep );


    // individual terms of the score

    // crumple term computer and its score
    CCrumpleScoringTerms m_CrumpleDetector[NUI_SKELETON_COUNT];
	FLOAT  m_fCrumpleScore[NUI_SKELETON_COUNT];
    
    // occlusion detector and its scores
    CBlobOverlapDetector m_BlobOverlap;
    FLOAT  m_fOcclScore[NUI_SKELETON_COUNT];

    // cliped by top plane detector and its scores
    CClipedFromTopDetector m_ClippedFromTop;
    FLOAT m_fHeadClip[NUI_SKELETON_COUNT];

#ifdef USECHANGE
    // change detector
    CDepthSkelChangeScore m_DepthChangeMeasure;
#endif   

#ifdef USETWICH
    FLOAT m_fDepthTwitchScore[NUI_SKELETON_COUNT];
#endif

    // overall scores
    FLOAT m_fOveralScore[NUI_SKELETON_COUNT];

};