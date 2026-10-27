//--------------------------------------------------------------------------------------
// RxdkSweep.h
//
// Shared RXDK-360 sample-sweep frame marker. The app frameworks call RxdkSweepFrame()
// once per rendered frame so an automated kit sweep can tell a solidly-running sample
// from one that drew a frame then faulted or hung. No runtime/CRT involvement.
//--------------------------------------------------------------------------------------
#pragma once

namespace ATG
{
    // Call once per rendered frame. Emits "[RXDK] rendering" on the first frame and
    // "[RXDK] pass" once the title sustains RXDK_PASS_FRAMES frames without faulting.
    void RxdkSweepFrame();
}
