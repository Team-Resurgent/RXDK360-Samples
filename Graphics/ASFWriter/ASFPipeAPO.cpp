//--------------------------------------------------------------------------------------
// ASFPipeAPO.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "ASFPipeAPO.h"
#include <AtgAudio.h>

//--------------------------------------------------------------------------------------
// Name: CASFPipeAPO::DoProcess
// Desc: Copies the buffer to a lockless queue for later archiving.
//--------------------------------------------------------------------------------------
void CASFPipeAPO::DoProcess(   const AudioPipeParams& /* params */,
        FLOAT32* __restrict pData,
        UINT32 cFrames,
        UINT32 cChannels,
        BOOL   bIsEnabled )
{
    if( !bIsEnabled || !cFrames )
        return;

    // Ensure the buffer has been correctly sized
    assert( m_PipeBytesPerQuantum == cFrames * cChannels * sizeof(INT16) ); 

    // Note: This could be vectorized, but the performance is fine here for most uses
	INT16* pPerQuantumBuffer = m_PerQuantumBuffer;
    for( UINT32 i = 0; i < cFrames * cChannels; i += 1 )
    {
        if( _isnan( pData[i] ) || !_finite( pData[i] ) )
            *pPerQuantumBuffer++ = 0;
        else if( pData[i] > 1.0f )
            *pPerQuantumBuffer++ = 32767;
        else if( pData[i] < -1.0f )
            *pPerQuantumBuffer++ = -32768;
        else
            *pPerQuantumBuffer++ = ( INT16 )( pData[i] * 32767.0f );
    }

    if( !m_AudioPipe.Write( &m_PerQuantumBuffer, m_PipeBytesPerQuantum ) )
        ATG::DebugSpew( "Warning, PipeAPO Pipe is Full: Dropping audio packet.\n" );

}

//--------------------------------------------------------------------------------------
// Name: CASFPipeAPO::LockForProcess
// Desc: Performs type checking and allocations in preperation for processing
//--------------------------------------------------------------------------------------
HRESULT CASFPipeAPO::LockForProcess(
         UINT32 InputLockedParameterCount,
         const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pInputLockedParameters,
         UINT32 OutputLockedParameterCount,
         const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pOutputLockedParameters )
{
	// Ensure that this is an XAudio2 FLOAT pipeline.
	assert( pInputLockedParameters[0].pFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE );
    WAVEFORMATEXTENSIBLE *pWaveFormat = ( WAVEFORMATEXTENSIBLE* )pInputLockedParameters->pFormat;
	assert( pWaveFormat->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT );
    
    //Allocate the INT16 buffer that recieves
    m_PipeBytesPerQuantum = static_cast<UINT32>( pWaveFormat->Format.nChannels * 
                              pWaveFormat->Format.nSamplesPerSec * 
                              sizeof( INT16 ) * 
                              ( static_cast<float>( XAUDIO2_QUANTUM_NUMERATOR ) / static_cast<float>( XAUDIO2_QUANTUM_DENOMINATOR ) ) ); 

    assert( m_PipeBytesPerQuantum <= MAX_BUFFERSIZE_PER_XAUDIO2_QUANTUM );
	return CSampleXAPOBase::LockForProcess( InputLockedParameterCount, pInputLockedParameters, OutputLockedParameterCount,pOutputLockedParameters );
}

//--------------------------------------------------------------------------------------
// Name: CASFPipeAPO::UnlockForProcess
// Desc: Releases resources allocated during LockForProcess
//--------------------------------------------------------------------------------------
void CASFPipeAPO::UnlockForProcess()
{
	CSampleXAPOBase::UnlockForProcess();
}


