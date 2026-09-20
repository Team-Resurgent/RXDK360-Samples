
#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif

#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

#pragma warning(disable : 4793)
#include "MuscleTracking.h"

namespace ATGGestureDetector
{

#ifdef MUSCLE_DEBUG
#define MUSCLE_ASSERT(stmt)	{if(!(stmt)){OutputDebugStringA("Assert Failed: " #stmt);int*bad_ptr=NULL;*bad_ptr=0xBAADF00D;}}
#else
#define	MUSCLE_ASSERT(stmt)
#endif
#define MUSCLE_TODO()	MUSCLE_ASSERT("MUSCLE_TODO"&&false)



// Implementation:

typedef NUI_SKELETON_POSITION_INDEX		MINDEX;
#define MCOUNT							NUI_SKELETON_POSITION_COUNT
#define GRAVITY							(-9.8f)


#define FAKE_JOINT_HIP_FORWARD			((NUI_SKELETON_POSITION_INDEX)(NUI_SKELETON_POSITION_COUNT+1))
#define FAKE_JOINT_SHOULDER_FORWARD		((NUI_SKELETON_POSITION_INDEX)(NUI_SKELETON_POSITION_COUNT+2))

/************************************************************************/
// The density estimate of each segment is based off of 
// standard tables (http://www.univie.ac.at/cga/teach-in/inverse-dynamics.html)
// and by measuring my body (Rahul Agarwal) as seen by the Natal.
// Note that for some segments (such as head, feet), the Natal segments are
// shorter than actuall body segments
/************************************************************************/
const MUSCLE_DESC		GMuscleDescs[ MCOUNT ] = {

 { NUI_SKELETON_POSITION_HIP_CENTER,		NUI_SKELETON_POSITION_SPINE,			FAKE_JOINT_HIP_FORWARD,					NUI_SKELETON_POSITION_HIP_CENTER,		5.61f },
 { NUI_SKELETON_POSITION_SPINE,				NUI_SKELETON_POSITION_HIP_CENTER,		FAKE_JOINT_HIP_FORWARD,					NUI_SKELETON_POSITION_HIP_CENTER,		5.61f},
 { NUI_SKELETON_POSITION_SHOULDER_CENTER,	NUI_SKELETON_POSITION_SPINE,			FAKE_JOINT_HIP_FORWARD,					NUI_SKELETON_POSITION_SPINE,			30.61f },
 { NUI_SKELETON_POSITION_HEAD,				NUI_SKELETON_POSITION_SHOULDER_CENTER,	FAKE_JOINT_SHOULDER_FORWARD,			NUI_SKELETON_POSITION_SHOULDER_CENTER,	29.87f},
 { NUI_SKELETON_POSITION_SHOULDER_LEFT,		NUI_SKELETON_POSITION_SHOULDER_CENTER,	NUI_SKELETON_POSITION_HIP_CENTER,		NUI_SKELETON_POSITION_SHOULDER_CENTER,	35.61f },
 { NUI_SKELETON_POSITION_ELBOW_LEFT,		NUI_SKELETON_POSITION_SHOULDER_LEFT,	NUI_SKELETON_POSITION_SHOULDER_CENTER,	NUI_SKELETON_POSITION_SHOULDER_LEFT,	7.23f},
 { NUI_SKELETON_POSITION_WRIST_LEFT,		NUI_SKELETON_POSITION_ELBOW_LEFT,		NUI_SKELETON_POSITION_SHOULDER_LEFT,	NUI_SKELETON_POSITION_ELBOW_LEFT,		6.66f },
 { NUI_SKELETON_POSITION_HAND_LEFT,			NUI_SKELETON_POSITION_WRIST_LEFT,		NUI_SKELETON_POSITION_ELBOW_LEFT,		NUI_SKELETON_POSITION_WRIST_LEFT,		1.91f },
 { NUI_SKELETON_POSITION_SHOULDER_RIGHT,	NUI_SKELETON_POSITION_SHOULDER_CENTER,	NUI_SKELETON_POSITION_HIP_CENTER,		NUI_SKELETON_POSITION_SHOULDER_CENTER,	35.61f },
 { NUI_SKELETON_POSITION_ELBOW_RIGHT,		NUI_SKELETON_POSITION_SHOULDER_RIGHT,	NUI_SKELETON_POSITION_SHOULDER_CENTER,	NUI_SKELETON_POSITION_SHOULDER_RIGHT,	7.23f},
 { NUI_SKELETON_POSITION_WRIST_RIGHT,		NUI_SKELETON_POSITION_ELBOW_RIGHT,		NUI_SKELETON_POSITION_SHOULDER_RIGHT,	NUI_SKELETON_POSITION_ELBOW_RIGHT,		6.66f },
 { NUI_SKELETON_POSITION_HAND_RIGHT,		NUI_SKELETON_POSITION_WRIST_RIGHT,		NUI_SKELETON_POSITION_ELBOW_RIGHT,		NUI_SKELETON_POSITION_WRIST_RIGHT,		2.91f },
 { NUI_SKELETON_POSITION_HIP_LEFT,			NUI_SKELETON_POSITION_HIP_CENTER,		NUI_SKELETON_POSITION_SPINE,			NUI_SKELETON_POSITION_HIP_CENTER,		35.61f},
 { NUI_SKELETON_POSITION_KNEE_LEFT,			NUI_SKELETON_POSITION_HIP_LEFT,			NUI_SKELETON_POSITION_HIP_CENTER,		NUI_SKELETON_POSITION_HIP_LEFT,			16.61f },
 { NUI_SKELETON_POSITION_ANKLE_LEFT,		NUI_SKELETON_POSITION_KNEE_LEFT,		NUI_SKELETON_POSITION_HIP_LEFT,			NUI_SKELETON_POSITION_KNEE_LEFT,		5.055f },
 { NUI_SKELETON_POSITION_FOOT_LEFT,			NUI_SKELETON_POSITION_ANKLE_LEFT,		NUI_SKELETON_POSITION_KNEE_LEFT,		NUI_SKELETON_POSITION_ANKLE_LEFT,		8.33f },
 { NUI_SKELETON_POSITION_HIP_RIGHT,			NUI_SKELETON_POSITION_HIP_CENTER,		NUI_SKELETON_POSITION_SPINE,			NUI_SKELETON_POSITION_HIP_CENTER,		35.61f},
 { NUI_SKELETON_POSITION_KNEE_RIGHT,		NUI_SKELETON_POSITION_HIP_RIGHT,		NUI_SKELETON_POSITION_HIP_CENTER,		NUI_SKELETON_POSITION_HIP_RIGHT,		16.61f},
 { NUI_SKELETON_POSITION_ANKLE_RIGHT,		NUI_SKELETON_POSITION_KNEE_RIGHT,		NUI_SKELETON_POSITION_HIP_RIGHT,		NUI_SKELETON_POSITION_KNEE_RIGHT,		5.055f },
 { NUI_SKELETON_POSITION_FOOT_RIGHT,		NUI_SKELETON_POSITION_ANKLE_RIGHT,		NUI_SKELETON_POSITION_KNEE_RIGHT,		NUI_SKELETON_POSITION_ANKLE_RIGHT,		8.33f },
};

const MUSCLE_DESC* MuscleDescriptions()
{
	return GMuscleDescs;
}


class MuscleCalculator
{
private:
	const XMVECTOR*				m_pJoints;
	const NUI_SKELETON_POSITION_TRACKING_STATE*	m_pStates;
	const MUSCLE_FRAME*			m_pPrevious;
	float					    m_fDeltaSeconds;
	MUSCLE_FRAME*				m_pResult;
	float						m_fLeftRightScale;
	float						m_fGlobalForceScale;
	bool						m_bIsFirstFrame;
public:
	MuscleCalculator(
		const XMVECTOR*				pJoints,
		const NUI_SKELETON_POSITION_TRACKING_STATE*	pStates,
		const MUSCLE_FRAME*			pPrevious,
		const float					deltaSeconds,
		MUSCLE_FRAME*			pResult,
		bool					firstFrame ): 
		m_pJoints( pJoints ),
		m_pStates( pStates ),
		m_pPrevious( pPrevious ),
		m_fDeltaSeconds(deltaSeconds ),
		m_pResult( pResult ),
		m_bIsFirstFrame (firstFrame)
	{ 
	};

	~MuscleCalculator()
	{
	}

private:

	
	//************************************
	// Method:    readJointPosition
	// This method returns the position of joint IND by looking it up in pJoints.
	// It handles the case of looking for the joints FAKE_JOINT_SHOULDER_FORWARD, 
	// and FAKE_JOINT_HIP_FORWARD, which are computed as they are not actually stored anywhere
	//************************************
	XMVECTOR readJointPosition( const XMVECTOR* pJoints, int ind )
	{
		if ( ind < NUI_SKELETON_POSITION_COUNT )
		{
			return pJoints[ ind ];
		}
		
		//Optional: Makes lower body torques more stable
		if(ind == NUI_SKELETON_POSITION_FOOT_LEFT)
			return m_pJoints[NUI_SKELETON_POSITION_ANKLE_LEFT] + XMVectorSet(0, 0, -0.1f, 0.0f);
		if(ind == NUI_SKELETON_POSITION_FOOT_RIGHT)
			return m_pJoints[NUI_SKELETON_POSITION_ANKLE_RIGHT] + XMVectorSet(0, 0, -0.1f, 0.0f);
		
		static float forwardOffset = 1.0f;
		XMVECTOR across = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), center = pJoints[NUI_SKELETON_POSITION_HIP_CENTER];
		if ( ind == FAKE_JOINT_HIP_FORWARD )
		{
			across = pJoints[NUI_SKELETON_POSITION_HIP_RIGHT] - pJoints[NUI_SKELETON_POSITION_HIP_LEFT];
			center = pJoints[NUI_SKELETON_POSITION_HIP_CENTER];
		}
		else if ( ind == FAKE_JOINT_SHOULDER_FORWARD )
		{
			across = pJoints[NUI_SKELETON_POSITION_SHOULDER_RIGHT] - pJoints[NUI_SKELETON_POSITION_SHOULDER_LEFT];
			center = pJoints[NUI_SKELETON_POSITION_SHOULDER_CENTER];
		}
		
		XMVECTOR trunk = readJointPosition(pJoints, NUI_SKELETON_POSITION_SHOULDER_CENTER) - readJointPosition(pJoints, NUI_SKELETON_POSITION_HIP_CENTER);
		XMVECTOR forward = -XMVector3Cross(trunk,across);
		return ( forwardOffset * XMVector3NormalizeEst(forward) ) + center;
		
		
	}


	//************************************
	// Method:    IsLowerBody
	// Used during force normalization.
	//************************************
	static bool IsLowerBody( int joint )
	{
		switch ( joint )
		{
			case NUI_SKELETON_POSITION_HIP_LEFT:
			case NUI_SKELETON_POSITION_HIP_RIGHT:
			case NUI_SKELETON_POSITION_KNEE_LEFT:
			case NUI_SKELETON_POSITION_KNEE_RIGHT:
			case NUI_SKELETON_POSITION_ANKLE_LEFT:
			case NUI_SKELETON_POSITION_ANKLE_RIGHT:
			case NUI_SKELETON_POSITION_FOOT_LEFT:
			case NUI_SKELETON_POSITION_FOOT_RIGHT:
				return true;
			default:
				return false;
		};
	}

	//************************************
	// Method:    CalculateSensorToMuscleMatrix
	// Calculates the transformation matrix from
	// sensor space to muscle space for a given muscle. 
	// There are many special cases to help improve stability
	// (ex. cases where all 3 joints become colinear) and 
	// to conform to conventions. 
	//
	// The X direction in muscle space corresponds to "pull" muscle action
	// The Y direction in muscle space corresponds to "twist" muscle action
	// This follows the convention that bring the endpoint forward or out of
	// the body is an action in the positive direction. This is enforced by
	// flipping the axes of certain segments
	//************************************
	XMMATRIX CalculateSensorToMuscleMatrix(MUSCLE_DESC desc)
	{
		XMVECTOR endPos = readJointPosition( m_pJoints, desc.EndPoint );
		XMVECTOR inflection = readJointPosition( m_pJoints, desc.Inflection );
		XMVECTOR axisJoint = readJointPosition( m_pJoints, desc.Axis );
		
		XMVECTOR yDir = axisJoint - inflection;
		XMVECTOR xDir = XMVector3Cross(endPos - inflection, yDir);
		XMVECTOR other = XMVectorZero();
	
		switch(desc.EndPoint){
			case NUI_SKELETON_POSITION_ELBOW_RIGHT:
				yDir *= -1;
			case NUI_SKELETON_POSITION_ELBOW_LEFT:
				other = readJointPosition( m_pJoints, NUI_SKELETON_POSITION_HIP_CENTER);
				xDir = XMVector3Cross(XMVector3Normalize(inflection - axisJoint), XMVector3Normalize(axisJoint - other));
				break;
			case NUI_SKELETON_POSITION_SHOULDER_RIGHT:
				yDir *= -1;
			case NUI_SKELETON_POSITION_SHOULDER_LEFT:
				xDir *= -1;
				break;
			case NUI_SKELETON_POSITION_ANKLE_LEFT:
				yDir *= -1;
			case NUI_SKELETON_POSITION_ANKLE_RIGHT:
				//If the segments are almost collinear, use a reference segment to compute the matrix
				if( (XM_PI - XMVectorGetX(XMVector3AngleBetweenVectors(axisJoint - inflection, endPos - inflection))) < XM_PI / 8 ){
					other = readJointPosition( m_pJoints, FAKE_JOINT_HIP_FORWARD) - readJointPosition( m_pJoints, NUI_SKELETON_POSITION_HIP_CENTER);
					xDir = XMVector3Cross(inflection - axisJoint, other);
				}
				else {
					xDir *= -1;
				}			
				break;
			case NUI_SKELETON_POSITION_KNEE_LEFT:
			case NUI_SKELETON_POSITION_KNEE_RIGHT:
				//xDir *= -1;
				//other = XMVector3Normalize(inflection - endPos);
				//yDir = yDir - XMVector3Dot(yDir, other)  * other;
				//if( (XM_PI - XMVectorGetX(XMVector3AngleBetweenVectors(axisJoint - inflection, endPos - inflection))) < XM_PI / 8 )
				yDir = XMVector3Cross(endPos - inflection, xDir);
				other = readJointPosition(m_pJoints, NUI_SKELETON_POSITION_SHOULDER_CENTER) - readJointPosition(m_pJoints, NUI_SKELETON_POSITION_HIP_CENTER);
				xDir = XMVector3Cross(inflection - axisJoint, other);
				
				if( (XM_PI - XMVectorGetX(XMVector3AngleBetweenVectors(endPos - inflection, other))) < XM_PI / 8 ){
					yDir = XMVector3Cross(endPos - inflection, xDir);
					if( desc.EndPoint == NUI_SKELETON_POSITION_KNEE_RIGHT)
						yDir *= -1;
				}
				else
					yDir = XMVector3Cross(endPos - inflection, other);
				break;
			case NUI_SKELETON_POSITION_FOOT_RIGHT:
				yDir *= -1;
			case NUI_SKELETON_POSITION_FOOT_LEFT:
				xDir *= -1;
				if(XMVector3Equal(xDir,XMVectorZero()))
				{
					other = readJointPosition( m_pJoints, NUI_SKELETON_POSITION_HIP_CENTER );
					XMVECTOR other2 = readJointPosition( m_pJoints, FAKE_JOINT_HIP_FORWARD );
					xDir = XMVector3Cross( other2-other, axisJoint - inflection );
				}
				break;
			case NUI_SKELETON_POSITION_WRIST_RIGHT:
				yDir *= -1.0f;
				break;
		}
		XMVECTOR finalDir = XMVector3Cross( xDir, yDir );
		if(XMVector3Equal(yDir,XMVectorZero()) || XMVectorGetX(XMVector3LengthSq(xDir)) < 0.00000001f)
			return XMMatrixIdentity();
		return XMMatrixLookToRH( XMVectorZero(), -finalDir, yDir );
	}

	static void AddToExternalCOM( MUSCLE_DATA* pInto, XMVECTOR pos, XMVECTOR vel, float mass )
	{
		if ( mass != 0.0f )
		{
			float totalWeight = pInto->mLimbMassExternal + mass;
			pInto->mLimbExternalCOM = ( 
				  ( pInto->mLimbExternalCOM * pInto->mLimbMassExternal )
				+ ( pos * mass ) ) / totalWeight;
			pInto->mLimbExternalCOMVelocity = ( 
				  ( pInto->mLimbExternalCOMVelocity * pInto->mLimbMassExternal )
				+ ( vel * mass ) ) / totalWeight;
			pInto->mLimbMassExternal = totalWeight;
		}
	}

	
	
	//************************************
	// Method:    ProcessGeneral
	//		This method does all the "heavy lifting" of computing the
	//		torques and forces for each muscle
	// Parameter: bool propagateExternalForce 
	//		This parameter is used to avoid propagating 
	//		ground reaction forces to the hips, 
	//		in order to make the annotation look better			
	//************************************
	void ProcessGeneral( int mi, bool propagateExternalForce = true)
	{
		MUSCLE_DATA* pMD = &( m_pResult->Muscles[ mi ] );
		const MUSCLE_DATA* pPM = &( m_pPrevious->Muscles[ mi ] );

		MUSCLE_DESC desc = GMuscleDescs[ mi ];
		MUSCLE_ASSERT( desc.EndPoint == mi ); // ensure that the joints are indexed by their end-point
		XMVECTOR endPos = readJointPosition( m_pJoints, mi );
		XMVECTOR inflection = readJointPosition( m_pJoints, desc.Inflection );
		XMVECTOR axisJoint = readJointPosition( m_pJoints, desc.Axis );
		if ( ( m_pStates != NULL ) && ( m_pStates[ mi ] == NUI_SKELETON_POSITION_NOT_TRACKED ) )
		{
#ifdef _XBOX
            XMemSet( pMD, 0, sizeof( *pMD ) );
#else
			memset( pMD, 0, sizeof( *pMD ) );
#endif
			pMD->mJointResolutionRadius = 4.0f; // somewhere within the sensor radius, 4 meters
			return;
		}
		XMVECTOR r = endPos - inflection;

		if ( ( mi == NUI_SKELETON_POSITION_SHOULDER_LEFT ) || ( mi == NUI_SKELETON_POSITION_SHOULDER_RIGHT ) )
		{
			// For the shoulders, simulate the effect of the muscles which hold up the shoulder bone by lowering the distance
			//	from the clavical to the shoulder:
			r *= 0.3f;
		}
		
		// Calculate velocity and acceleration:
		const float fullyTrackedRadius = 0.05f;
		bool isFullyTracked = m_pStates ? ( m_pStates[ mi ] == NUI_SKELETON_POSITION_TRACKED ) : true;
		bool wasFullyTracked = ( pPM->mJointResolutionRadius == fullyTrackedRadius );
		pMD->mJointPosition = endPos;
		pMD->mJointResolutionRadius = isFullyTracked ? fullyTrackedRadius : 0.2f; // noise radius, default is 5 cm
		UpdateLinearVelocityAndAcceleration(pMD, pPM);

		//Update angular quantities
		pMD->mLimbAngularAcceleration = XMVector3Cross(0.5 * r, pMD->mLimbLinearAcceleration );
		pMD->mLimbAngularVelocity = XMVector3Cross(0.5 * r, pMD->mLimbLinearVelocity);
		pMD->mLimbAngularExtension = XMVectorGetX(XMVector3AngleBetweenVectors(endPos - inflection, axisJoint - inflection));
		pMD->mLimbLength = XMVectorGetX(XMVector3Length(r));	

		if ( !isFullyTracked || !wasFullyTracked )
		{
			static float reduceInfered = 0.0f;
			pMD->mLimbLinearVelocity *= reduceInfered;
			pMD->mLimbLinearAcceleration *= reduceInfered;
			pMD->mLimbAngularVelocity *= reduceInfered;
			pMD->mLimbAngularAcceleration *= reduceInfered;
			pMD->mLimbTempJerk *= reduceInfered;
			pMD->mLimbTempAcceleration *= reduceInfered;
		}

		// Calculate muscle orientation:
		XMVECTOR det;
		XMMATRIX sensorToMuscle = CalculateSensorToMuscleMatrix(desc);
		XMMATRIX muscleToSensor = XMMatrixInverse( &det, sensorToMuscle );
		pMD->mMuscleToSensorX = muscleToSensor.r[0];
		pMD->mMuscleToSensorY = muscleToSensor.r[1];
		pMD->mMuscleToSensorZ = muscleToSensor.r[2];
		
		// Estimate mass:
		pMD->mLimbMassEstimate = XMVectorGetX( XMVector3Length( r ) ) * desc.Density;
		MUSCLE_ASSERT( pMD->mLimbMassEstimate != 0.0f );
		float momentOfInertiaEst = pMD->mLimbMassEstimate * 1.0f/3.0f * XMVectorGetX(XMVector3LengthSq( r )); //Moment of inertia of a thin rod around 1 end

		// Retrieve external forces:
		XMVECTOR externalForce = pMD->mDynamicExternalForce;
		XMVECTOR externalTorque = pMD->mDynamicExternalTorque;
		XMVECTOR weight = XMVectorSet(0, GRAVITY, 0, 0) * pMD->mLimbMassEstimate;
		float propagationDir = 1.0f;
		if ( IsLowerBody( mi ) && ( XMVectorGetY( externalForce ) < 0.0f ) )
		{
			propagationDir = -1.0f;
		}

		//Compute Torques 
		XMVECTOR netTorque = momentOfInertiaEst * pMD->mLimbAngularAcceleration;
		XMVECTOR gravityTorque = XMVector3Cross(0.5f * r, weight);
		XMVECTOR missingTorque = netTorque - gravityTorque - XMVector3Cross(r, externalForce) - externalTorque;
		pMD->mDynamicLocalTorqueBalance = missingTorque;
		pMD->mDynamicLocalTorqueMotion = XMVectorZero();

		//Static torques
		XMVECTOR missingStaticTorque = -gravityTorque - XMVector3Cross(r, pMD->mStaticExternalForce ) - pMD->mStaticExternalTorque;
		pMD->mStaticLocalTorqueBalance = missingStaticTorque;

		// Now figure out what the additional force must have been to account for the change in velocity of the center of mass:
		XMVECTOR dynLocalForce = ( pMD->mLimbLinearAcceleration * pMD->mLimbMassEstimate ) - ( weight * propagationDir ) - externalForce;
		XMVECTOR muscleStaticForce = -(weight + pMD->mStaticExternalForce);
	
		if(propagateExternalForce)
		{
			MUSCLE_DATA* pInto = &( m_pResult->Muscles[ desc.Inflection ] );
			pInto->mDynamicExternalForce += -dynLocalForce;
			pInto->mDynamicExternalTorque += -missingTorque;
			pInto->mStaticExternalTorque += -missingStaticTorque;
			pInto->mStaticExternalForce += -muscleStaticForce;
			AddToExternalCOM( pInto, pMD->mJointPosition + r*0.5f, pMD->mLimbLinearVelocity, pMD->mLimbMassEstimate );
			AddToExternalCOM( pInto, pMD->mLimbExternalCOM, pMD->mLimbExternalCOMVelocity, pMD->mLimbMassExternal );
		}

		// Transform the dynamic torque 
		pMD->mMuscleDynamicLocalTorque = XMVectorSet(
			XMVectorGetX( XMVector3Dot( missingTorque, pMD->mMuscleToSensorX ) ),
			XMVectorGetX( XMVector3Dot( missingTorque, pMD->mMuscleToSensorY ) ),
			XMVectorGetX( XMVector3Dot( missingTorque, pMD->mMuscleToSensorZ ) ), 0 );
	}

	//************************************
	// Method:    EstimateCenterOfMass
	// Calculates the center of mass. Makes the assumption
	// that the CoM for each segment is in its center.
	//************************************
	void EstimateCenterOfMass() 
	{
		m_pResult->CenterOfMass.mLimbMassEstimate = 0.0f;
		m_pResult->CenterOfMass.mJointPosition = XMVectorZero();
		for (int i = 0; i < MCOUNT ; i++)
		{
			MUSCLE_DESC desc = GMuscleDescs[ i];
			XMVECTOR endPos = readJointPosition( m_pJoints, desc.EndPoint );
			XMVECTOR inflection = readJointPosition( m_pJoints, desc.Inflection );
			float mass = desc.Density * XMVectorGetX(XMVector3Length(endPos-inflection));
			m_pResult->CenterOfMass.mLimbMassEstimate += mass;
			m_pResult->CenterOfMass.mJointPosition += mass * ( ( endPos + inflection ) * 0.5f );
		}
		
		if(m_pResult->CenterOfMass.mLimbMassEstimate == 0.0f)
			m_pResult->CenterOfMass.mJointPosition = XMVectorZero();
		else
			m_pResult->CenterOfMass.mJointPosition  /= m_pResult->CenterOfMass.mLimbMassEstimate;
		
		const MUSCLE_DATA* prevCM = m_bIsFirstFrame ? &m_pResult->CenterOfMass: &m_pPrevious->CenterOfMass;
		UpdateLinearVelocityAndAcceleration(&m_pResult->CenterOfMass, prevCM);
	}
	
	//************************************
	// These methods do a rough estimate of the forces on each foot by projecting the center of mass onto the 
	// X-Z plane. Note that the Z position is actually discarded, as the z-postion of the feet was found to be highly
	// unreliable (the Natal often made it appear as if the player was leaning forwards or backwards)
	//************************************
	
	void EstimateFootForces() 
	{
		float massEstimate = m_pResult->CenterOfMass.mLimbMassEstimate;
		XMVECTOR vDynFootForce = massEstimate * (XMVectorSet(0,-GRAVITY,0,0) - m_pResult->CenterOfMass.mLimbLinearAcceleration);
		XMVECTOR vStcFootForce = massEstimate * (XMVectorSet(0,-GRAVITY,0,0));

		m_pResult->CenterOfMass.mDynamicExternalForce = vDynFootForce;
		m_pResult->CenterOfMass.mStaticExternalForce = vStcFootForce;

		float weightNotFlying;
		float weightDistribution;
		EstimateWeightDistributionLR( &weightDistribution, &weightNotFlying );
		float weightDistributionLeft = weightDistribution * weightNotFlying;
		float weightDistributionRight = (1- weightDistribution) * weightNotFlying;
		const float minBodyPercentOnLegs = 0.618f; // even when there is no weight on a leg, use this amount of the mass of the body for normalization

		m_pResult->Muscles[NUI_SKELETON_POSITION_FOOT_LEFT].mDynamicExternalForce += weightDistributionLeft * vDynFootForce;
		m_pResult->Muscles[NUI_SKELETON_POSITION_FOOT_RIGHT].mDynamicExternalForce += weightDistributionRight * vDynFootForce;

		m_pResult->Muscles[NUI_SKELETON_POSITION_FOOT_LEFT].mStaticExternalForce +=  weightDistributionLeft * vStcFootForce;
		m_pResult->Muscles[NUI_SKELETON_POSITION_FOOT_RIGHT].mStaticExternalForce += weightDistributionRight * vStcFootForce;

		AddToExternalCOM( &( m_pResult->Muscles[NUI_SKELETON_POSITION_FOOT_LEFT] ),
			m_pResult->CenterOfMass.mJointPosition, m_pResult->CenterOfMass.mLimbLinearVelocity, 
			__max( weightDistributionLeft, minBodyPercentOnLegs ) * massEstimate );
		AddToExternalCOM( &( m_pResult->Muscles[NUI_SKELETON_POSITION_FOOT_RIGHT] ),
			m_pResult->CenterOfMass.mJointPosition, m_pResult->CenterOfMass.mLimbLinearVelocity, 
			__max( weightDistributionRight, minBodyPercentOnLegs ) * massEstimate );
	}

	void EstimateWeightDistributionLR( float* outPercentLeft, float* outPercentNotFlying ) 
	{
		//Set up a plane to minimize distortions
		XMVECTOR referenceAxis = XMVectorSet(1, 0 ,0 ,0);
		XMVECTOR referenceNormal  = XMVectorSet(0, 0, -1.0f, 0);
		referenceAxis = XMVectorSetY(referenceAxis, 0);
		referenceNormal = XMVectorSetY(referenceNormal, 0);

		XMVECTOR leftHeel = readJointPosition(m_pJoints, NUI_SKELETON_POSITION_ANKLE_LEFT);
		XMVECTOR rightHeel = readJointPosition(m_pJoints, NUI_SKELETON_POSITION_ANKLE_RIGHT);

		float distributionLeft;
		XMVECTOR vCMAccel = m_pResult->CenterOfMass.mLimbLinearAcceleration;
		XMVECTOR vFootDisp = leftHeel - rightHeel;
		XMVECTOR CMDisplacement = m_pResult->CenterOfMass.mJointPosition- rightHeel;
		float d_tot = fabs(XMVectorGetX(XMVector3Dot(vFootDisp,referenceAxis)));
		float d_r = max(-XMVectorGetX(XMVector3Dot(CMDisplacement, referenceAxis)), 0.0f);
		XMVECTOR projectedAccel = vCMAccel - XMVector3Dot(vCMAccel ,referenceNormal) * referenceNormal;
		XMVECTOR projectedCM = CMDisplacement - XMVector3Dot(CMDisplacement, referenceNormal) * referenceNormal;
		float netTorque  = XMVectorGetX(XMVector3Dot(XMVector3Cross(projectedCM, projectedAccel), referenceNormal));

		float force_left = (netTorque + d_r * -GRAVITY)/d_tot;
		distributionLeft = force_left / (XMVectorGetX(XMVector3Length(projectedAccel)) - GRAVITY);		
		distributionLeft = max(min(distributionLeft,1.0f),0.0f);

		// Special case: if one of the knees is bent a lot more than the other, then the weight
		//	of the body must be on the straighter knee.
		XMVECTOR refDown = XMVectorSet( 0, -1, 0, 0 );
		const float angleForBent = 3.14149f / 6.0f;
		float angleLeftDown = XMVectorGetX( XMVector3AngleBetweenVectors( refDown, 
			readJointPosition( m_pJoints, NUI_SKELETON_POSITION_KNEE_LEFT ) -
			readJointPosition( m_pJoints, NUI_SKELETON_POSITION_HIP_LEFT ) ) );
		float angleRightDown = XMVectorGetX( XMVector3AngleBetweenVectors( refDown, 
			readJointPosition( m_pJoints, NUI_SKELETON_POSITION_KNEE_RIGHT ) -
			readJointPosition( m_pJoints, NUI_SKELETON_POSITION_HIP_RIGHT ) ) );
		if ( ( angleRightDown - angleLeftDown ) > angleForBent )
			distributionLeft = 1.0f;
		if ( ( angleLeftDown - angleRightDown ) > angleForBent )
			distributionLeft = 0.0f;

		*outPercentLeft = distributionLeft;
		*outPercentNotFlying = 1.0f;
	}

	//************************************
	// Method: UpdateLinearVelocityAndAcceleration
	// This function uses a Newmark multivalue integration scheme 
	// that is modified to apply simultaneous differentiation and smoothing
	// based on this paper: http://www.springerlink.com/content/90134383w2027768/
	// The method is applied twice, once to get accurate velocity, and once to 
	// get accurate accelerations. The "jerk" is the term for the 3rd derivative
	// or position with respect to time
	//************************************
	void UpdateLinearVelocityAndAcceleration( MUSCLE_DATA* curFrame, const MUSCLE_DATA* prevFrame) 
	{
		static float beta1 = 100.0f;
		static float gamma1 = 20.0f;
		static float beta2 = 6.0f;
		static float gamma2 = 2.0f;

		float beta1DtLimit = sqrt( beta1 ) / (1.3f * 30.0f);
		float gama1DtLimit = beta1 / (1.5f * 30.0f*gamma1);
		float dt1Limit = min( beta1DtLimit, gama1DtLimit );
		float fDeltaSeconds1Limitted = min( m_fDeltaSeconds, dt1Limit );

		XMVECTOR vInstantaneousLinearVelocity = ( curFrame->mJointPosition - prevFrame->mJointPosition ) / m_fDeltaSeconds;
		curFrame->mLimbTempAcceleration = prevFrame->mLimbTempAcceleration + ( fDeltaSeconds1Limitted * 30.0f / beta1 ) * ( 30.0f * ( vInstantaneousLinearVelocity - prevFrame->mLimbLinearVelocity ) - ( prevFrame->mLimbTempAcceleration * 0.5f ) );
		XMVECTOR vIntermediateJerk = (curFrame->mLimbTempAcceleration - prevFrame->mLimbTempAcceleration ) / fDeltaSeconds1Limitted;
		curFrame->mLimbLinearVelocity = prevFrame->mLimbLinearVelocity + fDeltaSeconds1Limitted * ( curFrame->mLimbTempAcceleration + ( gamma1 - 1.0f ) * vIntermediateJerk / 30.0f );

		float beta2DtLimit = sqrt( beta2 ) / ( 1.3f * 30.0f );
		float gama2DtLimit = beta2 / ( 1.5f * 30.0f * gamma2 );
		float dt2Limit = min( beta2DtLimit, gama2DtLimit );
		float fDeltaSeconds2Limitted = min( m_fDeltaSeconds, dt2Limit );

		XMVECTOR vLinearVelocityDerivative = ( curFrame->mLimbLinearVelocity - prevFrame->mLimbLinearVelocity ) / m_fDeltaSeconds;
		curFrame->mLimbTempJerk = prevFrame->mLimbTempJerk + ( fDeltaSeconds2Limitted * 30.0f / beta2 ) * ( 30.0f * ( vLinearVelocityDerivative - prevFrame->mLimbLinearAcceleration ) - ( prevFrame->mLimbTempJerk * 0.5f ) );
		XMVECTOR vLinearJerkDerivative = ( curFrame->mLimbTempJerk - prevFrame->mLimbTempJerk ) / fDeltaSeconds2Limitted;
		curFrame->mLimbLinearAcceleration = prevFrame->mLimbLinearAcceleration + fDeltaSeconds2Limitted * ( curFrame->mLimbTempJerk + ( gamma2 - 1.0f ) * vLinearJerkDerivative / 30.0f );
	}

	//************************************
	// Method:    NormalizeForces
	// Calculates a normalization constant for each segment 
	// to make the rendering look pretty
	//************************************
	void NormalizeForces()
	{
		float fHalfHeight = 0.5f * XMVectorGetX(XMVector3Length(m_pResult->Muscles[NUI_SKELETON_POSITION_HIP_CENTER].mJointPosition - m_pResult->Muscles[NUI_SKELETON_POSITION_FOOT_LEFT].mJointPosition));
		fHalfHeight += 0.5f * XMVectorGetX(XMVector3Length(m_pResult->Muscles[NUI_SKELETON_POSITION_HIP_CENTER].mJointPosition - m_pResult->Muscles[NUI_SKELETON_POSITION_FOOT_RIGHT].mJointPosition));

		for ( int li = 0; li < MCOUNT; li++ ){
			MUSCLE_DATA* curMuscle = &m_pResult->Muscles[li];
			curMuscle->mForceNormalizationConstant = fabs(GRAVITY) * (curMuscle->mLimbMassExternal);
			curMuscle->mTorqueNormalizationConstant = fabs(GRAVITY) * ( curMuscle->mLimbLength ) * (curMuscle->mLimbMassExternal);

			if ( curMuscle->mLimbMassExternal == 0.0f )
			{
				curMuscle->mLimbExternalCOM = curMuscle->mJointPosition;
			}
			if ( curMuscle->mForceNormalizationConstant == 0.0f )
			{
				curMuscle->mForceNormalizationConstant = 1.0f;
			}
			if ( curMuscle->mTorqueNormalizationConstant == 0.0f )
			{
				curMuscle->mTorqueNormalizationConstant = 1.0f;
			}		
		}

		m_pResult->CenterOfMass.mForceNormalizationConstant = ( m_pResult->CenterOfMass.mLimbMassEstimate * -GRAVITY );
		m_pResult->CenterOfMass.mTorqueNormalizationConstant = fHalfHeight * ( m_pResult->CenterOfMass.mLimbMassEstimate * -GRAVITY );
	}

public:
	void Process()
 	{
#ifdef _XBOX
        XMemSet( m_pResult, 0, sizeof( *m_pResult ) );
#else
		memset( m_pResult, 0, sizeof( *m_pResult ) );
#endif

		EstimateCenterOfMass();

		ProcessGeneral( NUI_SKELETON_POSITION_HAND_LEFT ); 
		ProcessGeneral( NUI_SKELETON_POSITION_WRIST_LEFT );
		ProcessGeneral( NUI_SKELETON_POSITION_ELBOW_LEFT );
		ProcessGeneral( NUI_SKELETON_POSITION_SHOULDER_LEFT );

		ProcessGeneral( NUI_SKELETON_POSITION_HAND_RIGHT ); 
		ProcessGeneral( NUI_SKELETON_POSITION_WRIST_RIGHT ); 
		ProcessGeneral( NUI_SKELETON_POSITION_ELBOW_RIGHT ); 
		ProcessGeneral( NUI_SKELETON_POSITION_SHOULDER_RIGHT );

		ProcessGeneral( NUI_SKELETON_POSITION_HEAD ); 
		ProcessGeneral( NUI_SKELETON_POSITION_SHOULDER_CENTER );
		ProcessGeneral( NUI_SKELETON_POSITION_SPINE ); 

		EstimateFootForces();

		ProcessGeneral( NUI_SKELETON_POSITION_FOOT_LEFT );
		ProcessGeneral( NUI_SKELETON_POSITION_ANKLE_LEFT );
		ProcessGeneral( NUI_SKELETON_POSITION_KNEE_LEFT );
		ProcessGeneral( NUI_SKELETON_POSITION_HIP_LEFT ); 

		ProcessGeneral( NUI_SKELETON_POSITION_FOOT_RIGHT );
		ProcessGeneral( NUI_SKELETON_POSITION_ANKLE_RIGHT );
		ProcessGeneral( NUI_SKELETON_POSITION_KNEE_RIGHT );
		ProcessGeneral( NUI_SKELETON_POSITION_HIP_RIGHT );

		ProcessGeneral( NUI_SKELETON_POSITION_HIP_CENTER, false );

		NormalizeForces();
		
	};
};

XMMATRIX muscleUntiltMatrix( XMVECTOR vToNormal, XMVECTOR vFromNormal )
{
	if ( XMVector3Equal( vToNormal, vFromNormal ) )
	{
		return XMMatrixIdentity();
	}
	else
	{
		XMVECTOR axis = XMVector3Cross( vToNormal, vFromNormal );
		float angle = - XMVectorGetX( XMVector3AngleBetweenNormals( vToNormal, vFromNormal ) );
		return XMMatrixRotationAxis( axis, angle );
	}
}

HRESULT MuscleFrameCalculate(
	__out MUSCLE_FRAME*			pResult,
	const MUSCLE_FRAME*			pPrevious,
	const XMVECTOR*				pJoints,
	const XMVECTOR				vJointsUp,
	const NUI_SKELETON_POSITION_TRACKING_STATE*	pStates,
	const float					deltaSeconds
	)
{
	if(pResult == NULL || pJoints == NULL)
		return S_FALSE;

	XMVECTOR untiltedJoints[ NUI_SKELETON_POSITION_COUNT ];
	XMMATRIX untilter = muscleUntiltMatrix( XMVectorSet(0,1,0,0), vJointsUp );
	for ( int ji = 0; ji < NUI_SKELETON_POSITION_COUNT; ji++ )
	{
		untiltedJoints[ji] = XMVector3Transform( pJoints[ji], untilter );
	}
	pJoints = untiltedJoints;

	float effectiveDt = deltaSeconds;
	if(deltaSeconds > 1.0f || deltaSeconds < 0.0f)
	{
		// if the time passes is not greater than one second, then essentially
		//	reset the calculation.
		pPrevious = NULL;
		effectiveDt = 1.0f / 30.0f;
	}

	MUSCLE_FRAME otherPrev;
	bool firstFrame = false;
	if ( pPrevious == NULL )
	{
#ifdef _XBOX
        XMemSet( &otherPrev, 0, sizeof( otherPrev ) );
#else
		memset( &otherPrev, 0, sizeof( otherPrev ) );
#endif
		for ( int i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
		{
			otherPrev.Muscles[i].mJointPosition = pJoints[i];
		}
		pPrevious = &otherPrev;
		firstFrame = true;
	}
    else if ( pPrevious == pResult )
    {
        // if the src and dst muscle buffers are the same, make a copy and then use
        //  that as the previous:
        otherPrev = *pPrevious;
        pPrevious = &otherPrev;
    }

	MuscleCalculator calc( pJoints, pStates, pPrevious, effectiveDt, pResult, firstFrame);
	calc.Process();
	pResult->TimeSpan = deltaSeconds;
	return S_OK;
}

//Returns Dynamic torque when joint is speeding up, zero vector otherwise
XMVECTOR MuscleFrameGetAcceleratingTorque( const MUSCLE_FRAME* pFrame, NUI_SKELETON_POSITION_INDEX index )
{
	if(pFrame == NULL)
		return XMVectorZero();

	const MUSCLE_DATA* pMD = &pFrame->Muscles[index];
	float angularPower = XMVectorGetX( XMVector3Dot( pMD->mLimbAngularAcceleration, pMD->mLimbAngularVelocity ) );
	if(angularPower > 0.0f)
		return (pMD->mDynamicLocalTorqueMotion + pMD->mDynamicLocalTorqueBalance)/pMD->mTorqueNormalizationConstant;
	return XMVectorZero();
}

//Returns Dynamic torque when joint is slowing down, zero vector otherwise
XMVECTOR MuscleFrameGetDecceleratingTorque( const MUSCLE_FRAME* pFrame, NUI_SKELETON_POSITION_INDEX index )
{
	if(pFrame == NULL)
		return XMVectorZero();

	const MUSCLE_DATA* pMD = &pFrame->Muscles[index];
	float angularPower = XMVectorGetX( XMVector3Dot( pMD->mLimbAngularAcceleration, pMD->mLimbAngularVelocity ) );
	if(angularPower < 0.0f)
		return  (pMD->mDynamicLocalTorqueMotion + pMD->mDynamicLocalTorqueBalance)/pMD->mTorqueNormalizationConstant;
	return XMVectorZero();
}

//Returns Dynamic force when joint is speeding up, zero vector otherwise
XMVECTOR MuscleFrameGetAcceleratingForce(const MUSCLE_FRAME* pFrame, NUI_SKELETON_POSITION_INDEX index)
{
	if(pFrame == NULL)
		return XMVectorZero();

	const MUSCLE_DATA* pMD = &pFrame->Muscles[index];
	float linearPower = XMVectorGetX( XMVector3Dot( pMD->mLimbLinearAcceleration, pMD->mLimbAngularVelocity ) );
	if( linearPower > 0.0f )
		return (pMD->mDynamicExternalForce)/pMD->mForceNormalizationConstant;
	return XMVectorZero();
}
//Returns Dynamic force when joint is slowing down, zero vector otherwise
XMVECTOR MuscleFrameGetDecceleratingForce(const MUSCLE_FRAME* pFrame, NUI_SKELETON_POSITION_INDEX index)
{
	if(pFrame == NULL)
		return XMVectorZero();

	const MUSCLE_DATA* pMD = &pFrame->Muscles[index];
	float linearPower = XMVectorGetX( XMVector3Dot( pMD->mLimbLinearAcceleration, pMD->mLimbAngularVelocity ) );
	if( linearPower < 0.0f )
		return (pMD->mDynamicExternalForce)/pMD->mForceNormalizationConstant;
	return XMVectorZero();
}
//Returns the dot product of the linear acceleration and the linear velocity
float	 MuscleDataGetLinearPower(const MUSCLE_DATA* pData)
{
	return XMVectorGetX(XMVector3Dot(
		pData->mLimbLinearAcceleration,
		pData->mLimbLinearVelocity));
}
//Returns the normalized dot products of the normalized external force with the normalized external COM velocity
XMVECTOR MuscleFrameGetDynamicPower(const MUSCLE_FRAME* pFrame, NUI_SKELETON_POSITION_INDEX index)
{
	const MUSCLE_DATA* pMD = &pFrame->Muscles[index];
	float massEx = ( pMD->mLimbMassExternal != 0 ) ? pMD->mLimbMassExternal : 1.0f;
	float forceNorm = ( pMD->mForceNormalizationConstant != 0 ) ? pMD->mForceNormalizationConstant : 1.0f;
	return XMVectorMultiply( pMD->mDynamicExternalForce / forceNorm,
		pMD->mLimbExternalCOMVelocity / massEx ) * -1.0f;
}

// ----------------------------- Muscle Visualization -----------------------------

struct MuscleVisGroup
{
    NUI_SKELETON_POSITION_INDEX Origin;
    NUI_SKELETON_POSITION_INDEX EndPoint;
    NUI_SKELETON_POSITION_INDEX Axis;
    bool Revert;
    float MuscleScale;
};

const MuscleVisGroup gMuscleVisGroupsShort[MUSCLE_PATCH_SHORT_COUNT] = {
    { NUI_SKELETON_POSITION_SPINE,				NUI_SKELETON_POSITION_SHOULDER_CENTER,	NUI_SKELETON_POSITION_HEAD, false, 1.0f },
    { NUI_SKELETON_POSITION_HIP_CENTER,			NUI_SKELETON_POSITION_SPINE,			NUI_SKELETON_POSITION_SHOULDER_CENTER, false, 1.0f },
    { NUI_SKELETON_POSITION_SHOULDER_CENTER,	NUI_SKELETON_POSITION_HEAD,				NUI_SKELETON_POSITION_SPINE, false, 1.0f },
    
    { NUI_SKELETON_POSITION_WRIST_LEFT,			NUI_SKELETON_POSITION_HAND_LEFT,		NUI_SKELETON_POSITION_ELBOW_LEFT, true, 1.0f },
    { NUI_SKELETON_POSITION_ELBOW_LEFT,			NUI_SKELETON_POSITION_WRIST_LEFT,		NUI_SKELETON_POSITION_SHOULDER_LEFT, true, 1.0f },
    { NUI_SKELETON_POSITION_SHOULDER_LEFT,		NUI_SKELETON_POSITION_ELBOW_LEFT,		NUI_SKELETON_POSITION_SHOULDER_CENTER, true, 1.0f },
    { NUI_SKELETON_POSITION_SHOULDER_CENTER,	NUI_SKELETON_POSITION_SHOULDER_LEFT,	NUI_SKELETON_POSITION_SPINE, true, 1.0f },
    { NUI_SKELETON_POSITION_HIP_CENTER,			NUI_SKELETON_POSITION_HIP_LEFT,			NUI_SKELETON_POSITION_SPINE , true, 2.0f},
    { NUI_SKELETON_POSITION_HIP_LEFT,			NUI_SKELETON_POSITION_KNEE_LEFT,		NUI_SKELETON_POSITION_HIP_CENTER , true, 2.0f},
    { NUI_SKELETON_POSITION_KNEE_LEFT,			NUI_SKELETON_POSITION_ANKLE_LEFT,		NUI_SKELETON_POSITION_HIP_LEFT, true, 2.0f },
    { NUI_SKELETON_POSITION_ANKLE_LEFT,			NUI_SKELETON_POSITION_FOOT_LEFT,		NUI_SKELETON_POSITION_KNEE_LEFT, true, 2.0f },

    { NUI_SKELETON_POSITION_WRIST_RIGHT,		NUI_SKELETON_POSITION_HAND_RIGHT,		NUI_SKELETON_POSITION_ELBOW_RIGHT, false, 1.0f },
    { NUI_SKELETON_POSITION_ELBOW_RIGHT,		NUI_SKELETON_POSITION_WRIST_RIGHT,		NUI_SKELETON_POSITION_SHOULDER_RIGHT, false, 1.0f },
    { NUI_SKELETON_POSITION_SHOULDER_RIGHT,		NUI_SKELETON_POSITION_ELBOW_RIGHT,		NUI_SKELETON_POSITION_SHOULDER_CENTER, false, 1.0f },
    { NUI_SKELETON_POSITION_SHOULDER_CENTER,	NUI_SKELETON_POSITION_SHOULDER_RIGHT,	NUI_SKELETON_POSITION_SPINE, false, 1.0f },
    { NUI_SKELETON_POSITION_HIP_CENTER,			NUI_SKELETON_POSITION_HIP_RIGHT,		NUI_SKELETON_POSITION_SPINE , true, 2.0f},
    { NUI_SKELETON_POSITION_HIP_RIGHT,			NUI_SKELETON_POSITION_KNEE_RIGHT,		NUI_SKELETON_POSITION_HIP_CENTER, false, 2.0f },
    { NUI_SKELETON_POSITION_KNEE_RIGHT,			NUI_SKELETON_POSITION_ANKLE_RIGHT,		NUI_SKELETON_POSITION_HIP_RIGHT, false, 2.0f },
    { NUI_SKELETON_POSITION_ANKLE_RIGHT,		NUI_SKELETON_POSITION_FOOT_RIGHT,		NUI_SKELETON_POSITION_KNEE_RIGHT, false, 2.0f },
};

bool FloatIsSane( float f )
{
	return ( ( f > -1000000 ) && ( f < 1000000 ) );
}

bool VectorIsSane( XMVECTOR v )
{
	return true
		&& FloatIsSane( XMVectorGetX( v ) )
		&& FloatIsSane( XMVectorGetY( v ) )
		&& FloatIsSane( XMVectorGetZ( v ) );
}

enum TorqueSelector
{
	TorqueSelect_AllDynamic = 0,
	TorqueSelect_StaticBalance,
	TorqueSelect_Motion,
	TorqueSelect_Balance,
	TorqueSelect_MuscleX,
	TorqueSelect_MuscleY,
	TorqueSelect_MuscleZ,
};

XMVECTOR ValueFractions( XMVECTOR f )
{
	XMFLOAT4 v;
	XMStoreFloat4( &v, XMVectorAbs( f ) );
	float sum = v.x + v.y + v.z;
	if ( sum == 0.0f )
		sum = 1.0f;
	return XMVectorSet(
		v.x / sum, 
		v.y / sum, 
		v.z / sum, 0.0f );
}

unsigned int MuscleFrameVisualize_Short(
		const MUSCLE_FRAME*	pMuscles,
		TorqueSelector torqueSelector,
		MUSCLE_PATCH*	pPositions)
{
    const float invGoldenRatio = 0.5f / ( 1.0f + sqrt(5.0f) ) ; // width of unit arrow width
	MUSCLE_DATA zeroPos;
#ifdef _XBOX
    XMemSet( &zeroPos, 0, sizeof( zeroPos ) );
#else
    memset( &zeroPos, 0, sizeof( zeroPos ) );
#endif
	if ( pMuscles != NULL )
	{
		if ( pMuscles->CenterOfMass.mLimbMassEstimate == 0 )
		{
			// If muscles are not tracked, return all zeros
#ifdef _XBOX
            XMemSet( pPositions, 0, sizeof( MUSCLE_PATCH ) * MUSCLE_PATCH_SHORT_COUNT );
#else
			memset( pPositions, 0, sizeof( MUSCLE_PATCH ) * MUSCLE_PATCH_SHORT_COUNT );
#endif
			return MUSCLE_PATCH_SHORT_COUNT;
		}

		// Now fill out the patches
		for ( int vi = 0; vi < MUSCLE_PATCH_SHORT_COUNT; vi++ )
		{
			const MuscleVisGroup& group = gMuscleVisGroupsShort[ vi ];

			const MUSCLE_DATA* pInf = &( pMuscles->Muscles[ group.Origin ] );
			const MUSCLE_DATA* pEnd = &( pMuscles->Muscles[ group.EndPoint ] );

			XMVECTOR r = ( pEnd->mJointPosition - pInf->mJointPosition );
			float sr = XMVectorGetX( XMVector3Length( r ) ) 
				* invGoldenRatio * group.MuscleScale;

			// Skip this patch if it has no length, or if the joints have very low
			//	confidense (i.e. very large resolution radius of 2 meters or more).
			if ( sr == 0.0f 
				|| ( pEnd->mJointResolutionRadius > 2.0f ) 
				|| ( pInf->mJointResolutionRadius > 2.0f ) 
				)
			{
#ifdef _XBOX
                XMemSet( &( pPositions[ vi ] ), 0, sizeof( pPositions[ vi ] ) );
#else
				memset( &( pPositions[ vi ] ), 0, sizeof( pPositions[ vi ] ) );
#endif
				continue;
			}

			// Calculate the basis forces: 
			const MUSCLE_DATA *pCore=NULL, *pOther=NULL, *pForce=NULL, *pTorque=NULL;
			XMVECTOR aboutAxis, bendAxis, forceOffset;
			XMVECTOR torqueVec, forceVec;
			float magInf=0, magEnd=0;
			float torqueNormalizer = 1.0f;

			magInf = XMVectorGetX( XMVector3Length( pInf->mDynamicExternalForce ) );
			magEnd = XMVectorGetX( XMVector3Length( pEnd->mDynamicExternalForce ) );
			pForce = ( magInf < magEnd ) ? pEnd : pInf;
			float forceScaler = ( sqrtf( max(magInf,magEnd) / pForce->mForceNormalizationConstant ) );

			switch ( torqueSelector )
			{
			case TorqueSelect_AllDynamic:
				pTorque= pEnd;
				torqueVec = pTorque->mDynamicLocalTorqueBalance + pTorque->mDynamicLocalTorqueMotion;
				forceVec = pForce->mDynamicExternalForce;
				torqueNormalizer = pTorque->mTorqueNormalizationConstant;
				break;
			case TorqueSelect_StaticBalance:
				magInf = XMVectorGetX( XMVector3Length( pInf->mStaticExternalForce ) );
				magEnd = XMVectorGetX( XMVector3Length( pEnd->mStaticExternalForce ) );
				pForce = ( magInf < magEnd ) ? pEnd : pInf;
				pTorque= pEnd;
				torqueVec = pTorque->mStaticLocalTorqueBalance;
				forceVec = pForce->mStaticExternalForce;
				torqueNormalizer = pTorque->mTorqueNormalizationConstant;
				forceScaler = ( sqrtf( max(magInf,magEnd) / pForce->mForceNormalizationConstant ) );
				break;
			case TorqueSelect_Balance:
				pTorque= pEnd;
				torqueVec = pTorque->mDynamicLocalTorqueBalance;
				forceVec = pForce->mDynamicExternalForce;
				torqueNormalizer = pTorque->mTorqueNormalizationConstant;
				break;
			case TorqueSelect_Motion:
				pTorque= pEnd;
				torqueVec = pTorque->mDynamicLocalTorqueMotion;
				forceVec = pForce->mDynamicExternalForce;
				torqueNormalizer = pTorque->mTorqueNormalizationConstant;
				break;
			case TorqueSelect_MuscleX:
				pTorque= pEnd;
				torqueVec = pTorque->mMuscleToSensorX * XMVectorGetX( pTorque->mMuscleDynamicLocalTorque );
				forceVec = pForce->mDynamicExternalForce;
				forceScaler *= XMVectorGetX( ValueFractions( pTorque->mMuscleDynamicLocalTorque ) );
				torqueNormalizer = pTorque->mTorqueNormalizationConstant;
				break;
			case TorqueSelect_MuscleY:
				pTorque= pEnd;
				torqueVec = pTorque->mMuscleToSensorY * XMVectorGetY( pTorque->mMuscleDynamicLocalTorque );
				forceVec = pForce->mDynamicExternalForce;
				forceScaler *= XMVectorGetY( ValueFractions( pTorque->mMuscleDynamicLocalTorque ) );;
				torqueNormalizer = pTorque->mTorqueNormalizationConstant;
				break;
			case TorqueSelect_MuscleZ:
				pTorque= pEnd;
				torqueVec = pTorque->mMuscleToSensorZ * XMVectorGetZ( pTorque->mMuscleDynamicLocalTorque );
				forceVec = pForce->mDynamicExternalForce;
				forceScaler *= XMVectorGetZ( ValueFractions( pTorque->mMuscleDynamicLocalTorque ) );;
				torqueNormalizer = pTorque->mTorqueNormalizationConstant;
				break;
			default:
                torqueVec = XMVectorZero();
                forceVec = XMVectorZero();
				MUSCLE_TODO();
				break;
			};

			aboutAxis = ( XMVector3Normalize( torqueVec ) * ( sr * forceScaler ) );
			MUSCLE_ASSERT( VectorIsSane( aboutAxis ) );

			bendAxis = ( XMVector3Normalize( XMVector3Cross( r, aboutAxis ) ) 
				* ( sqrtf( fabs( XMVectorGetX( XMVector3Length( torqueVec ) ) ) / torqueNormalizer ) )
				* ( -sr ) );
			MUSCLE_ASSERT( VectorIsSane( bendAxis ) );

			forceOffset = ( forceVec * ( ( sr * -0.16f ) / pForce->mForceNormalizationConstant ) ) - ( bendAxis * 0.16f );
			MUSCLE_ASSERT( VectorIsSane( forceOffset ) );

			pCore = ( magInf > magEnd ) ? pInf : pEnd;
			pOther= ( magInf <=magEnd ) ? pInf : pEnd;

			// Generate the patch representation:
			MUSCLE_PATCH patch;
			patch.BaseMid = pCore->mJointPosition - forceOffset;
			patch.BaseLeft = pCore->mJointPosition - aboutAxis + forceOffset;
			patch.BaseRight = pCore->mJointPosition + aboutAxis + forceOffset;

			patch.EndMid = pOther->mJointPosition;
			patch.EndLeft = pOther->mJointPosition;
			patch.EndRight = pOther->mJointPosition;

			patch.MidLeft = ( patch.BaseLeft + patch.EndLeft ) * 0.5f + bendAxis;
			//patch.MidMid = ( patch.BaseMid + patch.EndMid ) * 0.5f + bendAxis;
			patch.MidRight = ( patch.BaseRight + patch.EndRight ) * 0.5f + bendAxis;

			pPositions[ vi ] = patch;
		}
	}
	return MUSCLE_PATCH_SHORT_COUNT;
}


unsigned int MuscleFrameVisualize_Complete(
		const MUSCLE_FRAME*	pMuscles,
		MUSCLE_PATCH*	pPositions )
{
	MuscleFrameVisualize_Short( pMuscles, TorqueSelect_MuscleX, pPositions );
	MuscleFrameVisualize_Short( pMuscles, TorqueSelect_MuscleY, pPositions + MUSCLE_PATCH_SHORT_COUNT );
	MuscleFrameVisualize_Short( pMuscles, TorqueSelect_MuscleZ, pPositions + (2*MUSCLE_PATCH_SHORT_COUNT) );

	return MUSCLE_PATCH_COUNT;
}

unsigned int MuscleFrameVisualize(
		const MUSCLE_FRAME*	pMuscles,
		int	totalPatchCount,
		MUSCLE_PATCH*	pOptPositions )
{
	if ( totalPatchCount == MUSCLE_PATCH_COUNT )
	{
		return MuscleFrameVisualize_Complete( pMuscles, pOptPositions );
	}
	else if ( totalPatchCount == MUSCLE_PATCH_SHORT_COUNT )
	{
		return MuscleFrameVisualize_Short( pMuscles, TorqueSelect_AllDynamic, pOptPositions );
	}
	else if ( totalPatchCount == 0 )
	{
		return 0;
	}
	else
	{
		// TODO(); !!! Unknown number of patches to fill into!!!
		MUSCLE_TODO();
		return 0;
	}
}

void MuscleFrameVisualizeAsFloats(
		const MUSCLE_FRAME* pMuscles,
		int	totalPatchCount,
		float*	pAsArrayOfPATCH_COUNTx8x3Floats )
{
	MUSCLE_PATCH patches[ MUSCLE_PATCH_COUNT ];
	MuscleFrameVisualize( pMuscles, totalPatchCount, patches );

	for ( int pi=0; pi<totalPatchCount; pi++ )
	{
		MUSCLE_PATCH patch = patches[ pi ];
		XMFLOAT3* pTo = (XMFLOAT3*)( pAsArrayOfPATCH_COUNTx8x3Floats + ( pi * 8 * 3 ) );

		XMStoreFloat3( pTo+0+0, patch.BaseLeft );
		XMStoreFloat3( pTo+0+1, patch.BaseMid );
		XMStoreFloat3( pTo+0+2, patch.BaseRight );

		XMStoreFloat3( pTo+3+0, patch.MidLeft );
		XMStoreFloat3( pTo+3+1, patch.MidRight );

		XMStoreFloat3( pTo+5+0, patch.EndLeft );
		XMStoreFloat3( pTo+5+1, patch.EndMid );
		XMStoreFloat3( pTo+5+2, patch.EndRight );
	}
}

#define INTERP3_MEM( MEM )	pInto->MEM = XMVectorLerp( pFrom->MEM, pTo->MEM, t )
#define INTERP1_MEM( MEM )	pInto->MEM = ( ( pFrom->MEM * ( 1.0f - t ) ) + ( pTo->MEM * t ) )

void MuscleDataInterpolate(
		MUSCLE_DATA*	pInto,
		const MUSCLE_DATA* pFrom,
		const MUSCLE_DATA* pTo,
		float t )
{
	INTERP3_MEM( mJointPosition );

	INTERP3_MEM( mLimbLinearVelocity );
	INTERP3_MEM( mLimbLinearAcceleration );
	INTERP3_MEM( mLimbAngularVelocity );
	INTERP3_MEM( mLimbAngularAcceleration );
	INTERP3_MEM( mLimbTempJerk );
	INTERP3_MEM( mLimbTempAcceleration );

	INTERP3_MEM( mStaticExternalForce );
	INTERP3_MEM( mStaticExternalTorque );
	INTERP3_MEM( mStaticLocalTorqueBalance );

	INTERP3_MEM( mDynamicExternalForce );
	INTERP3_MEM( mDynamicExternalTorque );
	INTERP3_MEM( mDynamicLocalTorqueMotion );
	INTERP3_MEM( mDynamicLocalTorqueBalance );

	INTERP3_MEM( mMuscleToSensorX );
	INTERP3_MEM( mMuscleToSensorY );
	INTERP3_MEM( mMuscleToSensorZ );
	INTERP3_MEM( mMuscleDynamicLocalTorque );

	INTERP1_MEM( mJointResolutionRadius );
	INTERP1_MEM( mLimbLength );
	INTERP1_MEM( mLimbMassEstimate );
	INTERP1_MEM( mLimbMassExternal );
	INTERP1_MEM( mLimbAngularExtension );
	INTERP1_MEM( mForceNormalizationConstant );
	INTERP1_MEM( mTorqueNormalizationConstant );
}

void MuscleFrameInterpolate(
		MUSCLE_FRAME*	pInto,
		const MUSCLE_FRAME* pFrom,
		const MUSCLE_FRAME* pTo,
		float t )
{
	for ( int i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
	{
		MuscleDataInterpolate( pInto->Muscles+i, pFrom->Muscles+i, pTo->Muscles+i, t );
	}
	MuscleDataInterpolate( &pInto->CenterOfMass, &pFrom->CenterOfMass, &pTo->CenterOfMass, t );
	INTERP1_MEM( TimeSpan );
}

}