//--------------------------------------------------------------------------------------
// File: RunningSpeedDetection.h
//
// Detects how fast you are running
// The returned speed indicates how many steps the user is taking in one second
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#include <NuiApi.h>
#include <list>

enum TwoKneesStatus
{
    BOTH_KNEES_NEUTRAL,
    RIGHT_GOING_UP,
    RIGHT_GOING_DOWN,
    LEFT_GOING_UP,
    LEFT_GOING_DOWN
};
// This must be at least 3
#define DELTAS_TO_TRACK 6
// FLATTENING_DELTAS must be smaller than DELTAS_TO_TRACK
#define FLATTENING_DELTAS 4



//--------------------------------------------------------------------------------------
// Name: KneeHistoryTracker
// This worker class determines the running state.
//--------------------------------------------------------------------------------------
class KneeHistoryTracker
{
public:
    ~KneeHistoryTracker(){};
    KneeHistoryTracker():
        m_fSlopeFromBeforePivoting( 0.0f ), // When we find a peak or valley we pivot and store the slope here.
        m_fPreviousPivotPoint( FLT_MAX), // The last local high or minimum
        m_fFlatteningTolerence( 0.001f ), // Delta difference must be below this for X frames to be standing still.
        m_fSlopeBufferPadding ( 0.001f ), // previously stored out slope must be greater than this to be an actual  peak
        m_fPivotDeadZone( 0.005f ), // Peaks and valleys must be bigger than this or they could just be noise.
        m_fKneesTogetherTolerence( 0.03f ), // Knee difference must be below this for X frames to be standing still.
        m_uTickCounter( 0 ), // Used to store foot fall history
        m_uStepStartTick( 0 ), // Used to keep track of foot fall timing
        m_fStepPivotPoint( 0.0f ), // Store out the last peak / valley for testing against aftershocks.
        m_fMinimumConsequentStepRatio( 0.2f ), // consequent steps must be this percentage of previous steps.  Used to remove aftershocks.
        m_fNoiseTolerence( 0.01f) // minimum foot step distance
    {
        for ( int index=0; index < DELTAS_TO_TRACK; ++index )   m_fLastSixDeltas[index] = 0.0f; // This array stores the last 6 differences between the left and right knee    
        m_fDerivatives[0] = 0.0f; // The derivitives of the last two knee deltas
        m_fDerivatives[1] = 0.0f;
    };

    VOID Step( TwoKneesStatus StepStatus )
    {
        // The rough length of a foot step is calculated.  This helps us remove aftershock foot steps.
        m_uSameFootWaitingLimit = ( ( m_uTickCounter - m_uStepStartTick ) * 3 ) /2;
        //Save out the pivot at the time of the step.  This is the magnitude of the step. 
        //We can now say that steps immediatly after have to have some percentage of this magnitude.
        m_fStepPivotPoint = m_fPreviousPivotPoint;
    }
    BOOL SlopeChangedNegative()
    {
        // Looks for negative slope change
        return ( m_fDerivatives[0] < 0 && m_fDerivatives[1] < 0 && 
                m_fSlopeFromBeforePivoting > -m_fSlopeBufferPadding );
    }
    BOOL SlopeChangedPositive()
    {
        // Looks for positive slope change
        return ( m_fDerivatives[0] > 0 && m_fDerivatives[1] >= 0 && 
                   m_fSlopeFromBeforePivoting < m_fSlopeBufferPadding );
    }

    // Is the footstep we found really just an after shock from an extra large step?
    BOOL AfterShock()
    {
        if ( m_fStepPivotPoint == 0.0f ) return false;
        UINT uTicksWaited = m_uTickCounter - m_uStepStartTick;
        if ( uTicksWaited >  m_uSameFootWaitingLimit ) return false;
        
        FLOAT fRatio = fabs( m_fPreviousPivotPoint / m_fStepPivotPoint );
        if ( fRatio < m_fMinimumConsequentStepRatio ) return true; 
        return false;
    }

    //Look at last to positions.  This is the end of a step.  We're just making sure the difference made it back to positive or negative again.
    BOOL CurrentAndPreviousPositionPositive()
    {
         return ( m_fLastSixDeltas[0] > 0.0f && m_fLastSixDeltas[1] > 0.0f );   
    }
    BOOL CurrentAndPreviousPositionNegative()
    {
         return ( m_fLastSixDeltas[0] < 0.0f && m_fLastSixDeltas[1] < 0.0f );   
    }

    // This is the code that looks in the past to see if we're positive or negative enough.  
    //This is used in conjunction wiht the peak detection code
    BOOL ThreePositionsInPastPostive()
    {
        float fAVG = m_fLastSixDeltas[1] + m_fLastSixDeltas[2] + m_fLastSixDeltas[3];
        fAVG *= 0.3333333334f;
        return ( fAVG > m_fPivotDeadZone && 
            m_fLastSixDeltas[1] > 0.0f && 
            m_fLastSixDeltas[2] > 0.0f &&
            m_fLastSixDeltas[3] > 0.0f 
            );
    }
    BOOL ThreePositionsInPastNegative()
    {
        float fAVG = m_fLastSixDeltas[1] + m_fLastSixDeltas[2] + m_fLastSixDeltas[3];
        fAVG *= 0.3333333334f;
        return ( fAVG < -m_fPivotDeadZone &&
                 m_fLastSixDeltas[1] < 0.0f &&
                 m_fLastSixDeltas[2] < 0.0f &&
                 m_fLastSixDeltas[3] < 0.0f
               );
    }

    BOOL NoiseThresholdPassedInLastThreeDeltas()
    {
        return ( fabs( m_fPreviousPivotPoint - m_fLastSixDeltas[0] ) > m_fNoiseTolerence ||
            fabs( m_fPreviousPivotPoint - m_fLastSixDeltas[1] ) > m_fNoiseTolerence ||
            fabs( m_fPreviousPivotPoint - m_fLastSixDeltas[2] ) > m_fNoiseTolerence );
    
    }

    // Changing foot direction
    VOID SetSlopeAndPivotPositive( )
    {
        m_fSlopeFromBeforePivoting = max( m_fDerivatives[0], m_fDerivatives[1] );
        m_fPreviousPivotPoint = min( min( m_fLastSixDeltas[2], m_fLastSixDeltas[3]), 
                                 min( m_fLastSixDeltas[1], m_fLastSixDeltas[0] ) ) ;
        m_uStepStartTick = m_uTickCounter;   
    }
    VOID SetSlopeAndPivotNegative( )
    {
        m_fSlopeFromBeforePivoting = min( m_fDerivatives[0], m_fDerivatives[1] );
        m_fPreviousPivotPoint = max( max( m_fLastSixDeltas[2], m_fLastSixDeltas[3]), 
                                 max( m_fLastSixDeltas[1], m_fLastSixDeltas[0] ) ) ;
        m_uStepStartTick = m_uTickCounter;   
    }

    VOID Update( FLOAT fNewValue )
    {

        ++m_uTickCounter;
        static bool bFirst = true;
        if ( bFirst )
        {
            bFirst = false;
            for ( int index = 0; index < DELTAS_TO_TRACK; ++index )
            {
               m_fLastSixDeltas[index] = fNewValue; 
            }
        }
        else 
        {
            // Keep a history of the knee deltas
            // this should be done with a circular buffer if DELTAS_TO_TRACK is set very high.
            
            for ( int index = DELTAS_TO_TRACK - 1; index > 0; --index )
            {
                m_fLastSixDeltas[index] = m_fLastSixDeltas[index-1];
            }
            m_fLastSixDeltas[0] = fNewValue;
            // keep last 2 derivatives
            m_fDerivatives[0]  =  m_fLastSixDeltas[0] - m_fLastSixDeltas[1];
            m_fDerivatives[1]  =  m_fLastSixDeltas[1] - m_fLastSixDeltas[2];
        }        

    };

    // Determine if deltas are flattening, and if difference is very near 0
    BOOL FlatteningOut ( ) 
    {
        BOOL bFlattening = TRUE;
        for ( int index = 0; index < FLATTENING_DELTAS; ++index ) 
        {
            bFlattening &= (BOOL)( fabs( m_fLastSixDeltas[index] - m_fLastSixDeltas[index+1] ) < m_fFlatteningTolerence );
            bFlattening &= (BOOL)( fabs( m_fLastSixDeltas[0]) < m_fKneesTogetherTolerence );
        }

        return bFlattening;
    };
    // See comments above
    FLOAT m_fLastSixDeltas[DELTAS_TO_TRACK];
    FLOAT m_fDerivatives[2]; 
    FLOAT m_fSlopeFromBeforePivoting; 
    FLOAT m_fPreviousPivotPoint;
    FLOAT m_fSlopeBufferPadding;
    FLOAT m_fPivotDeadZone;
    FLOAT m_fFlatteningTolerence;
    FLOAT m_fMinimumConsequentStepRatio;
    FLOAT m_fKneesTogetherTolerence;
    FLOAT m_fStepPivotPoint;
    UINT m_uTickCounter;
    UINT m_uStepStartTick;
    UINT m_uSameFootWaitingLimit;
    FLOAT m_fNoiseTolerence; 


};



class CRunningSpeedDetection
{
    static const INT MAX_FOOTFALL_HISTORY = 6;

public:
    CRunningSpeedDetection();

    HRESULT Update( const NUI_SKELETON_DATA* Skeleton );
    FLOAT GetRunningSpeed() { return m_fRunningSpeed; }

    UINT GetStepsSoFar() { return m_uStepsSoFar; }
    VOID ResetStepsSoFar() { m_uStepsSoFar = 0; }
    VOID TakeStep ();


protected:

    std::list<DWORD> m_FootfallTimes;
    FLOAT m_fRunningSpeed;
    UINT m_uStepsSoFar;

    KneeHistoryTracker m_LRKneeDiffHistory;

    TwoKneesStatus m_eKneesStatus;


    FLOAT m_fBigRunTolerence;
};
