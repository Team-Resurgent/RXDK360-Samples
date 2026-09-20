//--------------------------------------------------------------------------------------
// GlareDefD3D.h
//
// Define glare information.
//
// Original file provided by Masaki Kawase 
// http://www.daionet.gr.jp/~masa/rthdribl/
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#ifndef GLAREDEFD3D_H
#define GLAREDEFD3D_H


//--------------------------------------------------------------------------------------
// Star form library
//--------------------------------------------------------------------------------------
enum ESTARLIBTYPE
{
    STLT_DISABLE = 0,

    STLT_CROSS,
    STLT_CROSSFILTER,
    STLT_SNOWCROSS,
    STLT_VERTICAL,
    STLT_SUNNYCROSS,

    NUM_STARLIBTYPES,
};


//--------------------------------------------------------------------------------------
// Name: class CStarDef
// Desc: Star generation object
//--------------------------------------------------------------------------------------
class CStarDef
{
    static const DWORD MAX_STARLINES = 8;

public:
    // Define each line of the star.
    struct STARLINE
    {
        DWORD dwNumPasses;
        FLOAT fSampleLength;
        FLOAT fAttenuation;
        FLOAT fInclination;
    };

    CHAR        m_strStarName[256];
    DWORD m_dwNumStarLines;
    STARLINE    m_pStarLine[MAX_STARLINES];
    FLOAT m_fInclination;
    BOOL m_bRotation;    // Rotation is available from outside ?
    XMFLOAT4    m_avChromaticAberrationColor[8];

public:
    HRESULT     Initialize( const CHAR* strStarName, DWORD dwNumStarLines, DWORD dwNumPasses,
                            FLOAT fSampleLength, FLOAT fAttenuation, FLOAT fLongAttenuation,
                            FLOAT fInclination, BOOL bRotation );

                CStarDef();
};


//--------------------------------------------------------------------------------------
// Glare form library
//--------------------------------------------------------------------------------------
enum EGLARELIBTYPE
{
    GLT_DISABLE = 0,

    GLT_CAMERA,
    GLT_NATURAL,
    GLT_CHEAPLENS,
    //  GLT_AFTERIMAGE,
    GLT_FILTER_CROSSSCREEN,
    GLT_FILTER_CROSSSCREEN_SPECTRAL,
    GLT_FILTER_SNOWCROSS,
    GLT_FILTER_SNOWCROSS_SPECTRAL,
    GLT_FILTER_SUNNYCROSS,
    GLT_FILTER_SUNNYCROSS_SPECTRAL,
    GLT_CINECAM_VERTICALSLITS,
    GLT_CINECAM_HORIZONTALSLITS,

    NUM_GLARELIBTYPES,
};


//--------------------------------------------------------------------------------------
// Name: class CGlareDef
// Desc: Glare definition
//--------------------------------------------------------------------------------------
class CGlareDef
{
public:
    CHAR    m_strGlareName[256];

    FLOAT m_fGlareLuminance;        // Total glare intensity
    FLOAT m_fBloomLuminance;
    FLOAT m_fGhostLuminance;
    FLOAT m_fGhostDistortion;
    FLOAT m_fStarLuminance;
    FLOAT m_fStarInclination;

    FLOAT m_fChromaticAberration;

    FLOAT m_fAfterimageSensitivity; // Current weight
    FLOAT m_fAfterimageRatio;       // Afterimage weight
    FLOAT m_fAfterimageLuminance;

    CStarDef m_starDef;

public:
    HRESULT Initialize( EGLARELIBTYPE eGlareType );

            CGlareDef();
};


#endif  // GLAREDEFD3D_H
