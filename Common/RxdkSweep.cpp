//--------------------------------------------------------------------------------------
// RxdkSweep.cpp
//
// Implementation of the shared RXDK-360 sample-sweep frame marker (see RxdkSweep.h).
//--------------------------------------------------------------------------------------
#include "RxdkSweep.h"

extern "C" int DbgPrint( const char*, ... );

namespace ATG
{

void RxdkSweepFrame()
{
    // One sample runs per title (process), so a function-static counter is correct.
    // "[RXDK] rendering" the first frame, then "[RXDK] pass" once RXDK_PASS_FRAMES
    // frames have been drawn without a fault or hang (~2-4s of stable rendering).
    const unsigned RXDK_PASS_FRAMES = 120;
    static unsigned s_frames = 0;
    if( s_frames == 0 ) DbgPrint( "[RXDK] rendering\n" );
    ++s_frames;
    if( s_frames == RXDK_PASS_FRAMES ) DbgPrint( "[RXDK] pass\n" );
}

} // namespace ATG
