#ifndef PROFILING_SUPPORT_H
#define PROFILING_SUPPORT_H

// The CXBBeginEventObject compiles away to nothing if ENABLE_PROFILING,
// ENABLE_DEBUG, PROFILE, and _DEBUG are not defined.
#if defined(ENABLE_PROFILING) && !defined(ENABLE_DEBUG) && !defined(_DEBUG)
    #define PROFILE
#endif

#include <stdio.h>
#include <xbdm.h>


//-----------------------------------------------------------------------------
// Name: Profiling_CheckCommand()
// Desc: Returns TRUE if a command matching the specified name has been sent
// with the TriggerProfile tool on the PC.
// Typical usage would be to call this once per frame and trigger a
// trace recording or performance counter recording when it returns TRUE.
// This function will return TRUE once for each use of the TriggerProfile
// tool.
// Typical usage is like this:
//
//        ProfileRecording profile;
//        if ( Profiling_CheckCommand( "tracemain" ) )
//            profile.BeginCapture( "devkit:\\trace_main.pix2" );
//        if ( Profiling_CheckCommand( "pmcmain" ) )
//            profile.BeginPMC( PMC_SETUP_OVERVIEW_PB0T0 + Profiling_GetInt() );
//
//-----------------------------------------------------------------------------
BOOL Profiling_CheckCommand( const CHAR* commandName );


//-----------------------------------------------------------------------------
// Name: Profiling_GetInt()
// Desc: After Profiling_CheckCommand() returns true this function will return
// the optional integer parameter specified after the command, or zero.
// 
//-----------------------------------------------------------------------------
int Profiling_GetInt();


//-----------------------------------------------------------------------------
// Name: class ProfileRecording
// Desc: Simple class to manage starting and stopping trace recording and
// PMC (Performance Monitor Counter) timing.
//
//       To use it, declare an object of ProfileRecording type in your
// main loop. When you want to create a trace recording, call
// BeginCapture(). When the object leaves scope it's destructor will
// automatically call EndCapture(), or you can call it manually. The
// trace recording will be saved to the specified file on your devkit.
//       When you want to record performance counters, call BeginPMC().
// When the object leaves scope it's destructor will automatically call 
// EndPMC(), or you can call it manually. The performance counter information
// will be printed to the debug output.
//
// Trace recording works best on a full release build with no instrumentation.
//-----------------------------------------------------------------------------
class ProfileRecording
{
public:
    inline  ProfileRecording()
    {
        m_recordingTrace = false;
        m_recordingPMC = false;
    }

    inline  ~ProfileRecording()
    {
        EndCapture();
        EndPMC();
    }

    //-----------------------------------------------------------------------------
    // Name: BeginCapture()
    // Desc: Start a trace capture.
    //     You can stop the trace recording by calling EndCapture, or let
    // the destructor call it at the end of the frame.
    //-----------------------------------------------------------------------------
    void    BeginCapture( const CHAR* traceFilename );

    //-----------------------------------------------------------------------------
    // Name: EndCapture()
    // Desc: End trace recording. Then you can manually copy the resulting
    // .pix2 file to your PC and use tracedump, together with the .pe and .pdb
    // files, to analyze your performance.
    //-----------------------------------------------------------------------------
    void    EndCapture();

    //-----------------------------------------------------------------------------
    // Name: BeginPMC()
    // Desc: Start a PMC recording
    //     You can stop the PMC recording by calling EndPMC, or let
    // the destructor call it at the end of the frame.
    //-----------------------------------------------------------------------------
    void    BeginPMC( int whichSetup );

    //-----------------------------------------------------------------------------
    // Name: EndPMC()
    // Desc: End PMC recording. The results will be printed to the debug output.
    //-----------------------------------------------------------------------------
    void    EndPMC();

    //-----------------------------------------------------------------------------
    // Name: CustomDumpCounters()
    // Desc: Custom code to print the counters in the standard PMC format. This
    //       code can be modified to adjust the format or to display it on-screen.
    //       The results will be the same as PMCDumpCountersVerbose.
    //-----------------------------------------------------------------------------
    void    CustomDumpCounters( const PMCState& pmcstate );

private:
    bool m_recordingTrace;
    bool m_recordingPMC;
    ePMCSetup m_lastSetup;

    // The copy constructor and assignment operator are private and
    // unimplemented to disallow object copying.
    ProfileRecording& operator=( const ProfileRecording& rhs );
            ProfileRecording( const ProfileRecording& rhs );
};


//-----------------------------------------------------------------------------
// This object encapsulates the PIXBeginNamedEvent/EndEvent commands, to ensure
// that they are always properly nested, while requiring a minimum of code
// modifications to add these profiling calls - just one line per event.
// This class completely disappears in release builds.
// Example usage:
// BeginEventObject eventObject( "Object %s", object->GetName() );
// BeginEventObject eventObject( D3DCOLOR_XRGB( 0xff,0xff,0x7f ), "FrameMove" );
//-----------------------------------------------------------------------------
class CXBBeginEventObject
{
public:
    //-----------------------------------------------------------------------------
    // Name: CXBBeginEventObject()
    // Desc: The constructors call PIXBeginNamedEvent and the destructor will
    // call PIXEndNamedEvent. This constructor takes just a color and a name.
    // The color is optional, and you can specify up to two additional parameters
    // for sprintf style formatting of the event label - the appropriate template
    // constructor will be automatically called.
    //-----------------------------------------------------------------------------
    inline  CXBBeginEventObject( D3DCOLOR color, const CHAR* name )
    {
#if defined(PROFILE) || defined(_DEBUG)
        PIXBeginNamedEvent(color, name);
#endif
    }
    // This constructor is the same as the one above except that it
    // doesn't require the color parameter. A color will be chosen from a table.
    inline  CXBBeginEventObject( const CHAR* name )
    {
#if defined(PROFILE) || defined(_DEBUG)
        PIXBeginNamedEvent(GetNextColor(), name);
#endif
    }


    // Template constructor to support the sprintf style interface to PIXBeginNamedEvent
    // This constructor is used for one sprintf parameter of any type.
    template <typename T> inline CXBBeginEventObject( D3DCOLOR color, const CHAR* name, T data1 )
                          {
#if defined(PROFILE) || defined(_DEBUG)
        PIXBeginNamedEvent(color, name, data1);
#endif
                          }
    // This constructor is the same as the one above except that it
    // doesn't require the color parameter. A color will be chosen from a table.
    template <typename T> inline CXBBeginEventObject( const CHAR* name, T data1 )
                          {
#if defined(PROFILE) || defined(_DEBUG)
        PIXBeginNamedEvent(GetNextColor(), name, data1);
#endif
                          }


    // Template constructor to support the sprintf style interface to PIXBeginNamedEvent
    // This constructor is used for two sprintf parameters of any types.
    template <typename T, typename T2> inline CXBBeginEventObject( D3DCOLOR color, const CHAR* name, T data1,
                                                                   T2 data2 )
                                       {
#if defined(PROFILE) || defined(_DEBUG)
        PIXBeginNamedEvent(color, name, data1, data2);
#endif
                                       }
    // This constructor is the same as the one above except that it
    // doesn't require the color parameter. A color will be chosen from a table.
    template <typename T, typename T2> inline CXBBeginEventObject( const CHAR* name, T data1, T2 data2 )
                                       {
#if defined(PROFILE) || defined(_DEBUG)
        PIXBeginNamedEvent(GetNextColor(), name, data1, data2);
#endif
                                       }


    //-----------------------------------------------------------------------------
    // Name: ~CXBBeginEventObject()
    // Desc: The destructor for CXBBeginEventObject calls PIXEndNamedEvent to
    // ensure that Begin/End calls are always matched.
    //-----------------------------------------------------------------------------
    inline  ~CXBBeginEventObject()
    {
#if defined(PROFILE) || defined(_DEBUG)
        PIXEndNamedEvent();
#endif
    }

private:
#if defined(PROFILE) || defined(_DEBUG)

    //-----------------------------------------------------------------------------
    // GetNextColor() simplifies the effective usage of BeginEventObject by
    // returning a sequence of colors, so that adjacent events in the timeline
    // are different colors. When the hierarchy is collapsed it may end up that
    // adjacent events are the same color, but with enough colors in the
    // s_colors array this should be rare. Some extra code could avoid this
    // possibility, but it's not really important.
    // The colors will be different on different captures.
    //-----------------------------------------------------------------------------
    inline D3DCOLOR GetNextColor() const
    {
        static int s_currentColor = 0;
        // Add all your favorite colors to this array.
        static D3DCOLOR s_colors[] =
        {
            D3DCOLOR_XRGB(0xFF, 0x00, 0x00),    // Red
            D3DCOLOR_XRGB(0x00, 0xFF, 0x00),    // Green
            D3DCOLOR_XRGB(0x00, 0x00, 0xFF),    // Blue
            D3DCOLOR_XRGB(0xFF, 0xFF, 0x00),    // Yellow
            D3DCOLOR_XRGB(0xFF, 0x00, 0xFF),    // Magenta
            D3DCOLOR_XRGB(0x00, 0xFF, 0xFF),    // Cyan
        };
        ++s_currentColor;
        if ( s_currentColor >= ARRAYSIZE( s_colors ) )
            s_currentColor = 0;
        return s_colors[s_currentColor];
    }
#endif

    // The copy constructor and assignment operator are private and
    // unimplemented to disallow object copying.
            CXBBeginEventObject( const CXBBeginEventObject& rhs );
    CXBBeginEventObject& operator=( const CXBBeginEventObject& rhs );
};

#endif  // PROFILING_SUPPORT_H
