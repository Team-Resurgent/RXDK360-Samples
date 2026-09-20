#pragma once
#include <xtl.h>
#include <xaudio2.h>

#include <atgfont.h>

#include "SimpleAPO.h"
#include "Comp1APO.h"
#include <ATGSpectralDisplay.h>
#include "EqualizerAPO.h"

namespace ATG
{
    class PackedResource;
    struct GAMEPAD;
}

class IApoWidget
{
public:
    virtual void Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource ) = 0;
    virtual void Update( FLOAT flElapsed ) = 0;
    virtual void Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds ) = 0;
    virtual void HandleInput( ATG::GAMEPAD* pGamepad ) = 0;
};

class CMonitorApoWidget : public IApoWidget
{
public:
    const static DWORD nSamplesLog2 = 8;
    const static DWORD nSamples = 1 << nSamplesLog2;

    CMonitorApoWidget( ATG::MonitorAPOPipe* pInput );

    virtual void Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource );
    virtual void Update( FLOAT flElapsed );
    virtual void Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds );
    virtual void HandleInput( ATG::GAMEPAD* pGamepad );
private:
    ATG::MonitorAPOPipe*     m_pInput;
    ATG::CWaveDisplay   m_monitorDisplay;
    bool                m_monitorTimeDomain;
    IDirect3DTexture9*  m_pBackground;
    IDirect3DTexture9*  m_pLED;

};

class CSimpleApoWidget : public IApoWidget
{
public:
    CSimpleApoWidget( IXAudio2Voice* pVoice, int idxEffect );
    ~CSimpleApoWidget();

    virtual void Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource );
    virtual void Update( FLOAT flElapsed );
    virtual void Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds );
    virtual void HandleInput( ATG::GAMEPAD* pGamepad );
private:
    IXAudio2Voice*      m_pVoice;
    int                 m_idxEffect;
    IDirect3DTexture9*  m_pBackground;
    IDirect3DTexture9*  m_pSlider;
    SimpleAPOParams     m_fxParams;
};

class CComp1ApoWidget : public IApoWidget
{
public:
    CComp1ApoWidget( IXAudio2Voice* pVoice, int idxEffect );
    ~CComp1ApoWidget();

    virtual void Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource );
    virtual void Update( FLOAT flElapsed );
    virtual void Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds );
    virtual void HandleInput( ATG::GAMEPAD* pGamepad );
private:
    IXAudio2Voice*      m_pVoice;
    int                 m_idxEffect;

    ATG::CWaveDisplay  m_monitorDisplay;
    Comp1APOParams     m_fxParams;
    IDirect3DTexture9* m_pBackground;
    ATG::Font          m_font;
};

#pragma warning( push )
//Ignore the C4324 padding warning when compiling with /Analyze
#pragma warning( disable : 4324 )
class CEQApoWidget : public IApoWidget
{
public:
    CEQApoWidget( IXAudio2Voice* pVoice, int idxEffect );
    ~CEQApoWidget();

    virtual void Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource );
    virtual void Update( FLOAT flElapsed );
    virtual void Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds );
    virtual void HandleInput( ATG::GAMEPAD* pGamepad );
private:
    static const int nEQSamples = 4096;
	static const int sampleVectorCount = nEQSamples / 4;
    IXAudio2Voice*      m_pVoice;
    int                 m_idxEffect;
    ATG::CWaveDisplay   m_monitorDisplay;
    IDirect3DTexture9* m_pBackground;
    ATG::Font          m_font;
	int					m_iCurrentBand;
    EqualizerAPOParams m_fxParams;

	//These variables are used in Update, but due to size are
	//stored on the heap here rather than the stack:
	static const int nBinsLog2 = 12;
	static const int nBins = 1 << nBinsLog2;
	__vector4 imaginarySamples[nBins/4];
	__vector4 samples[sampleVectorCount];
    __vector4 m_UnityTable[nBins]; // unity table, used with FFT
};
#pragma warning( pop )

