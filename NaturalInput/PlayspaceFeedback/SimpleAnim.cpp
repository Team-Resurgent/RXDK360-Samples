//----------------------------------------------------------------------------------------------------------------------
// SimpleAnim.cpp
// 
// Implements simple animations for UI elements.
//
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include <AtgUtil.h>
#include "SimpleAnim.h"

//----------------------------------------------------------------------------------------------------------------------
// Stock Fade Operations
//----------------------------------------------------------------------------------------------------------------------

// Individual operations:

// Fade parameters: Type, FLOAT Target Value, DWORD Param2, FLOAT Param3
//              eg: TYPE_LOOP,              n/a  ,        count, n/a
//                  TYPE_END,               n/a  ,          n/a, n/a
//                  TYPE_SET,              alpha ,          n/a, n/a
//                  TYPE_RAMP,             alpha ,          n/a, rate (delta/sec)

const SimpleAnim FADE_END_SEQUENCE            = { SimpleAnim::TYPE_END };
const SimpleAnim FADE_JAMSET_FULLYOPAQUE      = { SimpleAnim::TYPE_SET, 1.0f, 0, 0 };
const SimpleAnim FADE_JAMSET_FULLYTRANSPARENT = { SimpleAnim::TYPE_SET, 0, 0, 0 };
const SimpleAnim FADE_HALFSEC_TO_TRANSPARENT  = { SimpleAnim::TYPE_RAMP, 0, 0, RateFromSweepTime( 0.5f ) };
const SimpleAnim FADE_HALFSEC_TO_OPAQUE       = { SimpleAnim::TYPE_RAMP, 1.0f, 0, RateFromSweepTime( 0.5f ) };
const SimpleAnim FADE_1SEC_TO_TRANSPARENT     = { SimpleAnim::TYPE_RAMP, 0, 0, RateFromSweepTime( 1.0f ) };
const SimpleAnim FADE_1SEC_TO_OPAQUE          = { SimpleAnim::TYPE_RAMP, 1.0f, 0, RateFromSweepTime( 1.0f ) };
const SimpleAnim FADE_WAIT_HALFSEC            = { SimpleAnim::TYPE_WAIT, 0,  0, 0.5f };
const SimpleAnim FADE_WAIT_1SEC               = { SimpleAnim::TYPE_WAIT, 0, 0, 1.0f };
const SimpleAnim FADE_WAIT_2SEC               = { SimpleAnim::TYPE_WAIT, 0, 0, 2.0f };
const SimpleAnim FADE_WAIT_3SEC               = { SimpleAnim::TYPE_WAIT, 0, 0, 3.0f };
const SimpleAnim FADE_WAIT_4SEC               = { SimpleAnim::TYPE_WAIT, 0, 0, 4.0f };
const SimpleAnim FADE_WAIT_5SEC               = { SimpleAnim::TYPE_WAIT, 0, 0, 5.0f };
const SimpleAnim FADE_INFINITE_LOOP           = { SimpleAnim::TYPE_LOOP, 0, LOOP_FOREVER, 0 };

//----------------------------------------------------------------------------------------------------------------------

// Sequences:

// Fades out the item immediately (equivalent of "Hide")

const SimpleAnim FADE_OUT_IMMEDIATE[] = {
    FADE_JAMSET_FULLYTRANSPARENT,
    FADE_END_SEQUENCE
};

// Fades out the item immediately (equivalent of "Show")

const SimpleAnim FADE_IN_IMMEDIATE[] = {
    FADE_JAMSET_FULLYOPAQUE,
    FADE_END_SEQUENCE
};

// Fades out the item over 1 second (or faster if is already partially transparent).

const SimpleAnim FADE_OUT_HALFSECMAX[] = {
    FADE_HALFSEC_TO_TRANSPARENT,
    FADE_END_SEQUENCE
};

// Fades out the item over 1 second (or faster if is already partially transparent).

const SimpleAnim FADE_OUT_1SECMAX[] = {
    FADE_1SEC_TO_TRANSPARENT,
    FADE_END_SEQUENCE
};

// Fades in the item over 1 second (or faster if already partially opaque)

const SimpleAnim FADE_IN_1SECMAX[] = {
    FADE_1SEC_TO_OPAQUE,
    FADE_END_SEQUENCE
};

// Pulses an item in and out.
// Loops infinitely. Total cycle time 5s.

const SimpleAnim FADE_PULSE_5SEC_INFINITE[] = {
    FADE_INFINITE_LOOP,             // Loops must come at the start of the sequence.
    FADE_JAMSET_FULLYTRANSPARENT,
    FADE_HALFSEC_TO_OPAQUE,
    FADE_WAIT_2SEC,
    FADE_HALFSEC_TO_TRANSPARENT,
    FADE_WAIT_2SEC,
    FADE_END_SEQUENCE
};

const SimpleAnim FADE_PULSE_5SEC_ONCE[] = {
    FADE_JAMSET_FULLYTRANSPARENT,
    FADE_HALFSEC_TO_OPAQUE,
    FADE_WAIT_4SEC,
    FADE_HALFSEC_TO_TRANSPARENT,
    FADE_END_SEQUENCE
};

// Pulses an item in and out. Used with the FADE_PULSE_5SEC_INFINITE, alternates with it.
// Loops infinitely. Total cycle time 5s.

const SimpleAnim FADE_PULSE_5SEC_INFINITE_ALTPHASE[] = {
    FADE_INFINITE_LOOP,             // Loops must come at the start of the sequence.
    FADE_JAMSET_FULLYOPAQUE,
    FADE_HALFSEC_TO_TRANSPARENT,
    FADE_WAIT_2SEC,
    FADE_HALFSEC_TO_OPAQUE,
    FADE_WAIT_2SEC,
    FADE_END_SEQUENCE
};

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: AlphaTrack::AlphaTrack
// Desc: Bind the alpha animation track to a target color value.
//----------------------------------------------------------------------------------------------------------------------
void AlphaTrack::Bind( D3DCOLOR* pTargetColor ) 
{
    m_pTargetValue = pTargetColor;
    m_fCurrentAlpha = D3DColorGetAlphaFloat( *pTargetColor );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: AlphaTrack::SetAnimation
// Desc: Applies an animation to the current track. Any updates which can be performed immediately to the track will be.
//----------------------------------------------------------------------------------------------------------------------
void AlphaTrack::SetAnimation( const SimpleAnim * pSequence )
{
    assert( m_pTargetValue != NULL && "Not bound to target" );

    m_dwLoopCount = 0;

    if ( ( pSequence == NULL ) || ( pSequence->m_type == SimpleAnim::TYPE_END ) )
    {
        Stop();
        return;
    }

    // Handle and eat any loop setup.

    if ( pSequence->m_type == SimpleAnim::TYPE_LOOP )
    {
        // Another edge case
        if ( pSequence[1].m_type == SimpleAnim::TYPE_END )
        {
            assert( !"Empty animation loop found" );
            Stop();
            return;
        }

        m_dwLoopCount = pSequence->m_dwParam;
        m_pSeqLoopStart = ++pSequence;
    }

    m_pSeqBase = pSequence;
    m_pSeqCurrent = pSequence;

    // Run the animation immediately, but with a delta-t of 0.
    Update( 0 );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: AlphaTrack::Update
// Desc: Applies an animation to a track.
//----------------------------------------------------------------------------------------------------------------------
void AlphaTrack::UpdateInternal( FLOAT fDeltaTime )
{
    // Note: We immediately return if we use up the entire timeslice during an operation. 

    while( fDeltaTime > 0.0f || m_pSeqCurrent->m_type != SimpleAnim::TYPE_RAMP )
    {
        assert( m_pSeqCurrent != NULL );

        switch( m_pSeqCurrent->m_type )
        {
            case SimpleAnim::TYPE_END:
            {
                if( m_dwLoopCount != 0 )
                {
                    if( m_dwLoopCount != LOOP_FOREVER )
                    {
                        --m_dwLoopCount;
                    }

                    m_state = ANIMSTATE_LOOPING;
                    m_pSeqCurrent = m_pSeqLoopStart;

                    // NOTE: We're making the assumption that we consume time for each item in the animation.
                    //       More robust code would check for this in an off-line pass.
                    continue;
                }
                else
                {
                    Stop();
                    return;
                }
            }

            case SimpleAnim::TYPE_CALLBACK:
                {
                    // callbacks always end the timeslice, AND they advance the sequencer before they get called.
                    DWORD dwParam = m_pSeqCurrent->m_dwParam;

                    ++m_pSeqCurrent;

                    if ( m_pfCallback != NULL )
                    {
                        (*m_pfCallback)( m_dwCallbackId, dwParam );
                    }
                                        
                    return; 
                }

            case SimpleAnim::TYPE_SET:
                {
                    m_fCurrentAlpha = m_pSeqCurrent->m_fTargetAlphaValue;
                    // Note: TYPE_SET operations don't consume any time.
                    break;
                }

            case SimpleAnim::TYPE_RAMP:
                {
                    ApplyRamp( fDeltaTime );

                    if ( fDeltaTime < FLT_EPSILON )
                    {
                       return;
                    }
                    break;
                }

            case SimpleAnim::TYPE_WAIT:
                {
                    if ( m_fCounter > fDeltaTime )
                    {
                        m_fCounter -= fDeltaTime;
                        fDeltaTime = 0;
                        return;
                    }
                    else
                    {
                        fDeltaTime -= m_fCounter;
                        m_fCounter = 0;
                    }

                    break;
                }
            
            case SimpleAnim::TYPE_LOOP:
            default:
            {
                assert( !"Invalid fade operation found");
                Stop();
                return;
            }
        }

        ++m_pSeqCurrent;

        // If our next item is a TYPE_WAIT, set up the counter.
        if ( m_pSeqCurrent->m_type == SimpleAnim::TYPE_WAIT)
        {
             m_fCounter = m_pSeqCurrent->m_fParam;
        }
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: AlphaTrack::Stop
// Desc: Stops any running animation.
//----------------------------------------------------------------------------------------------------------------------
void AlphaTrack::Stop()
{
    m_pSeqLoopStart = NULL;
    m_pSeqCurrent = NULL;
    m_pSeqBase = NULL;
    m_state = ANIMSTATE_STOPPED;

    // Update the value we're pointing to from our local cache.
    *m_pTargetValue = D3DColorSetAlpha( *m_pTargetValue, m_fCurrentAlpha );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: AlphaTrack::ApplyRamp
// Desc: Performs a time-limited interpolation on the alpha-value to a target value.
//----------------------------------------------------------------------------------------------------------------------
void AlphaTrack::ApplyRamp( FLOAT& fDeltaTime )
{
    FLOAT fAlphaTarget = m_pSeqCurrent->m_fTargetAlphaValue;

    // To ramp within the time remaining, we must calculate:
    // Time required to perform the ramp from Alpha to Target.

    // If this time is > dwMsecsRemaining, we calculate the amount of the ramp we actually can do, and set dwMsecs to 0
    // If this time is <= dwMsecRemaining, we subtract the time required from dwMsecsRemaining, and return that time to
    // the animation system. We then jam-set the current value to the target value.
    // (This helps roundoff errors)

    FLOAT fMaxChangePerSec = m_pSeqCurrent->m_fParam;
    FLOAT fAbsMaxChange = fMaxChangePerSec * fDeltaTime;

    FLOAT fDesiredChange = fAlphaTarget - m_fCurrentAlpha;

    FLOAT fAbsDesiredChange = fabs( fDesiredChange );

    FLOAT fSgnMaxDelta = SignMultiplyF( fDesiredChange, fAbsMaxChange );

     FLOAT fNewAlpha = SelectIfALessThanOrEqualToBF( fAbsDesiredChange, fAbsMaxChange, fAlphaTarget, m_fCurrentAlpha + fSgnMaxDelta);
     m_fCurrentAlpha = fNewAlpha;
 
     FLOAT fTimeConsumed = SelectIfALessThanOrEqualToBF( fAbsDesiredChange, fAbsMaxChange, fAbsDesiredChange / fMaxChangePerSec,
                                                           fDeltaTime );

     fDeltaTime -= fTimeConsumed;

//   Above branchless code equivalent to:
//
//   if ( fAbsDesiredChange > fAbsMaxChange )
//   {
//       m_fCurrentAlpha += fSgnMaxDelta;
//      fDeltaTime = 0.0f;
//   }
//   else
//   {
//      m_fCurrentAlpha = fAlphaTarget;
//      fDeltaTime = fDeltaTime - ( fAbsDesiredChange / fMaxChangePerSec);
//   }
    
}


//----------------------------------------------------------------------------------------------------------------------
// Name: AlphaTrack::Update
// Desc: Updates the animation on this track.
//----------------------------------------------------------------------------------------------------------------------
void AlphaTrack::Update( FLOAT fDeltaTime )
{
    if ( m_pSeqCurrent == NULL || m_pTargetValue == NULL )
        return;

    UpdateInternal( fDeltaTime );

    // Update the value we're pointing to from our local cache.
    *m_pTargetValue = D3DColorSetAlpha( *m_pTargetValue, m_fCurrentAlpha );
}