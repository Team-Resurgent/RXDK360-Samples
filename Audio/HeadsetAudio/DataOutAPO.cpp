//--------------------------------------------------------------------------------------
// DataOutAPO.cpp
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DataOutAPO.h"

DataOutAPO::DataOutAPO(IAPODataReceiver* pDataReceiver, bool bForceSilentVoice)
	: CXAPOBase(&m_regProps),
	m_pDataReceiver(pDataReceiver),
	m_uChannels(0),
	m_bSilentVoice(bForceSilentVoice)
{
	// validate that the IAPODataReceiver exists
	_ASSERT(pDataReceiver);
}

// 
// Called once when the voice is created.  This is our chance to gather some data about the voice, which we'll need while processing 
// 
HRESULT DataOutAPO::LockForProcess(UINT32 InputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pInputLockedParameters, 
								   UINT32 OutputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pOutputLockedParameters)
{
	m_uChannels = pInputLockedParameters->pFormat->nChannels;
	
	return CXAPOBase::LockForProcess(InputLockedParameterCount, pInputLockedParameters, OutputLockedParameterCount, pOutputLockedParameters);
}

// 
// This method is called every time XAudio2 processes another chunk of data for the voice this APO is applied to.  The method passes the 
// voice's data out to the previously registered data receiver (IAPODataReceiver), which can then process the data further.  
//
void DataOutAPO::Process(UINT32 /*InputProcessParameterCount*/, const XAPO_PROCESS_BUFFER_PARAMETERS* pInputProcessParameters, 
						 UINT32 /*OutputProcessParameterCount*/, XAPO_PROCESS_BUFFER_PARAMETERS* pOutputProcessParameters, BOOL IsEnabled)
{
	if (IsEnabled)
	{
		pOutputProcessParameters->ValidFrameCount = pInputProcessParameters->ValidFrameCount;
		
		// only send out the data if the receiver is still valid.  Otherwise, just return.  
		// This is to handle the case where the game is shutting down, and has deleted the object that we used to point to, but one last processing step happens before the
		// game is finished cleaning up.
		if (pInputProcessParameters->BufferFlags == XAPO_BUFFER_VALID)
		{
			m_pDataReceiver->DataReceived((float*)pInputProcessParameters->pBuffer, m_uChannels * pInputProcessParameters->ValidFrameCount);
		}

		if (m_bSilentVoice)
		{
			pOutputProcessParameters->BufferFlags = XAPO_BUFFER_SILENT;
		}
		else
		{
			pOutputProcessParameters->BufferFlags = pInputProcessParameters->BufferFlags;
		}
	}
}