//--------------------------------------------------------------------------------------
// MonitorAPOWidget.h
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#include <xtl.h>
#include <ATGSpectralDisplay.h>

class IApoWidget
{
public:
    virtual HRESULT Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource ) = 0;
    virtual void Update( ) = 0;
    virtual void Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds, 
        BOOL bActivated ) = 0;
    virtual void HandleInput( ATG::GAMEPAD* pGamepad ) = 0;
};

class CMonitorAPOWidget : public IApoWidget
{

public:
    const static DWORD nSamplesLog2 = 8;
    const static DWORD nSamples = 1 << nSamplesLog2;

    CMonitorAPOWidget( ATG::MonitorAPOPipe* pInput );

    HRESULT Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource );
    virtual void Update( );
    virtual void Reset( );
    virtual void Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds, BOOL bActivated );
    virtual void HandleInput( ATG::GAMEPAD* pGamepad );

private:
    ATG::MonitorAPOPipe*    m_pInput;
    ATG::CWaveDisplay       m_monitorDisplay;
    BOOL                    m_monitorTimeDomain;         
    IDirect3DTexture9*      m_pBackground;
    IDirect3DTexture9*      m_pLED;
};
