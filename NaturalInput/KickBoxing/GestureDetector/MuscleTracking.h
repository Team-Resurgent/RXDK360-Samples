
//=====================================================================
// MuscleTracking.h
//
// Description: 
//  Provides structures and functionality for estimating muscle
//	and bone stress from Kinect skeletal information.
//      - Lewey Geselowitz (leweyg@microsoft.com)
//		- Rahul Agarwal (t-rahula@microsoft.com)
//
// Usage:
//
//		// Declare
//		MUSCLE_FRAME	m_Muscles;
//
//		// Update:
//		MuscleFrameCalculate(	&m_Muscles, 
//                              bFirstFrame ? NULL : &m_Muscles, 
//                              pSkeletonData->SkeletonPositions, 
//								pSkeleton->vNormalToGravity,    // or XMVectorSet(0,1,0,0) if already untilted
//                              pSkeletonData->eSkeletonPositionTrackingState, 
//								fElapsedTime );
//
//      // Usage:
//      XMVECTOR muscleTorqueLeftArm = MuscleFrameGetAcceleratingTorque( &m_Muscles, NUI_SKELETON_POSITION_ELBOW_LEFT ); 
//
//
// Documentation:
//
// Coordinate Systems
// ------------------
// The force is reported in sensor space coordinates, with the player facing the -Z direction. 
//
// Muscle torques are reported in a bone-aligned coordinate system known as "muscle space."  This space
// is constructed such that a "pull" action is a rotation around the X-axis, and a "twist" action is
// a rotation around the y axis.
//
// A "pull" is a rotation around the axis perpendicular to both the endpoint - inflection and axis - inflection
// vectors. For example, a bicep curl is a pull action around the elbow. Lifting a leg directly out to the side 
// of the body is a pull action around the left/right hip joint
//
// A "twist" action is a rotation around the axis - inflection vector. For example, lifting a knee up to the 
// front of the body is twist action around the left/right hip joint
//
// These coordinate systems are constructed such that movement away from the body or to the front of the body
// is a positive torque. Care is taken to minimize degeneracies, such as when all three joints in a muscle are
// collinear, although in some cases (such as in the wrist-elbow-shoulder muscle) this is unavoidable
//
// Normalization
// --------------
// Normalization constants are provided to scale the force and torque to be independent of the person. The forces are 
// not normalized during the muscle calculation. Note that the upper and lower body segments are treated differently.
// 
// Upper Body:	Forces are normalized such that the weight of the segment + the weight of all the attached segments
//				hanging off of the end effector constitutes a force of 1.0f. Ex. The weight of shoulder-elbow segment +
//				the weight of the hand-wrist + wrist-elbow segments is a force of 1.0f on the elbow-shoulder-neck muscle.
//				
//				Torques are normalized such that the maximum torque produced by the weight of the segments hanging off the 
//				end effector, applied at the end effector, plus the torque of the weight of the segment applied at the segment's
//				center of mass constitutes a force of 1.0f
//
// Lower Body:	Forces are normalized such that the full weight of the person's body is normalized to 1.0f
//				Torques are normalized such that the full weight of the person's body, applied at the end effector,
//				creates a torque of 1.0f
//
// Static vs. Total Torque
// -----------------------
// The vMuscleTorque field is total of torques from 3 sources: 1) The torque required to accelerate the segment itself. 
// 2) The torque caused by gravity due to the weight of the segment, plus the weights of the segments attached to the 
// end joint. 3) The torque applied to the current segment by the segments attached to the end joint due to the movement 
// of the attached segments.
//
// The vMuscleStaticTorque is total of the torques caused ONLY by the gravitational forces on the current segment and 
// the segments attached to the end joint. That is, the vMuscleStaticTorque is computed by assuming that the current segment
// AND THE ATTACHED SEGMENTS have zero acceleration.
//
// The difference between the vMuscleTorque and vMuscleStaticTorque can be used to find the torque used to accelerate the
// current segment and the attached segments.
//
// The difference between static force is similar in that it assumes that the segment’s acceleration is zero
//
// Center of Mass
// ------------------
// The density of each segment was computed using anthropometric tables (http://www.univie.ac.at/cga/teach-in/inverse-dynamics.html) to 
// determine the relative distribution of segment masses. These densities are used to compute the center of mass of the entire body for 
// each frame. The velocity and acceleration of the center of mass are used to determine the total external force on the body, 
// which is assumed to come from the ground through the feet. We found that the position of the feet relative to the body in the Z direction 
// was very unreliable as it was dependent on the height of the camera as well as the clothing of the user. 
// Therefore, the foot pressure calculations are performed after projecting all relevant quantities into the XY-plane. 
// 
// We could not determine how much force is on the foot vs. on the ankle, so the forces are assumed to be equal. 
// That is, the external force is applied equally to the foot joint and the ankle joint of the foot.
//
// Copyright (c) Microsoft Corporation
//=====================================================================


#pragma once

#ifdef _XBOX

#include <xtl.h>
#include <nuiapi.h>
#else
#include <xnamath.h>
#include <NuiTools.h>
#include <XStudio.h>
#endif

namespace ATGGestureDetector
{

#define _aligned __declspec(align(16))

// ----------------------------- Muscle Calculation -----------------------------


// MUSCLE_DATA
//=============
//	Describes the physics and muscles acting on an end-joint. 
//	
// The main physical quantities of interest are the forces on the inflection joint (vExternal Force)
// and the torque produced by the muscle (vMuscleTorque). 
//
struct _aligned MUSCLE_DATA
{
	// Joint Position:
	XMVECTOR	mJointPosition;				//Position of end effector of muscle, in sensor space

	// Limb Motion:
	XMVECTOR	mLimbLinearVelocity;		//Velocity of center of mass of segment (m/s)
	XMVECTOR	mLimbLinearAcceleration;	//Acceleration of center of mass of segment (m/s^2)
	XMVECTOR	mLimbAngularVelocity;		//Angular velocity around inflection point
	XMVECTOR	mLimbAngularAcceleration;	//Angular acceleration around inflection point
	XMVECTOR	mLimbTempJerk;				//Derivative of the acceleration
	XMVECTOR	mLimbTempAcceleration;		//Intermediate value used in filtering
	XMVECTOR	mLimbExternalCOM;			//External weight * center of mass of external limbs
	XMVECTOR	mLimbExternalCOMVelocity;	//Velocity of external COM

	// Static Joint Pressures WITHOUT Motion (more stable because no force from jitter)
	XMVECTOR	mStaticExternalForce;		// External force on the end joint
	XMVECTOR	mStaticExternalTorque;		// External torque on the end joint
	XMVECTOR	mStaticLocalTorqueBalance;	// Torque applied by the inflection joint

	// Dynamic Joint Pressures including Motion:
	XMVECTOR	mDynamicExternalForce;		// External force on the end joint
	XMVECTOR	mDynamicExternalTorque;		// External torque on the end joint
	XMVECTOR	mDynamicLocalTorqueMotion;	// Torque applied by the muscle for motion
	XMVECTOR	mDynamicLocalTorqueBalance;	// Torque applied by the muscle to achieve balance
	
	// Muscle Alignment and Forces:
	XMVECTOR	mMuscleToSensorX;			// Pull about axis
	XMVECTOR	mMuscleToSensorY;			// Rotate about axis
	XMVECTOR	mMuscleToSensorZ;			// Twist about axis
	XMVECTOR	mMuscleDynamicLocalTorque;	// Muscle aligned total dynamic torque (X=pull, etc.)

	// Scalar Properties:
	float		mJointResolutionRadius;			//Estimated resolution/jitter radius of joint position
	float		mLimbLength;					//Eistance from inflection point to end point of muscle, in meters
	float		mLimbMassEstimate;				//Estimate of mass of end point to inflection segment
	float		mLimbMassExternal;				//Estimate of the mass of this part of the body hierarchy
	float		mLimbAngularExtension;			//Joint angle between axis joint and end joint around inflection
	float		mForceNormalizationConstant;	//Divide the force by this value to normalize it
	float		mTorqueNormalizationConstant;	//Divide the torque by this value to normalize it
};

// MUSCLE_FRAME
//=============
//	Contains all of the muscle data for a complete skeleton. The muscles are organized
//	in the same hierarchy as the NUI_SKELETON_POSITION_* joints, and are indexed
//	by their END EFFECTORS
//
struct _aligned MUSCLE_FRAME
{
	MUSCLE_DATA CenterOfMass;
	MUSCLE_DATA	 Muscles[ NUI_SKELETON_POSITION_COUNT ];
	float		TimeSpan;
};


// MuscleFrameCalculate(...)
//==========================
//				Given a the current joint positions of a skeleton, and optionally 
//				the previous muscle data for the skeleton, this method will generate 
//				the muscle data for the next frame
//
// Returns:		HRESULT
//				Returns S_FALSE if an invalid parameter is passed, S_OK otherwise
// Parameter:	MUSCLE_FRAME * pResult
//				Output MUSCLE_FRAME. Must not be NULL
// Parameter:	const MUSCLE_FRAME * pPrevious
//				Previously calculated MUSCLE_FRAME.  
//				A value of NULL indicates that this is the first frame.
// Parameter:	const XMVECTOR * pJoints
//				Array of skeletal joint positions
// Parameter:	const XMVECTOR vJointsUp
//				The current up vector of the skeleton, as determined by the gravity vector
//				of the sensor. This is used to perform tilt correction on the skeleton
//				prior to doing muscle calculations
// Parameter:	const NUI_SKELETON_POSITION_TRACKING_STATE * pStates
//				Confidence values for each joint. Not used if NULL
// Parameter:	const float deltaSeconds
//				Time interval between previous frame and this one. Must not be 0.0f
//
// Example Usage:
//		/* Initialize */
//		MUSCLE_FRAME	m_Muscles;
//
//		/* Call */
//		MuscleFrameCalculate(	&m_Muscles, bFirstFrame ? NULL : &m_Muscles, pSkeletonData->SkeletonPositions, 
//								pSkeleton->vNormalToGravity, pSkeletonData->eSkeletonPositionTrackingState, 
//								fElapsedTime );
//		
//
//************************************
HRESULT MuscleFrameCalculate(
	__out MUSCLE_FRAME*			pResult,
	const MUSCLE_FRAME*			pPrevious,
	const XMVECTOR*				pJoints,
	const XMVECTOR				vJointsUp,
	const NUI_SKELETON_POSITION_TRACKING_STATE*	pStates,
	const float					deltaSeconds
	);



// ----------------------------- Helper Functions -----------------------------

//
//	Simplify using muscle analysis in a game context.
//
//Returns Dynamic torque when joint is speeding up, zero vector otherwise
XMVECTOR MuscleFrameGetAcceleratingTorque(const MUSCLE_FRAME* pFrame, NUI_SKELETON_POSITION_INDEX index);
//Returns Dynamic torque when joint is slowing down, zero vector otherwise
XMVECTOR MuscleFrameGetDecceleratingTorque(const MUSCLE_FRAME* pFrame, NUI_SKELETON_POSITION_INDEX index);
//Returns Dynamic force when joint is speeding up, zero vector otherwise
XMVECTOR MuscleFrameGetAcceleratingForce(const MUSCLE_FRAME* pFrame, NUI_SKELETON_POSITION_INDEX index);
//Returns Dynamic force when joint is slowing down, zero vector otherwise
XMVECTOR MuscleFrameGetDecceleratingForce(const MUSCLE_FRAME* pFrame, NUI_SKELETON_POSITION_INDEX index);
//Returns the dot product of the linear acceleration and the linear velocity
float	 MuscleDataGetLinearPower(const MUSCLE_DATA* pData);
//Returns the normalized dot products of the normalized external force with the normalized external COM velocity
XMVECTOR MuscleFrameGetDynamicPower(const MUSCLE_FRAME* pFrame, NUI_SKELETON_POSITION_INDEX index);
//Returns the normalized dot products of the normalized external force with the normalized external COM velocity
XMVECTOR MuscleFrameGetDynamicBodyPower(const MUSCLE_FRAME* pFrame, NUI_SKELETON_POSITION_INDEX index);

// MuscleFrameInterpolate
//
//	Interpolates between two muscle frames.
//
void MuscleFrameInterpolate(
		MUSCLE_FRAME*	pInto,
		const MUSCLE_FRAME* pFrom,
		const MUSCLE_FRAME* pTo,
		float t );



// ----------------------------- Muscle Rig Description -----------------------------


// MUSCLE_DESC
//
//	Describes the hierarchical composition of the muscles used by a MUSCLE_FRAME
//
struct MUSCLE_DESC
{
	NUI_SKELETON_POSITION_INDEX		EndPoint;	// End Effector of muscle
	NUI_SKELETON_POSITION_INDEX		Inflection;	// Joint around which the muscle pivots
	NUI_SKELETON_POSITION_INDEX		Axis; // Joint opposite End Effector.
	NUI_SKELETON_POSITION_INDEX		Basis; // Climbable pointer through the heiarchy
	float							Density; //Density of segment, in kg/m
};

// MuscleDescriptions
//
//	Returns the default muscle configuration
//
const MUSCLE_DESC* MuscleDescriptions();


// ----------------------------- Muscle Visualization -----------------------------


#define MUSCLE_PATCH_WIDTH				3
#define MUSCLE_PATCH_SHORT_COUNT		19
#define MUSCLE_PATCH_COUNT				(MUSCLE_PATCH_SHORT_COUNT*3)

// MUSCLE_PATCH
//
//	Describes a Bezier surface "patch" which can be used to visualize a muscle state
//
struct _aligned MUSCLE_PATCH
{
	//XMVECTOR Vertices[ MUSCLE_PATCH_WIDTH * MUSCLE_PATCH_WIDTH ];
	XMVECTOR BaseLeft, BaseMid, BaseRight;
	XMVECTOR MidLeft, MidRight;
	XMVECTOR EndLeft, EndMid, EndRight;
};


// MuscleFrameVisualize
//
//	Fills the provided patch arrays with positions, normals, and colors for visualization,
//		from the startIndex until the count of patches has been generated.
//		Always, returns the total number of patches that could be filled.
//
//	Should be passed either MUSCLE_PATCH_COUNT or MUSCLE_PATCH_SHORT_COUNT for totalPatchCount
//
unsigned int MuscleFrameVisualize(
		const MUSCLE_FRAME*	pMuscles,
        int	totalPatchCount,
		MUSCLE_PATCH*	pOptPositions );



// MuscleFrameVisualizeAsFloats
//
//	Fills in a "PATCH_COUNT x 8 x 3" array of floats with patch data.
//		This is the same as MuscleFrameVisualize, except that it doesn't use
//		the PATCH struct and so can be called from managed contexts more easily
//
void MuscleFrameVisualizeAsFloats(
		const MUSCLE_FRAME* pMuscles,
		int	totalPatchCount,
		float*	pAsArrayOfPATCH_COUNTx8x3Floats );

}