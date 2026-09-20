//--------------------------------------------------------------------------------------
// DataInAPO.h
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#ifndef DataInAPOH
#define DataInAPOH

#include <XAudio2.h>
#include <XAPOBase.h>
#include <crtdbg.h>

interface __declspec(novtable) IAPODataSender
{
	//
	// Implementations of IAPODataSender::GetData _must not_ block.  The entire XAudio2 audio graph has to be processed in 5.33ms, or
	// glitching will occur.  
	// 
	// IAPODataSender::GetData will be called from the realtime audio processing thread, so care must be taken with multi-threaded 
	// programming concerns in the implementation
	//
	virtual bool WINAPI GetData (float* pData, UINT nSampleCount) = 0;
};

class __declspec( uuid("{B5D213B1-B7F6-49e4-ACEF-A81312721318}")) DataInAPO : public CXAPOBase
{
public:
	DataInAPO(IAPODataSender* pDataSender);

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
    virtual void WINAPI Process (
        UINT32 InputProcessParameterCount,
        const XAPO_PROCESS_BUFFER_PARAMETERS* pInputProcessParameters,
        UINT32 OutputProcessParameterCount,
        XAPO_PROCESS_BUFFER_PARAMETERS* pOutputProcessParameters,
        BOOL IsEnabled);

private:
    DataInAPO();
	DataInAPO(const &DataOutAPO);
	DataInAPO &operator=(const &DataOutAPO);
	
	IAPODataSender * m_pDataSender;
	
    // Registration properties defining this xAPO class.
    static XAPO_REGISTRATION_PROPERTIES m_regProps;
};

__declspec(selectany) XAPO_REGISTRATION_PROPERTIES DataInAPO::m_regProps = {
        __uuidof(DataInAPO),
        L"DataInAPO",
        L"Copyright (C) Microsoft Corporation",
        1,
        0,
        XAPOBASE_DEFAULT_FLAG,
        1, 1, 1, 1 };

#endif
