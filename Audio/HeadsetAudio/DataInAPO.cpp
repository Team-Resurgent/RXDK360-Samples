//--------------------------------------------------------------------------------------
// DataInAPO.cpp
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DataInAPO.h"

DataInAPO::DataInAPO(IAPODataSender* pDataSender)
	: CXAPOBase(&m_regProps),
	m_pDataSender(pDataSender)
{
	// validate that the IAPODataSender exists
	_ASSERT(pDataSender);
}

HRESULT DataInAPO::LockForProcess(UINT32 InputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pInputLockedParameters, 
								  UINT32 OutputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pOutputLockedParameters)
{
	return CXAPOBase::LockForProcess(InputLockedParameterCount, pInputLockedParameters, OutputLockedParameterCount, pOutputLockedParameters);
}

//
// Process handles the work of requesting data for this voice.  The method calls the 
// provided IAPODataSender implementation to request the data
//
void DataInAPO::Process(UINT32 /*InputProcessParameterCount*/, const XAPO_PROCESS_BUFFER_PARAMETERS* pInputProcessParameters, 
						UINT32 /*OutputProcessParameterCount*/, XAPO_PROCESS_BUFFER_PARAMETERS* pOutputProcessParameters, BOOL IsEnabled)
{
	if (IsEnabled)
	{
		pOutputProcessParameters->ValidFrameCount = pInputProcessParameters->ValidFrameCount;
		pOutputProcessParameters->BufferFlags = XAPO_BUFFER_SILENT;
		
		// only send out the data if the receiver is still valid.  Otherwise, just return.  
		if (m_pDataSender)
		{
			if (m_pDataSender->GetData((float*)pOutputProcessParameters->pBuffer, pInputProcessParameters->ValidFrameCount))
			{
				pOutputProcessParameters->BufferFlags = XAPO_BUFFER_VALID;
			}
		}
	}
}
