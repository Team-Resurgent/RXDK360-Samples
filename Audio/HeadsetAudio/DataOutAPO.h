//--------------------------------------------------------------------------------------
// DataOutAPO.h
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#ifndef DataOutAPOH
#define DataOutAPOH

#include <XAudio2.h>
#include <XAPOBase.h>
#include <crtdbg.h>

interface __declspec(novtable) IAPODataReceiver
{
public:
	//
	// Implementations of IAPODataReceiver::DataReceived _must not_ block.  The entire XAudio2 audio graph has to be processed in 5.33ms, or
	// glitching will occur.  
	//
	// IAPODataReceiver::DataReceived will be called from the realtime audio processing thread, so care must be taken with multi-threaded 
	// programming concerns in the implementation
	//
	virtual void WINAPI DataReceived(const float* pData, UINT nSampleCount) = 0;
};

//
//	A straightforward APO that passes data from an XAudio2 voice out to an arbitrary receiver.
//
class __declspec( uuid("{C945A6F5-BD08-41b9-A94B-B5791DE011C5}")) DataOutAPO : public CXAPOBase
{
public:
	// if bForceSilentVoice is set to true, the voice this APO is applied to will be silenced, after the data is 
	// sent out to the IAPODataReceiver
	DataOutAPO(IAPODataReceiver* pDataReceiver, bool bForceSilentVoice);

    //
    // IXAPO Implementation
    //
    virtual HRESULT WINAPI LockForProcess (
        UINT32 InputLockedParameterCount,
        const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pInputLockedParameters,
        UINT32 OutputLockedParameterCount,
        const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pOutputLockedParameters );

	//
	// Process is called once per quantum, and is the XAPO's time to process the audio data
	//
    virtual void WINAPI Process(
        UINT32 InputProcessParameterCount,
        const XAPO_PROCESS_BUFFER_PARAMETERS* pInputProcessParameters,
        UINT32 OutputProcessParameterCount,
        XAPO_PROCESS_BUFFER_PARAMETERS* pOutputProcessParameters,
        BOOL IsEnabled);

private:
    DataOutAPO();
	DataOutAPO(const DataOutAPO&);
	DataOutAPO &operator=(const DataOutAPO&);
	
	IAPODataReceiver * m_pDataReceiver;
	UINT m_uChannels;
	UINT m_uBytesPerSample;
	bool m_bSilentVoice;
	
    // Registration properties defining this xAPO class.
    static XAPO_REGISTRATION_PROPERTIES m_regProps;
};

__declspec(selectany) XAPO_REGISTRATION_PROPERTIES DataOutAPO::m_regProps = {
        __uuidof(DataOutAPO),
        L"DataOutAPO",
        L"Copyright (C) Microsoft Corporation",
        1,
        0,
        XAPOBASE_DEFAULT_FLAG,
        1, 1, 1, 1 };

#endif