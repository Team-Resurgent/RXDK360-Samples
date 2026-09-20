//--------------------------------------------------------------------------------------
// APOBridge.h
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DataOutAPO.h"
#include "DataInAPO.h"
#include <crtdbg.h>

// This class was defined to support a maximum of 48KHz sampling rate.  That equates to 256 samples per quantum.
// We will be declaring an array of 261 entries to hold the maximum supported number of samples, plus an extra per channel
#define BRIDGE_MAX_SAMPLE_COUNT 261

//
// APOBridge acts as a connecter between the APO on the XAudio2 source voice, and the APO on the XHV2 remote talker.
// This class receives input from the XAudio2 voice, and then supplies that input to the XHV2 voice on demand.
//
class APOBridge : public IAPODataReceiver, public IAPODataSender
{
public:
    APOBridge(UINT nSamplingRate, UINT nChannels)
        : m_bDataReceived(false)
    {
        // We can't guarantee that we will always have an integer number of expecte samples.  XAudio2
        // will provide a full extra sample if the division has a remainder, per channel.  We add another
        // sample per channel to have enough room to store our samples
        m_iSampleCount = (nSamplingRate * nChannels * XAUDIO2_QUANTUM_NUMERATOR / XAUDIO2_QUANTUM_DENOMINATOR) + nChannels;

        _ASSERT_EXPR(BRIDGE_MAX_SAMPLE_COUNT > m_iSampleCount, L"ERROR: APOBridge doesn't support this sampling rate");

        // For most of our operations, we're only going to care about a subset of the array.  When initializing,
        // however, we need to set all of the entries to zero
        XMemSet(m_fBuffer, 0, sizeof(m_fBuffer));
    }

    //
    // DataReceived is called by the XAudio2SourceVoice's XAPO.  This method is called every quantum,
    // when XAudio2 has produced more data for the voice.
    //
    void DataReceived(const float * pData, UINT nSampleCount)
    {
        _ASSERT_EXPR(nSampleCount <= m_iSampleCount, L"Input sample count must be less than or equal to expected sample count");
        _ASSERT_EXPR(nSampleCount <=  BRIDGE_MAX_SAMPLE_COUNT, L"Input sample count must not be larger than output buffer size");
        XMemCpy(m_fBuffer, pData, nSampleCount * sizeof(float));
        m_bDataReceived = true;
    }

    //
    // GetData is called by the XHV2 RemoteTalker's APO.  This method is called whenever the remote talker's
    // internal XAudio2 voice is processed.  If we've received data from the DataOutAPO, then we provide
    // that data to the XHV2 RemoteTalker
    //
    bool GetData(float * pData, UINT nSampleCount)
    {
        _ASSERT_EXPR(nSampleCount <= m_iSampleCount, L"Expected sample count must be less than or equal to input sample count");

        if (m_bDataReceived)
        {
            _ASSERT_EXPR(m_iSampleCount <=  BRIDGE_MAX_SAMPLE_COUNT, L"Input sample count must not be larger than buffer size");
            XMemCpy(pData, m_fBuffer, m_iSampleCount * sizeof(float));
        }

        return m_bDataReceived;
    }

private:
    float m_fBuffer[BRIDGE_MAX_SAMPLE_COUNT];
    UINT m_iSampleCount;
    bool m_bDataReceived;
};