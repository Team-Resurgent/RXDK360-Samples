//----------------------------------------------------------------------------------------------------------------------
// SimpleAnim.h
// 
// Provides simple, quick & dirty animations for 2D UI elements.
//
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#ifndef SIMPLEANIM_H_GUARD
#define SIMPLEANIM_H_GUARD

//----------------------------------------------------------------------------------------------------------------------
// Color Helpers
//----------------------------------------------------------------------------------------------------------------------

// Mask values for D3DCOLOR channels.
const D3DCOLOR D3DCOLOR_ALPHA_MASK = D3DCOLOR_RGBA(0,0,0,255);
const D3DCOLOR D3DCOLOR_RGB_MASK   = D3DCOLOR_RGBA(255,255,255,0);

//----------------------------------------------------------------------------------------------------------------------
// Name: D3DColorGetAlphaFloat
// Desc: Returns the A parameter from a D3DCOLOR as a float from 0.0-1.0
//----------------------------------------------------------------------------------------------------------------------
inline FLOAT D3DColorGetAlphaFloat( const D3DCOLOR color )
{
    return (D3DCOLOR_GETALPHA( color )) / 255.0f;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: D3DColorSetAlpha
// Desc: Sets the A parameter in a D3DCOLOR value from a float value from 0.0-1.0
//----------------------------------------------------------------------------------------------------------------------
inline D3DCOLOR D3DColorSetAlpha( D3DCOLOR color, FLOAT fAlpha )
{
    color &= D3DCOLOR_RGB_MASK;
    return color | D3DCOLOR_RGBA( 0, 0, 0, (BYTE)( fAlpha * 255.0f ) );
}

//----------------------------------------------------------------------------------------------------------------------
// Fast Float/If helpers
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: SignMultiplyF
// Desc: Takes the sign from signDonor (+/- 1), and multiplies it with value.
//----------------------------------------------------------------------------------------------------------------------
#define SignMultiplyF( signDonor, value ) __fself( signDonor, value, -value )

//----------------------------------------------------------------------------------------------------------------------
// Name: SelectIfAGreaterThanOrEqualToBF
// Desc: Version of __fself which selects one of two values depending on if A >= B, or A < B.
//----------------------------------------------------------------------------------------------------------------------
#define SelectIfAGreaterThanOrEqualToBF( A, B, A_GrtrOrEql_B, A_LessThan_B ) __fself( A - B, A_GrtrOrEql_B, A_LessThan_B )

//----------------------------------------------------------------------------------------------------------------------
// Name: SelectIfALessThanOrEqualToBF
// Desc: Version of __fself which selects one of two values depending on if A <= B, or A > B.
//----------------------------------------------------------------------------------------------------------------------
#define SelectIfALessThanOrEqualToBF( A, B, A_LessOrEql_B, A_GrtrThan_B ) __fself( B - A , A_LessOrEql_B, A_GrtrThan_B )

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: RateFromSweepTime
// Desc: Calculates the rate of change required to do a full-scale sweep in the time provided, returning a value in 
//       units/second.
//----------------------------------------------------------------------------------------------------------------------
inline FLOAT RateFromSweepTime( FLOAT fTimeInSeconds ) 
{ 
    return 1.0f / fTimeInSeconds;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: struct SimpleAnim
// Desc: A single entry in an animation sequence.
//----------------------------------------------------------------------------------------------------------------------
struct SimpleAnim
{
    enum TYPE
    {
        TYPE_LOOP,
        TYPE_WAIT,
        TYPE_SET,
        TYPE_RAMP,
        TYPE_CALLBACK,
        TYPE_END
    };

    TYPE m_type;
    FLOAT m_fTargetAlphaValue;
    DWORD m_dwParam;            // DWORD parameter holder; value depends on m_type.
    FLOAT m_fParam;             // FLOAT parameter holder; value depends on m_type.
};

//----------------------------------------------------------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------------------------------------------------------

// Signals an infinite loop when used as m_dwParam for a TYPE_LOOP entry.
const DWORD LOOP_FOREVER = 0xFFFFFFFF;

// Stock Operations

extern const SimpleAnim FADE_OUT_IMMEDIATE[];
extern const SimpleAnim FADE_OUT_HALFSECMAX[];
extern const SimpleAnim FADE_IN_IMMEDIATE[];
extern const SimpleAnim FADE_OUT_1SECMAX[];
extern const SimpleAnim FADE_IN_1SECMAX[];
extern const SimpleAnim FADE_PULSE_5SEC_ONCE[];
extern const SimpleAnim WAIT_THEN_FADE_5SEC_ONCE[];
extern const SimpleAnim FADE_PULSE_5SEC_INFINITE[];
extern const SimpleAnim FADE_PULSE_5SEC_INFINITE_ALTPHASE[];


//----------------------------------------------------------------------------------------------------------------------
// Name: PFSIMPLEANIMCALLBACK
// Desc: TYPE_CALLBACK function pointer used by animation events.
//----------------------------------------------------------------------------------------------------------------------
typedef void (*PFSIMPLEANIMCALLBACK)(DWORD id, DWORD dwParam );


//----------------------------------------------------------------------------------------------------------------------
// Name: class AlphaTrack
// Desc: A class used to animate the alpha-value of a D3DCOLOR variable.
//----------------------------------------------------------------------------------------------------------------------
class AlphaTrack
{
public:
    enum ANIMSTATE
    {
        ANIMSTATE_RUNNING,
        ANIMSTATE_LOOPING,
        ANIMSTATE_STOPPED
    };

    AlphaTrack() : m_pfCallback( NULL ), m_dwCallbackId( 0 ), m_state( ANIMSTATE_STOPPED ), 
                   m_pTargetValue( NULL ), m_pSeqCurrent( NULL ), m_pSeqLoopStart( NULL)
                    {};
    
    void SetEventCallback( DWORD dwID, PFSIMPLEANIMCALLBACK callback )
    {
        m_dwCallbackId = dwID; m_pfCallback = callback;
    }

    void Bind( D3DCOLOR* pTargetColor );
    void SetAnimation( const SimpleAnim * pSequence );
    void Update( FLOAT fDeltaTime );
    const SimpleAnim* GetCurrentAnimation() const { return m_pSeqBase; };
    ANIMSTATE GetState() const { return m_state; };

private:
    void Stop();
    void ApplyRamp( FLOAT& fDeltaTime );
    void UpdateInternal( FLOAT fDeltaTime );

    PFSIMPLEANIMCALLBACK m_pfCallback;
    DWORD m_dwCallbackId;
    ANIMSTATE m_state;
    D3DCOLOR* m_pTargetValue;
    FLOAT m_fCurrentAlpha;
    FLOAT m_fCounter;
    DWORD m_dwLoopCount;
    
    const SimpleAnim* m_pSeqBase;
    const SimpleAnim* m_pSeqLoopStart;
    const SimpleAnim* m_pSeqCurrent;
};

#endif //SIMPLEANIM_H_GUARD