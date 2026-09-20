//--------------------------------------------------------------------------------------
// Wordwrap.h
//
// Description of what the class does here
//
// Developed by Microsoft Game Studios Tools and Technology Group
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef WORDWRAP_H
#define WORDWRAP_H

#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "WordwrapUtil.h"

//--------------------------------------------------------------------------------------
// Possible String Types
//--------------------------------------------------------------------------------------
enum StringType
{
    StringType_Alphanumeric,
    StringType_Japanese,
    StringType_Korean,
    StringType_TraditionalChinese,
    StringType_SimplifiedChinese
};

extern const WCHAR g_wszTestString[];       // alphameric sample string
extern const WCHAR g_wszTestStringJPN[];    // Japanese sample string
extern const WCHAR g_wszTestStringKOR[];    // Korean sample string
extern const WCHAR g_wszTestStringCHT[];    // Traditional Chinese sample string
extern const WCHAR g_wszTestStringSimChin[]; // Simplified Chinese sample string
extern StringType   g_CurrentString;        // current string

INT MyGetCharWidthW( WCHAR c );

//--------------------------------------------------------------------------------------
// Name: Sample
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    LPDIRECT3DVERTEXDECLARATION9 m_pVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader; // Custom vertex shader
    LPDIRECT3DPIXELSHADER9 m_pPixelShader;  // Custom pixel shader
    LPDIRECT3DVERTEXBUFFER9 m_pVB;

    FLOAT   m_fPosScale[2];
    BOOL m_bEAString;        // TRUE to draw EA string
    RECT m_Rc;
    INT m_iOption;          // wordwrap option flag;

    void    Reset()
    {
        m_Rc.left = ( 640 / 10 ) + 100;
        m_Rc.top = ( 480 / 20 ) + 100;
        m_Rc.right = ( 640 - ( 640 / 10 ) ) - 20;
        m_Rc.bottom = ( 480 - ( 480 / 20 ) ) - 20;

        g_CurrentString = StringType_Alphanumeric;
        m_iOption = WW_PROHIBITION;
    }

    void    DrawOneChar( FLOAT fX, FLOAT fY, DWORD dwColor, WCHAR wch )
    {
        WCHAR wszOneCharStr[2] = { wch, '\0' };
        return m_Font.DrawText( fX, fY, 0xffffffff, wszOneCharStr );
    }

    void    RenderRect( DWORD dwColor=0x80202040 );
    void    MySimpleTextOut( FLOAT fXPos, FLOAT fYPos, FLOAT fWidth, FLOAT fHeight, const WCHAR* wszInput );

private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

};

#endif // WORDWRAP_H
