//--------------------------------------------------------------------------------------
// GlareDefD3D.cpp
//
// Glare information definition
//
// Original file provided by Masaki Kawase 
// http://www.daionet.gr.jp/~masa/rthdribl/

// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include "GlareDefD3D.h"

#define _Rad    XMConvertToRadians

//--------------------------------------------------------------------------------------
// Static star library information
//--------------------------------------------------------------------------------------
struct STARDEF
{
    CHAR* strStarName;
    DWORD dwNumStarLines;
    DWORD dwNumPasses;
    FLOAT fSampleLength;
    FLOAT fAttenuation;
    FLOAT fLongAttenuation;
    FLOAT fInclination;
    BOOL bRotation;
};

extern STARDEF s_aLibStarDef[NUM_STARLIBTYPES] =
{
    // Star name    lines  passes  length  attn  longattn inclination    bRotate
    { "Disable",      0,      0,    0.0f,  0.00f,  0.00f,  _Rad( 00.0f ),   FALSE,  }, // STLT_DISABLE
    { "Cross",        4,      3,    1.0f,  0.85f,  0.85f,  _Rad( 00.0f ),   TRUE,   }, // STLT_CROSS
    { "CrossFilter",  4,      3,    1.0f,  0.95f,  0.95f,  _Rad( 00.0f ),   TRUE,   }, // STLT_CROSS
    { "SnowCross",    6,      3,    1.0f,  0.96f,  0.96f,  _Rad( 20.0f ),   TRUE,   }, // STLT_SNOWCROSS
    { "Vertical",     2,      3,    1.0f,  0.96f,  0.96f,  _Rad( 00.0f ),   FALSE,  }, // STLT_VERTICAL
    { "SunnyCross",   8,      3,    1.0f,  0.88f,  0.95f,  _Rad( 00.0f ),   FALSE,  }, // STLT_SUNNYCROSS
};
static int  s_nLibStarDefs = sizeof( s_aLibStarDef ) / sizeof( STARDEF );


//--------------------------------------------------------------------------------------
// Static glare library information
//--------------------------------------------------------------------------------------
struct GLAREDEF
{
    CHAR* strGlareName;
    FLOAT fGlareLuminance;
    FLOAT fBloomLuminance;
    FLOAT fGhostLuminance;
    FLOAT fGhostDistortion;
    FLOAT fStarLuminance;
    ESTARLIBTYPE eStarType;
    FLOAT fStarInclination;
    FLOAT fChromaticAberration;
    FLOAT fAfterimageSensitivity; // Current weight
    FLOAT fAfterimageRatio;       // Afterimage weight
    FLOAT fAfterimageLuminance;
};

static GLAREDEF s_aLibGlareDef[NUM_GLARELIBTYPES] =
{
    //  Glare name              glare  bloom  ghost  distort  star  star type          rotate      C.A   current  after  ai lum
    { "Disable",                 0.0f,  0.0f,  0.0f,  0.01f,  0.0f, STLT_DISABLE,     _Rad( 0.0f ),  0.5f,  0.00f,
        0.00f,  0.0f,  }, // GLT_DISABLE

    { "Camera",                  1.5f,  1.2f,  1.0f,  0.00f,  1.0f, STLT_CROSS,       _Rad( 00.0f ), 0.5f,  0.25f,
        0.90f,  1.0f,  }, // GLT_CAMERA
    { "Natural Bloom",           1.5f,  1.2f,  0.0f,  0.00f,  0.0f, STLT_DISABLE,     _Rad( 00.0f ), 0.0f,  0.40f,
        0.85f,  0.5f,  }, // GLT_NATURAL
    { "Cheap Lens Camera",       1.25f, 2.0f,  1.5f,  0.05f,  2.0f, STLT_CROSS,       _Rad( 00.0f ), 0.5f,  0.18f,
        0.95f,  1.0f,  }, // GLT_CHEAPLENS
    //  { "Afterimage",              1.5f,  1.2f,  0.5f,  0.00f,  0.7f, STLT_CROSS,       _Rad(00.0f), 0.5f,  0.1f,   0.98f,  2.0f,  }, // GLT_AFTERIMAGE
    { "Cross Screen Filter",     1.0f,  2.0f,  1.7f,  0.00f,  1.5f, STLT_CROSSFILTER, _Rad( 25.0f ), 0.5f,  0.20f,
        0.93f,  1.0f,  }, // GLT_FILTER_CROSSSCREEN
    { "Spectral Cross Filter",   1.0f,  2.0f,  1.7f,  0.00f,  1.8f, STLT_CROSSFILTER, _Rad( 70.0f ), 1.5f,  0.20f,
        0.93f,  1.0f,  }, // GLT_FILTER_CROSSSCREEN_SPECTRAL
    { "Snow Cross Filter",       1.0f,  2.0f,  1.7f,  0.00f,  1.5f, STLT_SNOWCROSS,   _Rad( 10.0f ), 0.5f,  0.20f,
        0.93f,  1.0f,  }, // GLT_FILTER_SNOWCROSS
    { "Spectral Snow Cross",     1.0f,  2.0f,  1.7f,  0.00f,  1.8f, STLT_SNOWCROSS,   _Rad( 40.0f ), 1.5f,  0.20f,
        0.93f,  1.0f,  }, // GLT_FILTER_SNOWCROSS_SPECTRAL
    { "Sunny Cross Filter",      1.0f,  2.0f,  1.7f,  0.00f,  1.5f, STLT_SUNNYCROSS,  _Rad( 00.0f ), 0.5f,  0.20f,
        0.93f,  1.0f,  }, // GLT_FILTER_SUNNYCROSS
    { "Spectral Sunny Cross",    1.0f,  2.0f,  1.7f,  0.00f,  1.8f, STLT_SUNNYCROSS,  _Rad( 45.0f ), 1.5f,  0.20f,
        0.93f,  1.0f,  }, // GLT_FILTER_SUNNYCROSS_SPECTRAL
    { "Cine Camera Vert Slits",  1.0f,  2.0f,  1.5f,  0.00f,  1.0f, STLT_VERTICAL,    _Rad( 90.0f ), 0.5f,  0.20f,
        0.93f,  1.0f,  }, // GLT_CINECAM_VERTICALSLIT
    { "Cine Camera Horiz Slits", 1.0f,  2.0f,  1.5f,  0.00f,  1.0f, STLT_VERTICAL,    _Rad( 00.0f ), 0.5f,  0.20f,
        0.93f,  1.0f,  }, // GLT_CINECAM_HORIZONTALSLIT
};
static int  s_nLibGlareDefs = sizeof( s_aLibGlareDef ) / sizeof( GLAREDEF );


//--------------------------------------------------------------------------------------
// Generic simple star generation
//--------------------------------------------------------------------------------------

CStarDef::CStarDef()
{
    ZeroMemory( m_strStarName, sizeof( m_strStarName ) );
    m_dwNumStarLines = 0;
    m_fInclination = 0.0f;
    m_bRotation = FALSE;
}


HRESULT CStarDef::Initialize( const CHAR* strStarName,
                              DWORD dwNumStarLines, DWORD dwNumPasses,
                              FLOAT fSampleLength, FLOAT fAttenuation,
                              FLOAT fLongAttenuation, FLOAT fInclination,
                              BOOL bRotation )
{
    // Copy from parameters
    lstrcpyn( m_strStarName, strStarName, 255 );
    m_dwNumStarLines = dwNumStarLines;
    m_fInclination = fInclination;
    m_bRotation = bRotation;

    FLOAT fInc = XMConvertToRadians( 360.0f / ( FLOAT )m_dwNumStarLines );
    for( DWORD i = 0; i < m_dwNumStarLines; i++ )
    {
        m_pStarLine[i].dwNumPasses = dwNumPasses;
        m_pStarLine[i].fSampleLength = fSampleLength;
        m_pStarLine[i].fAttenuation = fAttenuation;
        m_pStarLine[i].fInclination = fInc * ( FLOAT )i;
    }

    m_avChromaticAberrationColor[0] = XMFLOAT4( 0.5f, 0.5f, 0.5f, 0.0f ); // w
    m_avChromaticAberrationColor[1] = XMFLOAT4( 0.8f, 0.3f, 0.3f, 0.0f );
    m_avChromaticAberrationColor[2] = XMFLOAT4( 1.0f, 0.2f, 0.2f, 0.0f ); // r
    m_avChromaticAberrationColor[3] = XMFLOAT4( 0.5f, 0.2f, 0.6f, 0.0f );
    m_avChromaticAberrationColor[4] = XMFLOAT4( 0.2f, 0.2f, 1.0f, 0.0f ); // b
    m_avChromaticAberrationColor[5] = XMFLOAT4( 0.2f, 0.3f, 0.7f, 0.0f );
    m_avChromaticAberrationColor[6] = XMFLOAT4( 0.2f, 0.6f, 0.2f, 0.0f ); // g
    m_avChromaticAberrationColor[7] = XMFLOAT4( 0.3f, 0.5f, 0.3f, 0.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Glare definition
//--------------------------------------------------------------------------------------

CGlareDef::CGlareDef()
{
    ZeroMemory( m_strGlareName, sizeof( m_strGlareName ) );

    m_fGlareLuminance = 0.0f;
    m_fBloomLuminance = 0.0f;
    m_fGhostLuminance = 0.0f;
    m_fStarLuminance = 0.0f;
    m_fStarInclination = 0.0f;
    m_fChromaticAberration = 0.0f;

    m_fAfterimageSensitivity = 0.0f;
    m_fAfterimageRatio = 0.0f;
    m_fAfterimageLuminance = 0.0f;
}


HRESULT CGlareDef::Initialize( EGLARELIBTYPE eGlareType )
{
    // Create parameters
    lstrcpyn( m_strGlareName, s_aLibGlareDef[eGlareType].strGlareName, 255 );
    m_fGlareLuminance = s_aLibGlareDef[eGlareType].fGlareLuminance;
    m_fBloomLuminance = s_aLibGlareDef[eGlareType].fBloomLuminance;
    m_fGhostLuminance = s_aLibGlareDef[eGlareType].fGhostLuminance;
    m_fGhostDistortion = s_aLibGlareDef[eGlareType].fGhostDistortion;
    m_fStarLuminance = s_aLibGlareDef[eGlareType].fStarLuminance;
    m_fStarInclination = s_aLibGlareDef[eGlareType].fStarInclination;
    m_fChromaticAberration = s_aLibGlareDef[eGlareType].fChromaticAberration;

    m_fAfterimageSensitivity = s_aLibGlareDef[eGlareType].fAfterimageSensitivity;
    m_fAfterimageRatio = s_aLibGlareDef[eGlareType].fAfterimageRatio;
    m_fAfterimageLuminance = s_aLibGlareDef[eGlareType].fAfterimageLuminance;

    // Create star form data
    int eStarType = s_aLibGlareDef[eGlareType].eStarType;

    m_starDef.Initialize( s_aLibStarDef[eStarType].strStarName,
                          s_aLibStarDef[eStarType].dwNumStarLines,
                          s_aLibStarDef[eStarType].dwNumPasses,
                          s_aLibStarDef[eStarType].fSampleLength,
                          s_aLibStarDef[eStarType].fAttenuation,
                          s_aLibStarDef[eStarType].fLongAttenuation,
                          s_aLibStarDef[eStarType].fInclination,
                          s_aLibStarDef[eStarType].bRotation );

    return S_OK;
}


