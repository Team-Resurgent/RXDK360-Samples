//---------------------------------------------------------------------------------------------------------
// AsyncSwaps.cpp
//
// Demonstrates different methods of handling the synchronization to Vblank.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------

#include <xtl.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>


// We support this many front buffers in a round-robin fashion
#define MAX_FRONT_BUFFERS 3


// Unit conversion for SetWaitableTimer.  Negative means relative time.
static CONST LONGLONG                  g_llRelative100NSToMS = -10000LL;


//---------------------------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//---------------------------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, 
        L"Change item focus (u/d)\nDecrease / increase active item (l/r)" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_2, 
        L"Change item focus (u/d)\nDecrease / increase active item (l/r)" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_1, L"Compress graph" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Expand graph" },
};
#define NUM_HELP_CALLOUTS _countof(g_HelpCallouts)


//---------------------------------------------------------------------------------------------------------
// Small helper functions
//---------------------------------------------------------------------------------------------------------
template<typename t_type>
static __forceinline t_type Min( t_type a, t_type b ) { return a < b ? a : b; }
template<typename t_type>
static __forceinline t_type Max( t_type a, t_type b ) { return a > b ? a : b; }
template<typename t_type>
static __forceinline t_type Saturate( t_type a, t_type b, t_type c ) { return Min( Max( a, b ), c ); }
template <typename type_t> 
static __forceinline type_t Lerp( type_t a, type_t b, FLOAT t ) { return a * t + b * ( 1 - t ); }
template <typename type_t> 
static __forceinline type_t Avg( type_t a, type_t b ) { return Lerp( a, b, 0.5f ); }


//---------------------------------------------------------------------------------------------------------
// PixEvent
//
// Class to simplify creation and pairing of Pix event brackets.  Unfortunately, it's hard to pass along 
// varargs verbatim.  So we just use explicit constructors for each vararg pattern we want.
//---------------------------------------------------------------------------------------------------------
class PixEvent
{
public:
    PixEvent( const CHAR* strFormat )
    {
        PIXBeginNamedEvent( 0, strFormat );
    }
    PixEvent( const CHAR* strFormat, UINT i )
    {
        PIXBeginNamedEvent( 0, strFormat, i );
    }
    PixEvent( const CHAR* strFormat, UINT i, UINT j )
    {
        PIXBeginNamedEvent( 0, strFormat, i, j );
    }
    ~PixEvent()
    {
        PIXEndNamedEvent();
    }
};
#define PIX_EVENT_FUNCTION() PixEvent PixEventFunction(__FUNCTION__)
#define PIX_EVENT_SCOPE(strFormat, ...) PixEvent PixEventScope(strFormat, __VA_ARGS__)


//---------------------------------------------------------------------------------------------------------
// RingUInt
//
// Unsigned integer which wraps around at some point t_N.  This prevents us from having to remember to add 
// '% t_N' everywhere, and saves a lot of heartache and debugging.
//---------------------------------------------------------------------------------------------------------
template< UINT t_N >
class RingUInt
{
public:
    __forceinline           RingUInt( UINT i ) : m_i( i % t_N ) {}
    __forceinline           RingUInt(  ) : m_i(0) {}

    __forceinline UINT      Get() const                         { return m_i; }

    __forceinline BOOL      operator == ( RingUInt r ) const    { return m_i == r.m_i; }
    __forceinline BOOL      operator != ( RingUInt r ) const    { return m_i != r.m_i; }

    __forceinline UINT      operator() () const                 { return m_i; }
    __forceinline UINT      operator ++ ()                      { m_i = ++m_i % t_N; return m_i; }
    __forceinline UINT      operator ++ ( int ) /*postfix*/     { return ++*this; }
    __forceinline UINT      operator -- ()                      { m_i = ( m_i + t_N - 1 ) % t_N; return m_i; }
    __forceinline UINT      operator -- ( int ) /*postfix*/     { return --*this; }
    __forceinline RingUInt  operator + ( UINT j ) const         { return RingUInt( m_i + j ); }
    __forceinline RingUInt  operator + ( RingUInt j ) const     { return *this + j.Get(); }
    __forceinline RingUInt  operator - ( UINT j ) const         { return RingUInt( m_i + t_N - j ); }
    __forceinline RingUInt  operator - ( RingUInt j ) const     { return *this - j.Get(); }
    __forceinline RingUInt  operator += ( UINT j )              { return *this = *this + j; }
    __forceinline RingUInt  operator -= ( UINT j )              { return *this = *this - j; }

private:
    UINT m_i;
};


//---------------------------------------------------------------------------------------------------------
// Smoother
//
// Class to smooth noisy data over time.  This prevents the text display from changing so fast that it's
// unreadable to the eye.  
//---------------------------------------------------------------------------------------------------------
class Smoother
{
public:
    Smoother( FLOAT fSmoothingFactor, FLOAT fSmoothingMin, FLOAT fSmoothingMax ) 
        : m_fSmoothingFactor(fSmoothingFactor) 
        , m_fSmoothingMin(fSmoothingMin) 
        , m_fSmoothingMax(fSmoothingMax) 
        , m_fSmoothedValue(0) 
    {}

    FLOAT AddObservedValue( FLOAT fObservedValue ) 
    { 
        // We want smoothed times to converge quickly, and become stable soon afterwards.
        FLOAT fLerpFactor = fabsf( fObservedValue - m_fSmoothedValue ) / m_fSmoothingFactor;
        fLerpFactor = Saturate( fLerpFactor, m_fSmoothingMin, m_fSmoothingMax );

        m_fSmoothedValue = Lerp( fObservedValue, m_fSmoothedValue, fLerpFactor );

        return GetSmoothedValue();
    }

    FLOAT GetSmoothedValue() const
    {
        return m_fSmoothedValue;
    }

private:
    FLOAT m_fSmoothingFactor;   // Should be larger than the typical variation in the data.
                                // Higher factor means slower convergence but less oscillation.  
    FLOAT m_fSmoothingMin;      // (0.0, 1.0] Bound the slowest possible convergence, even if data is highly variable
    FLOAT m_fSmoothingMax;      // (0.0, 1.0] Bound the fastest possible convergence, even if data is coherent

    FLOAT m_fSmoothedValue;
};


//---------------------------------------------------------------------------------------------------------
// PipelinedTimer
//
// Class to record latencies.  There may be several open timings in flight simultaneously, so we cannot 
// use a simple start/stop mechanism.
//
// We also cannot use ATG::Timers, because they don't work in a DPC (probably due to use of FLOAT ops).  
// Many of the latencies are timed within DPC callbacks.  So instead, we use the intrinsic __mftb(), 
// ignoring wraparound and occasional bugs with this instruction (see doc page for __mftb).
// 
// This class is not designed to be thread-safe.  Any synchronization errors should only cause momentary 
// data glitches though, not crash the app.
// 
// There are 3 ring counters maintained here:
// 
//  - m_iStartIndex:        Incremented each time Start is called
//  - m_iStopIndex:         Incremented each time Stop is called
//  - m_iRetrievedIndex:    Incremented each time a Start/Stop bracket is retrieved by the caller 
//                          (and therefore available for re-use)
//
// Underflow occurs if we call Stop before Start.  This is sometimes allowed, if we are measuring events
// with "negative" duration. In particular, the latency between Resolve to front buffer and flip is 
// negative when standard synchronous swaps are used.  The Resolve has a "grace period" of the VBlank 
// duration.
//
// Overflow occurs if we have at least t_iMaxInFlight more Starts than Stops.  In this case, we may have
// underestimated the amount of pipelining.  
//
// Overflow also occurs if we have at t_iMaxInFlight more Starts than Retrieves.  In this case, we are 
// not retrieving the data often enough.
//---------------------------------------------------------------------------------------------------------
template < UINT t_iMaxInFlight >
class PipelinedTimer
{
public:
    PipelinedTimer( UINT iUnderflowTolerance = 0, BOOL bDebug = FALSE )    // change to TRUE to print frame-by-frame debug info 
        : m_bDebug(bDebug)
        , m_iID(s_iInstance++)
        , m_bUnderflowed(FALSE)
        , m_bOverflowed(FALSE)
        , m_iUnderflowTolerance(iUnderflowTolerance)
        , m_iStartIndex(0)
        , m_iStopIndex(0)
        , m_iRetrievedIndex(0)
    {
        ZeroMemory( m_llStartTime, sizeof(m_llStartTime) );
        ZeroMemory( m_llStopTime, sizeof(m_llStopTime) );
        QueryPerformanceFrequency( &m_liPerfFreq );
    }

    // Thi swill run from a DPC, and can therefore execute only certain code.
    // In particular, no blocking and no vector/float operations (see Kernel DPC Callbacks in the docs).
    //
    // The 'm_iUnderflowTolerance' is also to account for the FrontBufferReadyToFlip timer, which can 
    // have negative values.  That's because we stop the Resolve at the Vsync, but it's possible for the
    // front buffer to not be ready until slight after this.
    __forceinline VOID Start()
    {
        m_llStartTime[m_iStartIndex++] = __mftb();
        m_bOverflowed = ( m_iStartIndex + m_iUnderflowTolerance == m_iStopIndex || m_iStartIndex == m_iRetrievedIndex );
    }

    __forceinline VOID Stop()
    {
        m_bUnderflowed = ( m_iStopIndex == m_iStartIndex + m_iUnderflowTolerance );
        m_llStopTime[m_iStopIndex++] = __mftb();
    }

    static UINT GetMaxInFlight()  
    {
        return t_iMaxInFlight;
    }

    UINT GetNumInFlight() const 
    {
        return (m_iStartIndex - m_iStopIndex).Get();
    }

    UINT GetNumUnretrieved() const 
    {
        return (m_iStartIndex - m_iRetrievedIndex).Get();
    }

    VOID DebugUnderflowOverflow() const 
    {
        // If this fires, then either start/stops don't match up, or else there can be more in flight
        // than we provided for, or else we're not retrieving the data fast enough.
        //        
        // The values for num in flight shown in the printf output may fail to match the values at the time 
        // the error occurred.  They also may fail to match the values shown in the debugger, since the 
        // callbacks continue running while this thread is paused.
        if( m_bDebug )
        {
            printf( "PipelineTimer instance #%d:  num in flight = %d\n", m_iID, GetNumInFlight() );
            printf( "PipelineTimer instance #%d:  num unretrieved = %d\n", m_iID, GetNumUnretrieved() );
        }
        else if ( m_bUnderflowed ) 
        {
            printf( "PipelineTimer instance #%d underflow:  num in flight = %d\n", m_iID, GetNumInFlight() );
        }
        else if ( m_bOverflowed ) 
        {
            printf( "PipelineTimer instance #%d overflow:  num in flight = %d\n", m_iID, GetNumInFlight() );
            printf( "PipelineTimer instance #%d overflow:  num unretrieved = %d\n", m_iID, GetNumUnretrieved() );
        }
    }

    // Get all times which have not yet been retrieved.
    VOID GetTimesInMS( FLOAT* fTimes, UINT* iCount ) 
    { 
        DebugUnderflowOverflow();

        *iCount = 0;
        while( m_iRetrievedIndex != m_iStopIndex && m_iRetrievedIndex != m_iStartIndex )
        {
            fTimes[*iCount] = (FLOAT) ( m_llStopTime[m_iRetrievedIndex.Get()] - m_llStartTime[m_iRetrievedIndex.Get()] ) 
                / (FLOAT) m_liPerfFreq.QuadPart * 1000.0f;
            ++*iCount; 
            ++m_iRetrievedIndex;
        }
    }

private:
    // Debugging only
    BOOL                            m_bDebug;
    static UINT                     s_iInstance;
    UINT                            m_iID;
    BOOL                            m_bUnderflowed;
    BOOL                            m_bOverflowed;

    // Non-debug data
    UINT                            m_iUnderflowTolerance;
    LARGE_INTEGER                   m_liPerfFreq;
    RingUInt<t_iMaxInFlight>        m_iStartIndex;
    LONGLONG                        m_llStartTime[t_iMaxInFlight];
    RingUInt<t_iMaxInFlight>        m_iStopIndex;
    LONGLONG                        m_llStopTime[t_iMaxInFlight];
    RingUInt<t_iMaxInFlight>        m_iRetrievedIndex;
};

template<UINT t_iMaxInFlight> UINT PipelinedTimer<t_iMaxInFlight>::s_iInstance = 0;


//---------------------------------------------------------------------------------------------------------
// enum PRESENT_INTERVAL:
// 
// UI control to set target frame rate of 60 Hz, 30 Hz, 20 Hz.
//---------------------------------------------------------------------------------------------------------
enum PRESENT_INTERVAL
{
    PRESENT_INTERVAL_ONE, 
    PRESENT_INTERVAL_TWO, 
    PRESENT_INTERVAL_THREE, 

    PRESENT_INTERVAL_COUNT
};

const WCHAR* g_strPresentIntervalNames[] = 
{
    L"1 (60 Hz)", 
    L"2 (30 Hz)", 
    L"3 (20 Hz)", 
};
C_ASSERT( _countof(g_strPresentIntervalNames) == PRESENT_INTERVAL_COUNT );


//---------------------------------------------------------------------------------------------------------
// enum ASYNC_THROTTLING_METHOD:
// 
// How do we stall to wait for a front buffer to be free?
//---------------------------------------------------------------------------------------------------------
enum ASYNC_THROTTLING_METHOD
{
    ASYNC_THROTTLING_METHOD_CPU_SPINLOCK, 
    ASYNC_THROTTLING_METHOD_GPU_STALL, 

    ASYNC_THROTTLING_METHOD_COUNT
};

const WCHAR* g_strAsyncThrottlingMethodNames[] = 
{
    L"CPU spinlock", 
    L"GPU stall", 
};
C_ASSERT( _countof(g_strAsyncThrottlingMethodNames) == ASYNC_THROTTLING_METHOD_COUNT );


//---------------------------------------------------------------------------------------------------------
// enum THROTTLING_POINT:
// 
// At what point do we stall when too much rendering is buffered up (either in D3D or in pending front 
// buffers)?  
//
// For synchronous swaps, the frame at which we throttle is always the next frame (this is a
// D3D restriction).  In this case, THROTTLING_POINT_START_OF_FRAME <---> !D3DRS_BUFFER2FRAMES, and 
// THROTTLING_POINT_END_OF_FRAME <---> D3DRS_BUFFER2FRAMES.
//
// For async swaps, we throttle at the frame at which we are about to run out of front buffers.
// So THROTTLING_POINT_END_OF_FRAME means "right before we hit a Resolve which will overwrite the actively
// displayed frame image".
//
// The notion of 'mid-frame' for THROTTLING_POINT_MID_FRAME is controlled by a separate parameter.
//---------------------------------------------------------------------------------------------------------
enum THROTTLING_POINT
{
    THROTTLING_POINT_START_OF_FRAME, 
    THROTTLING_POINT_MID_FRAME, 
    THROTTLING_POINT_END_OF_FRAME, 

    THROTTLING_POINT_COUNT
};

const WCHAR* g_strThrottlingPointNames[] = 
{
    L"Start of frame", 
    L"Mid-frame", 
    L"End of frame", 
};
C_ASSERT( _countof(g_strThrottlingPointNames) == THROTTLING_POINT_COUNT );


//---------------------------------------------------------------------------------------------------------
// Supported tweakable UI parameters.     
//---------------------------------------------------------------------------------------------------------
enum UIParamTypes 
{
    // Parameters which simulate title CPU and GPU processing load
    UI_PARAM_PROCESSOR_LOAD_PRESET,
    UI_PARAM_PRESENT_INTERVAL, 
    UI_PARAM_SIMULATED_CPU_LOAD, 
    UI_PARAM_SIMULATED_CPU_SPIKE, 
    UI_PARAM_SIMULATED_GPU_LOAD, 
    UI_PARAM_SIMULATED_GPU_SPIKE, 
    UI_PARAM_SIMULATED_SPIKE_FRAMES, 
    UI_PARAM_SIMULATED_SPIKE_PERIOD, 
    UI_PARAM_SIMULATED_GPU_STARVATION, 

    // Parameters which control frame cadence
    UI_PARAM_SWAP_METHOD_PRESET, 
    UI_PARAM_LOCK_FLIP_TO_VSYNC, 
    UI_PARAM_USE_PRESENT_THRESHOLD, 
    UI_PARAM_PRESENT_THRESHOLD, 
    UI_PARAM_THROTTLING_POINT, 
    UI_PARAM_MID_FRAME_THRESHOLD, 
    UI_PARAM_ASYNC_SWAPS, 
    UI_PARAM_NUM_FRONT_BUFFERS, 
    UI_PARAM_ASYNC_THROTTLING_METHOD, 

    UI_PARAM_COUNT
};


//--------------------------------------------------------------------------------------
// ProcessorLoadPresets:
//
// Each of the following determines a particular combination of the processor load
// options.
//--------------------------------------------------------------------------------------
enum PROCESSOR_LOAD_PRESET 
{
    // 60 Hz presets
    PROCESSOR_LOAD_PRESET_60HZ_MARGINAL_CPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_60HZ_MARGINAL_GPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_60HZ_SHORT_TERM_CPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_60HZ_SHORT_TERM_GPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_60HZ_LONG_TERM_CPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_60HZ_LONG_TERM_GPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_60HZ_CPU_GPU_SYNC_BOUND,

    // 30 Hz presets
    PROCESSOR_LOAD_PRESET_30HZ_MARGINAL_CPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_30HZ_MARGINAL_GPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_30HZ_SHORT_TERM_CPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_30HZ_SHORT_TERM_GPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_30HZ_LONG_TERM_CPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_30HZ_LONG_TERM_GPU_OVERLOAD, 
    PROCESSOR_LOAD_PRESET_30HZ_CPU_GPU_SYNC_BOUND,

    // Special case
    PROCESSOR_LOAD_PRESET_UNSTABLE_FRAME_RATE_WITHOUT_JITTER,

    PROCESSOR_LOAD_PRESET_COUNT
};

const WCHAR* g_strProcessorLoadPresetNames[] = 
{
    // 60 Hz presets
    L"Marginal CPU overload (60 Hz)",
    L"Marginal GPU overload (60 Hz)",
    L"Short-term CPU overload (60 Hz)",
    L"Short-term GPU overload (60 Hz)",
    L"Long-term CPU overload (60 Hz)",
    L"Long-term GPU overload (60 Hz)",
    L"CPU/GPU sync bound (60 Hz)",

    // 30 Hz presets
    L"Marginal CPU overload (30 Hz)",
    L"Marginal GPU overload (30 Hz)",
    L"Short-term CPU overload (30 Hz)",
    L"Short-term GPU overload (30 Hz)",
    L"Long-term CPU overload (30 Hz)",
    L"Long-term GPU overload (30 Hz)",
    L"CPU/GPU sync bound (30 Hz)",

    // Special case
    L"Unstable frame rate without jitter",
};
C_ASSERT( _countof(g_strProcessorLoadPresetNames) == PROCESSOR_LOAD_PRESET_COUNT );



// These are the params controlled by presets.  All other params
// are freely modifiable without affecting presets
static CONST UINT g_iProcessorLoadPresetParams[] = 
{
    UI_PARAM_PRESENT_INTERVAL, 
    UI_PARAM_SIMULATED_CPU_LOAD, 
    UI_PARAM_SIMULATED_CPU_SPIKE, 
    UI_PARAM_SIMULATED_GPU_LOAD, 
    UI_PARAM_SIMULATED_GPU_SPIKE, 
    UI_PARAM_SIMULATED_SPIKE_FRAMES, 
    UI_PARAM_SIMULATED_SPIKE_PERIOD, 
    UI_PARAM_SIMULATED_GPU_STARVATION, 
};

typedef UINT ProcessorLoadPreset[_countof(g_iProcessorLoadPresetParams)];

ProcessorLoadPreset g_ProcessorLoadPresets[] = 
{
    //PROCESSOR_LOAD_PRESET_60HZ_MARGINAL_CPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_ONE,   //UI_PARAM_PRESENT_INTERVAL, 
        16,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        16,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        10,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        10,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        0,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        50,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_60HZ_MARGINAL_GPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_ONE,   //UI_PARAM_PRESENT_INTERVAL, 
        10,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        10,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        16,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        16,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        0,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        50,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_60HZ_SHORT_TERM_CPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_ONE,   //UI_PARAM_PRESENT_INTERVAL, 
        15,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        20,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        10,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        10,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        5,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        50,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_60HZ_SHORT_TERM_GPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_ONE,   //UI_PARAM_PRESENT_INTERVAL, 
        10,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        10,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        14,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        20,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        5,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        50,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_60HZ_LONG_TERM_CPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_ONE,   //UI_PARAM_PRESENT_INTERVAL, 
        20,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        20,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        10,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        10,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        0,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        50,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_60HZ_LONG_TERM_GPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_ONE,   //UI_PARAM_PRESENT_INTERVAL, 
        10,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        10,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        20,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        20,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        0,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        50,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_60HZ_CPU_GPU_SYNC_BOUND,
    {
        PRESENT_INTERVAL_ONE,   //UI_PARAM_PRESENT_INTERVAL, 
        15,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        15,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        14,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        14,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        0,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        50,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        3,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_30HZ_MARGINAL_CPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_TWO,   //UI_PARAM_PRESENT_INTERVAL, 
        33,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        33,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        20,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        20,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        0,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        25,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_30HZ_MARGINAL_GPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_TWO,   //UI_PARAM_PRESENT_INTERVAL, 
        20,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        20,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        33,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        33,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        0,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        25,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_30HZ_SHORT_TERM_CPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_TWO,   //UI_PARAM_PRESENT_INTERVAL, 
        30,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        40,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        20,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        20,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        5,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        25,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_30HZ_SHORT_TERM_GPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_TWO,   //UI_PARAM_PRESENT_INTERVAL, 
        20,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        20,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        30,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        40,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        5,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        25,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_30HZ_LONG_TERM_CPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_TWO,   //UI_PARAM_PRESENT_INTERVAL, 
        40,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        40,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        20,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        20,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        0,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        25,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_30HZ_LONG_TERM_GPU_OVERLOAD, 
    {
        PRESENT_INTERVAL_TWO,   //UI_PARAM_PRESENT_INTERVAL, 
        20,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        20,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        40,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        40,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        0,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        25,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_30HZ_CPU_GPU_SYNC_BOUND,
    {
        PRESENT_INTERVAL_TWO,   //UI_PARAM_PRESENT_INTERVAL, 
        30,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        30,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        30,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        30,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        0,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        25,                     //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        5,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 

    //PROCESSOR_LOAD_PRESET_UNSTABLE_FRAME_RATE_WITHOUT_JITTER
    {
        PRESENT_INTERVAL_ONE,   //UI_PARAM_PRESENT_INTERVAL, 
        10,                     //UI_PARAM_SIMULATED_CPU_LOAD, 
        10,                     //UI_PARAM_SIMULATED_CPU_SPIKE, 
        10,                     //UI_PARAM_SIMULATED_GPU_LOAD, 
        25,                     //UI_PARAM_SIMULATED_GPU_SPIKE, 
        1,                      //UI_PARAM_SIMULATED_SPIKE_FRAMES, 
        2,                      //UI_PARAM_SIMULATED_SPIKE_PERIOD, 
        0,                      //UI_PARAM_SIMULATED_GPU_STARVATION, 
    }, 
};
C_ASSERT( _countof(g_ProcessorLoadPresets) == PROCESSOR_LOAD_PRESET_COUNT );


//--------------------------------------------------------------------------------------
// SwapMethodPresets:
//
// Each of the following determines a particular combination of the processor load
// options.
//--------------------------------------------------------------------------------------
enum SWAP_METHOD_PRESET 
{
    SWAP_METHOD_PRESET_STANDARD, 
    SWAP_METHOD_PRESET_IMMEDIATE, 
    SWAP_METHOD_PRESET_PRESENT_THRESHOLD_15, 
    SWAP_METHOD_PRESET_SYNC_BUFFER2FRAMES, 
    SWAP_METHOD_PRESET_ASYNC, 

    SWAP_METHOD_PRESET_COUNT
};


const WCHAR* g_strSwapMethodPresetNames[] = 
{
    L"D3D defaults",
    L"Immediate",
    L"Present threshold 15",
    L"Buffer 2 frames",
    L"Async swaps",
};
C_ASSERT( _countof(g_strSwapMethodPresetNames) == SWAP_METHOD_PRESET_COUNT );



// These are the params controlled by presets.  All other params
// are freely modifiable without affecting presets
static CONST UINT g_iSwapMethodPresetParams[] = 
{
    UI_PARAM_LOCK_FLIP_TO_VSYNC, 
    UI_PARAM_USE_PRESENT_THRESHOLD, 
    UI_PARAM_PRESENT_THRESHOLD, 
    UI_PARAM_THROTTLING_POINT, 
    UI_PARAM_MID_FRAME_THRESHOLD, 
    UI_PARAM_ASYNC_SWAPS, 
    UI_PARAM_NUM_FRONT_BUFFERS, 
    UI_PARAM_ASYNC_THROTTLING_METHOD, 
};

typedef UINT SwapMethodPreset[_countof(g_iSwapMethodPresetParams)];

SwapMethodPreset g_SwapMethodPresets[] = 
{
    //SWAP_METHOD_PRESET_STANDARD, 
    {
        TRUE,                                   //UI_PARAM_LOCK_FLIP_TO_VSYNC, 
        FALSE,                                  //UI_PARAM_USE_PRESENT_THRESHOLD, 
        0,                                      //UI_PARAM_PRESENT_THRESHOLD, 
        THROTTLING_POINT_START_OF_FRAME,        //UI_PARAM_THROTTLING_POINT, 
        50,                                     //UI_PARAM_MID_FRAME_THRESHOLD, 
        FALSE,                                  //UI_PARAM_ASYNC_SWAPS, 
        2,                                      //UI_PARAM_NUM_FRONT_BUFFERS, 
        ASYNC_THROTTLING_METHOD_CPU_SPINLOCK,   //UI_PARAM_ASYNC_THROTTLING_METHOD, 
    },

    //SWAP_METHOD_PRESET_IMMEDIATE, 
    {
        FALSE,                                  //UI_PARAM_LOCK_FLIP_TO_VSYNC, 
        FALSE,                                  //UI_PARAM_USE_PRESENT_THRESHOLD, 
        0,                                      //UI_PARAM_PRESENT_THRESHOLD, 
        THROTTLING_POINT_START_OF_FRAME,        //UI_PARAM_THROTTLING_POINT, 
        50,                                     //UI_PARAM_MID_FRAME_THRESHOLD, 
        FALSE,                                  //UI_PARAM_ASYNC_SWAPS, 
        2,                                      //UI_PARAM_NUM_FRONT_BUFFERS, 
        ASYNC_THROTTLING_METHOD_CPU_SPINLOCK,   //UI_PARAM_ASYNC_THROTTLING_METHOD, 
    },

    //SWAP_METHOD_PRESET_PRESENT_THRESHOLD_15, 
    {
        TRUE,                                   //UI_PARAM_LOCK_FLIP_TO_VSYNC, 
        TRUE,                                   //UI_PARAM_USE_PRESENT_THRESHOLD, 
        15,                                     //UI_PARAM_PRESENT_THRESHOLD, 
        THROTTLING_POINT_START_OF_FRAME,        //UI_PARAM_THROTTLING_POINT, 
        50,                                     //UI_PARAM_MID_FRAME_THRESHOLD, 
        FALSE,                                  //UI_PARAM_ASYNC_SWAPS, 
        2,                                      //UI_PARAM_NUM_FRONT_BUFFERS, 
        ASYNC_THROTTLING_METHOD_CPU_SPINLOCK,   //UI_PARAM_ASYNC_THROTTLING_METHOD, 
    },

    //SWAP_METHOD_PRESET_SYNC_BUFFER2FRAMES, 
    {
        TRUE,                                   //UI_PARAM_LOCK_FLIP_TO_VSYNC, 
        FALSE,                                  //UI_PARAM_USE_PRESENT_THRESHOLD, 
        0,                                      //UI_PARAM_PRESENT_THRESHOLD, 
        THROTTLING_POINT_END_OF_FRAME,          //UI_PARAM_THROTTLING_POINT, 
        50,                                     //UI_PARAM_MID_FRAME_THRESHOLD, 
        FALSE,                                  //UI_PARAM_ASYNC_SWAPS, 
        2,                                      //UI_PARAM_NUM_FRONT_BUFFERS, 
        ASYNC_THROTTLING_METHOD_CPU_SPINLOCK,   //UI_PARAM_ASYNC_THROTTLING_METHOD, 
    },

    //SWAP_METHOD_PRESET_ASYNC, 
    {
        TRUE,                                   //UI_PARAM_LOCK_FLIP_TO_VSYNC, 
        FALSE,                                  //UI_PARAM_USE_PRESENT_THRESHOLD, 
        0,                                      //UI_PARAM_PRESENT_THRESHOLD, 
        THROTTLING_POINT_END_OF_FRAME,          //UI_PARAM_THROTTLING_POINT, 
        50,                                     //UI_PARAM_MID_FRAME_THRESHOLD, 
        TRUE,                                   //UI_PARAM_ASYNC_SWAPS, 
        2,                                      //UI_PARAM_NUM_FRONT_BUFFERS, 
        ASYNC_THROTTLING_METHOD_CPU_SPINLOCK,   //UI_PARAM_ASYNC_THROTTLING_METHOD, 
    },
};


//---------------------------------------------------------------------------------------------------------
// UIParam
//
// Base class for various types of menu selectors
//---------------------------------------------------------------------------------------------------------
class UIParam
{
public:
    static const DWORD  m_dwActiveParamColor = 0xffffff00;   
    static const DWORD  m_dwInactiveParamColor = 0xffffffff;   
    static const DWORD  m_dwActiveOptionColor = 0xff00ff00;   
    static const DWORD  m_dwInactiveOptionColor = 0xff808080;   
    static const DWORD  m_dwInvalidOptionColor = 0x80404040;   
    static const FLOAT  m_fActiveParamScale;
    static const FLOAT  m_fInactiveParamScale;
    static const FLOAT  m_fParamX;
    static const FLOAT  m_fOptionX;

    UIParam( const WCHAR* ParamName ) : m_ParamName( ParamName ), m_bValid( TRUE )
    {}

    virtual VOID RenderOptionUI( ATG::Font* pFont, FLOAT fParamX, FLOAT fParamY, BOOL bActive, 
        BOOL bValid ) = 0;

    VOID RenderUI( ATG::Font* pFont, FLOAT fParamX, FLOAT fParamY, BOOL bActive )
    {
        if( bActive ) 
        {
            pFont->SetScaleFactors( m_fActiveParamScale, m_fActiveParamScale );
            pFont->DrawText( fParamX, fParamY, m_dwActiveParamColor, m_ParamName );
        }
        else
        {
            pFont->SetScaleFactors( m_fInactiveParamScale, m_fInactiveParamScale );
            pFont->DrawText( fParamX, fParamY, m_dwInactiveParamColor, m_ParamName );
        }
        RenderOptionUI( pFont, fParamX, fParamY, bActive, m_bValid );
    }

    virtual VOID        DecreaseValue( FLOAT fScale = 1.0f ) = 0;
    virtual VOID        IncreaseValue( FLOAT fScale = 1.0f ) = 0;
    virtual VOID        SetValue( UINT iValue ) = 0;

    virtual WCHAR*      GetOptionName() = 0;

    BOOL                GetValid() { return m_bValid; }
    VOID                SetValid( BOOL bValid ) { m_bValid = bValid; }

protected:
    const WCHAR*        m_ParamName;
    BOOL                m_bValid;
};

const FLOAT UIParam::m_fActiveParamScale = 0.8f;
const FLOAT UIParam::m_fInactiveParamScale = 0.8f;
const FLOAT UIParam::m_fOptionX = 300.0f;

class UIParamEnum : public UIParam
{
public:
    UIParamEnum( const WCHAR* ParamName, const WCHAR** OptionNames, UINT iCount, UINT iValue = 0, UINT iStep = 1 )
        : UIParam( ParamName )
        , m_OptionNames( OptionNames )
        , m_iCount( iCount )
        , m_iValue( iValue )
        , m_iStep( iStep )
    {}

    VOID RenderOptionUI( ATG::Font* pFont, FLOAT fParamX, FLOAT fParamY, BOOL bActive, BOOL bValid )
    {
        const WCHAR *OptionText = GetOptionName();
        DWORD dwOptionTextColor = bValid 
            ? ( bActive ? m_dwActiveOptionColor : m_dwInactiveOptionColor )
            : m_dwInvalidOptionColor;
        WCHAR SelectText[256];
        if( bActive ) 
        {
            swprintf_s( SelectText, L"< %s >", OptionText );
            OptionText = SelectText;
        }
        pFont->DrawText( fParamX + m_fOptionX, fParamY, dwOptionTextColor, OptionText );
    }

    virtual WCHAR* GetOptionName() 
    { 
        UINT iValue = GetValue( );
        return GetValid() ? m_OptionNames[iValue] : L"n/a"; 
    }

    UINT                GetValue() { return m_iValue; };
    VOID                SetValue( UINT iValue ) { m_iValue = iValue; };
    VOID                DecreaseValue( FLOAT fScale = 1.0f ) { m_iValue += m_iCount - m_iStep; m_iValue %= m_iCount; }
    VOID                IncreaseValue( FLOAT fScale = 1.0f ) { m_iValue += m_iStep;            m_iValue %= m_iCount; }

protected:
    const WCHAR**       m_OptionNames;
    UINT                m_iCount;
    UINT                m_iValue;
    UINT                m_iStep;
};

static const WCHAR* g_BoolOptionNames[2] = { L"FALSE", L"TRUE" };
class UIParamBool : public UIParamEnum
{
public:
    UIParamBool( const WCHAR* ParamName, BOOL bValue = FALSE )
        : UIParamEnum( ParamName, g_BoolOptionNames, 2, (UINT) bValue )
    {}

    BOOL                GetValue() { return UIParamEnum::GetValue() == 0 ? FALSE : TRUE; };
};

static const WCHAR* g_UIntOptionNames[] = { 
     L"0",  L"1",  L"2",  L"3",  L"4",  L"5",  L"6",  L"7",  L"8",   L"9", 
    L"10", L"11", L"12", L"13", L"14", L"15", L"16", L"17", L"18", L"19", 
    L"20", L"21", L"22", L"23", L"24", L"25", L"26", L"27", L"28", L"29", 
    L"30", L"31", L"32", L"33", L"34", L"35", L"36", L"37", L"38", L"39", 
    L"40", L"41", L"42", L"43", L"44", L"45", L"46", L"47", L"48", L"49", 
    L"50", L"51", L"52", L"53", L"54", L"55", L"56", L"57", L"58", L"59", 
    L"60", L"61", L"62", L"63", L"64", L"65", L"66", L"67", L"68", L"69", 
    L"70", L"71", L"72", L"73", L"74", L"75", L"76", L"77", L"78", L"79", 
    L"80", L"81", L"82", L"83", L"84", L"85", L"86", L"87", L"88", L"89", 
    L"90", L"91", L"92", L"93", L"94", L"95", L"96", L"97", L"98", L"99", 
    L"100", 
};
class UIParamUInt : public UIParamEnum
{
public:
    UIParamUInt( const WCHAR* ParamName, UINT iMin, UINT iMax, UINT iValue = 0, UINT iStep = 1 )
        : UIParamEnum( ParamName, g_UIntOptionNames + iMin, iMax - iMin + 1, iValue - iMin, iStep )
        , m_iMin( iMin )
    {
        assert( iMax / iStep * iStep <= _countof( g_UIntOptionNames ) );
    }

    BOOL                GetValue( ) { return UIParamEnum::GetValue( ) + m_iMin; };
    VOID                SetValue( UINT iValue ) { UIParamEnum::SetValue( iValue - m_iMin ); };

private:
    UINT m_iMin;
};


//---------------------------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//---------------------------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // ATG helper items
    ATG::Font                       m_Font;                 // Font for drawing text
    ATG::Help                       m_Help;                 // Display help
    BOOL                            m_bDrawHelp;

    // Zoom level of graph
    FLOAT                           m_fZoom;

    // Helpers for latency indicator
    ATG::Timer                      m_Timer;
    DOUBLE                          m_lfSystemTimeInMS;
    FLOAT                           m_fSpeedInScreensPerMS;

    // UI elements and parameters
    BOOL                            m_bBigMenu;
    RingUInt<UI_PARAM_COUNT>        m_iActiveUIParameter;   // Which UI item is affected by l/r input
    RingUInt<UI_PARAM_COUNT>        m_iVisibleUIStart;
    const static UINT               m_iVisibleUICount = 15;

    UIParamEnum                     m_ProcessorLoadPresetParam;
    UIParamEnum                     m_PresentIntervalParam;
    UIParamUInt                     m_SimulatedCPULoadParam;
    UIParamUInt                     m_SimulatedCPUSpikeParam;
    UIParamUInt                     m_SimulatedGPULoadParam;
    UIParamUInt                     m_SimulatedGPUSpikeParam;
    UIParamUInt                     m_SimulatedSpikeFramesParam;
    UIParamUInt                     m_SimulatedSpikePeriodParam;
    UIParamUInt                     m_SimulatedGPUStarvationParam;

    UIParamEnum                     m_SwapMethodPresetParam;
    UIParamBool                     m_LockFlipToVsyncParam;
    UIParamBool                     m_UsePresentThresholdParam;
    UIParamUInt                     m_PresentThresholdParam;
    UIParamEnum                     m_ThrottlingPointParam;
    UIParamUInt                     m_MidFrameThresholdParam;
    UIParamBool                     m_AsyncSwapsParam;
    UIParamUInt                     m_NumFrontBuffersParam;
    UIParamEnum                     m_AsyncThrottlingMethodParam;

    // The Params as the Menu sees them
    UIParam*                        m_UIParamArray[UI_PARAM_COUNT];

    // Multiple front buffers
    IDirect3DTexture9*              m_pFrontBuffer[MAX_FRONT_BUFFERS];
    UINT                            m_iFrontBufferIndex;

    // The max #GPU frames in flight is the number of frame buffers, plus the
    // frame currently being rendered, plus possibly a frame being buffered in D3D.
    static CONST UINT               m_iMaxGPUFramesInFlight = MAX_FRONT_BUFFERS + 2;
    RingUInt<m_iMaxGPUFramesInFlight> m_iGPUFrameIndex;

    // The max #swaps which we could conceivably issue between Vsyncs.  It simplifies the code to
    // have this be the same as m_iMaxGPUFramesInFlight.  We enforce this by minimum bounds on 
    // the simulated processor load.  Otherwise, with IMMEDIATE present mode, we could do
    // dozens of Swaps per VBlank.
    static CONST UINT               m_iMaxSwapsInFlight = m_iMaxGPUFramesInFlight;
    static CONST UINT               m_iMaxLatenciesInFlight = m_iMaxGPUFramesInFlight;
    
    // Latency timers
    enum LATENCY_TIMER
    {
        LATENCY_TIMER_D3D_START_TO_GPU_START, 
        LATENCY_TIMER_GPU_START_TO_BACK_BUFFER_READY, 
        LATENCY_TIMER_BACK_BUFFER_READY_TO_FRONT_BUFFER_READY, 
        LATENCY_TIMER_FRONT_BUFFER_READY_TO_FLIP, 

        LATENCY_TIMER_COUNT, 
    };
    typedef PipelinedTimer<m_iMaxLatenciesInFlight> PipelinedLatencyTimer;
    PipelinedLatencyTimer*          m_pLatencyTimer[LATENCY_TIMER_COUNT];

    // CPU simulated frame load primitives
    HANDLE                          m_hWaitableTimerSimulatedCPULoad;
    HANDLE                          m_hWaitableTimerSimulatedCPUGPULag;
    
    // GPU simulated frame load primitives
    D3DAsyncCommandBufferCall*      m_pAsyncStallSimulatedGPULoad[m_iMaxGPUFramesInFlight];
    D3DAsyncCommandBufferCall*      m_pAsyncStallMidFrameSimulatedGPULoad[m_iMaxGPUFramesInFlight];
    D3DAsyncCommandBufferCall*      m_pAsyncStallAsyncThrottling[m_iMaxSwapsInFlight];
    HANDLE                          m_hThreadWaitOnSimulatedGPULoad;
    HANDLE                          m_hWaitableTimerSimulatedGPULoad;
    HANDLE                          m_hWaitableTimerMidFrameSimulatedGPULoad;

    // Throttling fences
    DWORD                           m_dwMidFrameFence[m_iMaxGPUFramesInFlight];
    DWORD                           m_dwEndFrameFence[m_iMaxGPUFramesInFlight];

    // Signals for Swap/VBlank events, etc
    HANDLE                          m_hSwapEvent;
    HANDLE                          m_hThreadMarkSwaps;
    HANDLE                          m_hVBlankEvent;
    HANDLE                          m_hThreadMarkVBlanks;
    HANDLE                          m_hVBlankThrottleSemaphore;
    HANDLE                          m_hThreadGpuThrottle;
    HANDLE                          m_hStartGPULoadTimerEvent;
    HANDLE                          m_hThreadReportGPUTimerStart;

    // Make changes in response to UI actions
    VOID ResetSwapAndPresentModes();

    // Enforce the user-selected CPU and GPU loads
    VOID StartSimulatedFrame();
    VOID MidSimulatedFrame();
    VOID EndSimulatedFrame();

    // Async throttling
    VOID WaitOnFreeFrontBuffer();

    // Present using regular swaps or async swaps
    HRESULT Present();

    // UI rendering
    VOID RenderUI();
    VOID RenderPerformanceData();

    // Ring buffers to record swap and vblank data
    // m_iMaxSwapsPerRender is the max # times we could hit a swap callback during one game loop.
    // m_iMaxVBlanksPerRender is the max # times we could hit a VBlank callback during one game loop.
    static CONST UINT               m_iMaxSwapsPerRender = MAX_FRONT_BUFFERS + 1; // Maybe less?
    RingUInt<m_iMaxSwapsPerRender>  m_iSwapDataIndex;
    D3DSWAPDATA                     m_SwapDataRing[m_iMaxSwapsPerRender];
    D3DRASTER_STATUS                m_RasterStatusRing[m_iMaxSwapsPerRender];
    static CONST UINT               m_iMaxVBlanksPerRender = 10; // Dependent on frame rate
    RingUInt<m_iMaxVBlanksPerRender> m_iVBlankDataIndex;
    D3DVBLANKDATA                   m_VBlankDataRing[m_iMaxVBlanksPerRender];

    // Counters to mark the Swap and VBlank number in PIX
    UINT                            m_iSwapDataSwap;
    UINT                            m_iSwapDataVBlank;
    UINT                            m_iVBlankDataSwap;
    UINT                            m_iVBlankDataVBlank;

    // Callback functions.  Each static function calls in to the non-static function.
    static VOID SwapCallback( D3DSWAPDATA* pData );
    VOID _SwapCallback( D3DSWAPDATA* pData );
    static VOID VBlankCallback( D3DVBLANKDATA* pData );
    VOID _VBlankCallback( D3DVBLANKDATA* pData );
    static VOID GPUCallbackStartOfFrame( DWORD Context );
    VOID _GPUCallbackStartOfFrame();
    static VOID GPUCallbackBackBufferReady( DWORD Context );
    VOID _GPUCallbackBackBufferReady();
    static VOID GPUCallbackFrontBufferReady( DWORD Context );
    VOID _GPUCallbackFrontBufferReady();
    
    // Auxiliary threads
    static DWORD WaitOnSimulatedGPULoad( LPVOID lpThreadParameter );
    VOID _WaitOnSimulatedGPULoad();
    static DWORD MarkSwaps( LPVOID lpThreadParameter );
    VOID _MarkSwaps();
    static DWORD MarkVBlanks( LPVOID lpThreadParameter );
    VOID _MarkVBlanks();
    static DWORD GPUThrottle( LPVOID lpThreadParameter );
    VOID _GPUThrottle();
    static DWORD ReportGPUTimerStart( LPVOID lpThreadParameter );
    VOID _ReportGPUTimerStart();

public:
    Sample();

    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};

// Global access for callbacks
static Sample atgApp;


//---------------------------------------------------------------------------------------------------------
// Name: Sample::Sample
// Desc: Initializes the UI elements.
//---------------------------------------------------------------------------------------------------------
Sample::Sample()
: m_ProcessorLoadPresetParam( L"PROCESSOR LOAD PRESET", g_strProcessorLoadPresetNames, _countof(g_strProcessorLoadPresetNames) )
, m_PresentIntervalParam( L"Present interval", g_strPresentIntervalNames, _countof(g_strPresentIntervalNames), PRESENT_INTERVAL_ONE )
, m_SimulatedCPULoadParam( L"CPU load (approx ms)", 10, 50, 15 )
, m_SimulatedCPUSpikeParam( L"CPU spike (approx ms)", 10, 50, 20 )
, m_SimulatedGPULoadParam( L"GPU load (approx ms)", 10, 50, 13 )
, m_SimulatedGPUSpikeParam( L"GPU spike (approx ms)", 10, 50, 20 )
, m_SimulatedSpikeFramesParam( L"spike duration (frames)", 0, 20, 0 )
, m_SimulatedSpikePeriodParam( L"spike period (frames)", 1, 50, 50 )
, m_SimulatedGPUStarvationParam( L"CPU-GPU lag (approx ms)", 0, 30, 2 )
, m_SwapMethodPresetParam( L"SWAP METHOD PRESET", g_strSwapMethodPresetNames, _countof(g_strSwapMethodPresetNames) )
, m_LockFlipToVsyncParam( L"Lock flip to VSync", TRUE )
, m_UsePresentThresholdParam( L"Use present threshold", FALSE )
, m_PresentThresholdParam( L"Present threshold", 0, 104, 5, 5 )
, m_ThrottlingPointParam( L"Throttling point", g_strThrottlingPointNames, _countof(g_strThrottlingPointNames), THROTTLING_POINT_START_OF_FRAME )
, m_MidFrameThresholdParam( L"Mid-frame threshold (%)", 0, 109, 50, 10 )
, m_AsyncSwapsParam( L"Use async swaps", FALSE )
, m_NumFrontBuffersParam( L"#Async front buffers", 2, 3, 2 )
, m_AsyncThrottlingMethodParam( L"Async throttling method", g_strAsyncThrottlingMethodNames, _countof(g_strAsyncThrottlingMethodNames), ASYNC_THROTTLING_METHOD_CPU_SPINLOCK )
{
    m_UIParamArray[UI_PARAM_PROCESSOR_LOAD_PRESET]      = &m_ProcessorLoadPresetParam;
    m_UIParamArray[UI_PARAM_PRESENT_INTERVAL]           = &m_PresentIntervalParam;
    m_UIParamArray[UI_PARAM_SIMULATED_CPU_LOAD]         = &m_SimulatedCPULoadParam;
    m_UIParamArray[UI_PARAM_SIMULATED_CPU_SPIKE]        = &m_SimulatedCPUSpikeParam;
    m_UIParamArray[UI_PARAM_SIMULATED_GPU_LOAD]         = &m_SimulatedGPULoadParam;
    m_UIParamArray[UI_PARAM_SIMULATED_GPU_SPIKE]        = &m_SimulatedGPUSpikeParam;
    m_UIParamArray[UI_PARAM_SIMULATED_SPIKE_FRAMES]     = &m_SimulatedSpikeFramesParam;
    m_UIParamArray[UI_PARAM_SIMULATED_SPIKE_PERIOD]     = &m_SimulatedSpikePeriodParam;
    m_UIParamArray[UI_PARAM_SIMULATED_GPU_STARVATION]   = &m_SimulatedGPUStarvationParam;

    m_UIParamArray[UI_PARAM_SWAP_METHOD_PRESET]         = &m_SwapMethodPresetParam;
    m_UIParamArray[UI_PARAM_LOCK_FLIP_TO_VSYNC]         = &m_LockFlipToVsyncParam;
    m_UIParamArray[UI_PARAM_USE_PRESENT_THRESHOLD]      = &m_UsePresentThresholdParam;
    m_UIParamArray[UI_PARAM_PRESENT_THRESHOLD]          = &m_PresentThresholdParam;
    m_UIParamArray[UI_PARAM_THROTTLING_POINT]           = &m_ThrottlingPointParam;
    m_UIParamArray[UI_PARAM_MID_FRAME_THRESHOLD]        = &m_MidFrameThresholdParam;
    m_UIParamArray[UI_PARAM_ASYNC_SWAPS]                = &m_AsyncSwapsParam;
    m_UIParamArray[UI_PARAM_NUM_FRONT_BUFFERS]          = &m_NumFrontBuffersParam;
    m_UIParamArray[UI_PARAM_ASYNC_THROTTLING_METHOD]    = &m_AsyncThrottlingMethodParam;

    m_pLatencyTimer[LATENCY_TIMER_D3D_START_TO_GPU_START]                       = new PipelinedLatencyTimer();
    m_pLatencyTimer[LATENCY_TIMER_GPU_START_TO_BACK_BUFFER_READY]               = new PipelinedLatencyTimer();
    m_pLatencyTimer[LATENCY_TIMER_BACK_BUFFER_READY_TO_FRONT_BUFFER_READY]      = new PipelinedLatencyTimer();
    m_pLatencyTimer[LATENCY_TIMER_FRONT_BUFFER_READY_TO_FLIP]                   = new PipelinedLatencyTimer( 1 );
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::Initialize( )
// Desc: Initialize app-dependent objects.
//---------------------------------------------------------------------------------------------------------
HRESULT Sample::Initialize( )
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Font Arial_16.xpr\n" );
    }

    // Use 0 margin, to make it easier to match font placement with graph placement...
    m_Font.SetWindow( 0, 0, 1280 - 0, 720 - 0 );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Help.xpr\n" );
    }

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Allocate front buffers
    for( UINT i = 0; i < MAX_FRONT_BUFFERS; ++i )
    {
        m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, 
            m_d3dpp.BackBufferHeight, 
            1, 
            0, 
            m_d3dpp.FrontBufferFormat, 
            0, 
            &m_pFrontBuffer[i], 
            NULL );
    }

    m_iFrontBufferIndex = 0;

    m_fZoom = 3.0f;

    m_lfSystemTimeInMS = 0.0f;
    m_fSpeedInScreensPerMS = 0.0003f;

    m_bDrawHelp = FALSE;
    m_bBigMenu = TRUE;

    // Async command buffer calls --- there will be no command buffer, just an enforced GPU stall
    for( UINT i = 0; i < m_iMaxGPUFramesInFlight; ++i )
    {
        m_pd3dDevice->CreateAsyncCommandBufferCall( NULL, NULL, 0, 0, &m_pAsyncStallSimulatedGPULoad[i] );
        m_pd3dDevice->CreateAsyncCommandBufferCall( NULL, NULL, 0, 0, &m_pAsyncStallMidFrameSimulatedGPULoad[i] );
        m_pd3dDevice->CreateAsyncCommandBufferCall( NULL, NULL, 0, 0, &m_pAsyncStallAsyncThrottling[i] );
        
        m_dwMidFrameFence[i] = 0;
        m_dwEndFrameFence[i] = 0;
    }

    // To simulate CPU load, simply start a timer at the beginning of the CPU frame,
    // then wait on it at the end of the CPU frame.  
    m_hWaitableTimerSimulatedCPULoad = CreateWaitableTimer( NULL, FALSE, NULL );
    m_hWaitableTimerSimulatedCPUGPULag = CreateWaitableTimer( NULL, FALSE, NULL );
    
    // To simulate GPU load, we run an auxiliary thread.  This thread waits on a 
    // GPU callback which signals that the GPU is ready to start processing.  At
    // this point, it starts a timer, counting up to the artificial GPU load set
    // by the user.  When the timer elapses, the thread sends a signal telling the
    // GPU to continue.
    //
    // The thread runs on HW1 to avoid system activity, and to avoid contention with
    // the main thread on HW0.
    m_hWaitableTimerSimulatedGPULoad = CreateWaitableTimer( NULL, FALSE, NULL );
    m_hWaitableTimerMidFrameSimulatedGPULoad = CreateWaitableTimer( NULL, FALSE, NULL );
    m_hThreadWaitOnSimulatedGPULoad = CreateThread( NULL, 64 * 1024, WaitOnSimulatedGPULoad, NULL, CREATE_SUSPENDED, NULL );
    if( m_hThreadWaitOnSimulatedGPULoad == 0 )
    {
        ATG::FatalError( "Couldn't create thread.\n" );
    }
    XSetThreadProcessor( m_hThreadWaitOnSimulatedGPULoad, 1 );
    ResumeThread( m_hThreadWaitOnSimulatedGPULoad );

    // Mark swaps for PIX
    m_hSwapEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    m_hThreadMarkSwaps = CreateThread( NULL, 64 * 1024, MarkSwaps, NULL, CREATE_SUSPENDED, NULL );
    if( m_hThreadMarkSwaps == 0 )
    {
        ATG::FatalError( "Couldn't create thread.\n" );
    }
    XSetThreadProcessor( m_hThreadMarkSwaps, 1 );
    ResumeThread( m_hThreadMarkSwaps );

    // Mark VBlanks for PIX
    m_hVBlankEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    m_hThreadMarkVBlanks = CreateThread( NULL, 64 * 1024, MarkVBlanks, NULL, CREATE_SUSPENDED, NULL );
    if( m_hThreadMarkVBlanks == 0 )
    {
        ATG::FatalError( "Couldn't create thread.\n" );
    }
    XSetThreadProcessor( m_hThreadMarkVBlanks, 1 );
    ResumeThread( m_hThreadMarkVBlanks );

    // Perform async throttling on the GPU
    m_hVBlankThrottleSemaphore = CreateSemaphore( NULL, 0, m_iMaxVBlanksPerRender, NULL );
    m_hThreadGpuThrottle = CreateThread( NULL, 64 * 1024, GPUThrottle, NULL, CREATE_SUSPENDED, NULL );
    if( m_hThreadGpuThrottle == 0 )
    {
        ATG::FatalError( "Couldn't create thread.\n" );
    }
    XSetThreadProcessor( m_hThreadGpuThrottle, 1 );
    ResumeThread( m_hThreadGpuThrottle );

    // Report the start of the GPU load timers in PIX
    m_hStartGPULoadTimerEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    m_hThreadReportGPUTimerStart = CreateThread( NULL, 64 * 1024, ReportGPUTimerStart, NULL, CREATE_SUSPENDED, NULL );
    if( m_hThreadReportGPUTimerStart == 0 )
    {
        ATG::FatalError( "Couldn't create thread.\n" );
    }
    XSetThreadProcessor( m_hThreadReportGPUTimerStart, 1 );
    ResumeThread( m_hThreadReportGPUTimerStart );

    // Best to do these last, since the VBlank callback could occur immediately, and
    // it might require some of the resources initialized above.
    m_pd3dDevice->SetSwapCallback( SwapCallback );
    m_pd3dDevice->SetVerticalBlankCallback( VBlankCallback );

    return S_OK;
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::GPUCallbackStartOfFrame( )
// Desc: Callback upon the start of a new frame.  Signal worker thread to start the GPU timer for this 
// frame.
//
// WARNING:  This function runs in a DPC, which means certain things will crash the app:
//      - Setting a breakpoint here
//      - Using kernel blocking APIs such as WaitForSingleObject
//      - Using vector or float instructions
//---------------------------------------------------------------------------------------------------------
VOID Sample::GPUCallbackStartOfFrame( DWORD Context )
{
    atgApp._GPUCallbackStartOfFrame();
}
VOID Sample::_GPUCallbackStartOfFrame()
{
    static UINT s_iFrameCounter = 0;
    ++s_iFrameCounter;

    UINT iMidFrameThreshold         = m_MidFrameThresholdParam.GetValue();
    UINT iSimulatedGPULoad          = m_SimulatedGPULoadParam.GetValue();
    UINT iSimulatedGPUSpike         = m_SimulatedGPUSpikeParam.GetValue();
    UINT iSimulatedSpikeFrames      = m_SimulatedSpikeFramesParam.GetValue();
    UINT iSimulatedSpikePeriod      = m_SimulatedSpikePeriodParam.GetValue();

    UINT iSimulatedGPUTime = ( s_iFrameCounter % iSimulatedSpikePeriod < iSimulatedSpikeFrames )
        ? iSimulatedGPUSpike
        : iSimulatedGPULoad;

    LARGE_INTEGER liGPUDueTime; 
    liGPUDueTime.QuadPart = g_llRelative100NSToMS * ( iSimulatedGPUTime * ( 100 - iMidFrameThreshold ) ) / 100 ;
    SetWaitableTimer( m_hWaitableTimerMidFrameSimulatedGPULoad, &liGPUDueTime, 0, NULL, NULL, FALSE );

    liGPUDueTime.QuadPart = g_llRelative100NSToMS * iSimulatedGPUTime;
    SetWaitableTimer( m_hWaitableTimerSimulatedGPULoad, &liGPUDueTime, 0, NULL, NULL, FALSE );

    SetEvent( m_hStartGPULoadTimerEvent );

    m_pLatencyTimer[LATENCY_TIMER_D3D_START_TO_GPU_START]->Stop();
    m_pLatencyTimer[LATENCY_TIMER_GPU_START_TO_BACK_BUFFER_READY]->Start();
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::GPUCallbackBackBufferReady( )
// Desc: Callback upon the completion of rendering.  
//
// WARNING:  This function runs in a DPC, which means certain things will crash the app:
//      - Setting a breakpoint here
//      - Using kernel blocking APIs such as WaitForSingleObject
//      - Using vector or float instructions
//---------------------------------------------------------------------------------------------------------
VOID Sample::GPUCallbackBackBufferReady( DWORD Context )
{
    atgApp._GPUCallbackBackBufferReady();
}
VOID Sample::_GPUCallbackBackBufferReady()
{
    m_pLatencyTimer[LATENCY_TIMER_GPU_START_TO_BACK_BUFFER_READY]->Stop();
    m_pLatencyTimer[LATENCY_TIMER_BACK_BUFFER_READY_TO_FRONT_BUFFER_READY]->Start();
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::GPUCallbackFrontBufferReady( )
// Desc: Callback upon completion of resolve of back buffer to front buffer.  
//
// WARNING:  This function runs in a DPC, which means certain things will crash the app:
//      - Setting a breakpoint here
//      - Using kernel blocking APIs such as WaitForSingleObject
//      - Using vector or float instructions
//---------------------------------------------------------------------------------------------------------
VOID Sample::GPUCallbackFrontBufferReady( DWORD Context )
{
    atgApp._GPUCallbackFrontBufferReady();
}
VOID Sample::_GPUCallbackFrontBufferReady()
{
    m_pLatencyTimer[LATENCY_TIMER_BACK_BUFFER_READY_TO_FRONT_BUFFER_READY]->Stop();
    m_pLatencyTimer[LATENCY_TIMER_FRONT_BUFFER_READY_TO_FLIP]->Start();
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::WaitOnSimulatedGPULoad( )
// Desc: Thread which waits on the timers simulating the GPU load each frame, and then calls 
// SignalAsyncResources to unblock the GPU after the right amount of time.
//---------------------------------------------------------------------------------------------------------
#pragma warning(push)
#pragma warning(disable:4702)   // unreachable code
DWORD Sample::WaitOnSimulatedGPULoad( LPVOID lpThreadParameter )
{
    atgApp._WaitOnSimulatedGPULoad();

    return 0;
}
#pragma warning(pop)

VOID Sample::_WaitOnSimulatedGPULoad()
{
    PIXNameThread( __FUNCTION__ );

    for( RingUInt<m_iMaxGPUFramesInFlight> iNextGpuFrame = 0; ; ++iNextGpuFrame )
    {
        {
            PIX_EVENT_SCOPE( "GPU #%d: Signal mid frame", iNextGpuFrame.Get() );
            WaitForSingleObject( m_hWaitableTimerMidFrameSimulatedGPULoad, INFINITE );
            m_pAsyncStallMidFrameSimulatedGPULoad[iNextGpuFrame.Get()]->FixupAndSignal( NULL, 0, 0 );
        }

        {
            PIX_EVENT_SCOPE( "GPU #%d: Signal end frame", iNextGpuFrame.Get() );
            WaitForSingleObject( m_hWaitableTimerSimulatedGPULoad, INFINITE );
            m_pAsyncStallSimulatedGPULoad[iNextGpuFrame.Get()]->FixupAndSignal( NULL, 0, 0 );
        }
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::MarkSwaps( )
// Desc: Mark the occurrences of Swap in PIX Timing Captures.
//---------------------------------------------------------------------------------------------------------
#pragma warning(push)
#pragma warning(disable:4702)   // unreachable code
DWORD Sample::MarkSwaps( LPVOID lpThreadParameter )
{
    atgApp._MarkSwaps();

    return 0;
}
#pragma warning(pop)

VOID Sample::_MarkSwaps()
{
    PIXNameThread( __FUNCTION__ );

    m_iSwapDataSwap = 0;
    m_iSwapDataVBlank = 0;

    BOOL bAlways = TRUE;
    while( bAlways )
    {
        WaitForSingleObject( m_hSwapEvent, INFINITE );
        PIX_EVENT_SCOPE( "SwapCallback #%d, VBlank #%d", m_iSwapDataSwap, m_iSwapDataVBlank );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::SwapCallback( )
// Desc: Callback when an async swap is requested.  
//
// WARNING:  This function runs in a DPC, which means certain things will crash the app:
//      - Setting a breakpoint here
//      - Using kernel blocking APIs such as WaitForSingleObject
//      - Using vector or float instructions
//---------------------------------------------------------------------------------------------------------
VOID Sample::SwapCallback( D3DSWAPDATA* pData )
{
    atgApp._SwapCallback( pData );
}
VOID Sample::_SwapCallback( D3DSWAPDATA* pData )
{
    m_iSwapDataSwap = pData->Swap;
    m_iSwapDataVBlank = pData->SwapVBlank;
    SetEvent( m_hSwapEvent );

    m_pd3dDevice->GetRasterStatus( 0, &m_RasterStatusRing[m_iSwapDataIndex.Get()] );
    m_SwapDataRing[m_iSwapDataIndex.Get()] = *pData;
    ++m_iSwapDataIndex;
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::MarkVBlanks( )
// Desc: Mark the occurrences of VBlank in PIX Timing Captures.
//---------------------------------------------------------------------------------------------------------
#pragma warning(push)
#pragma warning(disable:4702)   // unreachable code
DWORD Sample::MarkVBlanks( LPVOID lpThreadParameter )
{
    atgApp._MarkVBlanks();

    return 0;
}
#pragma warning(pop)

VOID Sample::_MarkVBlanks()
{
    PIXNameThread( __FUNCTION__ );

    m_iVBlankDataSwap = 0;
    m_iVBlankDataVBlank = 0;

    BOOL bAlways = TRUE;
    while( bAlways )
    {
        WaitForSingleObject( m_hVBlankEvent, INFINITE );
        PIX_EVENT_SCOPE( "VBlankCallback #%d, Swap #%d", m_iVBlankDataVBlank, m_iVBlankDataSwap );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::GPUThrottle( )
// Desc: Thread which releases async throttling, when throttling is implemented on the GPU.  
//---------------------------------------------------------------------------------------------------------
#pragma warning(push)
#pragma warning(disable:4702)   // unreachable code
DWORD Sample::GPUThrottle( LPVOID lpThreadParameter )
{
    atgApp._GPUThrottle();

    return 0;
}
#pragma warning(pop)

VOID Sample::_GPUThrottle()
{
    PIXNameThread( __FUNCTION__ );

    for( RingUInt<m_iMaxGPUFramesInFlight> iNextGpuFrame = 0; ; ++iNextGpuFrame )
    {
        PIX_EVENT_SCOPE( "GPU #%d: Signal front buffer released", iNextGpuFrame.Get() );

        WaitForSingleObject( m_hVBlankThrottleSemaphore, INFINITE );

        m_pAsyncStallAsyncThrottling[iNextGpuFrame.Get()]->FixupAndSignal( NULL, 0, 0 );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::ReportGPUTimerStart( )
// Desc: Thread which reports the start of the GPU load timer in PIX.  
//---------------------------------------------------------------------------------------------------------
#pragma warning(push)
#pragma warning(disable:4702)   // unreachable code
DWORD Sample::ReportGPUTimerStart( LPVOID lpThreadParameter )
{
    atgApp._ReportGPUTimerStart();

    return 0;
}
#pragma warning(pop)

VOID Sample::_ReportGPUTimerStart()
{
    PIXNameThread( __FUNCTION__ );

    for( RingUInt<m_iMaxGPUFramesInFlight> iNextGpuFrame = 0; ; ++iNextGpuFrame )
    {
        PIX_EVENT_SCOPE( "GPU #%d: Start timers", iNextGpuFrame.Get() );

        WaitForSingleObject( m_hStartGPULoadTimerEvent, INFINITE );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::VBlankCallback( )
// Desc: Callback when an vblank is encountered.  
//
// WARNING:  This function runs in a DPC, which means certain things will crash the app:
//      - Setting a breakpoint here
//      - Using kernel blocking APIs such as WaitForSingleObject
//      - Using vector or float instructions
//---------------------------------------------------------------------------------------------------------
VOID Sample::VBlankCallback( D3DVBLANKDATA* pData )
{
    atgApp._VBlankCallback( pData );
}
VOID Sample::_VBlankCallback( D3DVBLANKDATA* pData )
{
     m_iVBlankDataSwap = pData->Swap;
     m_iVBlankDataVBlank = pData->VBlank;

    SetEvent( m_hVBlankEvent );

    m_VBlankDataRing[m_iVBlankDataIndex.Get()] = *pData;
    ++m_iVBlankDataIndex;

    // Do not record a frame unless this VBlank has a swap queued.
    // But record multiple frames if there are multiple swaps queued.
    static DWORD dwPrevSwap = 0;
    while( dwPrevSwap < pData->Swap )
    {
        ++dwPrevSwap;
        m_pLatencyTimer[LATENCY_TIMER_FRONT_BUFFER_READY_TO_FLIP]->Stop();

        ReleaseSemaphore( m_hVBlankThrottleSemaphore, 1, NULL );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::ResetSwapAndPresentModes( )
// Desc: Reset the D3D device when menu parameters are changed by the user.
//---------------------------------------------------------------------------------------------------------
VOID Sample::ResetSwapAndPresentModes( )
{
    BOOL bLockFlipToVsync       = m_LockFlipToVsyncParam.GetValue();
    UINT iPresentInterval       = m_PresentIntervalParam.GetValue();
    BOOL bUsePresentThreshold   = m_UsePresentThresholdParam.GetValue();
    UINT iPresentThreshold      = m_PresentThresholdParam.GetValue();
    UINT iThrottlingPoint       = m_ThrottlingPointParam.GetValue();
    //UINT MidFrameThreshold      = m_MidFrameThresholdParam.GetValue();
    BOOL bAsyncSwaps            = m_AsyncSwapsParam.GetValue();
    //UINT iNumFrontBuffers       = m_NumFrontBuffersParam.GetValue();
    //UINT iAsyncThrottlingMethod = m_AsyncThrottlingMethodParam.GetValue();

    DWORD d3dPresentInterval = 0;
    if( !bLockFlipToVsync )
    {
        d3dPresentInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    }
    else switch( iPresentInterval )
    {
    case PRESENT_INTERVAL_ONE: d3dPresentInterval = D3DPRESENT_INTERVAL_ONE; break;
    case PRESENT_INTERVAL_TWO: d3dPresentInterval = D3DPRESENT_INTERVAL_TWO; break;
    case PRESENT_INTERVAL_THREE: d3dPresentInterval = D3DPRESENT_INTERVAL_THREE; break;
    default: assert(FALSE); break;
    }

    m_pd3dDevice->SetRenderState( D3DRS_PRESENTINTERVAL, d3dPresentInterval );
    m_pd3dDevice->SetRenderState( D3DRS_PRESENTIMMEDIATETHRESHOLD, bUsePresentThreshold ? iPresentThreshold : 0 );
    m_pd3dDevice->SetRenderState( D3DRS_BUFFER2FRAMES, !bAsyncSwaps && iThrottlingPoint != THROTTLING_POINT_START_OF_FRAME );

    m_pd3dDevice->SetSwapMode( bAsyncSwaps );
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::Update( )
// Desc: Called once per frame, the call is the entry point for animating the scene.
//---------------------------------------------------------------------------------------------------------
HRESULT Sample::Update( )
{
    // Start timers for simulated processing loads
    StartSimulatedFrame();

    PIX_EVENT_FUNCTION();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput( );

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Zoom level of graphs
    {
        CONST FLOAT fZoomRate = 0.1f;

        m_fZoom *= ( 1.0f + fZoomRate * ( pGamepad->bRightTrigger / 255.0f ) );
        m_fZoom *= ( 1.0f - fZoomRate * ( pGamepad->bLeftTrigger / 255.0f ) );
        m_fZoom = Min( m_fZoom, 10.0f );
        m_fZoom = Max( m_fZoom, 1.0f );
    }

    if( !m_bDrawHelp )
    {
        // UI controls:
        {
            // Record state prior to applying controller input
            static BOOL g_bFirstUpdate = TRUE;

            BOOL bProcessorLoadPresetTurnedOn = FALSE, bProcessorLoadPresetTurnedOff = FALSE;
            BOOL bSwapMethodPresetTurnedOn = FALSE, bSwapMethodPresetTurnedOff = FALSE;

            BOOL bOldLockFlipToVsync            = m_LockFlipToVsyncParam.GetValue();
            UINT iOldPresentInterval            = m_PresentIntervalParam.GetValue();
            BOOL bOldUsePresentThreshold        = m_UsePresentThresholdParam.GetValue();
            UINT iOldPresentThreshold           = m_PresentThresholdParam.GetValue();
            UINT iOldThrottlingPoint            = m_ThrottlingPointParam.GetValue();
            //UINT iOldMidFrameThreshold          = m_MidFrameThresholdParam.GetValue();
            BOOL bOldAsyncSwaps                 = m_AsyncSwapsParam.GetValue();
            //UINT iOldNumFrontBuffers            = m_NumFrontBuffersParam.GetValue();
            //UINT iOldAsyncThrottlingMethod      = m_AsyncThrottlingMethodParam.GetValue();

            static FLOAT fLastX1 = 0.0f, fLastY1 = 0.0f;
            FLOAT fDecrease = 0.0f;
            FLOAT fIncrease = 0.0f;

            // Process UI Input
            if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP ) 
                || ( pGamepad->fY1 > 0.1f && fLastY1 <= 0.1f ) )
            {
                --m_iActiveUIParameter;
            }
            if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN ) 
                || ( pGamepad->fY1 < -0.1f && fLastY1 >= -0.1f ) )
            {
                ++m_iActiveUIParameter;
            }

#pragma warning( push )
#pragma warning( disable:4127 6326 )   // conditional expression is constant
            // Keep the selected parameter in view, in the small menu
            if( m_iVisibleUICount < UI_PARAM_COUNT )
#pragma warning( pop )
            {
                if( m_iActiveUIParameter + 1 == m_iVisibleUIStart )
                {
                    --m_iVisibleUIStart;
                }
                else if( m_iActiveUIParameter == m_iVisibleUIStart + m_iVisibleUICount )
                {
                    ++m_iVisibleUIStart;
                }
            }
            if( pGamepad->fX1 < -0.1f && fLastX1 >= -0.1f )
                fDecrease = 1.0f;
            if( pGamepad->fX1 > 0.1f && fLastX1 <= 0.1f )
                fIncrease = 1.0f;
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                fDecrease = 1.0f;
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                fIncrease = 1.0f;

            // Correct for bias in thumbsticks, and limit to one move at a time
            fLastX1 = pGamepad->fX1;
            fLastY1 = pGamepad->fY1;

            // Handle preset going on or getting auto-disabled
            if( fDecrease > 0.0f || fIncrease > 0.0f )
            {
                switch( m_iActiveUIParameter.Get() )
                {
                case UI_PARAM_PROCESSOR_LOAD_PRESET:
                    bProcessorLoadPresetTurnedOn = TRUE;
                    break;

                case UI_PARAM_PRESENT_INTERVAL: 
                case UI_PARAM_SIMULATED_CPU_LOAD: 
                case UI_PARAM_SIMULATED_CPU_SPIKE: 
                case UI_PARAM_SIMULATED_SPIKE_FRAMES: 
                case UI_PARAM_SIMULATED_GPU_STARVATION: 
                case UI_PARAM_SIMULATED_GPU_LOAD: 
                case UI_PARAM_SIMULATED_GPU_SPIKE: 
                case UI_PARAM_SIMULATED_SPIKE_PERIOD: 
                    bProcessorLoadPresetTurnedOff = TRUE;
                    break;
                }

                switch( m_iActiveUIParameter.Get() )
                {
                case UI_PARAM_SWAP_METHOD_PRESET:
                    bSwapMethodPresetTurnedOn = TRUE;
                    break;

                case UI_PARAM_LOCK_FLIP_TO_VSYNC: 
                case UI_PARAM_USE_PRESENT_THRESHOLD: 
                case UI_PARAM_PRESENT_THRESHOLD: 
                case UI_PARAM_THROTTLING_POINT: 
                case UI_PARAM_MID_FRAME_THRESHOLD: 
                case UI_PARAM_ASYNC_SWAPS: 
                case UI_PARAM_NUM_FRONT_BUFFERS: 
                case UI_PARAM_ASYNC_THROTTLING_METHOD: 
                    bSwapMethodPresetTurnedOff = TRUE;
                    break;
                }
            }

            if( bProcessorLoadPresetTurnedOn && !m_ProcessorLoadPresetParam.GetValid() )
            {
                fDecrease = fIncrease = 0.0f;   // If preset is being re-enabled, don't move it
            }
            if( bSwapMethodPresetTurnedOn && !m_SwapMethodPresetParam.GetValid() )
            {
                fDecrease = fIncrease = 0.0f;   // If preset is being re-enabled, don't move it
            }

            // Change selected menu item
            if( fDecrease > 0.0f )
            {
                m_UIParamArray[m_iActiveUIParameter.Get()]->DecreaseValue( fDecrease );
            }
            if( fIncrease > 0.0f )
            {
                m_UIParamArray[m_iActiveUIParameter.Get()]->IncreaseValue( fIncrease );
            }

            // Force preset, if selected
            // Kill preset, if an option changes manually
            if( g_bFirstUpdate || bProcessorLoadPresetTurnedOn )
            {
                m_ProcessorLoadPresetParam.SetValid( TRUE );

                const ProcessorLoadPreset& Preset = g_ProcessorLoadPresets[ m_ProcessorLoadPresetParam.GetValue() ];
                for( UINT i = 0; i < _countof(g_iProcessorLoadPresetParams); ++i )
                {
                    m_UIParamArray[g_iProcessorLoadPresetParams[i]]->SetValue( Preset[i] );
                }
            }
            else if( bProcessorLoadPresetTurnedOff )
            {
                m_ProcessorLoadPresetParam.SetValid( FALSE );    // forces display of "n/a"
            }
            if( g_bFirstUpdate || bSwapMethodPresetTurnedOn )
            {
                m_SwapMethodPresetParam.SetValid( TRUE );

                const SwapMethodPreset& Preset = g_SwapMethodPresets[ m_SwapMethodPresetParam.GetValue() ];
                for( UINT i = 0; i < _countof(g_iSwapMethodPresetParams); ++i )
                {
                    m_UIParamArray[g_iSwapMethodPresetParams[i]]->SetValue( Preset[i] );
                }
            }
            else if( bSwapMethodPresetTurnedOff )
            {
                m_SwapMethodPresetParam.SetValid( FALSE );    // forces display of "n/a"
            }

            BOOL bNewLockFlipToVsync            = m_LockFlipToVsyncParam.GetValue();
            UINT iNewPresentInterval            = m_PresentIntervalParam.GetValue();
            BOOL bNewUsePresentThreshold        = m_UsePresentThresholdParam.GetValue();
            UINT iNewPresentThreshold           = m_PresentThresholdParam.GetValue();
            UINT iNewThrottlingPoint            = m_ThrottlingPointParam.GetValue();
            //UINT iNewMidFrameThreshold          = m_MidFrameThresholdParam.GetValue();
            BOOL bNewAsyncSwaps                 = m_AsyncSwapsParam.GetValue();
            //UINT iNewNumFrontBuffers            = m_NumFrontBuffersParam.GetValue();
            //UINT iNewAsyncThrottlingMethod      = m_AsyncThrottlingMethodParam.GetValue();

            if( g_bFirstUpdate
                || bOldLockFlipToVsync          != bNewLockFlipToVsync            
                || iOldPresentInterval          != iNewPresentInterval        
                || bOldUsePresentThreshold      != bNewUsePresentThreshold    
                || iOldPresentThreshold         != iNewPresentThreshold       
                || iOldThrottlingPoint          != iNewThrottlingPoint        
                //|| iOldMidFrameThreshold        != iNewMidFrameThreshold        
                || bOldAsyncSwaps               != bNewAsyncSwaps             
                //|| iOldNumFrontBuffers          != iNewNumFrontBuffers        
                //|| iOldAsyncThrottlingMethod    != iNewAsyncThrottlingMethod       
                )
            {
                ResetSwapAndPresentModes();
            }

            g_bFirstUpdate = FALSE;
        }
    }

    return S_OK;
}


//---------------------------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Render the screen display for our custom menu.
//---------------------------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    PIX_EVENT_FUNCTION();

    m_Font.Begin();

    FLOAT fMarginX = 60.0f;
    FLOAT fMarginY = 20.0f;

    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( fMarginX, fMarginY, 0xffff00ff, L"Async Swaps" );

    FLOAT fParamX = fMarginX + 500.0f;
    FLOAT fParamY = fMarginY;
    FLOAT fParamYInc = 20.0f;
    FLOAT fSectionYInc = 10.0f;

    RingUInt<UI_PARAM_COUNT> iVisibleUIStart = m_bBigMenu ? 0 : m_iVisibleUIStart;
    UINT iVisibleUICount = m_bBigMenu ? UI_PARAM_COUNT : m_iVisibleUICount;

    if( !m_bBigMenu )
    {
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( fParamX + 40.0f, fParamY, 0xffff00ff, GLYPH_UP_ARROW ); 
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( fParamX + 100.0f, fParamY, 0xffffffff, L"(" GLYPH_B_BUTTON L" to expand)" ); 
        fParamY += fParamYInc;
    }

    for( UINT i = 0; i < iVisibleUICount; ++i )
    {
        RingUInt<UI_PARAM_COUNT> iParamIndex = iVisibleUIStart + i;

        UIParam* Param = m_UIParamArray[iParamIndex.Get()];

        Param->RenderUI( &m_Font, fParamX, fParamY, ( iParamIndex == m_iActiveUIParameter ) );
        fParamY += fParamYInc;

        // Separate the processor load options from the swap options
        if( iParamIndex + 1 == UI_PARAM_PROCESSOR_LOAD_PRESET 
            || iParamIndex + 1 == UI_PARAM_SWAP_METHOD_PRESET )
        {
            fParamY += fSectionYInc;
        }
    }

    if( !m_bBigMenu )
    {
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( fParamX + 40.0f, fParamY, 0xffff00ff, GLYPH_DOWN_ARROW );
        fParamY += fParamYInc;
    }

    m_Font.End();
}


//---------------------------------------------------------------------------------------------------------
// Name: RenderPerformanceData()
// Desc: Render the screen display for the various types of performance data.  The elements rendered 
// include:
//
//      - Performance statistics
//      - Performance graphs
//      - Latency and tearing indicators
//---------------------------------------------------------------------------------------------------------
VOID Sample::RenderPerformanceData()
{
    PIX_EVENT_FUNCTION();

    DOUBLE lfDummy;
    WCHAR strBuffer[256];

    // Matching colors for graph and text
    enum GRAPH_TYPE
    {
        GRAPH_TYPE_JITTER, 
        GRAPH_TYPE_TEARING, 
        GRAPH_TYPE_LATENCY, 
        GRAPH_TYPE_FRAME_RATE, 

        GRAPH_TYPE_COUNT
    };
    static CONST D3DCOLOR d3dColorGraphType[] = 
    {
        D3DCOLOR_ARGB( 255, 255,  64,   0 ), 
        D3DCOLOR_ARGB( 255, 255,   0,   0 ), 
        D3DCOLOR_ARGB( 255,   0, 255, 255 ), 
        D3DCOLOR_ARGB( 255,   0, 128,   0 ), 
    };
    C_ASSERT( _countof(d3dColorGraphType) == GRAPH_TYPE_COUNT );

    // m_iSwapDataIndex may change in another thread during this function, so sample it only once.
    // We don't need this to be synchronized, just consistent throughout a single execution pass.
    // The data in the range [s_iEarliestValidSwapIndex, iLatestValidSwapIndex] will be added to
    // our graphs and statistics.
    static RingUInt<m_iMaxSwapsPerRender> s_iEarliestValidSwapIndex = 0;
    RingUInt<m_iMaxSwapsPerRender> iEarliestInvalidSwapIndex = m_iSwapDataIndex;
    RingUInt<m_iMaxSwapsPerRender> iLatestValidSwapIndex = iEarliestInvalidSwapIndex - 1;

    CONST UINT iVBlanksPerSec = 60;  // Really something slightly off
    CONST FLOAT fMSPerVBlank = 1000.0f / iVBlanksPerSec;
    D3DRECT GraphRect = { 100, 420, 1100, 620 };
    CONST UINT iIdealNumVBlanksInGraph = 500;
    CONST FLOAT fVBlanksPerXUnit = ( iIdealNumVBlanksInGraph / (FLOAT) ( GraphRect.x2 - GraphRect.x1 ) ) / m_fZoom;
    CONST FLOAT fMSPerXUnit = fVBlanksPerXUnit * fMSPerVBlank;

    CONST UINT iMaxDataCount = iIdealNumVBlanksInGraph + 20; // Some slack to ensure we don't leave a gap

    // All the data we need, per swap, to reproduce the graph.  The absolute times must be DOUBLE precision or
    // else the relative times lose precision after long (overnight) runs.
    struct s_GraphData
    {
        DOUBLE  m_lfAppTimeInMS;
        DOUBLE  m_lfSwapTimeInMS;
        FLOAT   m_fAppDeltaInMS;
        FLOAT   m_fSwapDeltaInMS;
        FLOAT   m_fSwapRateInFPS;
        FLOAT   m_fJitter;
        BOOL    m_bTearingOccurred;
        UINT    m_iTearingLocation;
        FLOAT   m_fLatencyInMS[LATENCY_TIMER_COUNT];
    };
    static s_GraphData s_GraphDataRing[iMaxDataCount];

    // These indices record the latest valid data in the ring.  The indices are roughly the same, but might
    // vary slightly from each other due to timing differences.  For the most recent swap or two, we might have
    // only partial latency information.
    static RingUInt<iMaxDataCount> s_iGraphRingIndexApp = 1;    // Inter-frame time cannot be measured on frame 0
    static RingUInt<iMaxDataCount> s_iGraphRingIndexSwap = 1;   // Inter-frame time cannot be measured on frame 0
    static RingUInt<iMaxDataCount> s_iGraphRingIndexLatency[LATENCY_TIMER_COUNT] = { 0 };

    // Update the app time in the graph data.  Potentially, the app might use a different time delta than the actual 
    // system time delta.
    DOUBLE lfAppTimeInMS = m_lfSystemTimeInMS;
    static DOUBLE lfPrevAppTimeInMS = 0.0f;
    {
        s_GraphData* pCurrGraphData = &s_GraphDataRing[s_iGraphRingIndexApp.Get()];
        s_GraphData* pPrevGraphData = &s_GraphDataRing[(s_iGraphRingIndexApp - 1).Get()];

        pCurrGraphData->m_lfAppTimeInMS = lfAppTimeInMS;
        pCurrGraphData->m_fAppDeltaInMS = (FLOAT) ( pCurrGraphData->m_lfAppTimeInMS - pPrevGraphData->m_lfAppTimeInMS );

        ++s_iGraphRingIndexApp;
    }

    static Smoother s_SwapDeltaSmoother( 100.0f, 0.01f, 0.1f );
    static Smoother s_JitterSmoother( 1000.0f, 0.01f, 0.1f );
    static Smoother s_TearingFreqSmoother( 1000.0f, 0.01f, 0.1f );
    static Smoother s_TearingLocationSmoother( 1000.0f, 0.01f, 0.1f );

    // Update the graph data with the frame rate and tearing information which has arrived since the previous call.
    // Not all the data will necessarily be up to the same frame, since some items run ahead of others.
    for( RingUInt<m_iMaxSwapsPerRender> iSwapIndex = s_iEarliestValidSwapIndex; iSwapIndex != iEarliestInvalidSwapIndex; ++iSwapIndex )
    {
        D3DSWAPDATA* pCurrSwapData = &m_SwapDataRing[iSwapIndex.Get()];
        D3DRASTER_STATUS* pCurrRasterStatus = &m_RasterStatusRing[iSwapIndex.Get()];

        BOOL bTearingOccurred = pCurrSwapData->SwapVBlank == pCurrSwapData->LastVBlank;
        UINT iTearingLocation = bTearingOccurred ? pCurrRasterStatus->ScanLine : 0; // ScanLine will be 0 within VBlank
        DOUBLE lfCurrSwapTimeInVBlanks = pCurrSwapData->LastVBlank + iTearingLocation / (FLOAT) m_d3dpp.BackBufferHeight;
        
        s_GraphData* pCurrGraphData = &s_GraphDataRing[s_iGraphRingIndexSwap.Get()];
        s_GraphData* pPrevGraphData = &s_GraphDataRing[(s_iGraphRingIndexSwap - 1).Get()];
        pCurrGraphData->m_lfSwapTimeInMS = lfCurrSwapTimeInVBlanks * fMSPerVBlank;
        pCurrGraphData->m_fSwapDeltaInMS = (FLOAT) ( pCurrGraphData->m_lfSwapTimeInMS - pPrevGraphData->m_lfSwapTimeInMS );
        pCurrGraphData->m_fSwapRateInFPS = 1000.0f / pCurrGraphData->m_fSwapDeltaInMS;
        pCurrGraphData->m_fJitter = ( pCurrGraphData->m_fAppDeltaInMS - pCurrGraphData->m_fSwapDeltaInMS );
        pCurrGraphData->m_bTearingOccurred = bTearingOccurred;
        pCurrGraphData->m_iTearingLocation = iTearingLocation;

        s_SwapDeltaSmoother.AddObservedValue( pCurrGraphData->m_fSwapDeltaInMS );
        s_JitterSmoother.AddObservedValue( fabsf( pCurrGraphData->m_fJitter ) );
        s_TearingFreqSmoother.AddObservedValue( ( pCurrGraphData->m_bTearingOccurred ) ? 100.0f : 0.0f );
        s_TearingLocationSmoother.AddObservedValue( (FLOAT) pCurrGraphData->m_iTearingLocation );

        ++s_iGraphRingIndexSwap;
    }

    // Update the graph data with the latency information which has arrived since the previous call.
    FLOAT fTimes[m_iMaxLatenciesInFlight];
    UINT iCount = 0;
    FLOAT fEndToEndLatencyInMS = 0.0f;
    static Smoother s_LatencySmoother[LATENCY_TIMER_COUNT] = { 
        Smoother( 100.0f, 0.01f, 0.1f ),  
        Smoother( 100.0f, 0.01f, 0.1f ),  
        Smoother( 100.0f, 0.01f, 0.1f ),  
        Smoother( 100.0f, 0.01f, 0.1f ),  
    };
    for( UINT iTimer = 0; iTimer < LATENCY_TIMER_COUNT; ++iTimer )
    {
        assert( _countof( fTimes ) >= m_pLatencyTimer[iTimer]->GetMaxInFlight() );
        m_pLatencyTimer[iTimer]->GetTimesInMS( fTimes, &iCount );
        for( UINT i = 0; i < iCount; ++i )
        {
            s_GraphData* pCurrGraphData = &s_GraphDataRing[s_iGraphRingIndexLatency[iTimer].Get()];

            // The VBlank latency does not count towards total latency unless we actually waited on VBlank.
            if( iTimer == LATENCY_TIMER_FRONT_BUFFER_READY_TO_FLIP && pCurrGraphData->m_bTearingOccurred ) 
            {
                fTimes[i] = 0.0;
            }

            pCurrGraphData->m_fLatencyInMS[iTimer] = fTimes[i];
            ++s_iGraphRingIndexLatency[iTimer];

            s_LatencySmoother[iTimer].AddObservedValue( fTimes[i] );
        }

        fEndToEndLatencyInMS += s_LatencySmoother[iTimer].GetSmoothedValue();
    }

    s_GraphData* s_CurrGraphData = &s_GraphDataRing[(s_iGraphRingIndexSwap - 1).Get()];

    // Draw the estimated tearing indicator
    FLOAT fYTearingIndicator = (FLOAT) s_CurrGraphData->m_iTearingLocation;
    FLOAT fYTearingIndicatorAvg = Min( s_TearingLocationSmoother.GetSmoothedValue(), 640.0f );  // leave room for text
    if( s_CurrGraphData->m_bTearingOccurred )
    {
        XMFLOAT2 LineStart = XMFLOAT2( 0.0f, fYTearingIndicator );
        XMFLOAT2 LineEnd = XMFLOAT2( (FLOAT) m_d3dpp.BackBufferWidth, fYTearingIndicator );
        ATG::DebugDraw::DrawScreenSpaceLine( LineStart, LineEnd, d3dColorGraphType[GRAPH_TYPE_TEARING], 4.0f );
    }

    // Draw the delayed part of the latency indicator.  This is also the ground truth tearing indicator, if your
    // eyes are sharp enough to see the tearing.  This is also the jitter indicator.
    FLOAT fXJitterIndicator = (FLOAT) modf( lfAppTimeInMS * m_fSpeedInScreensPerMS, &lfDummy ) * m_d3dpp.BackBufferWidth;
    {
        XMFLOAT2 LineStart = XMFLOAT2( fXJitterIndicator, 0.0f );
        XMFLOAT2 LineEnd = XMFLOAT2( fXJitterIndicator, (FLOAT) m_d3dpp.BackBufferHeight );
        ATG::DebugDraw::DrawScreenSpaceLine( LineStart, LineEnd, d3dColorGraphType[GRAPH_TYPE_JITTER], 4.0f );
    }

    // Regenerate the graph vertex data
    UINT iDataCount = 0;
    static XMFLOAT2 DataPoints[GRAPH_TYPE_COUNT][iMaxDataCount+1];
    {
        RingUInt<iMaxDataCount> iGraphRingIndex = s_iGraphRingIndexSwap - 1;
        FLOAT fBaseX = (FLOAT) GraphRect.x2;
        FLOAT fX = fBaseX;
        DOUBLE lfBaseSwapTimeInMS = s_GraphDataRing[iGraphRingIndex.Get()].m_lfSwapTimeInMS;
        FLOAT fYMin = 0.0f;
        FLOAT fYMax = 100.0f;
        FLOAT fYScale = ( GraphRect.y2 - GraphRect.y1 ) / ( fYMax - fYMin );

        // Graph until we run out of memory, or until we run out of reliable data, or until we run out of screen space
        while( iDataCount <= iMaxDataCount 
            && iGraphRingIndex != s_iGraphRingIndexSwap 
            && fX >= GraphRect.x1 - 5 )
        {
            CONST s_GraphData* pCurrGraphData = &s_GraphDataRing[iGraphRingIndex.Get()];

            fX = fBaseX - (FLOAT) ( lfBaseSwapTimeInMS - pCurrGraphData->m_lfSwapTimeInMS ) / fMSPerXUnit;

            // We do not cache end-to-end latency, because we may not receive all components at the
            // same frame.
            FLOAT fEndToEndLatency = 0.0f;
            for( UINT iTimer = 0; iTimer < LATENCY_TIMER_COUNT; ++iTimer )
            {
                fEndToEndLatency += pCurrGraphData->m_fLatencyInMS[iTimer];
            }

            FLOAT fRawDataY[] = 
            {
                ( 50.0f / fMSPerVBlank ) * pCurrGraphData->m_fJitter + 50.0f, 
                100.0f * pCurrGraphData->m_iTearingLocation / (FLOAT) m_d3dpp.BackBufferHeight, 
                fEndToEndLatency, 
                pCurrGraphData->m_fSwapRateInFPS, 
            };
            C_ASSERT( _countof(fRawDataY) == GRAPH_TYPE_COUNT );

            for( UINT i = 0; i < GRAPH_TYPE_COUNT; ++i )
            {
                DataPoints[i][iDataCount].x = fX;
                DataPoints[i][iDataCount].y = Saturate( GraphRect.y2 - fYScale * fRawDataY[i], (FLOAT) GraphRect.y1 + 1.0f, (FLOAT) GraphRect.y2 - 1.0f );
            }

            --iGraphRingIndex;
            ++iDataCount;
        }
        --iDataCount;
    }

    // Draw the graph frame and axes
    {
        ATG::DebugDraw::DrawScreenSpaceRect( GraphRect, 2.0f, D3DCOLOR_ARGB( 255, 255, 255, 255 ) );

        m_Font.Begin();

        m_Font.SetScaleFactors( 0.8f, 0.8f );

        // Draw horizontal axis labels
        {
            m_Font.DrawText( (FLOAT) GraphRect.x1 - 5.0f, (FLOAT) GraphRect.y2, D3DCOLOR_ARGB( 255, 255, 255, 255 ), L"sec", ATGFONT_CENTER_Y | ATGFONT_RIGHT );

            // Current time
            DOUBLE lfSwapTimeInSeconds = s_CurrGraphData->m_lfSwapTimeInMS / 1000.0f;
            INT iSwapTimeInSeconds = (UINT) lfSwapTimeInSeconds;
            FLOAT fFractionalSwapTimeInSeconds = (FLOAT) lfSwapTimeInSeconds - (FLOAT) iSwapTimeInSeconds;
            FLOAT fX = (FLOAT) GraphRect.x2 - fFractionalSwapTimeInSeconds * 1000.0f / fMSPerXUnit;

            while( iSwapTimeInSeconds >= 0 && fX >= GraphRect.x1 )
            {
                swprintf_s( strBuffer, L"%d", iSwapTimeInSeconds );
                m_Font.DrawText( fX, (FLOAT) GraphRect.y2 + 10.0f, D3DCOLOR_ARGB( 255, 255, 255, 255 ), strBuffer, ATGFONT_CENTER_X );

                --iSwapTimeInSeconds;
                fX -= 1000.0f / fMSPerXUnit;
            }
        }

        // Draw vertical axis labels
        m_Font.DrawText( (FLOAT) GraphRect.x2 + 5.0f, (FLOAT) GraphRect.y2, D3DCOLOR_ARGB( 255, 255, 255, 255 ), L"  0", ATGFONT_CENTER_Y );
        m_Font.DrawText( (FLOAT) GraphRect.x2 + 5.0f, Lerp( (FLOAT) GraphRect.y1, (FLOAT) GraphRect.y2, 0.5f ), D3DCOLOR_ARGB( 255, 255, 255, 255 ), L" 50", ATGFONT_CENTER_Y );
        m_Font.DrawText( (FLOAT) GraphRect.x2 + 5.0f, (FLOAT) GraphRect.y1, D3DCOLOR_ARGB( 255, 255, 255, 255 ), L"100", ATGFONT_CENTER_Y );

        m_Font.End();
    }

    // Draw the graph data
    {

        D3DVIEWPORT9 OldViewport;
        m_pd3dDevice->GetViewport( &OldViewport );
        D3DVIEWPORT9 GraphViewport = { GraphRect.x1, GraphRect.y1, GraphRect.x2 - GraphRect.x1, GraphRect.y2 - GraphRect.y1, 0.0f, 1.0f };
        m_pd3dDevice->SetViewport( &GraphViewport );

        if( iDataCount > 1 )
        {
            for( UINT i = 0; i < GRAPH_TYPE_COUNT; ++i )
            {
                ATG::DebugDraw::DrawScreenSpaceLineList( DataPoints[i], iDataCount, d3dColorGraphType[i], 2.0f );
            }
        }

        m_pd3dDevice->SetViewport( &OldViewport );
    }

    // Draw the performance information as text
    {
        FLOAT fMarginX = 60.0f;
        FLOAT fMarginY = 20.0f;

        FLOAT fHeadingScale = 1.0f;
        FLOAT fDataScale = 0.8f;
        FLOAT fIndicatorScale = 0.9f;

        m_Font.Begin();

        FLOAT fTabX = 300.0f;
        FLOAT fHeadingYInc = 30.0f * fHeadingScale;
        FLOAT fDataYInc = 30.0f * fDataScale;
        FLOAT fLineY = fMarginY + 20.0f;

        // Frame rate statistics
        m_Font.SetScaleFactors( fHeadingScale, fHeadingScale );

        fLineY += fHeadingYInc;
        m_Font.DrawText( fMarginX, fLineY, 0xffffff00, L"Render Throughput:" );

        m_Font.SetScaleFactors( fDataScale, fDataScale );

        fLineY += fDataYInc;
        swprintf_s( strBuffer, L"%2.2f ms", 1000.0f / s_SwapDeltaSmoother.GetSmoothedValue() );
        m_Font.DrawText( fMarginX, fLineY, d3dColorGraphType[GRAPH_TYPE_FRAME_RATE], L"  Frame Rate:" );
        m_Font.DrawText( fMarginX + fTabX, fLineY, d3dColorGraphType[GRAPH_TYPE_FRAME_RATE], strBuffer );

        fLineY += fDataYInc;
        swprintf_s( strBuffer, L"%2.2f ms", s_SwapDeltaSmoother.GetSmoothedValue() );
        m_Font.DrawText( fMarginX, fLineY, 0xffffff00, L"  Frame Time:" );
        m_Font.DrawText( fMarginX + fTabX, fLineY, 0xffffff00, strBuffer );

        fLineY += fDataYInc;
        swprintf_s( strBuffer, L"%2.2f ms", s_JitterSmoother.GetSmoothedValue() );
        m_Font.DrawText( fMarginX, fLineY, d3dColorGraphType[GRAPH_TYPE_JITTER], L"  Jitter:" );
        m_Font.DrawText( fMarginX + fTabX, fLineY, d3dColorGraphType[GRAPH_TYPE_JITTER], strBuffer );

        // Latency statistics
        m_Font.SetScaleFactors( fHeadingScale, fHeadingScale );

        fLineY += fHeadingYInc;
        m_Font.DrawText( fMarginX, fLineY, 0xffffff00, L"Render Latency:" );

        m_Font.SetScaleFactors( fDataScale, fDataScale );

        fLineY += fDataYInc;
        swprintf_s( strBuffer, L"%2.2f ms", fEndToEndLatencyInMS );
        m_Font.DrawText( fMarginX, fLineY, d3dColorGraphType[GRAPH_TYPE_LATENCY], L"  End-to-End:" );
        m_Font.DrawText( fMarginX + fTabX, fLineY, d3dColorGraphType[GRAPH_TYPE_LATENCY], strBuffer );

        fLineY += fDataYInc;
        swprintf_s( strBuffer, L"%2.2f ms", s_LatencySmoother[LATENCY_TIMER_D3D_START_TO_GPU_START].GetSmoothedValue() );
        m_Font.DrawText( fMarginX, fLineY, 0xffffff00, L"  D3D start --> GPU start:" );
        m_Font.DrawText( fMarginX + fTabX, fLineY, 0xffffff00, strBuffer );

        fLineY += fDataYInc;
        swprintf_s( strBuffer, L"%2.2f ms", s_LatencySmoother[LATENCY_TIMER_GPU_START_TO_BACK_BUFFER_READY].GetSmoothedValue() );
        m_Font.DrawText( fMarginX, fLineY, 0xffffff00, L"  GPU start --> back buf ready:" );
        m_Font.DrawText( fMarginX + fTabX, fLineY, 0xffffff00, strBuffer );

        fLineY += fDataYInc;
        swprintf_s( strBuffer, L"%2.2f ms", s_LatencySmoother[LATENCY_TIMER_BACK_BUFFER_READY_TO_FRONT_BUFFER_READY].GetSmoothedValue() );
        m_Font.DrawText( fMarginX, fLineY, 0xffffff00, L"  Back buf ready --> front buf ready:" );
        m_Font.DrawText( fMarginX + fTabX, fLineY, 0xffffff00, strBuffer );

        fLineY += fDataYInc;
        swprintf_s( strBuffer, L"%2.2f ms", s_LatencySmoother[LATENCY_TIMER_FRONT_BUFFER_READY_TO_FLIP].GetSmoothedValue() );
        m_Font.DrawText( fMarginX, fLineY, 0xffffff00, L"  Front buf ready --> flip:" );
        m_Font.DrawText( fMarginX + fTabX, fLineY, 0xffffff00, strBuffer );

        // Tearing statistics
        m_Font.SetScaleFactors( fHeadingScale, fHeadingScale );

        fLineY += fHeadingYInc;
        m_Font.DrawText( fMarginX, fLineY, 0xffffff00, L"Tearing:" );

        m_Font.SetScaleFactors( fDataScale, fDataScale );

        fLineY += fDataYInc;
        swprintf_s( strBuffer, L"%2.2f%%", s_TearingFreqSmoother.GetSmoothedValue() );
        m_Font.DrawText( fMarginX, fLineY, 0xffffff00, L"  % of Frames:" );
        m_Font.DrawText( fMarginX + fTabX, fLineY, 0xffffff00, strBuffer );

        fLineY += fDataYInc;
        swprintf_s( strBuffer, L"%2.2f%%", 100.f * s_TearingLocationSmoother.GetSmoothedValue() / (FLOAT) m_d3dpp.BackBufferHeight );
        m_Font.DrawText( fMarginX, fLineY, d3dColorGraphType[GRAPH_TYPE_TEARING], L"  % of Screen:" );
        m_Font.DrawText( fMarginX + fTabX, fLineY, d3dColorGraphType[GRAPH_TYPE_TEARING], strBuffer );

        // Latency and tearing indicator labels
        m_Font.SetScaleFactors( fIndicatorScale, fIndicatorScale );

        m_Font.DrawText( fXJitterIndicator - 30.0f, 660.0f, d3dColorGraphType[GRAPH_TYPE_JITTER], 
            L"Jitter\nindicator ", ATGFONT_RIGHT );
        m_Font.DrawText( fXJitterIndicator - 30.0f, 680.0f, d3dColorGraphType[GRAPH_TYPE_JITTER], 
            GLYPH_RIGHT_ARROW );
        m_Font.DrawText( fXJitterIndicator + 20.0f, 680.0f, d3dColorGraphType[GRAPH_TYPE_LATENCY], 
            GLYPH_LR_ARROW );
        m_Font.DrawText( fXJitterIndicator + 50.0f, 660.0f, d3dColorGraphType[GRAPH_TYPE_LATENCY], 
            L"Latency\nindicator " );
        m_Font.DrawText( 1240.0f, fYTearingIndicatorAvg, d3dColorGraphType[GRAPH_TYPE_TEARING], 
            GLYPH_UP_ARROW L"\nTearing\nindicator\n" GLYPH_DOWN_ARROW, ATGFONT_RIGHT );

        m_Font.End();
    }

    // Reset for next time
    s_iEarliestValidSwapIndex = iEarliestInvalidSwapIndex;
}


//---------------------------------------------------------------------------------------------------------
// Name: StartSimulatedFrame()
// Desc: Called once per frame, to mark the start of the scene, and begin countdowns for simulated 
// app activity on the CPU and GPU.
//---------------------------------------------------------------------------------------------------------
VOID Sample::StartSimulatedFrame()
{
    UINT iSimulatedCPULoad          = m_SimulatedCPULoadParam.GetValue();
    UINT iSimulatedCPUSpike         = m_SimulatedCPUSpikeParam.GetValue();
    UINT iSimulatedSpikeFrames      = m_SimulatedSpikeFramesParam.GetValue();
    UINT iSimulatedSpikePeriod      = m_SimulatedSpikePeriodParam.GetValue();
    BOOL bAsyncSwaps                = m_AsyncSwapsParam.GetValue();
    UINT iThrottlingPoint           = m_ThrottlingPointParam.GetValue();

    // We manually throttle D3D in the non-async frame case, to guarantee that the throttling 
    // occurs in the right place for the CPU-GPU lag simulation.
    if( iThrottlingPoint == THROTTLING_POINT_START_OF_FRAME )
    {
        if( bAsyncSwaps )
        {
            WaitOnFreeFrontBuffer();
        }
        else
        {
            PIX_EVENT_SCOPE( "CPU #%d: Wait for #%d end frame fence", m_iGPUFrameIndex.Get(), (m_iGPUFrameIndex - 1).Get() );

            DWORD dwEndFrameFence = m_dwEndFrameFence[(m_iGPUFrameIndex - 1).Get()];
            if( dwEndFrameFence != 0 )
            {
                m_pd3dDevice->BlockOnFence( dwEndFrameFence );
            }
        }
    }

    if( iThrottlingPoint == THROTTLING_POINT_MID_FRAME )
    {
        PIX_EVENT_SCOPE( "CPU #%d: Wait for #%d mid frame fence", m_iGPUFrameIndex.Get(), (m_iGPUFrameIndex - 1).Get() );

        DWORD dwMidFrameFence = m_dwMidFrameFence[(m_iGPUFrameIndex - 1).Get()];
        if( dwMidFrameFence )
        {
            m_pd3dDevice->BlockOnFence( dwMidFrameFence );
        }
    }

    m_lfSystemTimeInMS = m_Timer.GetAppTime() * 1000.0f;

    {
        PIX_EVENT_SCOPE( "CPU #%d: Start of frame", m_iGPUFrameIndex.Get() );

        static UINT s_iFrameCounter = 0;
        ++s_iFrameCounter;

        UINT iSimulatedCPUTime = ( s_iFrameCounter % iSimulatedSpikePeriod < iSimulatedSpikeFrames )
            ? iSimulatedCPUSpike
            : iSimulatedCPULoad;

        LARGE_INTEGER liCPUDueTime; 
        liCPUDueTime.QuadPart = g_llRelative100NSToMS * iSimulatedCPUTime;
        SetWaitableTimer( m_hWaitableTimerSimulatedCPULoad, &liCPUDueTime, 0, NULL, NULL, FALSE );

        m_pLatencyTimer[LATENCY_TIMER_D3D_START_TO_GPU_START]->Start();

        // Simulate bubbles in render submission by assuming they all occur at the beginning of the
        // frame.  In other words, the CPU simulates some work for N ms before the first draw calls
        // are submitted.  This might represent some non-rendering work, or scene traversal/setup.  
        // It might represent predicated tiling.  
        UINT iSimulatedGPUStarvation    = m_SimulatedGPUStarvationParam.GetValue();

        LARGE_INTEGER liGPUStarvationDueTime; 
        liGPUStarvationDueTime.QuadPart = g_llRelative100NSToMS * iSimulatedGPUStarvation;
        SetWaitableTimer( m_hWaitableTimerSimulatedCPUGPULag, &liGPUStarvationDueTime, 0, NULL, NULL, FALSE );
    }

    // "Draw" ground truth latency indicator, by writing directly to all front buffers
    {
        PIX_EVENT_SCOPE( "CPU #%d: Draw ground truth latency indicator", m_iGPUFrameIndex.Get() );

        DOUBLE lfDummy;
        UINT x = (UINT) ( modf( m_lfSystemTimeInMS * m_fSpeedInScreensPerMS, &lfDummy ) * m_d3dpp.BackBufferWidth );
        for( UINT y = 0; y < m_d3dpp.BackBufferHeight; ++y )
        {
            UINT iPixelOffset = XGAddress2DTiledOffset( x, y, m_d3dpp.BackBufferWidth, 4 );

            for( UINT i = 0; i < MAX_FRONT_BUFFERS; ++i )
            {
                DWORD* pFrontBufferData = (DWORD*) ( m_pFrontBuffer[i]->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT );
                DWORD* pPixel = pFrontBufferData + iPixelOffset;
                *pPixel = 0x00ffffff;   // hard-coded endian-swapped white (anything darker is hard to see)
            }
        }
    }
    
    {
        PIX_EVENT_SCOPE( "CPU #%d: GPU enforced lag", m_iGPUFrameIndex.Get() );

        WaitForSingleObject( m_hWaitableTimerSimulatedCPUGPULag, INFINITE );
    }

    {
        PIX_EVENT_SCOPE( "GPU #%d: Start of frame", m_iGPUFrameIndex.Get() );

        // These must be reset between FixupAndSignal and the following InsertAsyncCommandBufferCall
        m_pAsyncStallSimulatedGPULoad[m_iGPUFrameIndex.Get()]->Reset( NULL, NULL, 0, 0 );
        m_pAsyncStallMidFrameSimulatedGPULoad[m_iGPUFrameIndex.Get()]->Reset( NULL, NULL, 0, 0 );

        // Signal to worker thread when the GPU is available to begin work on this frame
        m_pd3dDevice->InsertCallback( D3DCALLBACK_IDLE, GPUCallbackStartOfFrame, 0 );

        // Force immediate kickoff to GPU once this reaches the head of the command buffer
        m_pd3dDevice->InsertFence();  

        // For accuracy, need to make sure D3D's BlockOnSwap happens here for BUFFER 1 FRAME.
        // We do this by using very small ring buffer segments.
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: MidSimulatedFrame()
// Desc: Called once per frame, to implement mid-frame D3D throttling.
//---------------------------------------------------------------------------------------------------------
VOID Sample::MidSimulatedFrame()
{
    // After the GPU has done about half a frame's work mark the location with a fence
    {
        PIX_EVENT_SCOPE( "GPU #%d: Wait on mid-frame load", m_iGPUFrameIndex.Get() );
        m_pd3dDevice->InsertAsyncCommandBufferCall( m_pAsyncStallMidFrameSimulatedGPULoad[m_iGPUFrameIndex.Get()], 0, 0 );
    }

    {
        PIX_EVENT_SCOPE( "GPU #%d: Mid frame", m_iGPUFrameIndex.Get() );
        m_dwMidFrameFence[m_iGPUFrameIndex.Get()] = m_pd3dDevice->InsertFence();
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: EndSimulatedFrame()
// Desc: Called once per frame, to mark the end of the scene, and end countdowns for simulated app 
// activity on the CPU and GPU.
//---------------------------------------------------------------------------------------------------------
VOID Sample::EndSimulatedFrame()
{
    {
        PIX_EVENT_SCOPE( "GPU #%d: Wait on end frame load", m_iGPUFrameIndex.Get() );
        m_pd3dDevice->InsertAsyncCommandBufferCall( m_pAsyncStallSimulatedGPULoad[m_iGPUFrameIndex.Get()], 0, 0 );
    }

    {
        PIX_EVENT_SCOPE( "CPU #%d: Wait on end frame load", m_iGPUFrameIndex.Get() );
        WaitForSingleObject( m_hWaitableTimerSimulatedCPULoad, INFINITE );
    }
    {
        PIX_EVENT_SCOPE( "CPU #%d: End of frame", m_iGPUFrameIndex.Get() );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: WaitOnFreeFrontBuffer()
// Desc: Block until the next front buffer is not in use.
//---------------------------------------------------------------------------------------------------------
VOID Sample::WaitOnFreeFrontBuffer()
{
    UINT iNumFrontBuffers           = m_NumFrontBuffersParam.GetValue();
    UINT iAsyncThrottlingMethod     = m_AsyncThrottlingMethodParam.GetValue();

    UINT iMaxEnqueuedCount = iNumFrontBuffers;

    if( iAsyncThrottlingMethod == ASYNC_THROTTLING_METHOD_GPU_STALL )
    {
        PIX_EVENT_SCOPE( "GPU #%d: Wait on front buffer #%d", m_iGPUFrameIndex.Get(), m_iFrontBufferIndex );

        // Allow one additional frame of buffering
        iMaxEnqueuedCount += 1;

        // Force the GPU to wait next front buffer is finished displaying.  This front buffer began displaying
        // 'iNumFrontBuffers' frames ago.  So it will be finished displaying on the frame after that one.
        RingUInt<m_iMaxGPUFramesInFlight> iThrottlingFrameIndex = m_iGPUFrameIndex - iNumFrontBuffers + 1;
        m_pd3dDevice->InsertAsyncCommandBufferCall( m_pAsyncStallAsyncThrottling[iThrottlingFrameIndex.Get()], 0, 0 );
    }

    // Even if we are using GPU throttling, we must throttle the CPU at some point.  Otherwise, we can 
    // queue up arbitrarily many frames of commands, at least until we use up the secondary ring buffer. 
    //
    // So we allow one more frame enqueued in the GPU case.  Any more would probably be counterproductive,
    // unless you don't care about latency at all, and have lots of D3D ring buffer memory available.  (That 
    // might be the case for cutscenes, or replay, for instance.)  We'd need to increase m_iMaxGPUFramesInFlight
    // to try that out.
    {    
        PIX_EVENT_SCOPE( "CPU #%d: Wait on front buffer #%d", m_iGPUFrameIndex.Get(), m_iFrontBufferIndex );

        // There are 'EnqueuedCount' front buffers waiting to be displayed, plus one front buffer
        // currently being displayed.
        D3DSWAP_STATUS SwapStatus;
        do
        {
            m_pd3dDevice->QuerySwapStatus( &SwapStatus );
        } while( SwapStatus.EnqueuedCount + 1 >= iMaxEnqueuedCount );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Present()
// Desc: Do whatever sort of Present we have chosen --- sync or async.
//---------------------------------------------------------------------------------------------------------
HRESULT Sample::Present()
{
    BOOL bAsyncSwaps            = m_AsyncSwapsParam.GetValue();
    UINT iThrottlingPoint       = m_ThrottlingPointParam.GetValue();

    IDirect3DTexture9* pFrontBuffer = m_pFrontBuffer[m_iFrontBufferIndex];

    {
        PIX_EVENT_SCOPE( "GPU #%d: Back buffer ready", m_iGPUFrameIndex.Get() );
        // Signal to worker thread when the GPU is finished with this frame
        m_pd3dDevice->InsertCallback( D3DCALLBACK_IDLE, GPUCallbackBackBufferReady, 0 );

        // Force immediate kickoff to GPU once this reaches the head of the command buffer
        m_pd3dDevice->InsertFence();  
    }

    if( bAsyncSwaps 
        && ( iThrottlingPoint == THROTTLING_POINT_END_OF_FRAME || iThrottlingPoint == THROTTLING_POINT_MID_FRAME ) )
    {
        WaitOnFreeFrontBuffer();
    }

    if( !bAsyncSwaps )
    {
        PIX_EVENT_SCOPE( "GPU #%d: Wait until present interval", m_iGPUFrameIndex.Get() );

        // With async swaps inactive, what happens in SynchronizeToPresentationInterval is this:
        //  - D3D may insert a GPU command, depending on our Present settings
        //  - When the GPU reaches this point in the command buffer, depending on our Present settings it either
        //    - does nothing, or
        //    - stalls until the next Vsync
        m_pd3dDevice->SynchronizeToPresentationInterval();
    }

    {
        PIX_EVENT_SCOPE( "GPU #%d: Resolve to front buffer #%d", m_iGPUFrameIndex.Get(), m_iFrontBufferIndex );
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pFrontBuffer, NULL, 0, 0, NULL, 0.0f, 0, NULL );
    }

    {
        // This is slightly weird, because we can get negative times for the latency between front buffer
        // ready and flip.  That's actually the truth, since we begin the Resolve at the flip, and it takes
        // non-zero time to complete.  The negative latency reflects the fact that the VBlank interval gives 
        // you some leeway to modify the front buffer after the Vsync.
        PIX_EVENT_SCOPE( "GPU #%d: Front buffer #%d ready", m_iGPUFrameIndex.Get(), m_iFrontBufferIndex );

        // Signal to worker thread when the GPU is finished with this frame
        m_pd3dDevice->InsertCallback( D3DCALLBACK_IDLE, GPUCallbackFrontBufferReady, 0 );

        // Force immediate kickoff to GPU once this reaches the head of the command buffer
        m_dwEndFrameFence[m_iGPUFrameIndex.Get()] = m_pd3dDevice->InsertFence();  
    }

    {
        // With async swaps inactive, what happens in Swap is this:
        //  - D3D inserts a GPU command
        //  - The GPU switches the front buffer pointer when it executes the command
        //
        // With async swaps active, what happens in Swap is this:
        //  - D3D schedules a swap callback when runs as soon as all preceding GPU activity is complete
        //  - At that callback, depending on our Present settings, D3D may either:
        //    - Switch the front buffer pointer immediately, or
        //    - Wait until the following Vsync
        //  - In the latter case, during the next VBlank callback, D3D switches the front buffer pointer
        D3DSWAP_STATUS SwapStatus;
        m_pd3dDevice->QuerySwapStatus( &SwapStatus );
        PIX_EVENT_SCOPE( "Swap #%d: Using front buffer #%d", SwapStatus.Swap + 1, m_iFrontBufferIndex );

        m_pd3dDevice->Swap( pFrontBuffer, NULL );

    }

    return S_OK;
}


//---------------------------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, to render the scene.
//---------------------------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    ATG::RenderBackground( D3DCOLOR_ARGB( 0xff, 0x10, 0x10, 0x60 ), D3DCOLOR_ARGB( 0xff, 0x10, 0x10, 0x60 ) );

    RenderPerformanceData();

    RenderUI();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }

    MidSimulatedFrame();

    EndSimulatedFrame();

    // Present the backbuffer contents to the display
    Present();

    m_pd3dDevice->UnsetAll();

    // Increment per-frame counters
    UINT iNumFrontBuffers = m_AsyncSwapsParam.GetValue() ? m_NumFrontBuffersParam.GetValue() : 1;
    m_iFrontBufferIndex = ++m_iFrontBufferIndex % iNumFrontBuffers;
    ++m_iGPUFrameIndex;
    m_pAsyncStallAsyncThrottling[m_iGPUFrameIndex.Get()]->Reset( NULL, NULL, 0, 0 );

    return S_OK;
}


//---------------------------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//---------------------------------------------------------------------------------------------------------
INT __cdecl main()
{
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    // Required to support async swaps
    atgApp.m_d3dpp.DisableAutoFrontBuffer = TRUE;

    atgApp.Run();
}


