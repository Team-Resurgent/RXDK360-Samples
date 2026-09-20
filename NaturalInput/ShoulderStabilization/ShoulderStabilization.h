//--------------------------------------------------------------------
// HeadShoulderStabalize.h
//
// This class creates a filter that stabilizes the head and shoulder joints 
// as they are self-occluded by arm/hand joints.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <nuiapi.h>
#include <xnamath.h>

const FLOAT EPSILON = 1e-8;

//Depth camera field of view
const FLOAT DEPTH_SCALE_FACTOR		= NUI_CAMERA_DEPTH_NOMINAL_FOCAL_LENGTH_IN_PIXELS / 320.0f;

//Inverse of depth camera field of view
const FLOAT DEPTH_SCALE_FACTOR_INV	= 1.0f / DEPTH_SCALE_FACTOR;	

//Depth camera resolution
const INT DEPTH_CAMERA_RESOLUTION_X = 320;
const INT DEPTH_CAMERA_RESOLUTION_Y = 240;

//Derived values for convenience
const FLOAT DEPTH_CAMERA_CENTER_X = DEPTH_CAMERA_RESOLUTION_X * 0.5f;
const FLOAT DEPTH_CAMERA_CENTER_Y = DEPTH_CAMERA_RESOLUTION_Y * 0.5f;
const FLOAT DEPTH_SCALE_FACTOR_SCREEN	= DEPTH_SCALE_FACTOR * DEPTH_CAMERA_RESOLUTION_X;

const XMVECTOR g_vDepthCameraCenter = XMVectorSet( DEPTH_CAMERA_CENTER_X, DEPTH_CAMERA_CENTER_Y, 0.0f, 0.0f );
const XMVECTOR g_vDepthCameraFOVScreen    = XMVectorSet( DEPTH_SCALE_FACTOR_SCREEN, DEPTH_SCALE_FACTOR_SCREEN, 1.0f, 1.0f );
const XMVECTOR g_vInvDepthCameraFOV = XMVectorSet( 1.0f/DEPTH_SCALE_FACTOR_SCREEN, 1.0f/DEPTH_SCALE_FACTOR_SCREEN, 1.0f, 1.0f );

//--------------------------------------------------------------------------------------
// Name: Project
// Desc: Project point into depth camera projection plane
//--------------------------------------------------------------------------------------
inline XMVECTOR Project( XMVECTOR p )
{
    XMVECTOR vProject   = XMVectorSplatZ( p );              // vProject = ( p.z, p.z, p.z, p.z )
    vProject            = XMVectorReciprocal( vProject );   // vProject = ( 1.0f / p.z, 1.0f / p.z, 1.0f / p.z, 1.0f / p.z )
    XMVECTOR vResult    = XMVectorMultiplyAdd( p, g_vDepthCameraFOVScreen * vProject, g_vDepthCameraCenter );
    return vResult;
}


//--------------------------------------------------------------------------------------
// Name: Unproject
// Desc: Calculate 3D vPosition from vPosition in depth camera projection plane and depth
//--------------------------------------------------------------------------------------
inline XMVECTOR Unproject( XMVECTOR p )
{   
    XMVECTOR res = p;
    FLOAT q = p.z / DEPTH_SCALE_FACTOR_SCREEN;
    res.x = (p.x - DEPTH_CAMERA_CENTER_X)*q;
    res.y = (DEPTH_CAMERA_CENTER_Y - p.y)*q;
    return res;
}


#define  NUM_MODEL_PARAMS                   9   // 6 for position and orienation and 3 for shape
#define  NUM_HEAD_SHOULDER_JOINTS           4

static const XMMATRIX t_matIdentity = XMMatrixIdentity();
static const XMMATRIX t_matZero( 0,0,0,0, 0,0,0,0,  0,0,0,0,   0,0,0,1 );

//--------------------------------------------------------------------
// Name: CGradientDesecnt
// Desc: A numerical solver that minimize the magnitude of a function
//       N is the maximum number of parameters for the function.  
//       errorFunction is a functor that takesa vector of length N and returns
//       an error value.  
//--------------------------------------------------------------------
template< class CErrorFunction, INT N >
class   CGradientDesecnt {

public:  

    CGradientDesecnt( 
        INT iMaxNumberOfMajorIterations = 100,
        FLOAT fGradientStepScale = .01,
        INT iMaxNumberOfMinorIterations=5,
        FLOAT fDeltaForNumericalGradientComputation = 1e-3)
    {
        assert(  N >0 );
        assert( fDeltaForNumericalGradientComputation > 0 );
        assert( iMaxNumberOfMajorIterations > 0);
        assert( iMaxNumberOfMinorIterations > 0 );
        assert( fGradientStepScale > 0 );

        m_iMaxNumSmallSteps = 3;;
        m_fRelativeDropInErrorThreshold = 1e-3;

        m_iNumVariables = N;
        m_iMaxNumberOfMajorIterations = iMaxNumberOfMajorIterations;

        m_fDeltaForNumericalGradientComputation = fDeltaForNumericalGradientComputation;
        m_iMaxNumberOfMinorIterations = iMaxNumberOfMinorIterations;
        m_fGradientStepScale = fGradientStepScale;
    }


    //--------------------------------------------------------------------
    // Name: Minimize
    // Desc: Minimizes the value the error in errFunction starting at X
    //--------------------------------------------------------------------
    FLOAT Minimize( FLOAT* X, CErrorFunction& errorFunction )
    {
        FLOAT error = errorFunction(X);

        INT numSmallSteps = 0;

        INT iter =0;
        FLOAT deltaError =FLT_MAX;

        // In each major iteration compute the gradient and take
        // a step in its direction.
        for( iter = 0; iter < m_iMaxNumberOfMajorIterations; ++iter )
        {
            ComputeGradient( X, error, errorFunction,  m_dX );

            INT nInterItters  = 0;
            FLOAT errorNew=0;

            for( ; nInterItters < m_iMaxNumberOfMinorIterations; ++nInterItters )
            {
                for( INT i = 0; i < m_iNumVariables; ++i )
                {
                    m_Xnew[i] =  X[i] +   -m_fGradientStepScale * m_dX[i];
                }

                errorNew = errorFunction(m_Xnew); 

                // adjust step size based on whether the error went down
                // or not.
                if (errorNew < error )
                {
                    // if the error went down increase step size and break out of inner loop.
                    m_fGradientStepScale *= 2;
                    break;
                }
                else
                {
                    // decrease step size.
                    m_fGradientStepScale *= .5f;
                }
            }

            // if the error does not decrease after m_iMaxNumberOfMinorIterations
            // attempts, we are at a local minimum.			
            if ( nInterItters >= m_iMaxNumberOfMinorIterations )
            {
                return error ; // give up
            }

            deltaError = error - errorNew;

            //  if the rate of change of error falls bellow m_fRelativeDropInErrorThreshold
            //  we assume convergance.
            if ( deltaError > 0 && deltaError / error < m_fRelativeDropInErrorThreshold )
            {
                numSmallSteps++;
                if ( numSmallSteps > m_iMaxNumSmallSteps )
                {
                    return errorNew;
                }
            }
            else
            {
                numSmallSteps = 0;
            }

            error = errorNew;

            for( INT i = 0; i < m_iNumVariables; ++i )
            {
                X[i] =  m_Xnew[i];
            }
        }

        return error;
    }	


    //--------------------------------------------------------------------
    // Name: ComputeGradient
    // Desc:  numerically computes the gradient of errorFunc
    //--------------------------------------------------------------------
    void  ComputeGradient(const FLOAT* xi, FLOAT y, CErrorFunction& errorFunction,  FLOAT* dx) 
    {
        FLOAT delta = m_fDeltaForNumericalGradientComputation;

        FLOAT old;
        FLOAT one_delta = 1.0f/delta;

        FLOAT err = y;
        FLOAT* x = (FLOAT*) xi;

        for( INT j = 0; j < m_iNumVariables; ++j ) 
        {    
            // compute derivative for each component of m_X.
            old =     x[j];
            x[j]=x[j]+delta; 

            FLOAT err2 = errorFunction(x);
            dx[j] = ( err2 - err ) * one_delta;

            // put back the old value of x
            x[j]=old;
        }  
    }


    INT m_iNumVariables;
    INT m_iMaxNumberOfMajorIterations ;
    INT m_iMaxNumberOfMinorIterations; 

    INT    m_iMaxNumSmallSteps;
    FLOAT m_fRelativeDropInErrorThreshold;

    FLOAT m_fDeltaForNumericalGradientComputation ;
    FLOAT m_fGradientStepScale;      

private:

    FLOAT m_dX[N], m_Xnew[N];

};


const INT MAXIMUM_NUMBER_GRADIENT_ITERATIONS = 25;   // maximum number of major iterations in gradient descent
const FLOAT MOTION_STIFFNESS_DECAY =  .02;           // Smaller motions are stiffened more then large motions to removed jitter,
                                                     // this parameter controls the decay in motion stiffness

const FLOAT LOCK_SHAPE_PARAMETERS_THRESHOLD =  10;   // when the score is below this threshold shape parameters are locked.
const FLOAT POSITIONAL_STIFFNESS_WEIGHT = 3;         // damping term for the position of the head/shoulder frame    
const FLOAT ORIENTATION_STIFNESS_WEIGHT = .02*3 ;    // damping term for the orientation of the head/shoulder frame.
const FLOAT STARTING_GRADIENT_STEP_SCALE =  .1;      // initial value of the gradient scale step in the solver
const FLOAT JOINT_STIFFNESS_SCALE = 2;               // the weight given to the mismatch between observed and predicated joint
                                                     // positions.
const FLOAT JOINT_STIFFNESS_MIN =.05; ;              // the minimum weight given to the joint stiffness


//--------------------------------------------------------------------
// Name: CHeadShoulderFrame
// Desc: manages the head and shoulder
//--------------------------------------------------------------------
class  CHeadShoulderFrame 
{
public:

    enum JOINT_IDS
    {
        JOINT_IDS_TOP_HEAD =0,
        JOINT_IDS_SHOULDER_CENTER,
        JOINT_IDS_SHOULDER_LEFT,
        JOINT_IDS_SHOULDER_RIGHT,
        JOINT_IDS_MAX_NUM_JOINTS
    };

    CHeadShoulderFrame() : m_bFirst (TRUE),  
        m_fNeckHeight(0), 
        m_fNeckHeightLast(0), 
        m_fShoulderWidth(1), 
        m_fHeadHeight(1), 
        m_fParameterStiffness(0), 
        m_vFramePositionalStiffnessSqr(0), 
        m_vFrameOrientationStiffnessSqr(0),
        m_bLockShapeParams(FALSE)
    {
        m_GradientSolver.m_iMaxNumberOfMajorIterations = MAXIMUM_NUMBER_GRADIENT_ITERATIONS;
        m_vFramePosition  = XMVectorSet(0,0,1,0);		
        m_vFrameOrientation = XMVectorZero();
        m_matFrameOrienation = t_matIdentity;
        m_fNumParams = NUM_MODEL_PARAMS;
    }


    //--------------------------------------------------------------------------------------
    // Name: UpdateHeadShoulderFrame
    // Desc: stabilizes the joint positions in frameOfJoints, using the occlusions scores in occlusionScores
    //--------------------------------------------------------------------------------------
    void UpdateHeadShoulderFrame( const XMVECTOR frameOfJoints[NUM_HEAD_SHOULDER_JOINTS], 
                                  const FLOAT occlusionScores[NUM_HEAD_SHOULDER_JOINTS] )		
    {  

        // compute the overall probability that the joints are unoccluded
        FLOAT probUnOccluded = 1;
        FLOAT maxProbUnOccleded = 0;
        for( INT i = 0; i < NUM_HEAD_SHOULDER_JOINTS ; ++i )
        {

            m_OcclusionScores[i] = occlusionScores[i];
            assert( m_OcclusionScores[i] >= 0 );
            assert( m_OcclusionScores[i] <= 1.0f );

            FLOAT probUnOcclOfJoint = (1.0f -  m_OcclusionScores[i] );
            probUnOccluded  *= probUnOcclOfJoint;

            if (  maxProbUnOccleded <  probUnOcclOfJoint )
            {
                maxProbUnOccleded = probUnOcclOfJoint;
            }
        }

        // compute an estimate of the overall displacement 
        // of the joints between frames
        XMVECTOR displacement = XMVectorZero();
        FLOAT weight(0);
        for( INT i = 0; i < NUM_HEAD_SHOULDER_JOINTS; ++i )
        {
            XMVECTOR oldPos=m_JointPositions[i];
            XMVECTOR newPos=frameOfJoints[i];

            FLOAT probUnOcclOfJoint = ( occlusionScores[i] );

            XMVECTOR dPos = ( newPos - oldPos ) ;

            displacement += dPos * probUnOcclOfJoint ;
            weight+=  probUnOcclOfJoint ;
        }

        if ( weight > EPSILON )
            displacement *=  1.0f / ( weight );
        else
            displacement = XMVectorSet(0,0,0,0);

        // compute the motion score between frames,
        // slower motion has a higher score
        m_fMotionBetweenFrameScore =  expf( -  XMVectorGetX(XMVector3LengthSq( displacement )) / (  MOTION_STIFFNESS_DECAY  *  MOTION_STIFFNESS_DECAY  ) );

        // compute average displacement of pixels (weighted by confidence)
        // IN screen space
        for( INT i = 0; i < NUM_HEAD_SHOULDER_JOINTS; ++i )
        {
            m_JointPositions[i] = frameOfJoints[i];
            m_OcclusionScores[i]= occlusionScores[i];
        }

        //
        // if the head and shoulders are un-occluded  make the width/height stiff
        //
        FLOAT probOccluded = 1- probUnOccluded ;
        m_fParameterStiffness = expf( probOccluded * 10);

        if ( m_fParameterStiffness >  LOCK_SHAPE_PARAMETERS_THRESHOLD )
        {
            m_bLockShapeParams = TRUE;
        }
        else
        {
            m_bLockShapeParams = FALSE;
        }

        // compute joint weights
        for( INT i = 0; i < JOINT_IDS_MAX_NUM_JOINTS; ++i )
        {
            FLOAT jointWeight = max( ( 1-m_OcclusionScores[i] ) * JOINT_STIFFNESS_SCALE  ,  JOINT_STIFFNESS_MIN  );
            m_JointWeights[i] =   jointWeight*jointWeight;
        }

        // compute stiffnesses
        m_vFramePositionalStiffnessSqr=   sqr( POSITIONAL_STIFFNESS_WEIGHT * m_fMotionBetweenFrameScore );
        m_vFrameOrientationStiffnessSqr = sqr( ORIENTATION_STIFNESS_WEIGHT *m_fMotionBetweenFrameScore );

        // on the first iteration compute an estimate of
        // all the parameters
        if ( m_bFirst )
        {
            m_fShoulderWidth = XMVectorGetX( XMVector3Length( frameOfJoints[JOINT_IDS_SHOULDER_LEFT] -  frameOfJoints[JOINT_IDS_SHOULDER_RIGHT] ));
            m_fHeadHeight = XMVectorGetX( XMVector3Length( frameOfJoints[JOINT_IDS_SHOULDER_CENTER] -  frameOfJoints[JOINT_IDS_TOP_HEAD] ));
            m_vFramePosition = frameOfJoints[JOINT_IDS_SHOULDER_CENTER];
            m_vFrameOrientation = XMVectorZero();
            m_bFirst = FALSE;

            XMVECTOR headDir  = (frameOfJoints[JOINT_IDS_TOP_HEAD] -  frameOfJoints[JOINT_IDS_SHOULDER_CENTER]) /  m_fHeadHeight ;

            FLOAT rightShoulderDrop = XMVectorGetX(XMVector3Dot( headDir ,   frameOfJoints[JOINT_IDS_SHOULDER_RIGHT]  - frameOfJoints[JOINT_IDS_SHOULDER_CENTER]    ));
            FLOAT leftShoulderDrop =  XMVectorGetX( XMVector3Dot( headDir,   frameOfJoints[JOINT_IDS_SHOULDER_LEFT]  - frameOfJoints[JOINT_IDS_SHOULDER_CENTER]  ));

            m_fNeckHeight = -(rightShoulderDrop + leftShoulderDrop ) * .5f;

        }
        else
        {
            // on subsequent iterations use the solver to estimate the parameters
            m_GradientSolver.m_fGradientStepScale = STARTING_GRADIENT_STEP_SCALE;

            if ( m_bLockShapeParams )
            {
                m_GradientSolver.m_iNumVariables = m_fNumParams - 3;;
            }
            else
            {
                m_GradientSolver.m_iNumVariables = m_fNumParams;
            }

            AdjustPositionOfHeadAndShoulders();
            PackParametersIntoVector(m_X);
            m_GradientSolver.Minimize(m_X, *this );
            UnPackParametersFromVector(m_X);
        }

        // update velocities
        m_fShoulderWidthLast= m_fShoulderWidth;
        m_fHeadHeightLast = m_fHeadHeight;
        m_vFramePositionLast = m_vFramePosition;
        m_vFrameOrientationLast = m_vFrameOrientation;
        m_fNeckHeightLast = m_fNeckHeight;
        RotationMatrixFromRodriguezParameters( m_vFrameOrientationLast.x, m_vFrameOrientationLast.y, m_vFrameOrientationLast.z, m_matFrameOrienationLast );
    }


    //--------------------------------------------------------------------------------------
    // Name: GetPointInWorld
    // Desc: gets the position of the joint specified by the index in the world 
    //--------------------------------------------------------------------------------------
    XMVECTOR  GetPointInWorld(JOINT_IDS i) const 
    {
        switch ( i )
        {
        case JOINT_IDS_TOP_HEAD:  return m_vFramePosition  + ( XMVector3TransformCoord ( XMVectorSet(0,m_fHeadHeight,0, 0 ), m_matFrameOrienation ));
        case JOINT_IDS_SHOULDER_LEFT:  return m_vFramePosition  + (  XMVector3TransformCoord ( XMVectorSet(-m_fShoulderWidth/2,-m_fNeckHeight,0,0), m_matFrameOrienation));
        case JOINT_IDS_SHOULDER_RIGHT:  return m_vFramePosition  + ( XMVector3TransformCoord(  XMVectorSet(m_fShoulderWidth/2,-m_fNeckHeight,0,0), m_matFrameOrienation ));
        case JOINT_IDS_SHOULDER_CENTER:  return m_vFramePosition  ;
        }

        return XMVectorZero();
    }


    FLOAT ShoulderWidth() const { return m_fShoulderWidth; }
    FLOAT HeadHeight() const { return m_fHeadHeight; }


    //--------------------------------------------------------------------------------------
    // Name: operator()
    // Desc: returns the cost of the parameters in X.
    //--------------------------------------------------------------------------------------
    FLOAT operator()( const FLOAT* X   )
    {
        UnPackParametersFromVector( X);

        FLOAT sumErrorsSqr=0;

        //elastic force to observed joints
        XMVECTOR pt[JOINT_IDS_MAX_NUM_JOINTS];

        XMVECTOR c1w = 
            XMVectorSet
            ( m_matFrameOrienation(0,0), m_matFrameOrienation(0,1),m_matFrameOrienation(0,2), 0 ) * m_fShoulderWidth/2;

        XMVECTOR c2 = 
            XMVectorSet( m_matFrameOrienation(1,0), m_matFrameOrienation(1,1),m_matFrameOrienation(1,2), 0 );

        XMVECTOR c2nh = c2 * m_fNeckHeight;

        pt[JOINT_IDS_TOP_HEAD]       = m_vFramePosition  +  c2 * m_fHeadHeight;
        pt[JOINT_IDS_SHOULDER_LEFT]  = m_vFramePosition  -    c1w  - c2nh;
        pt[JOINT_IDS_SHOULDER_RIGHT] = m_vFramePosition  +    c1w  - c2nh;
        pt[JOINT_IDS_SHOULDER_CENTER]     = m_vFramePosition  ;

        for( INT i = 0; i < JOINT_IDS_MAX_NUM_JOINTS; ++i )
        {
            XMVECTOR ptM = m_JointPositions[ i];
            sumErrorsSqr += XMVectorGetX(XMVector3LengthSq( pt[i] - ptM )) * m_JointWeights[i]; 
        }

        // damping force
        sumErrorsSqr += XMVectorGetX(XMVector3LengthSq( m_vFramePositionLast - m_vFramePosition )) * m_vFramePositionalStiffnessSqr;
        sumErrorsSqr += XMVectorGetX(XMVector3LengthSq( m_vFrameOrientationLast - m_vFrameOrientation )) * m_vFrameOrientationStiffnessSqr;

        if ( !m_bLockShapeParams )
        {
            // parameter scores
            sumErrorsSqr += sqr( ( m_fShoulderWidthLast - m_fShoulderWidth ) * m_fParameterStiffness );
            sumErrorsSqr += sqr( (m_fHeadHeightLast - m_fHeadHeight ) * m_fParameterStiffness );
            sumErrorsSqr += sqr((m_fNeckHeightLast - m_fNeckHeight ) * m_fParameterStiffness );
        }

        return sumErrorsSqr;
    }

    void Reset()
    {
        m_bFirst = TRUE;
    }


private:

    FLOAT sqr( FLOAT val ) const
    {
        return val*val;
    }


    // Name:  RotationMatrixFromRodriguezParameters
    // Desc:  converts the 3 parameter a,b,c into a rotation matrix.  the direction the 3D
    //        vector formed by (a,b,c) is the axis of rotation and the magnitude of the vector (a,b,c)
    //        is the ammount (in radians) to rotate about.
    //
    static void RotationMatrixFromRodriguezParameters(FLOAT a, FLOAT b, FLOAT c,  XMMATRIX& R)
    {
        FLOAT theta,num;

        theta =  sqrtf(a*a+b*b+c*c);
        num = floor(theta/2*XM_PI);

        R = t_matIdentity;

        if (fabs(theta) >  EPSILON ) 
        {
            XMMATRIX H (  0, -c,  b,  0,
                c,  0, -a,  0,
                -b,  a,  0,  0,
                0,  0,  0,  0 );

            FLOAT a2 = a*a;
            FLOAT b2 = b*b;
            FLOAT c2 = c*c;
            FLOAT ab = a*b;
            FLOAT ac = a*c;
            FLOAT bc = b*c;

            XMMATRIX H2 ( -(c2+b2), ab,  ac,   0,
                ab,  -(c2+ a2), bc, 0,
                ac,  bc, -(b2+a2),  0,
                0,0,0,0);

            FLOAT sinTheta = sin(theta)/theta;
            FLOAT oneMcosTheta  =  (1-cos(theta))/(theta*theta);

            for( INT i = 0; i < 3 ; ++i )
            {
                for( INT j = 0; j < 3 ; ++j )
                {
                    R(i,j) +=  H(i,j)* sinTheta + H2(i,j) * oneMcosTheta;
                }
            }
        }
    }


    //--------------------------------------------------------------------------------------
    // Name: AdjustPositionOfHeadAndShoulders
    // Desc: adjusts the position of the head shoulder frame to fit the observed joint positions
    //--------------------------------------------------------------------------------------
    void AdjustPositionOfHeadAndShoulders() 
    {
        XMVECTOR pt[JOINT_IDS_MAX_NUM_JOINTS];

        XMVECTOR c1w = XMVectorSet( m_matFrameOrienation(0,0), m_matFrameOrienation(0,1),m_matFrameOrienation(0,2), 	0 ) * m_fShoulderWidth/2;
        XMVECTOR c2 = XMVectorSet( m_matFrameOrienation(1,0), m_matFrameOrienation(1,1),m_matFrameOrienation(1,2), 	0 );
        XMVECTOR c2nh = c2 * m_fNeckHeight;

        pt[JOINT_IDS_TOP_HEAD]       = m_vFramePosition + c2 * m_fHeadHeight;
        pt[JOINT_IDS_SHOULDER_LEFT]  = m_vFramePosition -    c1w  - c2nh; 
        pt[JOINT_IDS_SHOULDER_RIGHT] = m_vFramePosition +    c1w  - c2nh;     
        pt[JOINT_IDS_SHOULDER_CENTER]     = m_vFramePosition ;

        XMVECTOR delta = XMVectorZero();
        FLOAT    weight = 0;

        for( INT i = 0; i < JOINT_IDS_MAX_NUM_JOINTS; ++i )
        {
            delta += ( m_JointPositions[i]  - pt[i] ) * m_JointWeights[i];
            weight += m_JointWeights[i];		
        }

        weight += m_vFramePositionalStiffnessSqr;

        if ( weight > EPSILON )
        {
            delta *= 1.0f/ (weight );
        }
        else
        {
            delta = XMVectorSet(0,0,0,0);
        }

        m_vFramePosition += delta;
    }


    enum VARID
    {
        VARID_POS_X=0,
        VARID_POS_Y,
        VARID_POS_Z,
        VARID_ORIENT_X,
        VARID_ORIENT_Y,
        VARID_ORIENT_Z,
        VARID_WIDTH,
        VARID_HEIGHT,
        VARID_NECK_HEIGHT
    };


    //--------------------------------------------------------------------------------------
    // Name: PackParametersIntoVector
    // Desc: packs the parameters in local variables into the vector x
    //--------------------------------------------------------------------------------------
    void PackParametersIntoVector (  FLOAT* x ) const
    {
        x[VARID_POS_X] = XMVectorGetX(m_vFramePosition);
        x[VARID_POS_Y] = XMVectorGetY(m_vFramePosition);
        x[VARID_POS_Z] = XMVectorGetZ(m_vFramePosition);
        x[VARID_ORIENT_X] = XMVectorGetX(m_vFrameOrientation);
        x[VARID_ORIENT_Y] = XMVectorGetY(m_vFrameOrientation);
        x[VARID_ORIENT_Z] = XMVectorGetZ(m_vFrameOrientation);

        if ( !m_bLockShapeParams )
        {
            x[VARID_WIDTH] = m_fShoulderWidth;
            x[VARID_HEIGHT] = m_fHeadHeight;
            x[VARID_NECK_HEIGHT] = m_fNeckHeight;
        }
    }


    //--------------------------------------------------------------------------------------
    // Name: UnPackParametersFromVector
    // Desc: unpacks the parameters in x into member variables.
    //--------------------------------------------------------------------------------------
    void UnPackParametersFromVector( const FLOAT* x ) 
    {
        m_vFramePosition = XMVectorSet(x[VARID_POS_X],x[VARID_POS_Y],x[VARID_POS_Z], 0);
        m_vFrameOrientation = XMVectorSet(x[VARID_ORIENT_X],x[VARID_ORIENT_Y],x[VARID_ORIENT_Z],0 );
        RotationMatrixFromRodriguezParameters( m_vFrameOrientation.x, m_vFrameOrientation.y, m_vFrameOrientation.z, m_matFrameOrienation );

        if (!m_bLockShapeParams )
        {
            m_fShoulderWidth =  x[VARID_WIDTH];
            m_fHeadHeight = x[VARID_HEIGHT];
            m_fNeckHeight= x[VARID_NECK_HEIGHT];
        }
    }


    BOOL m_bFirst;

    FLOAT m_fNeckHeight;
    FLOAT m_fNeckHeightLast;

    FLOAT m_fShoulderWidth;
    FLOAT m_fHeadHeight;
    XMVECTOR m_vFramePosition;

    FLOAT m_fShoulderWidthLast;
    FLOAT m_fHeadHeightLast;
    XMVECTOR m_vFramePositionLast;

    XMVECTOR m_vFrameOrientation;
    XMVECTOR m_vFrameOrientationLast;

    XMMATRIX m_matFrameOrienation;
    XMMATRIX m_matFrameOrienationLast;

    BOOL m_bLockShapeParams;
    INT m_fNumParams;

    FLOAT m_fParameterStiffness;
    FLOAT m_vFramePositionalStiffnessSqr;
    FLOAT m_vFrameOrientationStiffnessSqr;

    XMVECTOR m_JointPositions[NUM_HEAD_SHOULDER_JOINTS];
    FLOAT  m_JointWeights[NUM_HEAD_SHOULDER_JOINTS];

    FLOAT  m_OcclusionScores[NUM_HEAD_SHOULDER_JOINTS];
    FLOAT m_fMotionBetweenFrameScore;

    FLOAT m_X[NUM_MODEL_PARAMS];
    CGradientDesecnt<CHeadShoulderFrame, NUM_MODEL_PARAMS>  m_GradientSolver;

};


const FLOAT JOINT_RADIUS_RELATIVE_TO_HEADHEIGHT = .1f; // the radius of a joint relative to the head height
const FLOAT JOINT_OCCUPANCY_DECAY = .2f;                  // the decay in occupancy of a joint as you move 
                                                          // away from the edge of a joint


const FLOAT LIMB_WIDTH_RELATIVE_TO_HEADHEIGHT = .1f*5; // the radius of a joint relative to the head height
const FLOAT LINE_OCCUPANCY_DECAY = 40*.2f;                // the decay in occupancy of a line/limb as you move 
                                                          // away from the edge of the limb




#define NUM_ARM_LINE_SEGMENTS 4


// ---------------------------------------------------------------------------
// Name: CHeadShoulderStabilizer
// Desc: stabalizes the head and shoulders in the presence of self-occlusions
// ----------------------------------------------------------------------------------
class CHeadShoulderStabalizer
{

public:

    // ---------------------------------------------------------------------------
    // Name: StabalizeHeadShoulderJoints
    // Desc: stabilizes the head and shoulders in the given skeleton in the presence of self-occlusions
    //       the head and shoulder joints are modified and their tracking state is set to NUI_SKELETON_POSITION_INFERRED
    // 
    // ----------------------------------------------------------------------------------
    void StabalizeHeadShoulderJoints(const NUI_SKELETON_DATA* pSkeletonIn, NUI_SKELETON_DATA *pSkeletonOut  )
    {
        PIXBeginNamedEvent( 0, __FUNCTION__ );

        *pSkeletonOut = *pSkeletonIn;

        if ( pSkeletonIn->eTrackingState != NUI_SKELETON_TRACKED  )  
        {  
            m_HeadShoulderFrame.Reset();
            return;
        }

        if ( pSkeletonIn->dwEnrollmentIndex != m_dwEnrollmentIndex ||
             pSkeletonIn->dwTrackingID != m_dwTrackingID  ||
             pSkeletonIn->dwUserIndex !=  m_dwUserIndex )
        {
            m_HeadShoulderFrame.Reset();
            m_dwEnrollmentIndex = pSkeletonIn->dwEnrollmentIndex;
            m_dwTrackingID = pSkeletonIn->dwTrackingID;
            m_dwUserIndex = pSkeletonIn->dwUserIndex;
        }

        // copy over head and shoulders	
        m_HeadShoulderPointsAndArms[CHeadShoulderFrame::JOINT_IDS_TOP_HEAD]  =( pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_HEAD] );
        m_HeadShoulderPointsAndArms[CHeadShoulderFrame::JOINT_IDS_SHOULDER_CENTER] = (pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER]);
        m_HeadShoulderPointsAndArms[CHeadShoulderFrame::JOINT_IDS_SHOULDER_LEFT] = (pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT]);
        m_HeadShoulderPointsAndArms[CHeadShoulderFrame::JOINT_IDS_SHOULDER_RIGHT] = (pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT]);

        m_HeadShoulderPointsAndArms[CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS] = (pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT]);
        m_HeadShoulderPointsAndArms[CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS+1] = (pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT]);
        m_HeadShoulderPointsAndArms[CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS+2] = (pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT]);
        m_HeadShoulderPointsAndArms[CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS+3] = (pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT]);
        INT numheadShoulderPoints = CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS+3;

        INT bothArmsLines[NUM_ARM_LINE_SEGMENTS][2] = { 
            {CHeadShoulderFrame::JOINT_IDS_SHOULDER_RIGHT, CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS},
            { CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS, CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS+1},
            {CHeadShoulderFrame::JOINT_IDS_SHOULDER_LEFT, CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS + 2},
            { CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS + 2, CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS+3} 
        };

        INT linesThatOccludeLeftShoulder[NUM_ARM_LINE_SEGMENTS-1][2] = {
            {CHeadShoulderFrame::JOINT_IDS_SHOULDER_RIGHT, CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS},
            { CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS, CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS+1},
            { CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS + 2, CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS+3} 
        };

        INT linesThatOccludeRightShoulder[NUM_ARM_LINE_SEGMENTS-1][2] = {
            { CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS, CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS+1},
            {CHeadShoulderFrame::JOINT_IDS_SHOULDER_LEFT, CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS + 2},
            { CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS + 2, CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS+3} 
        };

        // compute the degree to which each joint is self occluded if the joint is tracked
        if ( pSkeletonIn->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HEAD] == NUI_SKELETON_TRACKED )
        {
            m_DegreeOfOcclusion[ CHeadShoulderFrame::JOINT_IDS_TOP_HEAD] =            
               DegreeOfOcclusion(CHeadShoulderFrame::JOINT_IDS_TOP_HEAD, m_HeadShoulderPointsAndArms, numheadShoulderPoints,bothArmsLines, 4 ) ;
        }
        else
        {
            m_DegreeOfOcclusion[ CHeadShoulderFrame::JOINT_IDS_TOP_HEAD] = 1.0f;
        }

        if ( pSkeletonIn->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_CENTER] == NUI_SKELETON_TRACKED )
        {
            m_DegreeOfOcclusion[CHeadShoulderFrame::JOINT_IDS_SHOULDER_CENTER]=
               DegreeOfOcclusion(CHeadShoulderFrame::JOINT_IDS_SHOULDER_CENTER, m_HeadShoulderPointsAndArms, numheadShoulderPoints ,bothArmsLines, 4 );
        }
        else
        {
            m_DegreeOfOcclusion[CHeadShoulderFrame::JOINT_IDS_SHOULDER_CENTER] = 1.0f;    
        }

        if ( pSkeletonIn->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_LEFT] == NUI_SKELETON_TRACKED )
        {
            m_DegreeOfOcclusion[ CHeadShoulderFrame::JOINT_IDS_SHOULDER_LEFT]=
               DegreeOfOcclusion(CHeadShoulderFrame::JOINT_IDS_SHOULDER_LEFT, m_HeadShoulderPointsAndArms,numheadShoulderPoints ,linesThatOccludeLeftShoulder, 3 ) ;
        }
        else
        {
            m_DegreeOfOcclusion[ CHeadShoulderFrame::JOINT_IDS_SHOULDER_LEFT] = 1.0f;
        }

        if ( pSkeletonIn->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_RIGHT] == NUI_SKELETON_TRACKED )
        {
            m_DegreeOfOcclusion[ CHeadShoulderFrame::JOINT_IDS_SHOULDER_RIGHT]=   
               DegreeOfOcclusion(CHeadShoulderFrame::JOINT_IDS_SHOULDER_RIGHT, m_HeadShoulderPointsAndArms, numheadShoulderPoints,linesThatOccludeRightShoulder, 3 ) ;
        }
        else
        {
            m_DegreeOfOcclusion[ CHeadShoulderFrame::JOINT_IDS_SHOULDER_RIGHT] = 1.0f;
        }

        // update head and shoulder frame based on observed joints and degree of self-occlusion
        UpdateHeadShoulderFrame( m_DegreeOfOcclusion, m_HeadShoulderPointsAndArms );

        // copy back results
        (pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_HEAD] ) = m_HeadShoulderFrame.GetPointInWorld(  CHeadShoulderFrame::JOINT_IDS_TOP_HEAD );
        (pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER]) = m_HeadShoulderFrame.GetPointInWorld(CHeadShoulderFrame::JOINT_IDS_SHOULDER_CENTER) ;
        (pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT]) = m_HeadShoulderFrame.GetPointInWorld(CHeadShoulderFrame::JOINT_IDS_SHOULDER_LEFT);
        (pSkeletonOut->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT]) = m_HeadShoulderFrame.GetPointInWorld(CHeadShoulderFrame::JOINT_IDS_SHOULDER_RIGHT) ;

        pSkeletonOut->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HEAD] = NUI_SKELETON_POSITION_INFERRED;
        pSkeletonOut->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_CENTER] = NUI_SKELETON_POSITION_INFERRED;
        pSkeletonOut->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_LEFT] = NUI_SKELETON_POSITION_INFERRED;
        pSkeletonOut->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_RIGHT] = NUI_SKELETON_POSITION_INFERRED;

        PIXEndNamedEvent();

    }


    // ---------------------------------------------------------------------------
    // Name: DegreeOfOcclusion
    // Desc: returns the degree of self occlusion ( between 0 and 1, where 0 is unoccluded and 1 is 
    //       fully occluded )  for the speficied joint.
    // ----------------------------------------------------------------------------------
    FLOAT DegreeOfOcclusion(CHeadShoulderFrame::JOINT_IDS joint) const
    {
        return m_DegreeOfOcclusion[ joint ];
    }


private:


    // ---------------------------------------------------------------------------
    // Name: PointLineSegmentDistance
    // Desc: returns the distance of the point pt, to the line segment between points linePtA and linePtB
    // ----------------------------------------------------------------------------------
    static inline
        FLOAT PointLineSegmentDistance( XMVECTOR pt,
        XMVECTOR linePtA,
        XMVECTOR linePtB )
    {
        FLOAT lineMag2 = XMVectorGetX(XMVector2LengthSq(  linePtA- linePtB));
        FLOAT U = XMVectorGetX(XMVector2Dot( pt-linePtA,    linePtB-linePtA)) / (lineMag2);

        if( U < 0.0f  ) return XMVectorGetX(XMVector2Length( pt-linePtA));
        if( U > 1.0f  ) return XMVectorGetX(XMVector2Length( pt-linePtB));

        XMVECTOR intersection = linePtA + ( linePtB-linePtA) * U;

        return XMVectorGetX(XMVector2Length(pt-intersection));
    }


    // ---------------------------------------------------------------------------
    // Name: DegreeOfOcclusion  
    // Desc: returns the degree of occluison of the joint idx in frameOfJoints with
    //       the other joints in that array and the line segments formed by lines.
    //       (where lines[i][0], and lines[i][1], are indices into frameOfJoints for the 
    //        i-th line segment )
    // ----------------------------------------------------------------------------------
    FLOAT DegreeOfOcclusion( INT idx, const XMVECTOR frameOfJoints[], INT numJoints ,
        const INT lines[][2], INT numLines )
    {
        assert(numLines > 0);

        // project
        XMVECTOR pointOfInterest;

        pointOfInterest = Project( frameOfJoints[idx] );

        FLOAT occScore =0;

        FLOAT jointWidth = m_HeadShoulderFrame.HeadHeight() * JOINT_RADIUS_RELATIVE_TO_HEADHEIGHT;       

        FLOAT limbWidth = m_HeadShoulderFrame.HeadHeight() * LIMB_WIDTH_RELATIVE_TO_HEADHEIGHT;       

        // find occlusion scores based on joint positions
        for( INT i = 0; i < numJoints; ++i )
        {
            if ( idx == i ) continue;

            XMVECTOR point, pointOffX, pointOffY;

            // if point is in front of this joint then consider it for occlusion
            if ( XMVectorGetZ( frameOfJoints[i] ) < XMVectorGetZ( frameOfJoints[idx] ) )
            {

                // find size of joint in image
                point = Project( frameOfJoints[i] );
                pointOffX = Project( frameOfJoints[i] + XMVectorSet( jointWidth , 0,0,0 ) );
                pointOffY = Project ( frameOfJoints[i] + XMVectorSet( 0, jointWidth,0,0 ) );

                FLOAT r1 = XMVectorGetX(XMVector2Length(pointOffX-point));
                FLOAT r2 = XMVectorGetX(XMVector2Length(pointOffY-point));

                FLOAT rad = max(r1,r2);
                FLOAT dist = XMVectorGetX(XMVector2Length( pointOfInterest-point ));

                FLOAT occScoreJoint = expf( - ( ( dist*dist )/ ( rad*rad + EPSILON ) )* JOINT_OCCUPANCY_DECAY  );

                // keep maximum occlusion score
                if ( occScore <  occScoreJoint ) occScore =  occScoreJoint;
            }
        }

        // find occlusion score based on line segments
        for( INT i = 0; i < numLines; ++i )
        {
            XMVECTOR point, pointOffX, pointOffY;

            INT ptA = lines[i][0];
            INT ptB = lines[i][1];

            // find size in of limb in image
            point = Project( frameOfJoints[ptA] );
            pointOffX = Project( frameOfJoints[ptA]+ XMVectorSet( limbWidth,0,0,0 ) );
            pointOffY = Project( frameOfJoints[ptA]+ XMVectorSet( 0, limbWidth,0,0 ) );

            FLOAT r1 = XMVectorGetX(XMVector2Length(pointOffX-point));
            FLOAT r2 = XMVectorGetX(XMVector2Length(pointOffY-point));

            FLOAT rad = max(r1,r2);
            XMVECTOR pointB;

            pointB = Project( frameOfJoints[ptB] );
            FLOAT dist = PointLineSegmentDistance( pointOfInterest, point, pointB );

            FLOAT occScoreJoint =0;

            if ( dist < rad ) 
            {
                // joint is occluded by limb if it is within rad of that limb
                occScoreJoint = 1;
            }
            else
            {
                // occlusion score rolls off as you move away from limb
                occScoreJoint = expf( - (dist-rad  )*( dist-rad ) /(  rad*rad + EPSILON) * LINE_OCCUPANCY_DECAY  );
            }

            // keep maximum occlusion score
            if ( occScore <  occScoreJoint ) occScore =  occScoreJoint;
        }

        return occScore;
    }


    // ---------------------------------------------------------------------------
    // Name: UpdateHeadShoulderFrame
    // Desc: updates the head shoulder frame based on the joints
    // ----------------------------------------------------------------------------------
    void UpdateHeadShoulderFrame( const FLOAT occScore[NUM_HEAD_SHOULDER_JOINTS],
        const XMVECTOR frameOfJoints[NUM_HEAD_SHOULDER_JOINTS] )
    {
        m_HeadShoulderFrame.UpdateHeadShoulderFrame( frameOfJoints, occScore );
    }

    FLOAT m_DegreeOfOcclusion[NUM_HEAD_SHOULDER_JOINTS];
    CHeadShoulderFrame m_HeadShoulderFrame;

    // skeleton id's
    DWORD m_dwTrackingID;
    DWORD m_dwEnrollmentIndex;
    DWORD m_dwUserIndex;

    // space for the head shoulder joints and the elbow and the hands
    XMVECTOR m_HeadShoulderPointsAndArms[CHeadShoulderFrame::JOINT_IDS_MAX_NUM_JOINTS + NUM_ARM_LINE_SEGMENTS];

};