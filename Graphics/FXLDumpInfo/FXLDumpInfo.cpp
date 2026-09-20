//--------------------------------------------------------------------------------------
// FXLDumpInfo.cpp
//
// Sample to demonstrate walking through an FXLite Effect to obtain information
// about the effect.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <fxl.h>

#include "AtgConsole.h"
#include "AtgInput.h"
#include "AtgUtil.h"

#pragma warning(disable:4127)   // we use some infinite loops, disable "conditional expression constant" warning


ATG::Console    g_console;                // console for output
FXLEffect*      g_pFXLSkybox;            // FXLite Effect


//--------------------------------------------------------------------------------------
// Declarations
//--------------------------------------------------------------------------------------
BOOL Initialize( VOID );
VOID Shutdown( VOID );
VOID
CDECL OutputString( const CHAR* strFormat, ... );


//--------------------------------------------------------------------------------------
// Name: FXLDumpInfo
// Desc: This class contains static members that walk through an FXLite Effect,
//       dumping descriptions to the OutputString function
//--------------------------------------------------------------------------------------
class FXLDumpInfo
{
public:
    static const char* RenderStateToChar( D3DRENDERSTATETYPE renderState );
    static const char* SamplerStateToChar( D3DSAMPLERSTATETYPE samplerState );
    static const char* ClassToChar( FXLDATA_CLASS fxlclass );
    static const char* TypeToChar( FXLDATA_TYPE fxltype );
    static void PrintTabs( UINT nTabs );

    static void WalkParam( FXLEffect* pEffect, FXLHANDLE hParam, UINT nTabLevel );
    static void WalkEffect( FXLEffect* pEffect );
};


//--------------------------------------------------------------------------------------
// Main
//
// Main game loop;
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Initialize the application, and load the FXLite effect
    if( !Initialize() )
        return;


    HRESULT hr = S_OK;

    FXLHANDLE h;
    D3DXMATRIX mat;

    // Set some parameter values in the effect to demostrate retrieving them later

    // If the variable defined a semantic name, and the effect was compiled without
    // /XfxPv - the parameter is looked up by it's semantic name.
    h = g_pFXLSkybox->GetParameterHandle( "PROJECTION" );

    // Otherwise, if the effect was compiled with /XfxPv (or a semantic was not
    // defined) - look up the parameter via the variable name.
    if( !h )
        h = g_pFXLSkybox->GetParameterHandle( "matProj" );

    // Set the projection matrix to a reasonable value
    if( h )
    {
        D3DXMatrixPerspectiveFovLH( &mat, D3DX_PI / 4.f, 640.f / 480.f, 1.f, 1000.f );
        g_pFXLSkybox->SetMatrix( h, ( FXLMATRIX* )&mat );
    }

    // Try retrieving view matrix via semantic name...
    h = g_pFXLSkybox->GetParameterHandle( "WORLD" );

    // ...or by variable name..
    if( !h )
        h = g_pFXLSkybox->GetParameterHandle( "matView" );

    // ...and set the value to a reasonable view matrix value.
    if( h )
    {
        D3DXMatrixIdentity( &mat );
        g_pFXLSkybox->SetMatrix( h, ( FXLMATRIX* )&mat );
    }

    // Demonstrate walking the effect descriptions
    FXLDumpInfo::WalkEffect( g_pFXLSkybox );

    g_console.Format( L"\nUse LEFT STICK to scroll up/down.\n" );
    g_console.Format( L"\nPress LT + RT + RB to exit.\n" );

    do
    {
        g_console.Render();

        // Detect reboot keypress and scroll input
        ATG::GAMEPAD* pGamepad;
        bool bInputAccepted;
        pGamepad = ATG::Input::GetMergedInput();

        bInputAccepted = false;

        // Analog stick scroll
        if( fabs( pGamepad->fY1 ) > 0.25f )
        {
            g_console.ScrollUp( ( INT )( pGamepad->fY1 * 4.f ) );
            bInputAccepted = true;
        }

        // Trigger scrolls
        if( pGamepad->bPressedRightTrigger )
        {
            g_console.ScrollUp( ATG::Console::PAGE_DOWN );
            bInputAccepted = true;
        }

        if( pGamepad->bPressedLeftTrigger )
        {
            g_console.ScrollUp( ATG::Console::PAGE_UP );
            bInputAccepted = true;
        }

        // Delay the next frame if we processed any input...
        if( bInputAccepted )
        {
            Sleep( 50 );
        }

    } while( SUCCEEDED( hr ) );

    // Release our resources
    Shutdown();

    // Reboot when done
    XLaunchNewImage( "", 0 );
}


//--------------------------------------------------------------------------------------
// Initialize
//
// Set up SNL and socket
//--------------------------------------------------------------------------------------
BOOL Initialize( VOID )
{
    // Initialize the console
    HRESULT hr = g_console.Create( "game:\\Media\\Fonts\\Arial_12.xpr", 0xff0000ff, 0xffffffff, 256 );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Console initialization failed.\n" );
    }

    g_console.Format( "*** INITIALIZING ***\n" );

    // Create the Skybox effect
    // The effect is precompiled on the PC-side into a binary file.
    // The binary file is then loaded, and instantiated via FXLCreateEffect
    VOID* pCode;
    DWORD dwSize;
    hr = ATG::LoadFile( "game:\\Media\\Effects\\skybox.fxobj", &pCode, &dwSize );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not load FXL Effect (skybox)\n" );
        return FALSE;
    }

    hr = FXLCreateEffect( NULL, pCode, NULL, &g_pFXLSkybox );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create FXL Effect (skybox)\n" );
        return FALSE;
    }

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Shutdown
//
// Tear everything down
//--------------------------------------------------------------------------------------
VOID Shutdown( VOID )
{
    g_console.Format( "*** SHUTTING DOWN ***\n" );

    return;
}


//--------------------------------------------------------------------------------------
// Name: OutputStringV()
// Desc: Internal helper function
//--------------------------------------------------------------------------------------
static VOID OutputStringV( const CHAR* strFormat, const va_list pArgList )
{
    CHAR str[2048];
    // Use the secure CRT to avoid buffer overruns. Specify a count of
    // _TRUNCATE so that too long strings will be silently truncated
    // rather than triggering an error.
    _vsnprintf_s( str, _TRUNCATE, strFormat, pArgList );

    // Output to debug spew
    ATG::DebugSpew( str );

    // Output to the console screen
    g_console.Format( str );
}


//--------------------------------------------------------------------------------------
// Name: OutputString()
// Desc: Prints formatted string
//--------------------------------------------------------------------------------------
VOID
CDECL OutputString( const CHAR* strFormat, ... )
{
    va_list pArgList;
    va_start( pArgList, strFormat );
    OutputStringV( strFormat, pArgList );
    va_end( pArgList );
}

//--------------------------------------------------------------------------------------
// Name: PrintTabs
// Desc:
//--------------------------------------------------------------------------------------
void FXLDumpInfo::PrintTabs( UINT nTabs )
{
    while( nTabs-- )
    {
        OutputString( "    " );
    }

}


//--------------------------------------------------------------------------------------
// Name: ClassToChar
// Desc: Convert an FXL data class to a string description
//--------------------------------------------------------------------------------------
const char* FXLDumpInfo::ClassToChar( FXLDATA_CLASS fxlclass )
{
    switch( fxlclass )
    {
    case FXLDCLASS_SCALAR : return "SCALAR";
    case FXLDCLASS_VECTOR : return "VECTOR";
    case FXLDCLASS_RMATRIX : return "RMATRIX";
    case FXLDCLASS_CMATRIX : return "CMATRIX";
    case FXLDCLASS_SAMPLER : return "SAMPLER";
    case FXLDCLASS_SAMPLER1D : return "SAMPLER1D";
    case FXLDCLASS_SAMPLER2D : return "SAMPLER2D";
    case FXLDCLASS_SAMPLER3D : return "SAMPLER3D";
    case FXLDCLASS_SAMPLERCUBE : return "SAMPLERCUBE";
    case FXLDCLASS_CONTAINER : return "CONTAINER";
    }
    return "unknown";
}


//--------------------------------------------------------------------------------------
// Name: TypeToChar
// Desc: Convert an FXL Type to a string description
//--------------------------------------------------------------------------------------
const char* FXLDumpInfo::TypeToChar( FXLDATA_TYPE fxltype )
{
    switch( fxltype )
    {
    case FXLDTYPE_FLOAT     : return "FLOAT";
    case FXLDTYPE_INT       : return "INT";
    case FXLDTYPE_BOOL      : return "BOOL";
    case FXLDTYPE_SAMPLER   : return "SAMPLER";
    case FXLDTYPE_STRING    : return "STRING";
    case FXLDTYPE_CONTAINER : return "CONTAINER";
    }
    return "unknown";
}


//--------------------------------------------------------------------------------------
// Name: RenderStateToChar
// Desc: Convert a renderstate type to a string description
//--------------------------------------------------------------------------------------
const char* FXLDumpInfo::RenderStateToChar( D3DRENDERSTATETYPE renderState )
{
    switch( renderState )
    {
    case D3DRS_ZENABLE                    : return "D3DRS_ZENABLE";
    case D3DRS_ZFUNC                      : return "D3DRS_ZFUNC";
    case D3DRS_ZWRITEENABLE               : return "D3DRS_ZWRITEENABLE";
    case D3DRS_FILLMODE                   : return "D3DRS_FILLMODE";
    case D3DRS_CULLMODE                   : return "D3DRS_CULLMODE";
    case D3DRS_ALPHABLENDENABLE           : return "D3DRS_ALPHABLENDENABLE";
    case D3DRS_SEPARATEALPHABLENDENABLE   : return "D3DRS_SEPARATEALPHABLENDENABLE";
    case D3DRS_BLENDFACTOR                : return "D3DRS_BLENDFACTOR";
    case D3DRS_SRCBLEND                   : return "D3DRS_SRCBLEND";
    case D3DRS_DESTBLEND                  : return "D3DRS_DESTBLEND";
    case D3DRS_BLENDOP                    : return "D3DRS_BLENDOP";
    case D3DRS_SRCBLENDALPHA              : return "D3DRS_SRCBLENDALPHA";
    case D3DRS_DESTBLENDALPHA             : return "D3DRS_DESTBLENDALPHA";
    case D3DRS_BLENDOPALPHA               : return "D3DRS_BLENDOPALPHA";
    case D3DRS_ALPHATESTENABLE            : return "D3DRS_ALPHATESTENABLE";
    case D3DRS_ALPHAREF                   : return "D3DRS_ALPHAREF";
    case D3DRS_ALPHAFUNC                  : return "D3DRS_ALPHAFUNC";
    case D3DRS_STENCILENABLE              : return "D3DRS_STENCILENABLE";
    case D3DRS_TWOSIDEDSTENCILMODE        : return "D3DRS_TWOSIDEDSTENCILMODE";
    case D3DRS_STENCILFAIL                : return "D3DRS_STENCILFAIL";
    case D3DRS_STENCILZFAIL               : return "D3DRS_STENCILZFAIL";
    case D3DRS_STENCILPASS                : return "D3DRS_STENCILPASS";
    case D3DRS_STENCILFUNC                : return "D3DRS_STENCILFUNC";
    case D3DRS_STENCILREF                 : return "D3DRS_STENCILREF";
    case D3DRS_STENCILMASK                : return "D3DRS_STENCILMASK";
    case D3DRS_STENCILWRITEMASK           : return "D3DRS_STENCILWRITEMASK";
    case D3DRS_CCW_STENCILFAIL            : return "D3DRS_CCW_STENCILFAIL";
    case D3DRS_CCW_STENCILZFAIL           : return "D3DRS_CCW_STENCILZFAIL";
    case D3DRS_CCW_STENCILPASS            : return "D3DRS_CCW_STENCILPASS";
    case D3DRS_CCW_STENCILFUNC            : return "D3DRS_CCW_STENCILFUNC";
    case D3DRS_CCW_STENCILREF             : return "D3DRS_CCW_STENCILREF";
    case D3DRS_CCW_STENCILMASK            : return "D3DRS_CCW_STENCILMASK";
    case D3DRS_CCW_STENCILWRITEMASK       : return "D3DRS_CCW_STENCILWRITEMASK";
    case D3DRS_CLIPPLANEENABLE            : return "D3DRS_CLIPPLANEENABLE";
    case D3DRS_POINTSIZE                  : return "D3DRS_POINTSIZE";
    case D3DRS_POINTSIZE_MIN              : return "D3DRS_POINTSIZE_MIN";
    case D3DRS_POINTSPRITEENABLE          : return "D3DRS_POINTSPRITEENABLE";
    case D3DRS_POINTSIZE_MAX              : return "D3DRS_POINTSIZE_MAX";
    case D3DRS_MULTISAMPLEANTIALIAS       : return "D3DRS_MULTISAMPLEANTIALIAS";
    case D3DRS_MULTISAMPLEMASK            : return "D3DRS_MULTISAMPLEMASK";
    case D3DRS_SCISSORTESTENABLE          : return "D3DRS_SCISSORTESTENABLE";
    case D3DRS_SLOPESCALEDEPTHBIAS        : return "D3DRS_SLOPESCALEDEPTHBIAS";
    case D3DRS_DEPTHBIAS                  : return "D3DRS_DEPTHBIAS";
    case D3DRS_COLORWRITEENABLE           : return "D3DRS_COLORWRITEENABLE";
    case D3DRS_COLORWRITEENABLE1          : return "D3DRS_COLORWRITEENABLE1";
    case D3DRS_COLORWRITEENABLE2          : return "D3DRS_COLORWRITEENABLE2";
    case D3DRS_COLORWRITEENABLE3          : return "D3DRS_COLORWRITEENABLE3";
    case D3DRS_TESSELLATIONMODE           : return "D3DRS_TESSELLATIONMODE";
    case D3DRS_MINTESSELLATIONLEVEL       : return "D3DRS_MINTESSELLATIONLEVEL";
    case D3DRS_MAXTESSELLATIONLEVEL       : return "D3DRS_MAXTESSELLATIONLEVEL";
    case D3DRS_WRAP0                      : return "D3DRS_WRAP0";
    case D3DRS_WRAP1                      : return "D3DRS_WRAP1";
    case D3DRS_WRAP2                      : return "D3DRS_WRAP2";
    case D3DRS_WRAP3                      : return "D3DRS_WRAP3";
    case D3DRS_WRAP4                      : return "D3DRS_WRAP4";
    case D3DRS_WRAP5                      : return "D3DRS_WRAP5";
    case D3DRS_WRAP6                      : return "D3DRS_WRAP6";
    case D3DRS_WRAP7                      : return "D3DRS_WRAP7";
    case D3DRS_WRAP8                      : return "D3DRS_WRAP8";
    case D3DRS_WRAP9                      : return "D3DRS_WRAP9";
    case D3DRS_WRAP10                     : return "D3DRS_WRAP10";
    case D3DRS_WRAP11                     : return "D3DRS_WRAP11";
    case D3DRS_WRAP12                     : return "D3DRS_WRAP12";
    case D3DRS_WRAP13                     : return "D3DRS_WRAP13";
    case D3DRS_WRAP14                     : return "D3DRS_WRAP14";
    case D3DRS_WRAP15                     : return "D3DRS_WRAP15";
    case D3DRS_VIEWPORTENABLE             : return "D3DRS_VIEWPORTENABLE";
    case D3DRS_HIGHPRECISIONBLENDENABLE   : return "D3DRS_HIGHPRECISIONBLENDENABLE";
    case D3DRS_HIGHPRECISIONBLENDENABLE1  : return "D3DRS_HIGHPRECISIONBLENDENABLE1";
    case D3DRS_HIGHPRECISIONBLENDENABLE2  : return "D3DRS_HIGHPRECISIONBLENDENABLE2";
    case D3DRS_HIGHPRECISIONBLENDENABLE3  : return "D3DRS_HIGHPRECISIONBLENDENABLE3";
    case D3DRS_HALFPIXELOFFSET            : return "D3DRS_HALFPIXELOFFSET";
    case D3DRS_PRIMITIVERESETENABLE       : return "D3DRS_PRIMITIVERESETENABLE";
    case D3DRS_PRIMITIVERESETINDEX        : return "D3DRS_PRIMITIVERESETINDEX";
    case D3DRS_ALPHATOMASKENABLE          : return "D3DRS_ALPHATOMASKENABLE";
    case D3DRS_ALPHATOMASKOFFSETS         : return "D3DRS_ALPHATOMASKOFFSETS";
    case D3DRS_GUARDBAND_X                : return "D3DRS_GUARDBAND_X";
    case D3DRS_GUARDBAND_Y                : return "D3DRS_GUARDBAND_Y";
    case D3DRS_DISCARDBAND_X              : return "D3DRS_DISCARDBAND_X";
    case D3DRS_DISCARDBAND_Y              : return "D3DRS_DISCARDBAND_Y";
    case D3DRS_HISTENCILENABLE            : return "D3DRS_HISTENCILENABLE";
    case D3DRS_HISTENCILWRITEENABLE       : return "D3DRS_HISTENCILWRITEENABLE";
    case D3DRS_HISTENCILFUNC              : return "D3DRS_HISTENCILFUNC";
    case D3DRS_HISTENCILREF               : return "D3DRS_HISTENCILREF";
    case D3DRS_PRESENTINTERVAL            : return "D3DRS_PRESENTINTERVAL";
    case D3DRS_PRESENTIMMEDIATETHRESHOLD  : return "D3DRS_PRESENTIMMEDIATETHRESHOLD";
    case D3DRS_HIZENABLE                  : return "D3DRS_HIZENABLE";
    }
    return "unknown";
}


//--------------------------------------------------------------------------------------
// Name: SamplerStateToChar
// Desc: Convert a sampler state type to a string description
//--------------------------------------------------------------------------------------
const char* FXLDumpInfo::SamplerStateToChar( D3DSAMPLERSTATETYPE samplerState )
{
    switch( samplerState )
    {
    case D3DSAMP_ADDRESSU:              return "D3DSAMP_ADDRESSU";
    case D3DSAMP_ADDRESSV:              return "D3DSAMP_ADDRESSV";
    case D3DSAMP_ADDRESSW:              return "D3DSAMP_ADDRESSW";
    case D3DSAMP_BORDERCOLOR:           return "D3DSAMP_BORDERCOLOR";
    case D3DSAMP_MINFILTER:             return "D3DSAMP_MINFILTER";
    case D3DSAMP_MAGFILTER:             return "D3DSAMP_MAGFILTER";
    case D3DSAMP_MIPFILTER:             return "D3DSAMP_MIPFILTER";
    case D3DSAMP_MIPMAPLODBIAS:         return "D3DSAMP_MIPMAPLODBIAS";
    case D3DSAMP_MAXMIPLEVEL:           return "D3DSAMP_MAXMIPLEVEL";
    case D3DSAMP_MAXANISOTROPY:         return "D3DSAMP_MAXANISOTROPY";
    case D3DSAMP_MAGFILTERZ:            return "D3DSAMP_MAGFILTERZ";
    case D3DSAMP_MINFILTERZ:            return "D3DSAMP_MINFILTERZ";
    case D3DSAMP_SEPARATEZFILTERENABLE: return "D3DSAMP_SEPARATEZFILTERENABLE";
    case D3DSAMP_MINMIPLEVEL:           return "D3DSAMP_MINMIPLEVEL";
    case D3DSAMP_TRILINEARTHRESHOLD:    return "D3DSAMP_TRILINEARTHRESHOLD";
    case D3DSAMP_ANISOTROPYBIAS:        return "D3DSAMP_ANISOTROPYBIAS";
    case D3DSAMP_HGRADIENTEXPBIAS:      return "D3DSAMP_HGRADIENTEXPBIAS";
    case D3DSAMP_VGRADIENTEXPBIAS:      return "D3DSAMP_VGRADIENTEXPBIAS";
    case D3DSAMP_WHITEBORDERCOLORW:     return "D3DSAMP_WHITEBORDERCOLORW";
    case D3DSAMP_POINTBORDERENABLE:     return "D3DSAMP_POINTBORDERENABLE";
    }
    return "unknown";
}


//--------------------------------------------------------------------------------------
// Name: WalkParam
// Desc: Walk a parameter description
//--------------------------------------------------------------------------------------
void FXLDumpInfo::WalkParam( FXLEffect* pEffect, FXLHANDLE hParam, UINT nTabLevel )
{
    // Obtain the parameter description
    FXLPARAMETER_DESC paramDesc;
    pEffect->GetParameterDesc( hParam, &paramDesc );

    // Output the name, and description of the parameter
    PrintTabs( ++nTabLevel );
    OutputString( "Param: %s", paramDesc.pName );

    if( ( FXLDTYPE_CONTAINER != paramDesc.Type ) && ( FXLDTYPE_STRING != paramDesc.Type ) )
    {
        OutputString( "  %s", TypeToChar( paramDesc.Type ) );
    }

    OutputString( "  %s", ClassToChar( paramDesc.Class ) );

    if( paramDesc.Rows != 1 || paramDesc.Columns != 1 )
    {
        OutputString( "  (%dx%d)", paramDesc.Rows, paramDesc.Columns );
    }
    OutputString( "\n" );

    // Enumerate the annotations, for string values - output them
    for( UINT i = 0; i < paramDesc.Annotations; i++ )
    {
        FXLHANDLE hAnnotation = pEffect->GetAnnotationHandleFromIndex( hParam, i );

        FXLANNOTATION_DESC desc;

        pEffect->GetAnnotationDesc( hAnnotation, &desc );

        PrintTabs( nTabLevel );
        OutputString( "    Annotation: %s\n", desc.pName );
        if( FXLDTYPE_STRING == desc.Type && desc.Size < MAX_PATH )
        {
            char szAnnotation[MAX_PATH];
            pEffect->GetAnnotation( hAnnotation, szAnnotation );

            PrintTabs( nTabLevel );
            OutputString( "        Value: %s\n", szAnnotation );
        }
    }

    nTabLevel++;

    // float scalar - output he param value
    if( ( FXLDCLASS_SCALAR == paramDesc.Class ) && ( FXLDTYPE_FLOAT == paramDesc.Type ) )
    {
        FLOAT f;
        pEffect->GetScalarF( hParam, &f );

        PrintTabs( nTabLevel );
        OutputString( "Value: %g\n", f );
    }

    // float vector - output he param value as 4 values
    if( ( FXLDCLASS_VECTOR == paramDesc.Class ) && ( FXLDTYPE_FLOAT == paramDesc.Type ) )
    {
        FLOAT f[4];
        pEffect->GetVectorF( hParam, f );

        PrintTabs( nTabLevel );
        OutputString( "Value: ( %g, %g, %g, %g )\n", f[0], f[1], f[2], f[3] );
    }

    // float matrix - output he param value as a 4x4 matrix
    if( ( FXLDCLASS_CMATRIX == paramDesc.Class ) && ( FXLDTYPE_FLOAT == paramDesc.Type ) )
    {
        FLOAT f[16];
        pEffect->GetMatrixF4x4( hParam, f );

        PrintTabs( nTabLevel );
        OutputString( "Value:\n" );
        PrintTabs( nTabLevel );
        OutputString( "    ( %g, %g, %g, %g )\n", f[0], f[1], f[2], f[3] );
        PrintTabs( nTabLevel );
        OutputString( "    ( %g, %g, %g, %g )\n", f[4], f[5], f[6], f[7] );
        PrintTabs( nTabLevel );
        OutputString( "    ( %g, %g, %g, %g )\n", f[8], f[9], f[10], f[11] );
        PrintTabs( nTabLevel );
        OutputString( "    ( %g, %g, %g, %g )\n", f[12], f[13], f[14], f[15] );
    }

    // For arrays and struct's, recurse in to enumerate the values
    if( FXLDCLASS_CONTAINER == paramDesc.Class )
    {
        PrintTabs( nTabLevel );

        if( FXLPACONTENT_STRUCT == paramDesc.Content )
        {
            OutputString( "struct members:\n" );

            for( UINT i = 0; i < paramDesc.Elements; ++i )
            {
                FXLHANDLE hMember = pEffect->GetMemberHandleFromIndex( hParam, i );
                WalkParam( pEffect, hMember, nTabLevel );
            }
        }
        else
        {
            OutputString( "array elements:\n" );

            for( UINT i = 0; i < paramDesc.Elements; ++i )
            {
                FXLHANDLE hMember = pEffect->GetElementHandle( hParam, i );
                WalkParam( pEffect, hMember, nTabLevel );
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: WalkEffect
// Desc: Walk an entire effect
//--------------------------------------------------------------------------------------
void FXLDumpInfo::WalkEffect( FXLEffect* pEffect )
{
    UINT nTabLevel = 0;
    FXLEFFECT_DESC effectDesc;

    // Retrieve the effect description
    pEffect->GetEffectDesc( &effectDesc );

    OutputString( "Walking Effect...\nEffect Creator: %s\n", effectDesc.pCreator );

    // Walk through each technique
    nTabLevel++;
    for( UINT i = 0; i < effectDesc.Techniques; ++i )
    {
        FXLHANDLE hTechnique = pEffect->GetTechniqueHandleFromIndex( i );

        FXLTECHNIQUE_DESC techniqueDesc;
        pEffect->GetTechniqueDesc( hTechnique, &techniqueDesc );

        PrintTabs( nTabLevel );
        OutputString( "Technique Name: %s\n", techniqueDesc.pName );

        // Walk through each pass
        nTabLevel++;
        for( UINT j = 0; j < techniqueDesc.Passes; ++j )
        {
            FXLHANDLE hPass = pEffect->GetPassHandleFromIndex( hTechnique, j );

            FXLPASS_DESC passDesc;
            pEffect->GetPassDesc( hPass, &passDesc );

            PrintTabs( nTabLevel );
            OutputString( "Pass Name: %s\n", passDesc.pName );

            // Disassemble the vertex shader
            LPD3DXBUFFER pBuffer;
            if( passDesc.pVertexShaderFunction )
            {
                D3DXDisassembleShaderEx( passDesc.pVertexShaderFunction,
                                         D3DXDISASSEMBLER_SHOW_TIMING_ESTIMATE,
                                         NULL,
                                         &pBuffer );
                if( pBuffer )
                {
                    OutputString( ( char* )pBuffer->GetBufferPointer() );
                    pBuffer->Release();
                    pBuffer = NULL;
                    OutputString( "\n" );
                }
            }
            // Disassemble the pixel shader
            if( passDesc.pPixelShaderFunction )
            {
                D3DXDisassembleShaderEx( passDesc.pPixelShaderFunction,
                                         D3DXDISASSEMBLER_SHOW_TIMING_ESTIMATE,
                                         NULL,
                                         &pBuffer );
                if( pBuffer )
                {
                    OutputString( "Pixel Shader:\n" );
                    OutputString( ( char* )pBuffer->GetBufferPointer() );
                    OutputString( "\n" );
                    pBuffer->Release();
                    pBuffer = NULL;
                }
            }

            // Enumerate each of the renderstates set by the pass
            nTabLevel++;
            for( UINT k = 0; k < passDesc.RenderStates; ++k )
            {
                D3DRENDERSTATETYPE rs;
                DWORD dwValue;

                pEffect->GetRenderState( hPass, k, &rs, &dwValue );

                PrintTabs( nTabLevel );
                OutputString( "Renderstate: %s\n",
                              RenderStateToChar( rs ) );

                PrintTabs( nTabLevel );
                OutputString( "    Value:%d\n", dwValue );

            }

            // Enumerate each of the sampler states set by the pass
            for( UINT k = 0; k < passDesc.SamplerStates; ++k )
            {
                D3DSAMPLERSTATETYPE ss;
                DWORD dwValue;
                UINT nSampler;

                pEffect->GetSamplerState( hPass, k, &nSampler, &ss, &dwValue );

                PrintTabs( nTabLevel );
                OutputString( "Sampler: %d  SamplerStateType: %s\n",
                              nSampler,
                              SamplerStateToChar( ss ) );

                PrintTabs( nTabLevel );
                OutputString( "    Value:%d\n", dwValue );
            }
            nTabLevel--;
        }
        nTabLevel--;
    }
    nTabLevel--;

    // Walk each parameter defined in the effect
    for( UINT i = 0; i < effectDesc.Parameters; ++i )
    {
        FXLHANDLE hParam = pEffect->GetParameterHandleFromIndex( i );

        WalkParam( pEffect, hParam, nTabLevel );
    }
}



