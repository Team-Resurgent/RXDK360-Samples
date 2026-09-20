//--------------------------------------------------------------------------------------
// CHighLatencySample.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "CLatencySample.h"

class CHighLatencySample : public CLatencySample
{
public:
    CHighLatencySample() { m_SampleType = SAMPLE_TYPE_HIGH_LATENCY; }

    virtual HRESULT Initialize();
    virtual VOID Render( const UINT uFrameBufferIdx );

protected:
    virtual VOID BeginRenderPass( const ERenderPass RenderPass, const UINT uFrameBufferIdx );
    virtual VOID EndRenderPass( const ERenderPass RenderPass );

    virtual VOID FilterJointPositions( const UINT uFrameBufferIdx );
};


