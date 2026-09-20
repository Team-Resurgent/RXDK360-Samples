//-----------------------------------------------------------------------------
// NormalCompression.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>    // must come before the others
#include <d3dx9.h>
#include <xgraphics.h>

#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>


// Define a symbol that is used to compile out the use of the GPU performance counter APIs 
// when using a release build of Direct3D.  The GPU performance counter APIs only work with 
// d3d9i.lib and d3d9d.lib.
#if defined( NDEBUG ) && ( !defined( PROFILE ) || defined( FASTCAP ) )
#define _RELEASED3D
#endif


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle\nBig/small menu" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Flashing\ndiffs" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, 
        L"Change item focus (u/d)\nDecrease / increase active item (l/r)" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_2, 
        L"Change item focus (u/d)\nDecrease / increase active item (l/r)" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_2, L"Rotate\ncamera" },
    { ATG::HELP_LEFT_TRIGGER,   ATG::HELP_PLACEMENT_2, L"Decrease\nactive item" },
    { ATG::HELP_RIGHT_TRIGGER,  ATG::HELP_PLACEMENT_2, L"Increase\nactive item" },
};
#define NUM_HELP_CALLOUTS _countof(g_HelpCallouts)

//--------------------------------------------------------------------------------------
// Struct for vertex of test geometry
//--------------------------------------------------------------------------------------
struct TestGeometryVertex
{
    XMFLOAT3 Position;
    XMFLOAT2 TexCoord;
    XMFLOAT3 Tangent;
    XMFLOAT3 Binormal;
    XMFLOAT3 Normal;
};

//--------------------------------------------------------------------------------------
// Helper functions.  
//--------------------------------------------------------------------------------------
template<typename t_type> 
t_type Squared( t_type a ) { return a * a; }
template<typename t_type>
t_type Min( t_type a, t_type b ) { return a < b ? a : b; }
template<typename t_type>
t_type Max( t_type a, t_type b ) { return a > b ? a : b; }

//--------------------------------------------------------------------------------------
// Definitions for the various UI selections.
//
// Each block consists of an enum and a corresponding list of UI strings.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// enum FORMAT_TYPES:
// 
// UI control for base compression format
//--------------------------------------------------------------------------------------
enum FORMAT_TYPES
{
    FORMAT_TYPE_32_32_FLOAT, 
    FORMAT_TYPE_16_16_FLOAT, 
    FORMAT_TYPE_16_16_FIXED, 
    FORMAT_TYPE_11_11_10_FIXED, 
    FORMAT_TYPE_8_8_FIXED, 
    FORMAT_TYPE_5_6_5_FIXED, 
    FORMAT_TYPE_DXN, 
    FORMAT_TYPE_DXT5,               // RGBA = ZX0Y - the '0' could be used for other data
    FORMAT_TYPE_CTX1, 
    FORMAT_TYPE_DXT1,               // RGB = XYZ

    FORMAT_TYPE_COUNT
};

const WCHAR* g_strFormatTypeNames[] = 
{
    L"32:32 float", 
    L"16:16 float", 
    L"16:16 fixed", 
    L"11:11:10 fixed", 
    L"8:8 fixed", 
    L"5:6:5 fixed", 
    L"DXN", 
    L"DXT5", 
    L"CTX1", 
    L"DXT1", 
};
C_ASSERT( _countof(g_strFormatTypeNames) == FORMAT_TYPE_COUNT );

//--------------------------------------------------------------------------------------
// enum FILTER_TYPES:
// 
// UI control for filtering option.  Currently just point or bilinear.
//--------------------------------------------------------------------------------------
enum FILTER_TYPES
{
    FILTER_TYPE_POINT, 
    FILTER_TYPE_BILINEAR, 

    FILTER_TYPE_COUNT
};

const WCHAR* g_strFilterTypeNames[] = 
{
    L"Point", 
    L"Bilinear", 
};
C_ASSERT( _countof(g_strFilterTypeNames) == FILTER_TYPE_COUNT );

//--------------------------------------------------------------------------------------
// enum ENVIRONMENT_TYPES:
// 
// UI control for environment map option.  You can have none, diffuse, or specular.
// Diffuse makes little sense for the content in the sample, but it's a legitimate 
// usage case for environment light maps.
//--------------------------------------------------------------------------------------
enum ENVIRONMENT_TYPES
{
    ENVIRONMENT_TYPE_NONE, 
    ENVIRONMENT_TYPE_DIFFUSE, 
    ENVIRONMENT_TYPE_SPECULAR, 

    ENVIRONMENT_TYPE_COUNT
};

const CHAR* g_strEnvironmentTypeLiteralNames[] = // names to pass along to HLSL preprocessor
{
    "ENVIRONMENT_TYPE_NONE", 
    "ENVIRONMENT_TYPE_DIFFUSE", 
    "ENVIRONMENT_TYPE_SPECULAR", 
};
C_ASSERT( _countof(g_strEnvironmentTypeLiteralNames) == ENVIRONMENT_TYPE_COUNT );

const WCHAR* g_strEnvironmentTypeNames[] = 
{
    L"None", 
    L"Diffuse", 
    L"Specular", 
};
C_ASSERT( _countof(g_strEnvironmentTypeNames) == ENVIRONMENT_TYPE_COUNT );

//--------------------------------------------------------------------------------------
// enum COORD_TYPES:
// 
// UI control for coordinate system in which compressed normals are encoded.  
// Rectangular is conventional xyz, while the others use angular units.
//--------------------------------------------------------------------------------------
enum COORD_TYPES
{
    COORD_TYPE_RECTANGULAR, 
    COORD_TYPE_CYLINDRICAL, 
    COORD_TYPE_SPHERICAL, 
    COORD_TYPE_PSEUDOSPHERICAL, 

    COORD_TYPE_COUNT
};

const CHAR* g_strCoordTypeLiteralNames[] = // names to pass along to HLSL preprocessor
{
    "COORD_TYPE_RECTANGULAR", 
    "COORD_TYPE_CYLINDRICAL", 
    "COORD_TYPE_SPHERICAL", 
    "COORD_TYPE_PSEUDOSPHERICAL", 
};
C_ASSERT( _countof(g_strCoordTypeLiteralNames) == COORD_TYPE_COUNT );

const WCHAR* g_strCoordTypeNames[] = 
{
    L"Rectangular", 
    L"Cylindrical", 
    L"Spherical", 
    L"Pseudo-Spherical", 
};
C_ASSERT( _countof(g_strCoordTypeNames) == COORD_TYPE_COUNT );

//--------------------------------------------------------------------------------------
// enum TEXTURE_DIMENSIONS:
// 
// UI control for texture dimensions.  Only square powers of 2 are supported, although
// other sizes might well work.
//--------------------------------------------------------------------------------------
enum TEXTURE_DIMENSIONS
{
    TEXTURE_DIMENSIONS_32x32, 
    TEXTURE_DIMENSIONS_64x64, 
    TEXTURE_DIMENSIONS_128x128, 
    TEXTURE_DIMENSIONS_256x256, 
    TEXTURE_DIMENSIONS_512x512, 

    TEXTURE_DIMENSIONS_COUNT
};

const WCHAR* g_strTextureDimensionsNames[] = 
{
    L"32x32", 
    L"64x64", 
    L"128x128", 
    L"256x256", 
    L"512x512", 
};
C_ASSERT( _countof(g_strTextureDimensionsNames) == TEXTURE_DIMENSIONS_COUNT );

const D3DPOINT g_vTextureDimensions[] = 
{
    {  32,  32 }, 
    {  64,  64 }, 
    { 128, 128 }, 
    { 256, 256 }, 
    { 512, 512 }, 
};
C_ASSERT( _countof(g_vTextureDimensions) == TEXTURE_DIMENSIONS_COUNT );

//--------------------------------------------------------------------------------------
// enum TEST_TEXTURE_TYPE:
// 
// UI control to iterate through sample textures.  To add another sample, add a 
// value here, and a corresponding function "ComputeRawNormals*()".
//--------------------------------------------------------------------------------------
enum TEST_TEXTURE_TYPE
{
    TEST_TEXTURE_HEMISPHERE_BUMP, 
    TEST_TEXTURE_CORRUGATIONS, 
    TEST_TEXTURE_EGG_CARTON, 
    TEST_TEXTURE_CROSSED_CRACKS, 
    TEST_TEXTURE_NOISE, 

    TEST_TEXTURE_COUNT
};

const WCHAR* g_strTestTextureNames[] = 
{
    L"Hemisphere bump", 
    L"Corrugations", 
    L"Egg carton", 
    L"Crossed cracks", 
    L"Noise", 
};
C_ASSERT( _countof(g_strTestTextureNames) == TEST_TEXTURE_COUNT );

//--------------------------------------------------------------------------------------
// enum TEST_GEOMETRY_TYPE:
// 
// UI control to iterate through sample meshes.  To add another sample, add a 
// value here, and a corresponding function "GenerateGeometry*()".
//--------------------------------------------------------------------------------------
enum TEST_GEOMETRY_TYPE
{
    TEST_GEOMETRY_QUAD, 
    TEST_GEOMETRY_HEMISPHERE, 

    TEST_GEOMETRY_COUNT
};

const WCHAR* g_strTestGeometryNames[] = 
{
    L"Flat quad", 
    L"Hemisphere", 
};
C_ASSERT( _countof(g_strTestGeometryNames) == TEST_GEOMETRY_COUNT );

//--------------------------------------------------------------------------------------
// Presets:
//
// Each of the following determines a particular combination of some of the preceding
// normal map parameters.  These are meant to illustrate particular scenarios which
// are either in common use, or recommended for various reasons.
//--------------------------------------------------------------------------------------
enum PresetTypes   // Each of these is a common combination of param options
{
    PRESET_UNCOMPRESSED, 
    PRESET_8_8_FIXED,
    PRESET_DXN_NO_BIAS, 
    PRESET_DXN_BIAS,
    PRESET_DXN_PSEUDOSPHERICAL, 
    PRESET_DXT5_2_CHANNEL, 
    PRESET_DXT5_3_CHANNEL, 
    PRESET_DXT5_UNNORMALIZED, 
    PRESET_CTX1, 
    PRESET_DXT1, 
    PRESET_DXT1_UNNORMALIZED, 

    PRESET_COUNT
};

const WCHAR* g_strPresetNames[PRESET_COUNT] = 
{
    L"Full float precision", 
    L"8-bit precision", 
    L"DXN (manual sign decode)", 
    L"DXN (auto-sign decode)", 
    L"DXN (pseudo-spherical)", 
    L"DXT5 (2 channel)", 
    L"DXT5 (3 channel)", 
    L"DXT5 (w/o renormalize)", 
    L"CTX1", 
    L"DXT1", 
    L"DXT1 (w/o renormalize)", 
};

// These are the params controlled by presets.  All other params
// are freely modifiable without affecting presets
enum PresetParams 
{
    // Key options
    PRESET_PARAM_FORMAT_TYPE, 
    PRESET_PARAM_COORD_TYPE, 

    // Encoding options
    PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
    PRESET_PARAM_USE_BIAS_FOR_SIGN, 
    PRESET_PARAM_BUILTIN_Z, 
    PRESET_PARAM_RENORMALIZE_TEXEL, 

    PRESET_PARAM_COUNT
};

typedef UINT Preset[PRESET_PARAM_COUNT];

Preset g_Presets[] = 
{
    // PRESET_UNCOMPRESSED, 
    {
        FORMAT_TYPE_32_32_FLOAT,    // PRESET_PARAM_FORMAT_TYPE, 
        COORD_TYPE_RECTANGULAR,     // PRESET_PARAM_COORD_TYPE, 

        FALSE,                      // PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
        TRUE,                       // PRESET_PARAM_USE_BIAS_FOR_SIGN, 
        FALSE,                      // PRESET_PARAM_BUILTIN_Z, 
        FALSE,                      // PRESET_PARAM_RENORMALIZE_TEXEL, 
    }, 
    // PRESET_8_8_FIXED, 
    {
        FORMAT_TYPE_8_8_FIXED,      // PRESET_PARAM_FORMAT_TYPE, 
        COORD_TYPE_RECTANGULAR,     // PRESET_PARAM_COORD_TYPE, 

        FALSE,                      // PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
        TRUE,                       // PRESET_PARAM_USE_BIAS_FOR_SIGN, 
        FALSE,                      // PRESET_PARAM_BUILTIN_Z, 
        FALSE,                      // PRESET_PARAM_RENORMALIZE_TEXEL, 
    }, 
    // PRESET_DXN_NO_BIAS, 
    {
        FORMAT_TYPE_DXN,            // PRESET_PARAM_FORMAT_TYPE, 
        COORD_TYPE_RECTANGULAR,     // PRESET_PARAM_COORD_TYPE, 

        FALSE,                      // PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
        FALSE,                      // PRESET_PARAM_USE_BIAS_FOR_SIGN, 
        FALSE,                      // PRESET_PARAM_BUILTIN_Z, 
        FALSE,                      // PRESET_PARAM_RENORMALIZE_TEXEL, 
    }, 
    // PRESET_DXN_BIAS, 
    {
        FORMAT_TYPE_DXN,            // PRESET_PARAM_FORMAT_TYPE, 
        COORD_TYPE_RECTANGULAR,     // PRESET_PARAM_COORD_TYPE, 

        FALSE,                      // PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
        TRUE,                       // PRESET_PARAM_USE_BIAS_FOR_SIGN, 
        FALSE,                      // PRESET_PARAM_BUILTIN_Z, 
        FALSE,                      // PRESET_PARAM_RENORMALIZE_TEXEL, 
    }, 
    // PRESET_DXN_PSEUDOSPHERICAL, 
    {
        FORMAT_TYPE_DXN,            // PRESET_PARAM_FORMAT_TYPE, 
        COORD_TYPE_PSEUDOSPHERICAL, // PRESET_PARAM_COORD_TYPE, 

        TRUE,                       // PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
        TRUE,                       // PRESET_PARAM_USE_BIAS_FOR_SIGN, 
        FALSE,                      // PRESET_PARAM_BUILTIN_Z, 
        FALSE,                      // PRESET_PARAM_RENORMALIZE_TEXEL, 
    }, 
    // PRESET_DXT5_2_CHANNEL, 
    {
        FORMAT_TYPE_DXT5,           // PRESET_PARAM_FORMAT_TYPE, 
        COORD_TYPE_RECTANGULAR,     // PRESET_PARAM_COORD_TYPE, 

        FALSE,                      // PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
        TRUE,                       // PRESET_PARAM_USE_BIAS_FOR_SIGN, 
        FALSE,                      // PRESET_PARAM_BUILTIN_Z, 
        FALSE,                      // PRESET_PARAM_RENORMALIZE_TEXEL, 
    }, 
    // PRESET_DXT5_3_CHANNEL, 
    {
        FORMAT_TYPE_DXT5,           // PRESET_PARAM_FORMAT_TYPE, 
        COORD_TYPE_RECTANGULAR,     // PRESET_PARAM_COORD_TYPE, 

        FALSE,                      // PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
        TRUE,                       // PRESET_PARAM_USE_BIAS_FOR_SIGN, 
        TRUE,                       // PRESET_PARAM_BUILTIN_Z, 
        TRUE,                       // PRESET_PARAM_RENORMALIZE_TEXEL, 
    }, 
    // PRESET_DXT5_UNNORMALIZED, 
    {
        FORMAT_TYPE_DXT5,           // PRESET_PARAM_FORMAT_TYPE, 
        COORD_TYPE_RECTANGULAR,     // PRESET_PARAM_COORD_TYPE, 

        FALSE,                      // PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
        TRUE,                       // PRESET_PARAM_USE_BIAS_FOR_SIGN, 
        TRUE,                       // PRESET_PARAM_BUILTIN_Z, 
        FALSE,                      // PRESET_PARAM_RENORMALIZE_TEXEL, 
    }, 
    // PRESET_CTX1, 
    {
        FORMAT_TYPE_CTX1,           // PRESET_PARAM_FORMAT_TYPE, 
        COORD_TYPE_RECTANGULAR,     // PRESET_PARAM_COORD_TYPE, 

        FALSE,                      // PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
        TRUE,                       // PRESET_PARAM_USE_BIAS_FOR_SIGN, 
        FALSE,                      // PRESET_PARAM_BUILTIN_Z, 
        FALSE,                      // PRESET_PARAM_RENORMALIZE_TEXEL, 
    }, 
    // PRESET_DXT1, 
    {
        FORMAT_TYPE_DXT1,           // PRESET_PARAM_FORMAT_TYPE, 
        COORD_TYPE_RECTANGULAR,     // PRESET_PARAM_COORD_TYPE, 

        FALSE,                      // PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
        TRUE,                       // PRESET_PARAM_USE_BIAS_FOR_SIGN, 
        TRUE,                       // PRESET_PARAM_BUILTIN_Z, 
        TRUE,                       // PRESET_PARAM_RENORMALIZE_TEXEL, 
    }, 
    // PRESET_DXT1_UNNORMALIZED, 
    {
        FORMAT_TYPE_DXT1,           // PRESET_PARAM_FORMAT_TYPE, 
        COORD_TYPE_RECTANGULAR,     // PRESET_PARAM_COORD_TYPE, 

        FALSE,                      // PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
        TRUE,                       // PRESET_PARAM_USE_BIAS_FOR_SIGN, 
        TRUE,                       // PRESET_PARAM_BUILTIN_Z, 
        FALSE,                      // PRESET_PARAM_RENORMALIZE_TEXEL, 
    }, 
};
C_ASSERT( _countof(g_Presets) == PRESET_COUNT );

//--------------------------------------------------------------------------------------
// Remappings of channels.  These are useful in situations where some channel has
// higher fidelity than others, and we need greater fidelity for one piece of data
// than for another.
//--------------------------------------------------------------------------------------
enum SWIZZLE_TYPES
{
    SWIZZLE_TYPE_NONE, 
    SWIZZLE_TYPE_RGBA_XYZ0 = SWIZZLE_TYPE_NONE, 
    SWIZZLE_TYPE_RGBA_ZX0Y, 
    SWIZZLE_TYPE_RGBA_ZY0X, 

    SWIZZLE_TYPE_COUNT
};

//--------------------------------------------------------------------------------------
// Specialized XNAMath-like types with hybrid signed/unsigned formats
//--------------------------------------------------------------------------------------
#pragma warning(push)
#pragma warning(disable:4201)   // anonymous struct/union

// 5:6:5-bit channels, signed:signed:unsigned
typedef struct _XMS5S6U5 {  
    union {
        struct {
             SHORT x : 5;
             SHORT y : 6;
            USHORT z : 5;
        };
        USHORT v;
    };
} XMS5S6U5;

// 11:11:10-bit channels, signed:signed:unsigned
typedef struct _XMS11S11U10N3 {
    union {
        struct {
             INT x : 11;
             INT y : 11;
            UINT z : 10;
        };
        UINT v;
    };
} XMS11S11U10N3;
#pragma warning(pop)

//--------------------------------------------------------------------------------------
// Structs aliasing normal data as various formats
//--------------------------------------------------------------------------------------
// Pointer to raw data, aliasing as all possible types
struct GenericRawNormalBuffer 
{
    union
    {
        void*                       m_pVoid;
        XMFLOAT4*                   m_pXMFLOAT4;
        XMVECTOR*                   m_pXMVECTOR;    // guarantee alignment
    };
};

// Pointer to compressed data, aliasing as all possible types
struct GenericCompressedNormalBuffer 
{
    union
    {
        void*                       m_pVoid;            // Typeless, or DXT-compressed
        XMS5S6U5*                   m_pXMS5S6U5;        // 5:6:5 signed/signed/unsigned
        XMU565*                     m_pXMU565;          // 5:6:5 unsigned
        XMBYTEN2*                   m_pXMBYTEN2;        // 8:8 signed - This is not a real XNA math type
        XMUBYTEN2*                  m_pXMUBYTEN2;       // 8:8 unsigned - This is not a real XNA math type
        XMS11S11U10N3*              m_pXMS11S11U10N3;   // 11:11:10 signed/signed/unsigned
        XMUHENDN3*                  m_pXMUHENDN3;       // 11:11:10 unsigned
        XMSHORTN2*                  m_pXMSHORTN2;       // 16:16 signed
        XMUSHORTN2*                 m_pXMUSHORTN2;      // 16:16 unsigned
        XMHALF2*                    m_pXMHALF2;         // 16:16 float
        XMFLOAT2*                   m_pXMFLOAT2;        // 32:32 float
    };
};

// Matching raw and compressed buffers
struct RawAndCompressedNormalBuffer
{
    UINT                            m_iTextureWidth;
    UINT                            m_iTextureHeight;
    GenericRawNormalBuffer          m_pRawData;
    IDirect3DTexture9*              m_pRawTexture;
    GenericCompressedNormalBuffer   m_pCompressedData;
    IDirect3DTexture9*              m_pCompressedTexture;
};

//--------------------------------------------------------------------------------------
// These macros make double-click on error work properly for the shaders
// defined inline below
//--------------------------------------------------------------------------------------
#define RECORD_FILE_LINE( Name )                                                \
    const char* g_str##Name##FILE = __FILE__;                                   \
    UINT g_i##Name##LINE = __LINE__ + 2;

#define ADD_FILE_LINE_DIRECTIVE( Buffer, Size, Conds, Prog, ProgName )          \
    sprintf_s( Buffer, Size, "%s\n#line %d \"%s\"\n%s",                         \
        Conds, g_i##ProgName##LINE, g_str##ProgName##FILE, Prog );

//--------------------------------------------------------------------------------------
// Allow shader global variables to be either compile-time or run-time constants 
//--------------------------------------------------------------------------------------
#define DECLARE_BOOL_CONST( ConstName, RegisterName )                           \
"#ifndef "#ConstName"\nbool "#ConstName" : register("#RegisterName");\n#endif\n"

#define DECLARE_INT_CONST( ConstName, RegisterName )                           \
"#ifndef "#ConstName"\nint "#ConstName" : register("#RegisterName");\n#endif\n"

//--------------------------------------------------------------------------------------
// Shaders built inline to allow free compilation of different combinations
//
// The same HLSL code can be built as either one monolithic ubershader with 
// branching, or a number of specialized shaders without branches
//--------------------------------------------------------------------------------------
const char* g_strVertexShaderConditionals = "";

RECORD_FILE_LINE( VertexShaderProgram )
const char* g_strVertexShaderProgram =
    "float4x4 matWVP : register(c0);                                                                \n"
    "float4x4 matWorld : register(c4);                                                              \n"
    "                                                                                               \n"
    "struct VS_IN                                                                                   \n"
    "{                                                                                              \n"
    "    float4 vLocalPos   : POSITION;                                                             \n"
    "    float2 vTexCoord   : TEXCOORD;                                                             \n"
    "    float3 vTangent    : TANGENT;                                                              \n"
    "    float3 vBinormal   : BINORMAL;                                                             \n"
    "    float3 vNormal     : NORMAL;                                                               \n"
    "};                                                                                             \n"
    "                                                                                               \n"
    "struct VS_OUT                                                                                  \n"
    "{                                                                                              \n"
    "    float4 vProjPos    : POSITION;                                                             \n"
    "    float4 vWorldPos   : POSITION1;                                                            \n"
    "    float2 vTexCoord   : TEXCOORD;                                                             \n"
    "    float3 vTangent    : TANGENT;                                                              \n"
    "    float3 vBinormal   : BINORMAL;                                                             \n"
    "    float3 vNormal     : NORMAL;                                                               \n"
    "};                                                                                             \n"
    "                                                                                               \n"
    "VS_OUT main( VS_IN In )                                                                        \n"
    "{                                                                                              \n"
    "    VS_OUT Out;                                                                                \n"
    "    Out.vProjPos = mul( In.vLocalPos, matWVP );                                                \n"
    "    Out.vWorldPos = mul( In.vLocalPos, matWorld );                                             \n"
    "    Out.vTexCoord = In.vTexCoord;                                                              \n"
    "    Out.vTangent = mul( In.vTangent, matWorld );                                               \n"
    "    Out.vBinormal = mul( In.vBinormal, matWorld );                                             \n"
    "    Out.vNormal = mul( In.vNormal, matWorld );                                                 \n"
    "    return Out;                                                                                \n"
    "}                                                                                              \n"
;                                                           

// The boolean branch conditions
enum PS_CONDITIONAL_BOOL_ENUM 
{
    PS_CONDITIONAL_BOOL_RENDER_NORMAL, 
    PS_CONDITIONAL_BOOL_DECODE_SIGN, 
    PS_CONDITIONAL_BOOL_BUILT_IN_Z,
    PS_CONDITIONAL_BOOL_RENORMALIZE_TEXEL,
    PS_CONDITIONAL_BOOL_RENORMALIZE_INTERPOLANTS,
    PS_CONDITIONAL_BOOL_RENORMALIZE_RESULT, 
    PS_CONDITIONAL_BOOL_AMBIENT, 
    PS_CONDITIONAL_BOOL_DIFFUSE, 
    PS_CONDITIONAL_BOOL_SPECULAR, 
    PS_CONDITIONAL_BOOL_USE_LOOKUP_MAP_FOR_TRIG, 

    PS_CONDITIONAL_BOOL_COUNT
};
const CHAR* g_strPSConditionalBoolNames[] = 
{
    "g_bRenderNormals", 
    "g_bDecodeSign", 
    "g_bBuiltInZ", 
    "g_bRenormalizeTexel", 
    "g_bRenormalizeInterpolants", 
    "g_bRenormalizeResult", 
    "g_bAmbient", 
    "g_bDiffuse", 
    "g_bSpecular", 
    "g_bUseLookupMapForTrig", 
};
C_ASSERT( _countof(g_strPSConditionalBoolNames) == PS_CONDITIONAL_BOOL_COUNT );

// The integer branch conditions
enum PS_CONDITIONAL_INT_ENUM 
{
    PS_CONDITIONAL_INT_COORD_TYPE, 
    PS_CONDITIONAL_INT_ENVIRONMENT_TYPE, 

    PS_CONDITIONAL_INT_COUNT
};
const CHAR* g_strPSConditionalIntNames[] = 
{
    "g_iCoordType", 
    "g_iEnvironmentType", 
};
C_ASSERT( _countof(g_strPSConditionalIntNames) == PS_CONDITIONAL_INT_COUNT );
const CHAR * const * g_strPSConditionalIntDefinitions[] = 
{
    g_strCoordTypeLiteralNames, 
    g_strEnvironmentTypeLiteralNames, 
};
C_ASSERT( _countof(g_strPSConditionalIntDefinitions) == PS_CONDITIONAL_INT_COUNT );

#define PS_CONDITIONAL_COUNT (PS_CONDITIONAL_BOOL_COUNT + PS_CONDITIONAL_INT_COUNT)

// The branch conditions.  These will become either shader constants
// or preprocessor defines to one of the possible named values.
const char* g_strPixelShaderConditionals =
DECLARE_BOOL_CONST( g_bRenderNormals,            b0 )
DECLARE_BOOL_CONST( g_bDecodeSign,              b1 )
DECLARE_BOOL_CONST( g_bBuiltInZ,                b2 )
DECLARE_BOOL_CONST( g_bRenormalizeTexel,        b3 )
DECLARE_BOOL_CONST( g_bRenormalizeInterpolants, b4 )
DECLARE_BOOL_CONST( g_bRenormalizeResult,       b5 )
DECLARE_BOOL_CONST( g_bAmbient,                 b6 )
DECLARE_BOOL_CONST( g_bDiffuse,                 b7 )
DECLARE_BOOL_CONST( g_bSpecular,                b8 )
DECLARE_BOOL_CONST( g_bUseLookupMapForTrig,     b9 )
DECLARE_INT_CONST ( g_iCoordType,               c1 )    // c0 is vEyePos
DECLARE_INT_CONST ( g_iEnvironmentType,         c2 )
;

// When viewed in PIX, this shader will be nicely syntax-colored
//
// To see ALU instruction costs for each conditional block individually, 
// choose "Specialize shaders" = "FALSE" from the menu, and investigate 
// microcode in PIX.  The ALU estimates in the comments were derived this 
// way.  They are generally overestimates, since they assume no cross-
// optimization with other code.
RECORD_FILE_LINE( PixelShaderProgram )
const char* g_strPixelShaderProgram =
    "                                                                                               \n"
    "#define COORD_TYPE_RECTANGULAR     0                                                           \n"
    "#define COORD_TYPE_CYLINDRICAL     1                                                           \n"
    "#define COORD_TYPE_SPHERICAL       2                                                           \n"
    "#define COORD_TYPE_PSEUDOSPHERICAL 3                                                           \n"
    "                                                                                               \n"
    "#define ENVIRONMENT_TYPE_NONE      0                                                           \n"
    "#define ENVIRONMENT_TYPE_DIFFUSE   1                                                           \n"
    "#define ENVIRONMENT_TYPE_SPECULAR  2                                                           \n"
    "                                                                                               \n"
    "float3 vEyeWorldPos : register(c0);                                                            \n"
    "                                                                                               \n"
    "struct PS_IN                                                                                   \n"
    "{                                                                                              \n"
    "    float4 vWorldPos   : POSITION1;                                                            \n"  
    "    float2 vTexCoord   : TEXCOORD;                                                             \n"  
    "    float3 vTangent    : TANGENT;                                                              \n"  
    "    float3 vBinormal   : BINORMAL;                                                             \n"  
    "    float3 vNormal     : NORMAL;                                                               \n"  
    "};                                                                                             \n"
    "                                                                                               \n"
    "sampler NormalMap                  : register(s0);                                             \n"
    "samplerCUBE EnvironmentMap         : register(s1);                                             \n"
    "sampler SinCosLookupNegPiToPi      : register(s2);                                             \n"
    "sampler SinCosLookupZeroToPi       : register(s3);                                             \n"
    "sampler SinCosLookupZeroToHalfPi   : register(s4);                                             \n"
    "                                                                                               \n"
    "static const float g_fPi = 3.141593f;                                                          \n"  
    "                                                                                               \n"
    "float2 DecodeSigned( float2 v )                                                                \n"
    "{                                                                                              \n"
    "    return 2.0f * v - 1.0f;                                                                    \n"
    "};                                                                                             \n"
    "                                                                                               \n"
    "// Assume incoming values are in [0,1], intended to signify [min,max].                         \n"
    "// In a specialized shader, if we use a lookup map, the function becomes a single tfetch1D.    \n"
    "// Otherwise, it becomes two scalar sin/cos ops and possibly one of: mulsc/addsc/mad.          \n"
    "float2 SinCos( float a, float min, float max )                                                 \n"
    "{                                                                                              \n"
    "    if( g_bUseLookupMapForTrig )                                                               \n"
    "    {                                                                                          \n"
    "        if( min == -g_fPi && max == g_fPi )                                                    \n"
    "        {                                                                                      \n"
    "            return tex1D( SinCosLookupNegPiToPi, a );                                          \n"
    "        }                                                                                      \n"
    "        else if( min == 0.0f && max == g_fPi )                                                 \n"
    "        {                                                                                      \n"
    "            return tex1D( SinCosLookupZeroToPi, a );                                           \n"
    "        }                                                                                      \n"
    "        else if( min == 0.0f && max == g_fPi * 0.5f )                                          \n"
    "        {                                                                                      \n"
    "            return tex1D( SinCosLookupZeroToHalfPi, a );                                       \n"
    "        }                                                                                      \n"
    "    }                                                                                          \n"
    "                                                                                               \n"
    "    // If min is zero, the compiler will omit the offset instruction                           \n"
    "    // Sometimes we could optimize away an instruction here by other means                     \n"
    "    a *= ( max - min );                                                                        \n"
    "    a += min;                                                                                  \n"
    "                                                                                               \n"
    "    // The ALU trig instructions assume an input range [-PI,PI], while HLSL                    \n"
    "    // allows an arbitrary input range.  If you call trig functions from HLSL,                 \n"
    "    // you get extra codegen to explicitly wrap the inputs into the correct range.             \n"
    "    float4 vSinCos;                                                                            \n"  
    "    asm                                                                                        \n"
    "    {                                                                                          \n"
    "        sin vSinCos.x, a                                                                       \n"
    "        cos vSinCos.y, a                                                                       \n"
    "    };                                                                                         \n"
    "    return vSinCos.xy;                                                                         \n"
    "};                                                                                             \n"
    "                                                                                               \n"
    "float4 main( PS_IN In ) : COLOR                                                                \n"
    "{                                                                                              \n"
    "    float3 vTexNormal = tex2D( NormalMap, In.vTexCoord ).xyz;                                  \n"  
    "                                                                                               \n"
    "                                                                                               \n"
    "    if( g_iCoordType == COORD_TYPE_RECTANGULAR )                                               \n"
    "    {                                                                                          \n"
    "        // If the normal map contains signed values encoded as unsigned, then decode           \n"
    "        // [1 vector op]                                                                       \n"
    "        if( g_bDecodeSign )                                                                    \n"
    "        {                                                                                      \n"
    "            vTexNormal.xy = DecodeSigned( vTexNormal.xy );                                     \n"
    "        }                                                                                      \n"
    "        // If the normal map does not contain a z channel, then compute z from xy              \n"
    "        // [1 vector op + 1 scalar op]                                                         \n"
    "        if( !g_bBuiltInZ )                                                                     \n"
    "        {                                                                                      \n"
    "           // This exact syntax works well, other variants produce an extra op                 \n"
    "           vTexNormal.z = sqrt( saturate( 1.0f + dot( vTexNormal.xy, -vTexNormal.xy ) ) );     \n"
    "        }                                                                                      \n"
    "    }                                                                                          \n"                                               
    "                                                                                               \n"
    "    // If the normal map contains values in cylindrical coordinates, then decode               \n"
    "    // No SinCos lookup map:  [3 vector ops + 3 scalar ops]                                    \n"
    "    // SinCos lookup map:  [2 vector ops + 1 scalar op + 1 texture op]                         \n"
    "    if( g_iCoordType == COORD_TYPE_CYLINDRICAL )                                               \n"
    "    {                                                                                          \n"
    "        float2 vSinCosPhi = SinCos( vTexNormal.x, -g_fPi, g_fPi );                             \n"
    "        vTexNormal.z = sqrt( saturate( 1.0f - vTexNormal.y * vTexNormal.y ) );                 \n"
    "        vTexNormal.xy = float2( vSinCosPhi.y, vSinCosPhi.x ) * vTexNormal.y;                   \n"
    "    }                                                                                          \n"
    "                                                                                               \n"
    "    // If the normal map contains values in spherical coordinates, then decode                 \n"
    "    // No SinCos lookup map:  [2 vector ops + 5 scalar ops]                                    \n"
    "    // SinCos lookup map:  [1 vector op + 2 texture ops]                                       \n"
    "    if( g_iCoordType == COORD_TYPE_SPHERICAL )                                                 \n"
    "    {                                                                                          \n"
    "        float2 vSinCosPhi = SinCos( vTexNormal.x, -g_fPi, g_fPi );                             \n"
    "        float2 vSinCosTheta = SinCos( vTexNormal.y, 0.0f, g_fPi * 0.5f );                      \n"
    "        vTexNormal = float3( vSinCosPhi.y * vSinCosTheta.x,                                    \n"
    "            vSinCosPhi.x * vSinCosTheta.x,                                                     \n"
    "            vSinCosTheta.y );                                                                  \n"
    "    }                                                                                          \n"
    "                                                                                               \n"
    "    // If the normal map contains values in pseudo-spherical coordinates, then decode          \n"
    "    // No SinCos lookup map:  [1 vector op + 6 scalar ops]                                     \n"
    "    // SinCos lookup map:  [1 vector op + 2 texture ops]                                       \n"
    "    if( g_iCoordType == COORD_TYPE_PSEUDOSPHERICAL )                                           \n"
    "    {                                                                                          \n"
    "        float2 vSinCosPhi = SinCos( vTexNormal.x, 0.0f, g_fPi );                               \n"
    "        float2 vSinCosTheta = SinCos( vTexNormal.y, 0.0f, g_fPi );                             \n"
    "        vTexNormal = float3( vSinCosPhi.y * vSinCosTheta.x,                                    \n"
    "            vSinCosTheta.y,                                                                    \n"
    "            vSinCosPhi.x * vSinCosTheta.x );                                                   \n"                        
    "    }                                                                                          \n"
    "                                                                                               \n"                                                    
    "    // If desired, renormalize to account for error from filtering,                            \n"
    "    // block-compression, quantization, etc.                                                   \n"
    "    // [2 vector ops + 1 scalar op]                                                            \n"
    "    if( g_bRenormalizeTexel )                                                                  \n"
    "    {                                                                                          \n"
    "        vTexNormal = normalize( vTexNormal );                                                  \n"
    "    }                                                                                          \n"
    "                                                                                               \n"
    "    // If desired, renormalize and reorthogonalize to account for error from interpolation     \n"
    "    // [6 vector ops + 2 scalar ops]                                                           \n"
    "    float3 vTangent = In.vTangent;                                                             \n"
    "    float3 vBinormal = In.vBinormal;                                                           \n"
    "    float3 vNormal = In.vNormal;                                                               \n"
    "    if( g_bRenormalizeInterpolants )                                                           \n"
    "    {                                                                                          \n"
    "        vTangent = normalize( In.vTangent );                                                   \n"
    "        vBinormal = normalize( In.vBinormal                                                    \n"
    "            - In.vTangent * dot( In.vTangent, In.vBinormal ) );                                \n"
    "        vNormal = cross( vTangent, vBinormal );                                                \n"
    "    }                                                                                          \n"
    "    float3x3 matTBN = float3x3( vTangent, vBinormal, vNormal );                                \n"
    "    float3 vWorldNormal = mul( vTexNormal, matTBN );                                           \n"
    "                                                                                               \n"  
    "    // If desired, renormalize to account for overall error                                    \n" 
    "    // [2 vector ops + 1 scalar op]                                                            \n"
    "    if( g_bRenormalizeResult )                                                                 \n"
    "    {                                                                                          \n"
    "        vWorldNormal = normalize( vWorldNormal );                                              \n"
    "    }                                                                                          \n"
    "                                                                                               \n"  
    "    if( g_bRenderNormals )                                                                      \n"
    "    {                                                                                          \n"
    "        // Return the normal vector, scaled/offset to [0,1]                                    \n"  
    "        vWorldNormal.xyz *= 0.5f;                                                              \n"  
    "        vWorldNormal.xyz += 0.5f;                                                              \n" 
    "        return float4( vWorldNormal, 1.0f );                                                   \n"  
    "    }                                                                                          \n"
    "    else                                                                                       \n"
    "    {                                                                                          \n"
    "        // Perform lighting...                                                                 \n"
    "        const float3 vAmbientColor = float3( 0.03f, 0.03f, 0.03f );                            \n"
    "        const float3 vDiffuseColor = float3( 0.08f, 0.07f, 0.05f );                            \n"
    "        const float3 vSpecularColor = float3( 0.2f, 0.5f, 0.7f );                              \n"
    "        const float fSpecularPower = 16.0f;                                                    \n"                                              
    "        const float3 vLightDirection = float3( 0.80f, 0.60f, -0.20f );                         \n"
    "        const float fEnvironmentColorScale = 0.1f;                                             \n"                                              
    "                                                                                               \n"
    "        float3 vColor = 0.0f;                                                                  \n"
    "                                                                                               \n"
    "        // Ambient light...                                                                    \n"
    "        if( g_bAmbient )                                                                       \n"
    "        {                                                                                      \n"
    "           vColor += vAmbientColor;                                                            \n"
    "        }                                                                                      \n"
    "                                                                                               \n"
    "        // Diffuse light...                                                                    \n"
    "        if( g_bDiffuse )                                                                       \n"
    "        {                                                                                      \n"
    "            float fDiffuseFraction = saturate( dot( -vLightDirection, vWorldNormal ) );        \n"
    "            vColor += vDiffuseColor * fDiffuseFraction;                                        \n"
    "        }                                                                                      \n"
    "                                                                                               \n"
    "        // Specular light...                                                                   \n"
    "        if( g_bSpecular )                                                                      \n"
    "        {                                                                                      \n"
    "            float3 vEyeToSurface = normalize( In.vWorldPos - vEyeWorldPos );                   \n"
    "            float3 vEyeToSurfaceReflected = reflect( vEyeToSurface, vWorldNormal );            \n"
    "            float fSpecularFraction =                                                          \n"
    "                saturate( dot( -vLightDirection, vEyeToSurfaceReflected ) );                   \n"
    "            fSpecularFraction = pow( fSpecularFraction, fSpecularPower );                      \n"
    "            vColor += vSpecularColor * fSpecularFraction;                                      \n"
    "        }                                                                                      \n"
    "                                                                                               \n"
    "        // Environment diffuse...                                                              \n"
    "        if( g_iEnvironmentType == ENVIRONMENT_TYPE_DIFFUSE )                                   \n"
    "        {                                                                                      \n"
    "            float4 vEnvironmentColor = texCUBE( EnvironmentMap, vWorldNormal );                \n"
    "            vColor += vEnvironmentColor * fEnvironmentColorScale;                              \n"
    "        }                                                                                      \n"
    "                                                                                               \n"
    "        // Environment reflection...                                                           \n"
    "        if( g_iEnvironmentType == ENVIRONMENT_TYPE_SPECULAR )                                  \n"
    "        {                                                                                      \n"
    "            float3 vEyeToSurface = normalize( In.vWorldPos - vEyeWorldPos );                   \n"
    "            float3 vEyeToSurfaceReflected = reflect( vEyeToSurface, vWorldNormal );            \n"
    "            float4 vEnvironmentColor = texCUBE( EnvironmentMap, vEyeToSurfaceReflected );      \n"
    "            vColor += vEnvironmentColor * fEnvironmentColorScale;                              \n"
    "        }                                                                                      \n"
    "                                                                                               \n"
    "        return float4( vColor, 1.0f );                                                         \n"
    "    }                                                                                          \n"
    "}                                                                                              \n"
;                                                                                                   
                                                                                                    
//--------------------------------------------------------------------------------------
// Supported tweakable UI parameters.     
//--------------------------------------------------------------------------------------
enum UIParamTypes 
{
    UI_PARAM_PRESET, 

    // Key options
    UI_PARAM_FORMAT_TYPE, 
    UI_PARAM_FILTER_TYPE, 

    // Data options
    UI_PARAM_TEXTURE_DIMENSIONS, 
    UI_PARAM_TEST_TEXTURE, 
    UI_PARAM_TEST_GEOMETRY, 

    // Rendering options
    UI_PARAM_AMBIENT, 
    UI_PARAM_DIFFUSE, 
    UI_PARAM_SPECULAR, 
    UI_PARAM_ENVIRONMENT, 
    UI_PARAM_RENDER_NORMALS, 

    // Encoding options
    UI_PARAM_COORD_TYPE, 
    UI_PARAM_USE_LOOKUP_MAP_FOR_TRIG, 
    UI_PARAM_USE_BIAS_FOR_SIGN, 
    UI_PARAM_BUILTIN_Z, 
    UI_PARAM_RENORMALIZE_TEXEL, 
    UI_PARAM_RENORMALIZE_INTERPOLANTS, 
    UI_PARAM_RENORMALIZE_RESULT, 
    UI_PARAM_SPECIALIZE_SHADERS, 

    UI_PARAM_COUNT
};

//--------------------------------------------------------------------------------------
// UIParam
//
// Base class for various types of menu selectors
//--------------------------------------------------------------------------------------
class UIParam
{
public:
    static const DWORD  m_dwActiveParamColor = 0xffffff00;   
    static const DWORD  m_dwInactiveParamColor = 0xffffffff;   
    static const DWORD  m_dwActiveOptionColor = 0xff00ff00;   
    static const DWORD  m_dwInactiveOptionColor = 0xff808080;   
    static const FLOAT  m_fActiveParamScale;
    static const FLOAT  m_fInactiveParamScale;
    static const FLOAT  m_fParamX;
    static const FLOAT  m_fOptionX;

    UIParam( const WCHAR* ParamName ) : m_ParamName( ParamName )
    {}

    virtual VOID RenderOptionUI( ATG::Font* pFont, FLOAT fParamX, FLOAT fParamY, BOOL bActive ) = 0;
    VOID RenderUI( ATG::Font* pFont, FLOAT fParamX, FLOAT fParamY, BOOL bActive )
    {
        if( bActive ) 
        {
            pFont->SetScaleFactors( m_fActiveParamScale, m_fActiveParamScale );
            pFont->DrawText( fParamX, fParamY, m_dwActiveParamColor, m_ParamName );
        }
        else
        {
            pFont->SetScaleFactors( m_fInactiveParamScale, m_fInactiveParamScale );
            pFont->DrawText( fParamX, fParamY, m_dwInactiveParamColor, m_ParamName );
        }
        RenderOptionUI( pFont, fParamX, fParamY, bActive );
    }

    virtual VOID        DecreaseValue( FLOAT fScale = 1.0f ) = 0;
    virtual VOID        IncreaseValue( FLOAT fScale = 1.0f ) = 0;

protected:
    const WCHAR*        m_ParamName;
};

const FLOAT  UIParam::m_fActiveParamScale = 1.1f;
const FLOAT  UIParam::m_fInactiveParamScale = 1.1f;
const FLOAT  UIParam::m_fOptionX = 300.0f;

class UIParamEnum : public UIParam
{
public:
    UIParamEnum( const WCHAR* ParamName, const WCHAR** OptionNames, UINT iCount, UINT iValue = 0 )
        : UIParam( ParamName )
        , m_OptionNames( OptionNames )
        , m_iCount( iCount )
        , m_iValue( iValue )
    {}

    VOID RenderOptionUI( ATG::Font* pFont, FLOAT fParamX, FLOAT fParamY, BOOL bActive )
    {
        UINT iValue = GetValue();
        const WCHAR *OptionName = ( iValue >= 0 && iValue < m_iCount ) ? m_OptionNames[iValue] : L"n/a";
        if( bActive ) 
        {
            WCHAR SelectText[256];
            swprintf_s( SelectText, L"< %s >", OptionName );
            pFont->DrawText( fParamX + m_fOptionX, fParamY, m_dwActiveOptionColor, SelectText );
        }
        else
        {
            pFont->DrawText( fParamX + m_fOptionX, fParamY, m_dwInactiveOptionColor, OptionName );
        }
    }

    UINT                GetValue() { return m_iValue; };
    VOID                SetValue( UINT iValue ) { m_iValue = iValue; };
    VOID                DecreaseValue( FLOAT fScale = 1.0f ) { m_iValue += m_iCount - 1; m_iValue %= m_iCount; }
    VOID                IncreaseValue( FLOAT fScale = 1.0f ) { m_iValue += 1;            m_iValue %= m_iCount; }

protected:
    const WCHAR**       m_OptionNames;
    UINT                m_iCount;
    UINT                m_iValue;
};

static const WCHAR* g_BoolOptionNames[2] = { L"FALSE", L"TRUE" };
class UIParamBool : public UIParamEnum
{
public:
    UIParamBool( const WCHAR* ParamName, BOOL bValue = FALSE )
        : UIParamEnum( ParamName, g_BoolOptionNames, 2, (UINT) bValue )
    {}

    BOOL                GetValue() { return UIParamEnum::GetValue() == 0 ? FALSE : TRUE; };
};

//-----------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//-----------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // ATG helper items
    ATG::PackedResource             m_Resource;
    ATG::Font                       m_Font;                 // Font for drawing text
    ATG::Timer                      m_Timer;                // Timer
    ATG::Help                       m_Help;                 // Display help

    // Sample options
    BOOL                            m_bDrawHelp;
    BOOL                            m_bBigMenu;
    BOOL                            m_bFlashingDiffs;
    FLOAT                           m_fFlashingTimer;

    // Camera parameters
    XMMATRIX                        m_matWorld;
    XMMATRIX                        m_matView;
    FLOAT                           m_fCameraPitch;
    FLOAT                           m_fCameraYaw;

    // Compression stats
    UINT                            m_iCompressedSize;
    UINT                            m_iCompressedBPP;

    // UI elements and parameters
    UINT                            m_iActiveUIParameter;   // Which UI item is affected by l/r input
    UINT                            m_iVisibleUIStart;
    const static UINT               m_iVisibleUICount = 4;

    UIParamEnum                     m_PresetParam;

    UIParamEnum                     m_FormatTypeParam;
    UIParamEnum                     m_FilterTypeParam;
    UIParamEnum                     m_TextureDimensionsParam;
    UIParamEnum                     m_TestTextureParam;
    UIParamEnum                     m_TestGeometryParam;
    UIParamBool                     m_AmbientParam;
    UIParamBool                     m_DiffuseParam;
    UIParamBool                     m_SpecularParam;
    UIParamEnum                     m_EnvironmentTypeParam;
    UIParamBool                     m_RenderNormalsParam;
    UIParamEnum                     m_CoordTypeParam;
    UIParamBool                     m_UseLookupMapForTrigParam;
    UIParamBool                     m_UseBiasForSignParam;
    UIParamBool                     m_BuiltInZParam;
    UIParamBool                     m_RenormalizeTexelParam;
    UIParamBool                     m_RenormalizeInterpolantsParam;
    UIParamBool                     m_RenormalizeResultParam;
    UIParamBool                     m_SpecializeShadersParam;

    // The Params as the Menu sees them
    UIParam*                        m_UIParamArray[UI_PARAM_COUNT];

    // The Params controlled by presets
    UIParamEnum*                    m_PresetParamArray[PRESET_PARAM_COUNT];

    // Encapsulates the normal map data
    RawAndCompressedNormalBuffer           m_NormalBuffer;

    // Resources for test geometry
    IDirect3DVertexDeclaration9*    m_pVertexDecl;
    IDirect3DVertexBuffer9*         m_pVB[TEST_GEOMETRY_COUNT];
    IDirect3DIndexBuffer9*          m_pIB[TEST_GEOMETRY_COUNT];
    UINT                            m_iIndexCount[TEST_GEOMETRY_COUNT];

    // Additional texture resources
    IDirect3DCubeTexture9*          m_pEnvironmentMap;
    IDirect3DLineTexture9*          m_pSinCosLookupMapNegPiToPi;
    IDirect3DLineTexture9*          m_pSinCosLookupMapZeroToPi;
    IDirect3DLineTexture9*          m_pSinCosLookupMapZeroToHalfPi;

    // Shaders 
    IDirect3DVertexShader9*         m_pVertexShader;
    IDirect3DPixelShader9*          m_pRawPixelShader;
    IDirect3DPixelShader9*          m_pCompressedPixelShader;

    // Performance data
    D3DPerfCounters*                m_pPerfCounterStart[3];
    D3DPerfCounters*                m_pPerfCounterEnd[3];
    DWORD                           m_dwFrameCount;
    const static DWORD              m_dwGpuCyclesPerMs = GPU_CLOCK_SPEED / 1000;   // 500 MHz
    XGIDEALSHADERCOST               m_ShaderCost;

    // Create test geometry
    VOID GenerateGeometryQuad( D3DVertexBuffer** pVB, D3DIndexBuffer** pIB, UINT* numIndices );
    VOID GenerateHemisphereVertices( UINT dwNumSlices, UINT dwNumStacks,
        TestGeometryVertex* pData );
    VOID GenerateHemisphereIndices( UINT dwNumSlices, UINT dwNumStacks, WORD* pIndices );
    VOID GenerateGeometryHemisphere( UINT numSlices, UINT numStacks, D3DVertexBuffer** pVB,
                                 D3DIndexBuffer** pIB, UINT* numIndices );

    // Create lookup texture for sin/cos
    VOID GenerateSinCosLookupMaps();

    // Simulated "offline" computation of texture data
    VOID ComputeRawNormalsHemisphereBump();
    VOID ComputeRawNormalsCorrugations();
    VOID ComputeRawNormalsEggCarton();
    VOID ComputeRawNormalsCrossedCracks();
    VOID ComputeRawNormalsNoise();
    VOID ComputeRawNormals( UINT iTestTexture );

    // Compress texture data
    struct TextureDims
    {
        UINT    iBitsPerTexel;
        UINT    iWidthInBlocks;
        UINT    iHeightInBlocks;
        UINT    iBlockSizeInBytes;
        UINT    iPitchInBlocks;
        UINT    iPitchInTexels;
        UINT    iPitchInBytes;
    };
    GPUTEXTUREFORMAT GetGPUFormat( UINT iFormatType, UINT iFilterType );
    BOOL CanHaveBuiltInZ( UINT iFormatType );
    BOOL RequiresSign( UINT iCoordType );
    BOOL IsNativelySigned( UINT iFormatType );
    BOOL IsFilterable( UINT iFormatType );
    D3DFORMAT BuildCompressionFormat( UINT iFormatType );
    D3DFORMAT BuildSamplingFormat( UINT iFormatType, UINT iCoordType, BOOL bUseBiasForSign, 
        UINT iFilterType );
    VOID GetTextureDims( UINT iFormatType, UINT iWidth, UINT iHeight, TextureDims* pTextureDims );
    FLOAT EncodeSignedValueAsUnsigned( FLOAT fValue, BOOL bUseBiasForSign );
    VOID ComputeCompressedData( UINT iFormatType, UINT iCoordType, BOOL bBuiltInZ, 
        BOOL bUseBiasForSign, UINT iFilterType );

    // Produce the D3D textures
    VOID GenerateRawTexture( BOOL bFirstUpdate );
    VOID GenerateCompressedTexture( BOOL bFirstUpdate );

    // Compile shaders for selected UI options
    HRESULT CompileShaders( BOOL bSpecialize );
    VOID GetPSConditionalsBool( BOOL (&pbPSConditionals)[PS_CONDITIONAL_BOOL_COUNT], BOOL bRaw );
    VOID GetPSConditionalsInt( UINT (&piPSConditionals)[PS_CONDITIONAL_INT_COUNT], BOOL bRaw );
    VOID GetPSConditionalsIntAsFloatVector( XMVECTOR (&pvPSConditionals)[PS_CONDITIONAL_INT_COUNT], BOOL bRaw );
    VOID PopulateMacros( D3DXMACRO (&pd3dMacros)[PS_CONDITIONAL_COUNT + 1], BOOL bRaw );

    // Performance analysis
    VOID PerfCounterInit();
    VOID PerfCounterDebugRender();
    UINT CalculateIdealTextureCost();
    VOID StaticAnalysisDebugRender();

    // Draw/Resolve helpers
    VOID DrawGeometryToRenderTarget(IDirect3DTexture9* pSrcTexture,
        UINT iFilterType, 
        IDirect3DPixelShader9* pPixelShader, 
        IDirect3DVertexBuffer9* pVB, 
        IDirect3DIndexBuffer9* pIB, 
        UINT iIBSize);
    VOID RenderUI();

public:
    Sample();

    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//-----------------------------------------------------------------------------
// Name: Sample::Sample
// Desc: Initializes the UI elements.
//-----------------------------------------------------------------------------
Sample::Sample()
: m_PresetParam( L"PRESET: ------------------------ ", g_strPresetNames, _countof(g_strPresetNames) )
, m_FormatTypeParam( L"Format", g_strFormatTypeNames, _countof(g_strFormatTypeNames) )
, m_FilterTypeParam( L"Filtering", g_strFilterTypeNames, _countof(g_strFilterTypeNames) )
, m_TextureDimensionsParam( L"Texture Dimensions", g_strTextureDimensionsNames, 
                           _countof(g_strTextureDimensionsNames), TEXTURE_DIMENSIONS_256x256 )
, m_TestTextureParam( L"Texture", g_strTestTextureNames, _countof(g_strTestTextureNames) )
, m_TestGeometryParam( L"Geometry", g_strTestGeometryNames, _countof(g_strTestGeometryNames) )
, m_AmbientParam( L"Enable ambient", TRUE )
, m_DiffuseParam( L"Enable diffuse", TRUE )
, m_SpecularParam( L"Enable specular", TRUE )
, m_EnvironmentTypeParam( L"Environment mapping", g_strEnvironmentTypeNames, 
                     _countof(g_strEnvironmentTypeNames), ENVIRONMENT_TYPE_SPECULAR )
, m_RenderNormalsParam( L"Render Normals", FALSE )
, m_CoordTypeParam( L"Coord system", g_strCoordTypeNames, _countof(g_strCoordTypeNames) )
, m_UseLookupMapForTrigParam( L"Use lookup map for trig", TRUE )
, m_UseBiasForSignParam( L"Use bias for sign", TRUE )
, m_BuiltInZParam( L"Encode Z in spare channel", FALSE )
, m_RenormalizeTexelParam( L"Renormalize Texel", FALSE )
, m_RenormalizeInterpolantsParam( L"Renormalize Interpolants", FALSE )
, m_RenormalizeResultParam( L"Renormalize Result", FALSE )
, m_SpecializeShadersParam( L"Specialize shaders", TRUE )
{
    m_UIParamArray[UI_PARAM_PRESET]                             = &m_PresetParam;
    m_UIParamArray[UI_PARAM_FORMAT_TYPE]                        = &m_FormatTypeParam;
    m_UIParamArray[UI_PARAM_FILTER_TYPE]                        = &m_FilterTypeParam;
    m_UIParamArray[UI_PARAM_TEXTURE_DIMENSIONS]                 = &m_TextureDimensionsParam;
    m_UIParamArray[UI_PARAM_TEST_TEXTURE]                       = &m_TestTextureParam;
    m_UIParamArray[UI_PARAM_TEST_GEOMETRY]                      = &m_TestGeometryParam;
    m_UIParamArray[UI_PARAM_AMBIENT]                            = &m_AmbientParam;
    m_UIParamArray[UI_PARAM_DIFFUSE]                            = &m_DiffuseParam;
    m_UIParamArray[UI_PARAM_SPECULAR]                           = &m_SpecularParam;
    m_UIParamArray[UI_PARAM_ENVIRONMENT]                        = &m_EnvironmentTypeParam;
    m_UIParamArray[UI_PARAM_RENDER_NORMALS]                     = &m_RenderNormalsParam;
    m_UIParamArray[UI_PARAM_COORD_TYPE]                         = &m_CoordTypeParam;
    m_UIParamArray[UI_PARAM_USE_LOOKUP_MAP_FOR_TRIG]            = &m_UseLookupMapForTrigParam;
    m_UIParamArray[UI_PARAM_USE_BIAS_FOR_SIGN]                  = &m_UseBiasForSignParam;
    m_UIParamArray[UI_PARAM_BUILTIN_Z]                          = &m_BuiltInZParam;
    m_UIParamArray[UI_PARAM_RENORMALIZE_TEXEL]                  = &m_RenormalizeTexelParam;
    m_UIParamArray[UI_PARAM_RENORMALIZE_INTERPOLANTS]           = &m_RenormalizeInterpolantsParam;
    m_UIParamArray[UI_PARAM_RENORMALIZE_RESULT]                 = &m_RenormalizeResultParam;
    m_UIParamArray[UI_PARAM_SPECIALIZE_SHADERS]                 = &m_SpecializeShadersParam;

    m_PresetParamArray[PRESET_PARAM_FORMAT_TYPE]                = &m_FormatTypeParam;
    m_PresetParamArray[PRESET_PARAM_COORD_TYPE]                 = &m_CoordTypeParam;
    m_PresetParamArray[PRESET_PARAM_USE_LOOKUP_MAP_FOR_TRIG]    = &m_UseLookupMapForTrigParam;
    m_PresetParamArray[PRESET_PARAM_USE_BIAS_FOR_SIGN]          = &m_UseBiasForSignParam;
    m_PresetParamArray[PRESET_PARAM_BUILTIN_Z]                  = &m_BuiltInZParam;
    m_PresetParamArray[PRESET_PARAM_RENORMALIZE_TEXEL]          = &m_RenormalizeTexelParam;
}


//--------------------------------------------------------------------------------------
// Name: ComputeRawNormalsHemisphereBump()
// Desc: Creates a normal map which is a spherical bump on a flat plane
//--------------------------------------------------------------------------------------
VOID Sample::ComputeRawNormalsHemisphereBump()
{
    RawAndCompressedNormalBuffer& NormalBuffer = m_NormalBuffer;
    const UINT iWidth = NormalBuffer.m_iTextureWidth;
    const UINT iHeight = NormalBuffer.m_iTextureHeight;

    // Fill in sample texture data
    GenericRawNormalBuffer& RawData = NormalBuffer.m_pRawData;

    for( UINT y = 0; y < iHeight; ++y )
    {
        for( UINT x = 0; x < iWidth; ++x )
        {
            XMFLOAT4& RawNormal = RawData.m_pXMFLOAT4[ y * iWidth + x ];
            FLOAT fRadius = (FLOAT) iWidth / 2;

            RawNormal.x = ((FLOAT) x ) / fRadius - 1.0f;
            RawNormal.y = ((FLOAT) y ) / fRadius - 1.0f;
            FLOAT fZSqr = 1.0f - Squared( RawNormal.x ) - Squared( RawNormal.y );
            if( fZSqr >= 0.0f )
            {
                RawNormal.z = sqrtf( fZSqr );
            }
            else
            {
                RawNormal.x = 0.0f;
                RawNormal.y = 0.0f;
                RawNormal.z = 1.0f;
            }
            RawNormal.w = 0.0f;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeRawNormalsCorrugations()
// Desc: Creates a normal map which is waves in the x-direction
//--------------------------------------------------------------------------------------
VOID Sample::ComputeRawNormalsCorrugations()
{
    RawAndCompressedNormalBuffer& NormalBuffer = m_NormalBuffer;
    const UINT iWidth = NormalBuffer.m_iTextureWidth;
    const UINT iHeight = NormalBuffer.m_iTextureHeight;

    const UINT iCycleCount = 4;
    const FLOAT fStrength = 0.4f;

    // Fill in sample texture data
    GenericRawNormalBuffer& RawData = NormalBuffer.m_pRawData;

    for( UINT y = 0; y < iHeight; ++y )
    {
        for( UINT x = 0; x < iWidth; ++x )
        {
            XMFLOAT4& RawNormal = RawData.m_pXMFLOAT4[ y * iWidth + x ];

            FLOAT fDeflection = fStrength * sinf( x * iCycleCount * XM_2PI / (FLOAT) iWidth );
            FLOAT fNorm = sqrt( 1.0f + Squared( fDeflection ) );

            RawNormal.x = fDeflection / fNorm;
            RawNormal.y = 0.0f;
            RawNormal.z = 1.0f / fNorm;
            RawNormal.w = 0.0f;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeRawNormalsEggCarton()
// Desc: Creates a normal map which is waves in both directions
//--------------------------------------------------------------------------------------
VOID Sample::ComputeRawNormalsEggCarton()
{
    RawAndCompressedNormalBuffer& NormalBuffer = m_NormalBuffer;
    const UINT iWidth = NormalBuffer.m_iTextureWidth;
    const UINT iHeight = NormalBuffer.m_iTextureHeight;

    const UINT iCycleCount = 2;
    const FLOAT fStrength = 0.2f;

    // Fill in sample texture data
    GenericRawNormalBuffer& RawData = NormalBuffer.m_pRawData;

    for( UINT y = 0; y < iHeight; ++y )
    {
        for( UINT x = 0; x < iWidth; ++x )
        {
            XMFLOAT4& RawNormal = RawData.m_pXMFLOAT4[ y * iWidth + x ];

            FLOAT fDeflectionX = fStrength * sinf( x * iCycleCount * XM_2PI / (FLOAT) iWidth );
            FLOAT fDeflectionY = fStrength * sinf( y * iCycleCount * XM_2PI / (FLOAT) iHeight );
            FLOAT fNorm = sqrt( 1.0f + Squared( fDeflectionX ) + Squared( fDeflectionY ) );

            RawNormal.x = fDeflectionX / fNorm;
            RawNormal.y = fDeflectionY / fNorm;
            RawNormal.z = 1.0f / fNorm;
            RawNormal.w = 0.0f;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeRawNormalsCrossedCracks()
// Desc: Creates a normal map consisting of thin vertical and horizontal cracks
//--------------------------------------------------------------------------------------
VOID Sample::ComputeRawNormalsCrossedCracks()
{
    RawAndCompressedNormalBuffer& NormalBuffer = m_NormalBuffer;
    const UINT iWidth = NormalBuffer.m_iTextureWidth;
    const UINT iHeight = NormalBuffer.m_iTextureHeight;
    const UINT iNumCracks = 4;
    const INT iCrackSeparationX = iWidth / iNumCracks;
    const INT iCrackSeparationY = iHeight / iNumCracks;
    const FLOAT iCrackHalfThicknessX = 0.02f * iWidth;
    const FLOAT iCrackHalfThicknessY = 0.02f * iHeight;
    const FLOAT fStrength = 1.0f;

    // Fill in sample texture data
    GenericRawNormalBuffer& RawData = NormalBuffer.m_pRawData;

    for( UINT y = 0; y < iHeight; ++y )
    {
        for( UINT x = 0; x < iWidth; ++x )
        {
            XMFLOAT4& RawNormal = RawData.m_pXMFLOAT4[ y * iWidth + x ];

            // 0 = crack center, +/-iCrackSeparation/2 = halfway between cracks
            INT iCrackUnitX = ( x % iCrackSeparationX ) - iCrackSeparationX / 2;
            INT iCrackUnitY = ( y % iCrackSeparationY ) - iCrackSeparationY / 2;

            // 0.0f = crack center, +/-1.0f = crack edge
            FLOAT fCrackUnitX = Min( Max( iCrackUnitX / (FLOAT) iCrackHalfThicknessX, -1.0f ), 1.0f );
            FLOAT fCrackUnitY = Min( Max( iCrackUnitY / (FLOAT) iCrackHalfThicknessY, -1.0f ), 1.0f );
            
            FLOAT fDeflectionX = -fStrength * sinf( fCrackUnitX * XM_PI );
            FLOAT fDeflectionY = fStrength * sinf( fCrackUnitY * XM_PI );
            FLOAT fNorm = sqrt( 1.0f + Squared( fDeflectionX ) + Squared( fDeflectionY ) );

            RawNormal.x = fDeflectionX / fNorm;
            RawNormal.y = fDeflectionY / fNorm;
            RawNormal.z = 1.0f / fNorm;
            RawNormal.w = 0.0f;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeRawNormalsNoise()
// Desc: Creates a normal map consisting of pseudo-random noise
//--------------------------------------------------------------------------------------
VOID Sample::ComputeRawNormalsNoise()
{
    RawAndCompressedNormalBuffer& NormalBuffer = m_NormalBuffer;
    const UINT iWidth = NormalBuffer.m_iTextureWidth;
    const UINT iHeight = NormalBuffer.m_iTextureHeight;

    XMFLOAT4* NoiseBuffer = new XMFLOAT4[iWidth*iHeight];
    FLOAT fStrength = 0.5f;

    srand(0);   // Don't change the output if we are only changing format or something.

    // Generate random noise
    for( UINT y = 0; y < iHeight; ++y )
    {
        for( UINT x = 0; x < iWidth; ++x )
        {
            XMFLOAT4& vNoise = NoiseBuffer[y * iWidth + x];

            vNoise.x = rand() / (FLOAT) RAND_MAX * 0.7f * fStrength;
            vNoise.y = rand() / (FLOAT) RAND_MAX * 0.7f * fStrength;

            vNoise.z = 0.0f;
            vNoise.w = 0.0f;
        }
    }

    // Fill in sample texture data
    GenericRawNormalBuffer& RawData = NormalBuffer.m_pRawData;

    // Blur noise so it looks like a physical surface
    for( UINT y = 0; y < iHeight; ++y )
    {
        for( UINT x = 0; x < iWidth; ++x )
        {
            XMFLOAT4& RawNormal = RawData.m_pXMFLOAT4[y * iWidth + x];

            RawNormal.x = 0.0f;
            RawNormal.y = 0.0f;

            // This is super-slow and could be improved
            const INT iBlur = 2;
            for( INT i = -iBlur; i <= iBlur; ++i )
            {
                for( INT j = -iBlur; j <= iBlur; ++j )
                {
                    INT xWrapped = ( x + i ) % iWidth;
                    INT yWrapped = ( y + j ) % iHeight;
                    const XMFLOAT4& vNoise = NoiseBuffer[yWrapped * iWidth + xWrapped];

                    RawNormal.x += vNoise.x;
                    RawNormal.y += vNoise.y;
                }
            }

            RawNormal.x /= Squared( 2 * iBlur + 1 );
            RawNormal.y /= Squared( 2 * iBlur + 1 );

            RawNormal.z = sqrtf( 1.0f - Squared( RawNormal.x ) - Squared( RawNormal.y ) );
            RawNormal.w = 0.0f;
        }
    }

    delete [] NoiseBuffer;
}


VOID Sample::ComputeRawNormals( UINT iTestTexture )
{
    switch( iTestTexture )
    {
    case TEST_TEXTURE_HEMISPHERE_BUMP:
        ComputeRawNormalsHemisphereBump();
        break;
    case TEST_TEXTURE_CORRUGATIONS:
        ComputeRawNormalsCorrugations();
        break;
    case TEST_TEXTURE_EGG_CARTON:
        ComputeRawNormalsEggCarton();
        break;
    case TEST_TEXTURE_CROSSED_CRACKS:
        ComputeRawNormalsCrossedCracks();
        break;
    case TEST_TEXTURE_NOISE:
        ComputeRawNormalsNoise();
        break;
    }
}


//--------------------------------------------------------------------------------------
// Name: BuildCompressionFormat()
// Desc: Builds some format compatible with XGCompressSurface for block-compressed formats.
// This format is simply used as an argument to this API.  It's not the actual format. 
// (Sometimes the actual format uses features incompatible with XGCompressSurface.)
//--------------------------------------------------------------------------------------
D3DFORMAT Sample::BuildCompressionFormat( UINT iFormatType )
{
    switch( iFormatType )
    {
    case FORMAT_TYPE_32_32_FLOAT: 
        return D3DFMT_LIN_G32R32F;
        break;
    case FORMAT_TYPE_16_16_FLOAT: 
        return D3DFMT_LIN_G16R16F;
        break;
    case FORMAT_TYPE_16_16_FIXED: 
        return D3DFMT_LIN_V16U16;
        break;
    case FORMAT_TYPE_11_11_10_FIXED: 
        return D3DFMT_LIN_W10V11U11;
        break;
    case FORMAT_TYPE_8_8_FIXED: 
        return D3DFMT_LIN_V8U8;
        break;
    case FORMAT_TYPE_5_6_5_FIXED: 
        return D3DFMT_R5G6B5;
        break;
    case FORMAT_TYPE_DXN: 
        return D3DFMT_LIN_DXN;
        break;
    case FORMAT_TYPE_DXT5: 
        return D3DFMT_LIN_DXT5;
        break;
    case FORMAT_TYPE_CTX1: 
        return D3DFMT_LIN_CTX1;
        break;
    case FORMAT_TYPE_DXT1: 
        return D3DFMT_LIN_DXT1;
        break;
    default:
        assert( false );
        return D3DFMT_UNKNOWN;
        break;
    }
}


//--------------------------------------------------------------------------------------
// Name: CanHaveBuiltInZ()
// Desc: Does this format have a third channel?
//--------------------------------------------------------------------------------------
BOOL Sample::CanHaveBuiltInZ( UINT iFormatType )
{
    switch( iFormatType )
    {
    case FORMAT_TYPE_32_32_FLOAT: 
    case FORMAT_TYPE_16_16_FLOAT: 
    case FORMAT_TYPE_16_16_FIXED: 
    case FORMAT_TYPE_8_8_FIXED: 
    case FORMAT_TYPE_DXN: 
    case FORMAT_TYPE_CTX1: 
        return FALSE;
        break;
    case FORMAT_TYPE_11_11_10_FIXED: 
    case FORMAT_TYPE_5_6_5_FIXED: 
    case FORMAT_TYPE_DXT5: 
    case FORMAT_TYPE_DXT1: 
        return TRUE;
        break;
    default:
        assert( false );
        return FALSE;
        break;
    }
}


//--------------------------------------------------------------------------------------
// Name: RequiresSign()
// Desc: Does this coordinate encoding require signed components?  We avoid signed 
// angles for simplicity, although they would speed up the shader implementation of
// cylindrical/spherical coords slightly.
//--------------------------------------------------------------------------------------
BOOL Sample::RequiresSign( UINT iCoordType )
{
    switch( iCoordType )
    {
    case COORD_TYPE_RECTANGULAR:
    default:
        return TRUE;
        break;

    case COORD_TYPE_CYLINDRICAL:
    case COORD_TYPE_SPHERICAL:
    case COORD_TYPE_PSEUDOSPHERICAL:
        return FALSE;
        break;
    }
}


//--------------------------------------------------------------------------------------
// Name: IsNativelySigned()
// Desc: Can this format support GPUSIGN_SIGNED sensibly.  All formats can do so
// except block-compressed formats, because the intra-block interpolation is natively
// unsigned.  Those formats can use GPUSIGN_BIAS instead.
//--------------------------------------------------------------------------------------
BOOL Sample::IsNativelySigned( UINT iFormatType )
{
    switch( iFormatType )
    {
    case FORMAT_TYPE_32_32_FLOAT:
    case FORMAT_TYPE_16_16_FLOAT:
    case FORMAT_TYPE_16_16_FIXED:
    case FORMAT_TYPE_11_11_10_FIXED:
    case FORMAT_TYPE_8_8_FIXED:
    case FORMAT_TYPE_5_6_5_FIXED:
    default:
        return TRUE;

    case FORMAT_TYPE_DXN:
    case FORMAT_TYPE_DXT5:
    case FORMAT_TYPE_CTX1:
    case FORMAT_TYPE_DXT1:
        return FALSE;
    }
}


//--------------------------------------------------------------------------------------
// Name: IsFilterable()
// Desc: Can this format be filtered?  Only fixed-point formats are filterable, or
// also formats which expand to fixed-point.
//--------------------------------------------------------------------------------------
BOOL Sample::IsFilterable( UINT iFormatType )
{
    switch( iFormatType )
    {
    case FORMAT_TYPE_16_16_FLOAT:
    case FORMAT_TYPE_16_16_FIXED:
    case FORMAT_TYPE_11_11_10_FIXED:
    case FORMAT_TYPE_8_8_FIXED:
    case FORMAT_TYPE_5_6_5_FIXED:
    case FORMAT_TYPE_DXN:
    case FORMAT_TYPE_DXT5:
    case FORMAT_TYPE_CTX1:
    case FORMAT_TYPE_DXT1:
    default:
        return TRUE;

    case FORMAT_TYPE_32_32_FLOAT:
        return FALSE;
    }
}


//--------------------------------------------------------------------------------------
// Name: GetGPUFormat()
// Desc: What is the natural GPUTEXTUREFORMAT for the given iFormatType?
//--------------------------------------------------------------------------------------
GPUTEXTUREFORMAT Sample::GetGPUFormat( UINT iFormatType, UINT iFilterType )
{
    switch( iFormatType )
    {
    case FORMAT_TYPE_32_32_FLOAT: 
    default:
        return GPUTEXTUREFORMAT_32_32_FLOAT;
        break;
    case FORMAT_TYPE_16_16_FLOAT: 
        switch( iFilterType )
        {
        case FILTER_TYPE_POINT:
            return GPUTEXTUREFORMAT_16_16_FLOAT;
            break;
        case FILTER_TYPE_BILINEAR:
        default:
            return GPUTEXTUREFORMAT_16_16_EXPAND;
            break;
        }
        break;
    case FORMAT_TYPE_16_16_FIXED: 
        return GPUTEXTUREFORMAT_16_16;
        break;
    case FORMAT_TYPE_11_11_10_FIXED:
        switch( iFilterType )
        {
        case FILTER_TYPE_POINT:
            return GPUTEXTUREFORMAT_10_11_11;
            break;
        case FILTER_TYPE_BILINEAR:
        default:
            return GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16;
            break;
        }
        break;
    case FORMAT_TYPE_8_8_FIXED: 
        return GPUTEXTUREFORMAT_8_8; 
        break;
    case FORMAT_TYPE_5_6_5_FIXED: 
        return GPUTEXTUREFORMAT_5_6_5; 
        break;
    case FORMAT_TYPE_DXN: 
        return GPUTEXTUREFORMAT_DXN;  
        break;
    case FORMAT_TYPE_DXT5: 
        return GPUTEXTUREFORMAT_DXT4_5;
        break;
    case FORMAT_TYPE_CTX1: 
        return GPUTEXTUREFORMAT_CTX1;  
        break;
    case FORMAT_TYPE_DXT1: 
        return GPUTEXTUREFORMAT_DXT1;
        break;
    }
}

//--------------------------------------------------------------------------------------
// Name: BuildSamplingFormat()
// Desc: Builds a full D3DFORMAT for sampling.  This format will actually be 
// associated with the texture (as opposed to BuildCompressionFormat).
//--------------------------------------------------------------------------------------
D3DFORMAT Sample::BuildSamplingFormat( UINT iFormatType, UINT iCoordType, BOOL bUseBiasForSign, 
                                      UINT iFilterType )
{
    GPUTEXTUREFORMAT dwGPUTextureFormat = GetGPUFormat( iFormatType, iFilterType );
    GPUENDIAN dwGPUEndian;
    GPUSIGN dwGPUSignX, dwGPUSignY, dwGPUSignZ, dwGPUSignW;
    GPUSWIZZLE dwGPUSwizzleX, dwGPUSwizzleY, dwGPUSwizzleZ, dwGPUSwizzleW;
    GPUNUMFORMAT dwGPUNumFormat;

    switch( dwGPUTextureFormat )
    {
    case GPUTEXTUREFORMAT_32_32_FLOAT:
    case GPUTEXTUREFORMAT_32_32_32_FLOAT:
    case GPUTEXTUREFORMAT_10_11_11:
    case GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16:
    default:
        dwGPUEndian = GPUENDIAN_8IN32;
        break;

    // These are naturally GPUENDIAN_8IN32, but for compatibility with XGCompressSurface,
    // we make them GPUENDIAN_8IN16.
    case GPUTEXTUREFORMAT_DXN:
    case GPUTEXTUREFORMAT_DXT4_5:
    case GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16:
    case GPUTEXTUREFORMAT_CTX1:
    case GPUTEXTUREFORMAT_DXT1:
    case GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16:
        dwGPUEndian = GPUENDIAN_8IN16;
        break;

    case GPUTEXTUREFORMAT_5_6_5:
    case GPUTEXTUREFORMAT_16_16:
    case GPUTEXTUREFORMAT_16_16_FLOAT:
    case GPUTEXTUREFORMAT_16_16_EXPAND:
        dwGPUEndian = GPUENDIAN_8IN16;
        break;

    case GPUTEXTUREFORMAT_8_8:
        dwGPUEndian = GPUENDIAN_NONE;
        break;
    }

    BOOL bIsNativelySigned = IsNativelySigned( iFormatType );
    BOOL bRequiresSign = RequiresSign( iCoordType );

    if( bIsNativelySigned )
    {
        dwGPUSignX = bRequiresSign ? GPUSIGN_SIGNED : GPUSIGN_UNSIGNED;
        dwGPUSignY = bRequiresSign ? GPUSIGN_SIGNED : GPUSIGN_UNSIGNED;
    }
    else if( bUseBiasForSign )
    {
        dwGPUSignX = bRequiresSign ? GPUSIGN_BIAS : GPUSIGN_UNSIGNED;
        dwGPUSignY = bRequiresSign ? GPUSIGN_BIAS : GPUSIGN_UNSIGNED;
    }
    else 
    {
        dwGPUSignX = GPUSIGN_UNSIGNED;
        dwGPUSignY = GPUSIGN_UNSIGNED;
    }
    dwGPUSignZ = GPUSIGN_UNSIGNED;
    dwGPUSignW = GPUSIGN_UNSIGNED;

    // For now, swizzle type is hard-coded for DXT5
    UINT iSwizzleType = SWIZZLE_TYPE_NONE;
    switch( iFormatType )
    {
    case FORMAT_TYPE_DXT5:
        iSwizzleType = SWIZZLE_TYPE_RGBA_ZX0Y;
        break;

    default:
        break;
    }

    switch( iSwizzleType )
    {
    case SWIZZLE_TYPE_RGBA_ZX0Y:
        {
            // swap according to swizzle
            GPUSIGN dwGPUSignXCopy = dwGPUSignX;
            GPUSIGN dwGPUSignYCopy = dwGPUSignY;
            GPUSIGN dwGPUSignZCopy = dwGPUSignZ;
            GPUSIGN dwGPUSignWCopy = dwGPUSignW;
            dwGPUSignX = dwGPUSignZCopy;   // Z channel of data goes into X of texture
            dwGPUSignY = dwGPUSignXCopy;   // X channel of data goes into Y of texture
            dwGPUSignZ = dwGPUSignWCopy;   // W channel of data goes into Z of texture
            dwGPUSignW = dwGPUSignYCopy;   // Y channel of data goes into W of texture
        }
        break;

    case SWIZZLE_TYPE_RGBA_ZY0X:
        {
            // swap according to swizzle
            GPUSIGN dwGPUSignXCopy = dwGPUSignX;
            GPUSIGN dwGPUSignYCopy = dwGPUSignY;
            GPUSIGN dwGPUSignZCopy = dwGPUSignZ;
            GPUSIGN dwGPUSignWCopy = dwGPUSignW;
            dwGPUSignX = dwGPUSignZCopy;   // Z channel of data goes into X of texture
            dwGPUSignY = dwGPUSignYCopy;   // Y channel of data goes into Y of texture
            dwGPUSignZ = dwGPUSignWCopy;   // W channel of data goes into Z of texture
            dwGPUSignW = dwGPUSignXCopy;   // X channel of data goes into W of texture
        }
        break;

    default:
        break;
    }

    switch( iSwizzleType )
    {
    case SWIZZLE_TYPE_RGBA_ZX0Y:
        dwGPUSwizzleX = GPUSWIZZLE_Y;   // X channel of data goes into Y of texture
        dwGPUSwizzleY = GPUSWIZZLE_W;   // Y channel of data goes into W of texture
        dwGPUSwizzleZ = GPUSWIZZLE_X;   // Z channel of data goes into X of texture
        dwGPUSwizzleW = GPUSWIZZLE_Z;   // W channel of data goes into Z of texture
        break;

    case SWIZZLE_TYPE_RGBA_ZY0X:
        dwGPUSwizzleX = GPUSWIZZLE_W;   // X channel of data goes into W of texture
        dwGPUSwizzleY = GPUSWIZZLE_Y;   // Y channel of data goes into Y of texture
        dwGPUSwizzleZ = GPUSWIZZLE_X;   // Z channel of data goes into X of texture
        dwGPUSwizzleW = GPUSWIZZLE_Z;   // W channel of data goes into Z of texture
        break;

    default:
        dwGPUSwizzleX = GPUSWIZZLE_X;
        dwGPUSwizzleY = GPUSWIZZLE_Y;
        dwGPUSwizzleZ = GPUSWIZZLE_Z;
        dwGPUSwizzleW = GPUSWIZZLE_W;
        break;
    }

    switch( dwGPUTextureFormat )
    {
        // Integer (or ExpBias) is required for EXPAND.
        // For all other float formats, the NumFormat is ignored
    case GPUTEXTUREFORMAT_16_16_EXPAND:
        dwGPUNumFormat = GPUNUMFORMAT_INTEGER;
        break;

    default:
        dwGPUNumFormat = GPUNUMFORMAT_FRACTION;
        break;
    }

    BOOL bTiled = TRUE; // always tile

    return (D3DFORMAT) MAKED3DFMT2(
        dwGPUTextureFormat,                     // GPUTEXTUREFORMAT 
        dwGPUEndian,                            // GPUENDIAN
        bTiled,                                 // BOOL (Tiled)
        dwGPUSignX,                             // GPUSIGN (x)
        dwGPUSignY,                             // GPUSIGN (y)
        dwGPUSignZ,                             // GPUSIGN (z)
        dwGPUSignW,                             // GPUSIGN (w)
        dwGPUNumFormat,                         // GPUNUMFORMAT
        dwGPUSwizzleX,                          // GPUSWIZZLE (x)
        dwGPUSwizzleY,                          // GPUSWIZZLE (y)
        dwGPUSwizzleZ,                          // GPUSWIZZLE (z)
        dwGPUSwizzleW                           // GPUSWIZZLE (w)
    ); 
}


//--------------------------------------------------------------------------------------
// Name: GetTextureDims()
// Desc: Computes the dimensions of the texture in several different senses,
// taking into account alignment requirements.  This works for 
// all the textures in the sample, but maybe not for arbitrary textures.
//--------------------------------------------------------------------------------------
VOID Sample::GetTextureDims( UINT iFormatType, UINT iWidth, UINT iHeight, TextureDims* pTextureDims )
{
    D3DFORMAT d3dCompressionFormat = BuildCompressionFormat( iFormatType );
    pTextureDims->iBitsPerTexel = XGBitsPerPixelFromFormat( d3dCompressionFormat );
    DWORD dwCompressionGPUFormat = XGGetGpuFormat( d3dCompressionFormat );

    UINT iBlockWidth, iBlockHeight;
    XGGetBlockDimensions( dwCompressionGPUFormat, &iBlockWidth, &iBlockHeight );
    pTextureDims->iBlockSizeInBytes = 
        ( iBlockWidth * iBlockHeight * pTextureDims->iBitsPerTexel ) / 8;
    pTextureDims->iWidthInBlocks = XGNextMultiple( iWidth, iBlockWidth ) / iBlockWidth;
    pTextureDims->iHeightInBlocks = XGNextMultiple( iHeight, iBlockHeight ) / iBlockHeight;
    pTextureDims->iPitchInBlocks = 
        XGNextMultiple( pTextureDims->iWidthInBlocks, GPU_TEXTURE_TILE_DIMENSION );
    pTextureDims->iPitchInTexels = pTextureDims->iPitchInBlocks
        * iBlockWidth * iBlockHeight;
    pTextureDims->iPitchInBytes = 
        XGNextMultiple( ( pTextureDims->iPitchInTexels * pTextureDims->iBitsPerTexel ) / 8, 
        GPU_LINEAR_TEXTURE_PITCH_BYTE_ALIGNMENT );

    // Other pitches may need to be re-evaluated now
    pTextureDims->iPitchInBlocks = pTextureDims->iPitchInBytes 
        / pTextureDims->iBlockSizeInBytes;
    pTextureDims->iPitchInTexels = pTextureDims->iPitchInBlocks
        * iBlockWidth * iBlockHeight;
}

//--------------------------------------------------------------------------------------
// The following functions are XNAMath-type store routines, but support custom
// formats not present in XNAMath
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: XMStoreS11S11U10N3()
// Desc: Copied from XNAMath functions XMStoreHenDN3 and XMStoreUHenDN3.  These are 
//          for signed and unsigned components respectively.  We want XY to be signed
//          11-bit numbers and Z to be an unsigned 10-bit number.
//
//          This function assumes V.z >= 0.0f because that requires only a trivial
//          change from XMStoreHenDN3.
//--------------------------------------------------------------------------------------
XMFINLINE VOID XMStoreS11S11U10N3
(
    XMS11S11U10N3* pDestination, 
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    #error We didn't implement this case
#elif defined(_XM_SSE_INTRINSICS_)
#else // _XM_VMX128_INTRINSICS_
    XMVECTOR               Convert;
    XMVECTOR               Select;
    XMVECTOR               Scale;
    XMVECTOR               X, Y, Z, XY;
    XMVECTOR               S;
    static CONST __vector4i C0 = {0x447FC000, 0x0007FF0B, 0x3FFFFF16, 0x43FF8000}; // (FLOAT)((1 << (11 - 1)) - 1), ..., (FLOAT)((1 << (10 - 1)) - 1)

    XMASSERT(pDestination);
    XMASSERT(((UINT_PTR)pDestination & 3) == 0);

    XMASSERT(V.z >= 0.0f);
    Scale = __vspltw(*(XMVECTOR*)&C0, 0);

    Convert = __vmulfp(V, Scale);
    Select = __vspltisw(8);
    Convert = __vrfin(Convert);
    Select = __vsrw(*(XMVECTOR*)&C0, Select);
    Convert = __vctsxs(Convert, 0);
    XY = __vspltw(Select, 1);
    S = __vspltw(Select, 2);
    Convert = __vslw(Convert, *(XMVECTOR*)&C0);
    X = __vspltw(Convert, 0);
    Y = __vspltw(Convert, 1);
    Z = __vspltw(Convert, 2);
    XY = __vsel(Y, X, XY);
    S = __vsel(Z, XY, S);

    __stvewx(S, pDestination, 0);
#endif // _XM_VMX128_INTRINSICS_
}


//--------------------------------------------------------------------------------------
// Name: XMStoreS5S6U5N()
// Desc: Copied from XNAMath function XMStoreU565.  Supports similar format where 
// XY are signed while Z remains unsigned.  Also, XYZ are all normalized to [-1,1] or 
// [0,1] rather than being ints.
//--------------------------------------------------------------------------------------
XMFINLINE VOID XMStoreS5S6U5N
(
    XMU565* pDestination,
    FXMVECTOR V
)
{
#if defined(_XM_SSE_INTRINSICS_) && !defined(_XM_NO_INTRINSICS_)
    #error We didn't implement this case
#else
    XMVECTOR               N;
    static CONST XMVECTORF32  Min = {-15.0f, -31.0f, 0.0f, 0.0f};
    static CONST XMVECTORF32  Max = {15.0f, 31.0f, 31.0f, 0.0f};

    XMASSERT(pDestination);

    N = __vmulfp(V, Max);
    N = XMVectorClamp(N, Min.v, Max.v);
    N = XMVectorRound(N);

    pDestination->v = (((USHORT)N.vector4_f32[2] & 0x1F) << 11) |
                      (((USHORT)N.vector4_f32[1] & 0x3F) << 5) |
                      (((USHORT)N.vector4_f32[0] & 0x1F));
#endif !_XM_SSE_INTRINSICS_
}


//--------------------------------------------------------------------------------------
// Name: XMStoreS5S6U5N()
// Desc: Copied from XNAMath function XMStoreU565.  Supports similar format where 
// XYZ are all normalized to [0,1] rather than ints.
//--------------------------------------------------------------------------------------
XMFINLINE VOID XMStoreU565N
(
    XMU565* pDestination,
    FXMVECTOR V
)
{
#if defined(_XM_SSE_INTRINSICS_) && !defined(_XM_NO_INTRINSICS_)
    #error We didn't implement this case
#else
    XMVECTOR               N;
    static CONST XMVECTORF32  Max = {31.0f, 63.0f, 31.0f, 0.0f};

    XMASSERT(pDestination);

    N = __vmulfp(V, Max);
    N = XMVectorClamp(N, XMVectorZero(), Max.v);
    N = XMVectorRound(N);

    pDestination->v = (((USHORT)N.vector4_f32[2] & 0x1F) << 11) |
                      (((USHORT)N.vector4_f32[1] & 0x3F) << 5) |
                      (((USHORT)N.vector4_f32[0] & 0x1F));
#endif !_XM_SSE_INTRINSICS_
}


//--------------------------------------------------------------------------------------
// Name: EncodeSignedValueAsUnsigned()
// Desc: Change signed values in [-1,1] into unsigned values in [0,1] for use with
// either GPUSIGN_UNSIGNED or GPUSIGN_BIAS.
//--------------------------------------------------------------------------------------
FLOAT Sample::EncodeSignedValueAsUnsigned( FLOAT fValue, BOOL bUseBiasForSign )
{
    if( bUseBiasForSign )
    {
        // The commented-out formula is hardware-accurate, and will reproduce 0 exactly for
        // DXN. 
        //
        // However, for the hemisphere normal map, the exact transformation seems to 
        // produce numerical instability in the DXT compressor, making the reflection 
        // map appear jittery. 
        //
        // The simpler equation actually gives more pleasing results in this case.  
        // That may vary depending on texture dimensions and contents, so  I'm leaving 
        // both equations in for reference.
        
        //return ( 127.0f / 255.0f ) * fValue + ( 128.0f / 255.0f );

        return 0.5f * fValue + 0.5f;
    }
    else
    {
        return 0.5f * fValue + 0.5f;
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeCompressedData()
// Desc: Generate a compressed texture from the raw data.  During this calculation,
//          - we optionally scale/offset to unsigned range
//          - we optionally convert to different coordinates (e.g. spherical)
//          - we optionally swizzle components
//          - we quantize/compress to the correct GPUTEXTUREFORMAT, including DXT
//--------------------------------------------------------------------------------------
VOID Sample::ComputeCompressedData( UINT iFormatType, UINT iCoordType, BOOL bBuiltInZ, 
                                   BOOL bUseBiasForSign, UINT iFilterType )
{
    RawAndCompressedNormalBuffer& NormalBuffer = m_NormalBuffer;
    const UINT iWidth = NormalBuffer.m_iTextureWidth;
    const UINT iHeight = NormalBuffer.m_iTextureHeight;
    TextureDims TextureDims;
    GetTextureDims( iFormatType, iWidth, iHeight, &TextureDims );

    // Clone raw data so we can modify it temporarily
    GenericRawNormalBuffer RawTextureDataCopy;
    RawTextureDataCopy.m_pXMFLOAT4 = new XMFLOAT4[iHeight * iWidth];
    memcpy( RawTextureDataCopy.m_pVoid, NormalBuffer.m_pRawData.m_pVoid, 
        iHeight * iWidth * sizeof(XMFLOAT4) );

    // Destination buffer
    GenericCompressedNormalBuffer& CompressedTextureData = NormalBuffer.m_pCompressedData;

    // Optionally convert raw data into cylindrical or spherical coordinates
    switch( iCoordType )
    {
    case COORD_TYPE_RECTANGULAR:
    default:
        break;

    case COORD_TYPE_CYLINDRICAL:
        for( UINT j = 0; j < iHeight; ++j )
        {
            for( UINT i = 0; i < iWidth; ++i )
            {
                const XMFLOAT4 RawNormalRectangular = RawTextureDataCopy.m_pXMFLOAT4[j * iWidth + i];
                XMFLOAT4& RawNormalCylindrical = RawTextureDataCopy.m_pXMFLOAT4[j * iWidth + i];

                // Range [0,1] --- we could use built-in sign here with some additional work
                RawNormalCylindrical.x = EncodeSignedValueAsUnsigned( 
                    atan2f( RawNormalCylindrical.y, RawNormalCylindrical.x ) / XM_PI, FALSE );

                // Range [0,1]
                RawNormalCylindrical.y = 
                    sqrtf( Squared( RawNormalRectangular.x ) + Squared( RawNormalRectangular.y ) );

                // Range [0,1]
                RawNormalCylindrical.z = RawNormalRectangular.z;

                RawNormalCylindrical.w = 0.0f;
            }
        }
        break;

    case COORD_TYPE_SPHERICAL:
        for( UINT j = 0; j < iHeight; ++j )
        {
            for( UINT i = 0; i < iWidth; ++i )
            {
                const XMFLOAT4 RawNormalRectangular = RawTextureDataCopy.m_pXMFLOAT4[j * iWidth + i];
                XMFLOAT4& RawNormalSpherical = RawTextureDataCopy.m_pXMFLOAT4[j * iWidth + i];

                // Range [0,1] --- we could use built-in sign here with some additional work
                RawNormalSpherical.x = EncodeSignedValueAsUnsigned( 
                    atan2f( RawNormalRectangular.y, RawNormalRectangular.x ) / XM_PI, FALSE );

                // Range [0,1]
                RawNormalSpherical.y = acosf( RawNormalRectangular.z ) / XM_PIDIV2;

                RawNormalSpherical.z = 0.0f;
                RawNormalSpherical.w = 0.0f;
            }
        }
        break;

    case COORD_TYPE_PSEUDOSPHERICAL:
        for( UINT j = 0; j < iHeight; ++j )
        {
            for( UINT i = 0; i < iWidth; ++i )
            {
                const XMFLOAT4 RawNormalRectangular = RawTextureDataCopy.m_pXMFLOAT4[j * iWidth + i];
                XMFLOAT4& RawNormalSpherical = RawTextureDataCopy.m_pXMFLOAT4[j * iWidth + i];

                // Range [0,1]
                RawNormalSpherical.x = atan2f( RawNormalRectangular.z, RawNormalRectangular.x ) / XM_PI;

                // Range [0,1]
                RawNormalSpherical.y = acosf( RawNormalRectangular.y ) / XM_PI;

                RawNormalSpherical.z = 0.0f;
                RawNormalSpherical.w = 0.0f;
            }
        }
        break;
    }

    BOOL bIsFormatNativelySigned = IsNativelySigned( iFormatType );
    BOOL bRequiresSign = RequiresSign( iCoordType );

    // For now, swizzle type is hard-coded for DXT5
    UINT iSwizzleType = SWIZZLE_TYPE_NONE;
    switch( iFormatType )
    {
    case FORMAT_TYPE_DXT5:
        iSwizzleType = SWIZZLE_TYPE_RGBA_ZX0Y;
        break;

    default:
        break;
    }

    // DXT formats do not natively handle signed values.  Must convert to unsigned before 
    // compressing.  The precise conversion depends whether we are recovering signed values 
    // with shader operations, or by using the GPUSIGN_BIAS tag.
    for( UINT j = 0; j < iHeight; ++j )
    {
        for( UINT i = 0; i < iWidth; ++i )
        {
            const XMFLOAT4 RawNormalSigned = RawTextureDataCopy.m_pXMFLOAT4[j * iWidth + i];
            XMFLOAT4& RawNormalNative = RawTextureDataCopy.m_pXMFLOAT4[j * iWidth + i];

            if( bRequiresSign && !bIsFormatNativelySigned )
            {
                RawNormalNative.x = 
                    EncodeSignedValueAsUnsigned( RawNormalSigned.x, bUseBiasForSign );
            }
            else 
            {
                RawNormalNative.x = RawNormalSigned.x;
            }

            if( bRequiresSign && !bIsFormatNativelySigned )
            {
                RawNormalNative.y = 
                    EncodeSignedValueAsUnsigned( RawNormalSigned.y, bUseBiasForSign );
            }
            else 
            {
                RawNormalNative.y = RawNormalSigned.y;
            }

            if( bBuiltInZ )
            {
                RawNormalNative.z = RawNormalSigned.z;
            }
            else
            {
                RawNormalNative.z = 0.0f;
            }

            RawNormalNative.w = RawNormalSigned.w;

            switch( iSwizzleType )
            {
            case SWIZZLE_TYPE_RGBA_XYZ0:
            default:
                break;

            case SWIZZLE_TYPE_RGBA_ZX0Y:
                {
                    const XMFLOAT4 RawNormalUnswizzled = RawNormalNative;
                    XMFLOAT4& RawNormalSwizzled = RawNormalNative;

                    RawNormalSwizzled.x = RawNormalUnswizzled.z;    // Z of data goes into X of texture
                    RawNormalSwizzled.y = RawNormalUnswizzled.x;    // X of data goes into Y of texture
                    RawNormalSwizzled.z = 0.0f;                     // W of data goes into Z of texture
                    RawNormalSwizzled.w = RawNormalUnswizzled.y;    // Y of data goes into W of texture
                }
                break;

            case SWIZZLE_TYPE_RGBA_ZY0X:
                {
                    const XMFLOAT4 RawNormalUnswizzled = RawNormalNative;
                    XMFLOAT4& RawNormalSwizzled = RawNormalNative;

                    RawNormalSwizzled.x = RawNormalUnswizzled.z;    // Z of data goes into X of texture
                    RawNormalSwizzled.y = RawNormalUnswizzled.y;    // Y of data goes into Y of texture
                    RawNormalSwizzled.z = 0.0f;                     // W of data goes into Z of texture
                    RawNormalSwizzled.w = RawNormalUnswizzled.x;    // X of data goes into W of texture
                }
                break;
            }
        }
    }

    switch( iFormatType )
    {
        // DXT compressed formats require a call to XGCompressSurface
    case FORMAT_TYPE_DXT1:
    case FORMAT_TYPE_DXT5:
    case FORMAT_TYPE_CTX1:
    case FORMAT_TYPE_DXN:
        {
            D3DFORMAT DstFormat = BuildCompressionFormat( iFormatType );
            XGCompressSurface( CompressedTextureData.m_pVoid, 
                TextureDims.iPitchInBytes, 
                iWidth, 
                iHeight, 
                DstFormat, 
                NULL, 
                RawTextureDataCopy.m_pVoid, 
                iWidth * sizeof(RawTextureDataCopy.m_pXMFLOAT4[0]), 
                D3DFMT_LIN_A32B32G32R32F, 
                NULL, 
                XGCOMPRESS_NO_DITHERING, 
                0.0f );
        }
        break;

        // non-DXT formats can be compressed manually
        // These use XNAMath, but aren't intended to be fast
    case FORMAT_TYPE_32_32_FLOAT:
        for( UINT j = 0; j < iHeight; ++j )
        {
            for( UINT i = 0; i < iWidth; ++i )
            {
                const XMVECTOR& NormalUncompressed = RawTextureDataCopy.m_pXMVECTOR[j * iWidth + i];
                XMFLOAT2& NormalCompressed = 
                    CompressedTextureData.m_pXMFLOAT2[j * TextureDims.iPitchInTexels + i];

                XMStoreFloat2( &NormalCompressed, NormalUncompressed );
            }
        }
        break;

    case FORMAT_TYPE_16_16_FLOAT:
        for( UINT j = 0; j < iHeight; ++j )
        {
            for( UINT i = 0; i < iWidth; ++i )
            {
                const XMVECTOR& NormalUncompressed = RawTextureDataCopy.m_pXMVECTOR[j * iWidth + i];
                XMHALF2& NormalCompressed = 
                    CompressedTextureData.m_pXMHALF2[j * TextureDims.iPitchInTexels + i];

                XMStoreHalf2( &NormalCompressed, NormalUncompressed );
            }
        }
        break;

    case FORMAT_TYPE_16_16_FIXED:
        for( UINT j = 0; j < iHeight; ++j )
        {
            for( UINT i = 0; i < iWidth; ++i )
            {
                const XMVECTOR& NormalUncompressed = RawTextureDataCopy.m_pXMVECTOR[j * iWidth + i];

                if( bRequiresSign )
                {
                    XMSHORTN2& NormalCompressed = 
                        CompressedTextureData.m_pXMSHORTN2[j * TextureDims.iPitchInTexels + i];
                    XMStoreShortN2( &NormalCompressed, NormalUncompressed );
                }
                else 
                {
                    XMUSHORTN2& NormalCompressed = 
                        CompressedTextureData.m_pXMUSHORTN2[j * TextureDims.iPitchInTexels + i];
                    XMStoreUShortN2( &NormalCompressed, NormalUncompressed );
                }
            }
        }
        break;

    case FORMAT_TYPE_11_11_10_FIXED:
        for( UINT j = 0; j < iHeight; ++j )
        {
            for( UINT i = 0; i < iWidth; ++i )
            {
                const XMVECTOR& NormalUncompressed = RawTextureDataCopy.m_pXMVECTOR[j * iWidth + i];

                if( bRequiresSign )
                {
                    XMS11S11U10N3& NormalCompressed = 
                        CompressedTextureData.m_pXMS11S11U10N3[j * TextureDims.iPitchInTexels + i];
                    XMStoreS11S11U10N3( &NormalCompressed, NormalUncompressed );
                }
                else 
                {
                    XMUHENDN3& NormalCompressed = 
                        CompressedTextureData.m_pXMUHENDN3[j * TextureDims.iPitchInTexels + i];
                    XMStoreUHenDN3( &NormalCompressed, NormalUncompressed );
                }
            }
        }
        break;

    case FORMAT_TYPE_8_8_FIXED:
        for( UINT j = 0; j < iHeight; ++j )
        {
            for( UINT i = 0; i < iWidth; i+=2 )
            {
                // XMStoreByteN2 is not VMX128 optimized, so pack two items at a time...
                XMVECTOR NormalsUncompressed = __vrlimi(RawTextureDataCopy.m_pXMVECTOR[j * iWidth + i], 
                    RawTextureDataCopy.m_pXMVECTOR[j * iWidth + i + 1], 0x3, 2);

                if( bRequiresSign )
                {
                    XMBYTEN4& NormalsCompressed = 
                        (XMBYTEN4&) CompressedTextureData.m_pXMBYTEN2[j * TextureDims.iPitchInTexels + i];
                    XMStoreByteN4( &NormalsCompressed, NormalsUncompressed );
                }
                else 
                {
                    XMUBYTEN4& NormalsCompressed = 
                        (XMUBYTEN4&) CompressedTextureData.m_pXMUBYTEN2[j * TextureDims.iPitchInTexels + i];
                    XMStoreUByteN4( &NormalsCompressed, NormalsUncompressed );
                }
            }
        }
        break;

    case FORMAT_TYPE_5_6_5_FIXED:
        for( UINT j = 0; j < iHeight; ++j )
        {
            for( UINT i = 0; i < iWidth; ++i )
            {
                XMVECTOR NormalUncompressed = RawTextureDataCopy.m_pXMVECTOR[j * iWidth + i];

                if( bRequiresSign )
                {
                    XMU565& NormalCompressed = 
                        CompressedTextureData.m_pXMU565[j * TextureDims.iPitchInTexels + i];
                    XMStoreS5S6U5N( &NormalCompressed, NormalUncompressed );
                }
                else 
                {
                    XMU565& NormalCompressed = 
                        CompressedTextureData.m_pXMU565[j * TextureDims.iPitchInTexels + i];
                    XMStoreU565N( &NormalCompressed, NormalUncompressed );
                }
            }
        }
        break;
    }

    XGTileSurface( CompressedTextureData.m_pVoid, 
        TextureDims.iPitchInBlocks, 
        TextureDims.iHeightInBlocks, 
        NULL, 
        CompressedTextureData.m_pVoid, 
        TextureDims.iPitchInBytes, 
        NULL, 
        TextureDims.iBlockSizeInBytes );

    delete [] RawTextureDataCopy.m_pVoid;
}


//--------------------------------------------------------------------------------------
// Name: GenerateSinCosLookupMaps()
// Desc: Compute 1D textures encoding sin/cos for various input domains.  In some 
// cases a single 2D texture for two angles at once might be more efficient than
// two lookups into 1D textures.
//--------------------------------------------------------------------------------------
VOID Sample::GenerateSinCosLookupMaps()
{
    HRESULT hr = S_OK;

    const UINT iLength = 256;

    // Fill in lookup map [-PI,PI]
    {
        hr = m_pd3dDevice->CreateLineTexture( iLength, 1, 0, D3DFMT_LIN_V16U16, 0, 
            &m_pSinCosLookupMapNegPiToPi, NULL );

        D3DLOCKED_RECT LockedRect;
        m_pSinCosLookupMapNegPiToPi->LockRect( 0, &LockedRect, NULL, 0 );

        XMSHORTN2* pSinCosLookupTable = (XMSHORTN2*) LockedRect.pBits;

        for( UINT i = 0; i < iLength; ++i )
        {
            FLOAT fTheta = XM_2PI * ( ( (FLOAT) i ) + 0.5f ) / iLength - XM_PI;

            // The swizzling of the texture will cause these to come out sin/cos in the shader
            const XMVECTOR SinCosUncompressed = { cosf( fTheta ), sinf( fTheta ) };
            XMSHORTN2& SinCosCompressed = pSinCosLookupTable[i];

            XMStoreShortN2( &SinCosCompressed, SinCosUncompressed );
        }

        m_pSinCosLookupMapNegPiToPi->UnlockRect( 0 );
    }

    // Fill in lookup map [0,PI]
    {
        hr = m_pd3dDevice->CreateLineTexture( iLength, 1, 0, D3DFMT_LIN_V16U16, 0, 
            &m_pSinCosLookupMapZeroToPi, NULL );

        D3DLOCKED_RECT LockedRect;
        m_pSinCosLookupMapZeroToPi->LockRect( 0, &LockedRect, NULL, 0 );

        XMSHORTN2* pSinCosLookupTable = (XMSHORTN2*) LockedRect.pBits;

        for( UINT i = 0; i < iLength; ++i )
        {
            FLOAT fTheta = XM_PI * ( ( (FLOAT) i ) + 0.5f ) / iLength;

            // The swizzling of the texture will cause these to come out sin/cos in the shader
            const XMVECTOR SinCosUncompressed = { cosf( fTheta ), sinf( fTheta ) };
            XMSHORTN2& SinCosCompressed = pSinCosLookupTable[i];

            XMStoreShortN2( &SinCosCompressed, SinCosUncompressed );
        }

        m_pSinCosLookupMapZeroToPi->UnlockRect( 0 );
    }

    // Fill in lookup map [0,PI/2]
    {
        hr = m_pd3dDevice->CreateLineTexture( iLength, 1, 0, D3DFMT_LIN_V16U16, 0, 
            &m_pSinCosLookupMapZeroToHalfPi, NULL );

        D3DLOCKED_RECT LockedRect;
        m_pSinCosLookupMapZeroToHalfPi->LockRect( 0, &LockedRect, NULL, 0 );

        XMSHORTN2* pSinCosLookupTable = (XMSHORTN2*) LockedRect.pBits;

        for( UINT i = 0; i < iLength; ++i )
        {
            FLOAT fTheta = XM_PIDIV2 * ( ( (FLOAT) i ) + 0.5f ) / iLength;

            // The swizzling of the texture will cause these to come out sin/cos in the shader
            const XMVECTOR SinCosUncompressed = { cosf( fTheta ), sinf( fTheta ) };
            XMSHORTN2& SinCosCompressed = pSinCosLookupTable[i];

            XMStoreShortN2( &SinCosCompressed, SinCosUncompressed );
        }

        m_pSinCosLookupMapZeroToHalfPi->UnlockRect( 0 );
    }
}


//--------------------------------------------------------------------------------------
// Name: GenerateGeometryQuad()
// Desc: Creates vertex and index buffer for a single fullscreen quad 
//       (overkill, but allows the code to look more parallel)
//--------------------------------------------------------------------------------------
VOID Sample::GenerateGeometryQuad( D3DVertexBuffer** pVB,
                                 D3DIndexBuffer** pIB, UINT* numIndices )
{
    // Create a vertex buffer and copy the mesh vertex data into it
    m_pd3dDevice->CreateVertexBuffer( sizeof( TestGeometryVertex ) * 4, 0, 0, D3DPOOL_DEFAULT, 
        pVB, NULL );
    TestGeometryVertex* pVBData = NULL;
    ( *pVB )->Lock( 0, 0, ( VOID** )&pVBData, 0 );

    pVBData[0].Position.x = -1.0f;
    pVBData[0].Position.y = -1.0f;
    pVBData[0].Position.z =  0.0f;
    pVBData[0].TexCoord.x =  0.0f;
    pVBData[0].TexCoord.y =  0.0f;
    pVBData[0].Tangent.x  =  1.0f;
    pVBData[0].Tangent.y  =  0.0f;
    pVBData[0].Tangent.z  =  0.0f;
    pVBData[0].Binormal.x =  0.0f;
    pVBData[0].Binormal.y =  1.0f;
    pVBData[0].Binormal.z =  0.0f;
    pVBData[0].Normal.x   =  0.0f;
    pVBData[0].Normal.y   =  0.0f;
    pVBData[0].Normal.z   =  1.0f;

    pVBData[1].Position.x = -1.0f;
    pVBData[1].Position.y =  1.0f;
    pVBData[1].Position.z =  0.0f;
    pVBData[1].TexCoord.x =  0.0f;
    pVBData[1].TexCoord.y =  1.0f;
    pVBData[1].Tangent.x  =  1.0f;
    pVBData[1].Tangent.y  =  0.0f;
    pVBData[1].Tangent.z  =  0.0f;
    pVBData[1].Binormal.x =  0.0f;
    pVBData[1].Binormal.y =  1.0f;
    pVBData[1].Binormal.z =  0.0f;
    pVBData[1].Normal.x   =  0.0f;
    pVBData[1].Normal.y   =  0.0f;
    pVBData[1].Normal.z   =  1.0f;

    pVBData[2].Position.x =  1.0f;
    pVBData[2].Position.y = -1.0f;
    pVBData[2].Position.z =  0.0f;
    pVBData[2].TexCoord.x =  1.0f;
    pVBData[2].TexCoord.y =  0.0f;
    pVBData[2].Tangent.x  =  1.0f;
    pVBData[2].Tangent.y  =  0.0f;
    pVBData[2].Tangent.z  =  0.0f;
    pVBData[2].Binormal.x =  0.0f;
    pVBData[2].Binormal.y =  1.0f;
    pVBData[2].Binormal.z =  0.0f;
    pVBData[2].Normal.x   =  0.0f;
    pVBData[2].Normal.y   =  0.0f;
    pVBData[2].Normal.z   =  1.0f;

    pVBData[3].Position.x =  1.0f;
    pVBData[3].Position.y =  1.0f;
    pVBData[3].Position.z =  0.0f;
    pVBData[3].TexCoord.x =  1.0f;
    pVBData[3].TexCoord.y =  1.0f;
    pVBData[3].Tangent.x  =  1.0f;
    pVBData[3].Tangent.y  =  0.0f;
    pVBData[3].Tangent.z  =  0.0f;
    pVBData[3].Binormal.x =  0.0f;
    pVBData[3].Binormal.y =  1.0f;
    pVBData[3].Binormal.z =  0.0f;
    pVBData[3].Normal.x   =  0.0f;
    pVBData[3].Normal.y   =  0.0f;
    pVBData[3].Normal.z   =  1.0f;

    ( *pVB )->Unlock();

    *numIndices = 4;

    // Create an index buffer and copy in the mesh index data.
    m_pd3dDevice->CreateIndexBuffer( *numIndices * sizeof( WORD ),
                                          0, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          pIB, NULL );
    WORD* pIBData = NULL;
    ( *pIB )->Lock( 0, 0, ( VOID** )&pIBData, 0 );
    *pIBData++ = 0;
    *pIBData++ = 2;
    *pIBData++ = 3;
    *pIBData++ = 1;
    ( *pIB )->Unlock();
}


//--------------------------------------------------------------------------------------
// Name: GenerateHemisphereVertices()
// Desc: Creates geometry for a hemisphere
//--------------------------------------------------------------------------------------
VOID Sample::GenerateHemisphereVertices( UINT numSlices, UINT numStacks,
                                     TestGeometryVertex* pData )
{
    INT iTilingCount = 2;
    for( DWORD i = 0; i < numSlices + 1; i++ )
    {
        for( DWORD j = 0; j < numStacks + 1; j++ )
        {
            FLOAT fPhi          = FLOAT( i ) / numSlices * XM_PI - XM_PIDIV2;
            FLOAT fTheta        = FLOAT( j ) / numStacks * XM_PI;
            pData->TexCoord.x   = iTilingCount * FLOAT( i ) / numSlices;
            pData->TexCoord.y   = iTilingCount * FLOAT( j ) / numStacks;
            pData->Position.x   =  sinf( fPhi )  * sinf( fTheta );
            pData->Position.y   =                  cosf( fTheta );
            pData->Position.z   =  cosf( fPhi )  * sinf( fTheta );
            pData->Tangent.x    =  cosf( fPhi );
            pData->Tangent.y    =                  0.0f;
            pData->Tangent.z    = -sinf( fPhi );
            pData->Binormal.x   =  sinf( fPhi ) * -cosf( fTheta );
            pData->Binormal.y   =                  sinf( fTheta );
            pData->Binormal.z   =  cosf( fPhi ) * -cosf( fTheta );
            pData->Normal       = pData->Position;

            pData++;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: GenerateHemisphereIndices()
// Desc: Creates indices for a hemisphere
//--------------------------------------------------------------------------------------
VOID Sample::GenerateHemisphereIndices( UINT dwNumSlices, UINT dwNumStacks, WORD* pIndices )
{
    WORD i0 = ( WORD )0;
    WORD i1 = ( WORD )1;
    WORD i2 = ( WORD )dwNumStacks + 2;
    WORD i3 = ( WORD )dwNumStacks + 1;

    for( DWORD i = 0; i < dwNumSlices; i++ )
    {
        for( DWORD j = 0; j < dwNumStacks; j++ )
        {
            pIndices[0] = i0;
            pIndices[1] = i1;
            pIndices[2] = i2;
            pIndices[3] = i3;

            pIndices += 4;
            i0++;
            i1++;
            i2++;
            i3++;
        }
        i0++;
        i1++;
        i2++;
        i3++;
    }
}


//--------------------------------------------------------------------------------------
// Name: GenerateGeometryHemisphere()
// Desc: Creates a hemisphere with texture coordinates
//--------------------------------------------------------------------------------------
VOID Sample::GenerateGeometryHemisphere( UINT numSlices, UINT numStacks, D3DVertexBuffer** pVB,
                             D3DIndexBuffer** pIB, UINT* numIndices )
{
    // Create a vertex buffer and copy the mesh vertex data into it
    m_pd3dDevice->BlockUntilIdle();
    m_pd3dDevice->CreateVertexBuffer(
        sizeof( TestGeometryVertex ) * ( numSlices + 1 ) * ( numStacks + 1 ),
        0, 0, D3DPOOL_DEFAULT, pVB, NULL );
    TestGeometryVertex* pVBData = NULL;
    ( *pVB )->Lock( 0, 0, ( VOID** )&pVBData, 0 );
    GenerateHemisphereVertices( numSlices, numStacks, pVBData );
    ( *pVB )->Unlock();

    *numIndices = 4 * numSlices * numStacks;

    // Create an index buffer and copy in the mesh index data.
    m_pd3dDevice->CreateIndexBuffer( *numIndices * sizeof( WORD ),
                                          0, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          pIB, NULL );
    WORD* pIBData = NULL;
    ( *pIB )->Lock( 0, 0, ( VOID** )&pIBData, 0 );
    GenerateHemisphereIndices( numSlices, numStacks, pIBData );
    ( *pIB )->Unlock();
}


//--------------------------------------------------------------------------------------
// Name: GetPSConditionalsBool()
// Desc: Find the values for the BOOL shader conditionals.  These will be passed
// along either as bool shader constants or as preprocessor macros.
//--------------------------------------------------------------------------------------
VOID Sample::GetPSConditionalsBool( BOOL (&pbPSConditionals)[PS_CONDITIONAL_BOOL_COUNT], 
    BOOL bRaw )
{
    BOOL bRenderNormals = m_RenderNormalsParam.GetValue();
    BOOL bAmbient = m_AmbientParam.GetValue();
    BOOL bDiffuse = m_DiffuseParam.GetValue();
    BOOL bSpecular = m_SpecularParam.GetValue();
    if( bRaw )
    {
        pbPSConditionals[PS_CONDITIONAL_BOOL_RENDER_NORMAL] = bRenderNormals;
        pbPSConditionals[PS_CONDITIONAL_BOOL_DECODE_SIGN] = FALSE;
        pbPSConditionals[PS_CONDITIONAL_BOOL_BUILT_IN_Z] = FALSE; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_RENORMALIZE_TEXEL] = TRUE; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_RENORMALIZE_INTERPOLANTS] = TRUE; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_RENORMALIZE_RESULT] = TRUE; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_AMBIENT] = bAmbient; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_DIFFUSE] = bDiffuse; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_SPECULAR] = bSpecular; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_USE_LOOKUP_MAP_FOR_TRIG] = FALSE; 
    }
    else
    {
        UINT iFormatType = m_FormatTypeParam.GetValue();
        BOOL bBuiltInZ = m_BuiltInZParam.GetValue();
        BOOL bUseBiasForSign = m_UseBiasForSignParam.GetValue();
        BOOL bUseLookupMapForTrig = m_UseLookupMapForTrigParam.GetValue();
        BOOL bRenormalizeTexel = m_RenormalizeTexelParam.GetValue();
        BOOL bRenormalizeInterpolants = m_RenormalizeInterpolantsParam.GetValue();
        BOOL bRenormalizeResult = m_RenormalizeResultParam.GetValue();

        pbPSConditionals[PS_CONDITIONAL_BOOL_RENDER_NORMAL] = bRenderNormals;
        pbPSConditionals[PS_CONDITIONAL_BOOL_DECODE_SIGN] = 
            !bUseBiasForSign && !IsNativelySigned( iFormatType ); 
        pbPSConditionals[PS_CONDITIONAL_BOOL_BUILT_IN_Z] = 
            bBuiltInZ && CanHaveBuiltInZ( iFormatType ); 
        pbPSConditionals[PS_CONDITIONAL_BOOL_RENORMALIZE_TEXEL] = bRenormalizeTexel; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_RENORMALIZE_INTERPOLANTS] = bRenormalizeInterpolants; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_RENORMALIZE_RESULT] = bRenormalizeResult; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_AMBIENT] = bAmbient; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_DIFFUSE] = bDiffuse; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_SPECULAR] = bSpecular; 
        pbPSConditionals[PS_CONDITIONAL_BOOL_USE_LOOKUP_MAP_FOR_TRIG] = bUseLookupMapForTrig; 
    }
}


//--------------------------------------------------------------------------------------
// Name: GetPSConditionalsInt()
// Desc: Find the values for the INT shader conditionals.  These will be passed
// along as preprocessor macros.
//--------------------------------------------------------------------------------------
VOID Sample::GetPSConditionalsInt( UINT (&piPSConditionals)[PS_CONDITIONAL_INT_COUNT], 
    BOOL bRaw )
{
    UINT iEnvironmentType = m_EnvironmentTypeParam.GetValue();
    if( bRaw )
    {
        piPSConditionals[PS_CONDITIONAL_INT_COORD_TYPE] = COORD_TYPE_RECTANGULAR;
        piPSConditionals[PS_CONDITIONAL_INT_ENVIRONMENT_TYPE] = iEnvironmentType;
    }
    else
    {
        UINT iCoordType = m_CoordTypeParam.GetValue();
        piPSConditionals[PS_CONDITIONAL_INT_COORD_TYPE] = iCoordType;
        piPSConditionals[PS_CONDITIONAL_INT_ENVIRONMENT_TYPE] = iEnvironmentType;
    }
}


//--------------------------------------------------------------------------------------
// Name: GetPSConditionalsInt()
// Desc: Find the values for the INT shader conditionals.  These will be passed
// along as float4 shader constants.
//--------------------------------------------------------------------------------------
VOID Sample::GetPSConditionalsIntAsFloatVector( XMVECTOR (&pvPSConditionals)[PS_CONDITIONAL_INT_COUNT], 
    BOOL bRaw )
{
    UINT iEnvironmentType = m_EnvironmentTypeParam.GetValue();
    if( bRaw )
    {
        pvPSConditionals[PS_CONDITIONAL_INT_COORD_TYPE].x = (FLOAT) COORD_TYPE_RECTANGULAR;
        pvPSConditionals[PS_CONDITIONAL_INT_ENVIRONMENT_TYPE].x = (FLOAT) iEnvironmentType;
    }
    else
    {
        UINT iCoordType = m_CoordTypeParam.GetValue();
        pvPSConditionals[PS_CONDITIONAL_INT_COORD_TYPE].x = (FLOAT) iCoordType;
        pvPSConditionals[PS_CONDITIONAL_INT_ENVIRONMENT_TYPE].x = (FLOAT) iEnvironmentType;
    }
}


//--------------------------------------------------------------------------------------
// Name: PopulateMacros()
// Desc: In the case where we use specialized shaders for each combination of 
// options, build the preprocessor macros which support specialization.
//--------------------------------------------------------------------------------------
VOID Sample::PopulateMacros( D3DXMACRO (&pd3dMacros)[PS_CONDITIONAL_COUNT + 1], 
    BOOL bRaw )
{
    BOOL pbPSConditionals[PS_CONDITIONAL_BOOL_COUNT] = { 0 };
    GetPSConditionalsBool( pbPSConditionals, bRaw );

    D3DXMACRO* pd3dBoolMacros = pd3dMacros;
    for( UINT i = 0; i < PS_CONDITIONAL_BOOL_COUNT; ++i )
    {
        pd3dBoolMacros[i].Name = g_strPSConditionalBoolNames[i]; 
        pd3dBoolMacros[i].Definition = pbPSConditionals[i] ? "true" : "false"; 
    }

    UINT piPSConditionals[PS_CONDITIONAL_INT_COUNT] = { 0 };
    GetPSConditionalsInt( piPSConditionals, bRaw );

    D3DXMACRO* pd3dIntMacros = &pd3dMacros[PS_CONDITIONAL_BOOL_COUNT];
    for( UINT i = 0; i < PS_CONDITIONAL_INT_COUNT; ++i )
    {
        pd3dIntMacros[i].Name = g_strPSConditionalIntNames[i]; 
        pd3dIntMacros[i].Definition = g_strPSConditionalIntDefinitions[i][ piPSConditionals[i] ]; 
    }

    D3DXMACRO* pd3dNullMacro = &pd3dMacros[PS_CONDITIONAL_COUNT];
    pd3dNullMacro->Name = pd3dNullMacro->Definition = NULL;
}


//--------------------------------------------------------------------------------------
// Name: CompileShaders()
// Desc: Compile the generic vertex shader, and the pixel shaders used for the
// raw and compressed viewports.
//--------------------------------------------------------------------------------------
HRESULT Sample::CompileShaders( BOOL bSpecialize )
{
    HRESULT hr = S_OK;

    // Buffers to hold compiled shaders and possible error messages
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Compile vertex shader.
    const UINT iBufferSize = 64 * 1024;
    CHAR* strShaderBuffer = new CHAR[iBufferSize];
    ADD_FILE_LINE_DIRECTIVE( strShaderBuffer, iBufferSize, g_strVertexShaderConditionals, 
        g_strVertexShaderProgram, VertexShaderProgram );
    hr = D3DXCompileShader( strShaderBuffer, strlen( strShaderBuffer ),
        NULL, NULL, "main", "vs_3_0", 0, &pShaderCode, &pErrorMsg, NULL );
    if( FAILED( hr ) )
    {
        ATG::FatalError( pErrorMsg ? ( const CHAR* )pErrorMsg->GetBufferPointer()
                         : "Vertex shader compilation error." );
    }

    // Create vertex shader.
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &m_pVertexShader );

    // Buffers for #defines
    D3DXMACRO d3dRawMacros[PS_CONDITIONAL_COUNT + 1];
    PopulateMacros( d3dRawMacros, TRUE );

    // Compile raw pixel shader.
    ADD_FILE_LINE_DIRECTIVE( strShaderBuffer, iBufferSize, g_strPixelShaderConditionals, 
        g_strPixelShaderProgram, PixelShaderProgram );
    hr = D3DXCompileShader( strShaderBuffer, strlen( strShaderBuffer ),
        d3dRawMacros, NULL, "main", "ps_3_0", 0, &pShaderCode, &pErrorMsg, NULL );
    if( FAILED( hr ) )
    {
        ATG::FatalError( pErrorMsg ? ( const CHAR* )pErrorMsg->GetBufferPointer()
                         : "Pixel shader compilation error." );
    }

    // Create raw pixel shader.
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &m_pRawPixelShader );

    // Buffers for #defines
    const D3DXMACRO* pd3dCompressedMacros = NULL;
    D3DXMACRO _d3dCompressedMacros[PS_CONDITIONAL_COUNT + 1];

    // If we use specialized shaders, resolve all the branches using preprocessor
    // macros.  Otherwise, leave them as runtime static branches based on shader
    // constants.
    if( bSpecialize )
    {
        PopulateMacros( _d3dCompressedMacros, FALSE );
        pd3dCompressedMacros = _d3dCompressedMacros;
    }

    // Compile compressed pixel shader.
    ADD_FILE_LINE_DIRECTIVE( strShaderBuffer, iBufferSize, g_strPixelShaderConditionals, 
        g_strPixelShaderProgram, PixelShaderProgram );
    hr = D3DXCompileShader( strShaderBuffer, strlen( strShaderBuffer ),
        pd3dCompressedMacros, NULL, "main", "ps_3_0", 0, &pShaderCode, &pErrorMsg, NULL );
    if( FAILED( hr ) )
    {
        ATG::FatalError( pErrorMsg ? ( const CHAR* )pErrorMsg->GetBufferPointer()
                         : "Pixel shader compilation error." );
    }

    // Create pixel shader.
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &m_pCompressedPixelShader );

    // Get static analysis for the compressed shader
    XGEstimateIdealShaderCost( pShaderCode->GetBufferPointer(), 0, &m_ShaderCost );

    // Shader code is no longer required.
    pShaderCode->Release();
    pShaderCode = NULL;

    delete [] strShaderBuffer;

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: PerfCounterInit()
// Desc: Set up the perf counters we plan to capture
//--------------------------------------------------------------------------------------
VOID Sample::PerfCounterInit()
{
#ifndef _RELEASED3D
    // Set up GPU performance counter structures.
    for( DWORD i = 0; i < 3; ++i )
    {
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterStart[i], 1 );
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterEnd[i], 1 );
    }
    m_pd3dDevice->EnablePerfCounters( TRUE );

    // Enable the performance counters we care about.
    D3DPERFCOUNTER_EVENTS PerfEvents;
    ZeroMemory( &PerfEvents, sizeof( D3DPERFCOUNTER_EVENTS ) );
    // CP clock cycles.
    PerfEvents.CP[0] = GPUPE_CP_COUNT;
    // NRT busy cycles.
    PerfEvents.RBBM[0] = GPUPE_RBBM_NRT_BUSY;
    m_pd3dDevice->SetPerfCounterEvents( &PerfEvents, 0 );

    m_dwFrameCount = 0;
#endif
}


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects.
//-----------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Font Arial_16.xpr\n" );
    }

    // Expanding Font area to get additional screen real estate...
    m_Font.SetWindow( 64, 8, 1280 - 64, 720 - 8 );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Help.xpr\n" );
    }

    m_bDrawHelp = FALSE;
    m_bBigMenu = FALSE;
    m_bFlashingDiffs = FALSE;
    m_fFlashingTimer = 0.0f;

    m_fCameraPitch = 0.0f;
    m_fCameraYaw = 0.0f;

    m_iActiveUIParameter = 0;
    m_iVisibleUIStart = 0;

    // Create common vertex declaration used by all the geometry
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 20, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
        { 0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },
        { 0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
        D3DDECL_END()
    };

    m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDecl );

    GenerateGeometryQuad( &m_pVB[TEST_GEOMETRY_QUAD],
        &m_pIB[TEST_GEOMETRY_QUAD], &m_iIndexCount[TEST_GEOMETRY_QUAD] );
    GenerateGeometryHemisphere( 50, 50, &m_pVB[TEST_GEOMETRY_HEMISPHERE],
        &m_pIB[TEST_GEOMETRY_HEMISPHERE], &m_iIndexCount[TEST_GEOMETRY_HEMISPHERE] );

    // Create the textures resource
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Resource.xpr\n" );
    }

    m_pEnvironmentMap = m_Resource.GetCubemap( "EnvMap" );

    GenerateSinCosLookupMaps();

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    PerfCounterInit();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: GenerateRawTexture()
// Desc: Generate the actual data for the raw texture displayed in the
// lefthand viewport.
//-----------------------------------------------------------------------------
VOID Sample::GenerateRawTexture( BOOL bFirstUpdate )
{
    UINT iTestTexture = m_TestTextureParam.GetValue();

    RawAndCompressedNormalBuffer& NormalBuffer = m_NormalBuffer;
    const UINT iWidth = NormalBuffer.m_iTextureWidth;
    const UINT iHeight = NormalBuffer.m_iTextureHeight;

    if( bFirstUpdate )
    {
        NormalBuffer.m_pRawTexture = new D3DTexture;
    }
    D3DTexture* pRawTexture = NormalBuffer.m_pRawTexture;

    UINT iPitch = XGNextMultiple( iWidth, GPU_TEXTURE_TILE_DIMENSION ) * sizeof(XMFLOAT4);

    D3DFORMAT d3dSamplingFormat = D3DFMT_LIN_A32B32G32R32F;
    UINT iSize = XGSetTextureHeader( iWidth, iHeight, 1, 0, d3dSamplingFormat, D3DPOOL_DEFAULT, 
        0, 0, iPitch, NormalBuffer.m_pRawTexture, NULL, NULL );

    GenericRawNormalBuffer& RawData = NormalBuffer.m_pRawData;
    if( !bFirstUpdate )
    {
        XPhysicalFree( RawData.m_pVoid );
    }
    RawData.m_pVoid = XPhysicalAlloc( iSize, MAXULONG_PTR, 0, 
        PAGE_READWRITE | PAGE_WRITECOMBINE | MEM_LARGE_PAGES );

    pRawTexture->Format.BaseAddress = NULL;
    XGOffsetBaseTextureAddress( pRawTexture, RawData.m_pVoid, NULL );

    D3DLOCKED_RECT LockedRect;
    pRawTexture->LockRect( 0, &LockedRect, NULL, 0 );

    ComputeRawNormals( iTestTexture );

    pRawTexture->UnlockRect( 0 );
}


//-----------------------------------------------------------------------------
// Name: GenerateCompressedTexture()
// Desc: Generate the actual data for the compressed texture displayed in the
// righthand viewport.
//-----------------------------------------------------------------------------
VOID Sample::GenerateCompressedTexture( BOOL bFirstUpdate )
{
    UINT iFormatType                 = m_FormatTypeParam.GetValue();
    UINT iCoordType                  = m_CoordTypeParam.GetValue();
    BOOL bBuiltInZ                   = m_BuiltInZParam.GetValue();
    BOOL bUseBiasForSign             = m_UseBiasForSignParam.GetValue();
    BOOL iFilterType                 = m_FilterTypeParam.GetValue();

    RawAndCompressedNormalBuffer& NormalBuffer = m_NormalBuffer;
    const UINT iWidth = NormalBuffer.m_iTextureWidth;
    const UINT iHeight = NormalBuffer.m_iTextureHeight;

    if( bFirstUpdate )
    {
        NormalBuffer.m_pCompressedTexture = new D3DTexture;
    }
    D3DTexture* pCompressedTexture = NormalBuffer.m_pCompressedTexture;

    TextureDims TextureDims;
    GetTextureDims( iFormatType, iWidth, iHeight, &TextureDims );

    D3DFORMAT d3dSamplingFormat = BuildSamplingFormat( iFormatType, iCoordType, bUseBiasForSign, 
        iFilterType );
    m_iCompressedSize = XGSetTextureHeader( iWidth, iHeight, 1, 0, d3dSamplingFormat, D3DPOOL_DEFAULT, 
        0, 0, TextureDims.iPitchInBytes, NormalBuffer.m_pCompressedTexture, NULL, NULL );

    GenericCompressedNormalBuffer& CompressedData = NormalBuffer.m_pCompressedData;
    if( !bFirstUpdate )
    {
        XPhysicalFree( CompressedData.m_pVoid );
    }
    CompressedData.m_pVoid = XPhysicalAlloc( m_iCompressedSize, MAXULONG_PTR, 0, 
        PAGE_READWRITE | PAGE_WRITECOMBINE | MEM_LARGE_PAGES );

    pCompressedTexture->Format.BaseAddress = NULL;
    XGOffsetBaseTextureAddress( pCompressedTexture, CompressedData.m_pVoid, NULL );

    D3DLOCKED_RECT LockedRect;
    pCompressedTexture->LockRect( 0, &LockedRect, NULL, 0 );

    ComputeCompressedData( iFormatType, iCoordType, bBuiltInZ, bUseBiasForSign, 
        iFilterType );

    pCompressedTexture->UnlockRect( 0 );
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( !m_bDrawHelp )
    {
        FLOAT fElapsedTime = (FLOAT) m_Timer.GetElapsedTime();
        m_fFlashingTimer += fElapsedTime;
        FLOAT fIntPart;
        m_fFlashingTimer = modf( m_fFlashingTimer, &fIntPart );

        // Rotate the camera
        m_fCameraPitch += pGamepad->fY2 * fElapsedTime;
        m_fCameraYaw += pGamepad->fX2 * fElapsedTime;

        // Build the view matrix
        m_matView = XMMatrixIdentity() *
            XMMatrixRotationY( -m_fCameraYaw ) *
            XMMatrixRotationX( -m_fCameraPitch );

        // Keep test geometry aligned with camera
        // Only the lighting changes
        XMVECTOR vDeterminant;
        m_matWorld = XMMatrixRotationY( XM_PI )         // flip +/- z, but a real flip would change handedness
            * XMMatrixTranslation( 0.0f, 0.0f, 2.0f )   // push hemisphere into positive z
            * XMMatrixInverse( &vDeterminant, m_matView );

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            m_bBigMenu = !m_bBigMenu;
        }
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            m_bFlashingDiffs = !m_bFlashingDiffs;
        }

        // UI controls:
        {
            // Record state prior to applying controller input
            static BOOL g_bFirstUpdate = TRUE;

            BOOL bPresetTurnedOn = FALSE, bPresetTurnedOff = FALSE;

            UINT iOldFormatType                 = m_FormatTypeParam.GetValue();
            UINT iOldCoordType                  = m_CoordTypeParam.GetValue();
            UINT iOldFilterType                 = m_FilterTypeParam.GetValue();
            BOOL bOldAmbient                    = m_AmbientParam.GetValue();
            BOOL bOldDiffuse                    = m_DiffuseParam.GetValue();
            BOOL bOldSpecular                   = m_SpecularParam.GetValue();
            UINT iOldEnvironmentType            = m_EnvironmentTypeParam.GetValue();
            BOOL bOldSpecializeShaders          = m_SpecializeShadersParam.GetValue();
            BOOL bOldBuiltInZ                   = m_BuiltInZParam.GetValue();
            BOOL bOldUseBiasForSign             = m_UseBiasForSignParam.GetValue();
            BOOL bOldUseLookupMapForTrig        = m_UseLookupMapForTrigParam.GetValue();
            BOOL bOldRenormalizeTexel           = m_RenormalizeTexelParam.GetValue();
            BOOL bOldRenormalizeInterpolants    = m_RenormalizeInterpolantsParam.GetValue();
            BOOL bOldRenormalizeResult          = m_RenormalizeResultParam.GetValue();
            BOOL bOldRenderNormals              = m_RenderNormalsParam.GetValue();
            UINT iOldTextureDimensions          = m_TextureDimensionsParam.GetValue();
            UINT iOldTestTexture                = m_TestTextureParam.GetValue();
            //UINT iOldTestGeometry               = m_TestGeometryParam.GetValue();

            static FLOAT fLastX1 = 0.0f, fLastY1 = 0.0f;
            FLOAT fDecrease = 0.0f;
            FLOAT fIncrease = 0.0f;

            // Process UI Input
            if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP ) 
                || ( pGamepad->fY1 > 0.1f && fLastY1 <= 0.1f ) )
            {
                m_iActiveUIParameter += UI_PARAM_COUNT - 1;
                m_iActiveUIParameter %= UI_PARAM_COUNT;
            }
            if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN ) 
                || ( pGamepad->fY1 < -0.1f && fLastY1 >= -0.1f ) )
            {
                m_iActiveUIParameter += 1;
                m_iActiveUIParameter %= UI_PARAM_COUNT;
            }

            // Keep the selected parameter in view, in the small menu
            if( ( m_iActiveUIParameter + 1 ) % UI_PARAM_COUNT == m_iVisibleUIStart )
            {
                m_iVisibleUIStart += UI_PARAM_COUNT - 1;
                m_iVisibleUIStart %= UI_PARAM_COUNT;
            }
            else if( ( m_iVisibleUIStart + m_iVisibleUICount ) % UI_PARAM_COUNT == m_iActiveUIParameter )
            {
                m_iVisibleUIStart += 1;
                m_iVisibleUIStart %= UI_PARAM_COUNT;
            }

            static BOOL bLeftTriggerWasDead = TRUE;
            static BOOL bRightTriggerWasDead = TRUE;

            if( bLeftTriggerWasDead && pGamepad->bLeftTrigger > 0 )
                fDecrease = pGamepad->bLeftTrigger / 255.0f;
            if( bRightTriggerWasDead && pGamepad->bRightTrigger > 0 )
                fIncrease = pGamepad->bRightTrigger / 255.0f;
            if( pGamepad->fX1 < -0.1f && fLastX1 >= -0.1f )
                fDecrease = 1.0f;
            if( pGamepad->fX1 > 0.1f && fLastX1 <= 0.1f )
                fIncrease = 1.0f;
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                fDecrease = 1.0f;
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                fIncrease = 1.0f;

            // Limit to one move at a time
            bLeftTriggerWasDead = ( pGamepad->bLeftTrigger == 0 );
            bRightTriggerWasDead = ( pGamepad->bRightTrigger == 0 );

            // Correct for bias in thumbsticks, and limit to one move at a time
            fLastX1 = pGamepad->fX1;
            fLastY1 = pGamepad->fY1;

            if( fDecrease > 0.0f )
            {
                m_UIParamArray[m_iActiveUIParameter]->DecreaseValue( fDecrease );
            }
            if( fIncrease > 0.0f )
            {
                m_UIParamArray[m_iActiveUIParameter]->IncreaseValue( fIncrease );
            }

            // Handle preset going on or getting auto-disabled
            if( fDecrease > 0.0f || fIncrease > 0.0f )
            {
                switch( m_iActiveUIParameter )
                {
                case UI_PARAM_PRESET:
                    bPresetTurnedOn = TRUE;
                    break;

                case UI_PARAM_FORMAT_TYPE: 
                case UI_PARAM_COORD_TYPE: 
                case UI_PARAM_USE_LOOKUP_MAP_FOR_TRIG: 
                case UI_PARAM_USE_BIAS_FOR_SIGN:
                case UI_PARAM_BUILTIN_Z: 
                case UI_PARAM_RENORMALIZE_TEXEL: 
                    bPresetTurnedOff = TRUE;
                    break;
                }
            }

            // Force preset, if selected
            // Kill preset, if an option changes manually
            if( g_bFirstUpdate || bPresetTurnedOn )
            {
                const Preset& Preset = g_Presets[ m_PresetParam.GetValue() ];
                for( UINT i = 0; i < PRESET_PARAM_COUNT; ++i )
                {
                    m_PresetParamArray[i]->SetValue( Preset[i] );
                }
            }
            else if( bPresetTurnedOff )
            {
                m_PresetParam.SetValue( 0xffffffff );    // forces display of "n/a"
            }

            // Detect state changes from applying controller input
            UINT iNewFormatType                 = m_FormatTypeParam.GetValue();
            UINT iNewCoordType                  = m_CoordTypeParam.GetValue();
            UINT iNewFilterType                 = m_FilterTypeParam.GetValue();
            BOOL bNewAmbient                    = m_AmbientParam.GetValue();
            BOOL bNewDiffuse                    = m_DiffuseParam.GetValue();
            BOOL bNewSpecular                   = m_SpecularParam.GetValue();
            UINT iNewEnvironmentType            = m_EnvironmentTypeParam.GetValue();
            BOOL bNewSpecializeShaders          = m_SpecializeShadersParam.GetValue();
            BOOL bNewBuiltInZ                   = m_BuiltInZParam.GetValue();
            BOOL bNewUseBiasForSign             = m_UseBiasForSignParam.GetValue();
            BOOL bNewUseLookupMapForTrig        = m_UseLookupMapForTrigParam.GetValue();
            BOOL bNewRenormalizeTexel           = m_RenormalizeTexelParam.GetValue();
            BOOL bNewRenormalizeInterpolants    = m_RenormalizeInterpolantsParam.GetValue();
            BOOL bNewRenormalizeResult          = m_RenormalizeResultParam.GetValue();
            BOOL bNewRenderNormals              = m_RenderNormalsParam.GetValue();
            UINT iNewTextureDimensions          = m_TextureDimensionsParam.GetValue();
            UINT iNewTestTexture                = m_TestTextureParam.GetValue();
            //UINT iNewTestGeometry               = m_TestGeometryParam.GetValue();

            // For now, we rebuild everything whenever any relevant option changes
            if( g_bFirstUpdate 
                || iNewFormatType               != iOldFormatType 
                || iNewFilterType               != iOldFilterType 
                || iNewCoordType                != iOldCoordType 
                || bNewAmbient                  != bOldAmbient
                || bNewDiffuse                  != bOldDiffuse
                || bNewSpecular                 != bOldSpecular
                || iNewEnvironmentType          != iOldEnvironmentType
                || bNewSpecializeShaders        != bOldSpecializeShaders
                || bNewBuiltInZ                 != bOldBuiltInZ 
                || bNewUseBiasForSign           != bOldUseBiasForSign 
                || bNewUseLookupMapForTrig      != bOldUseLookupMapForTrig 
                || bNewRenormalizeTexel         != bOldRenormalizeTexel 
                || bNewRenormalizeInterpolants  != bOldRenormalizeInterpolants 
                || bNewRenormalizeResult        != bOldRenormalizeResult 
                || bNewRenderNormals            != bOldRenderNormals 
                || iNewTextureDimensions        != iOldTextureDimensions
                || iNewTestTexture              != iOldTestTexture
                )
            {
                // Allow release of shaders, modification of texture data
                m_pd3dDevice->BlockUntilIdle();

                RawAndCompressedNormalBuffer& NormalBuffer = m_NormalBuffer;
                NormalBuffer.m_iTextureWidth =  g_vTextureDimensions[ iNewTextureDimensions ].x;
                NormalBuffer.m_iTextureHeight = g_vTextureDimensions[ iNewTextureDimensions ].y;

                GenerateRawTexture( g_bFirstUpdate );

                GenerateCompressedTexture( g_bFirstUpdate );

                if( !g_bFirstUpdate )
                {
                    m_pVertexShader->Release();
                    m_pRawPixelShader->Release();
                    m_pCompressedPixelShader->Release();
                }
                CompileShaders( bNewSpecializeShaders );
            }

            g_bFirstUpdate = FALSE;
        }
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CalculateIdealTextureCost()
// Desc: Build our own estimate for cycle count for all texture fetches in
// the shader.  The estimate from XGEstimateIdealShaderCost is not good 
// enough, because it doesn't know the actual texture formats and D3D state.
//-----------------------------------------------------------------------------
UINT Sample::CalculateIdealTextureCost()
{
    UINT iIdealTextureCost = 0;

    UINT iFilterType                = m_FilterTypeParam.GetValue();
    UINT iFormatType                = m_FormatTypeParam.GetValue();
    UINT iCoordType                 = m_CoordTypeParam.GetValue();
    BOOL bRenderNormals             = m_RenderNormalsParam.GetValue();
    BOOL bDiffuse                   = m_DiffuseParam.GetValue();
    BOOL bSpecular                  = m_SpecularParam.GetValue();
    UINT iEnvironmentType           = m_EnvironmentTypeParam.GetValue();
    BOOL bUseLookupMapForTrig       = m_UseLookupMapForTrigParam.GetValue();

    // Otherwise the fetch from the normal map will get optimized away
    if( bDiffuse || bSpecular || ( iEnvironmentType != ENVIRONMENT_TYPE_NONE ) || bRenderNormals )
    {
        UINT iFilterMultiplier;
        switch( iFilterType )
        {
        case FILTER_TYPE_POINT:
        default:
            iFilterMultiplier = 1;
            break;

        case FILTER_TYPE_BILINEAR:
            switch( iFormatType )
            {
            case FORMAT_TYPE_16_16_FLOAT: 
                iFilterMultiplier = 2; // filtering requires _EXPAND
                break;
            default:
                iFilterMultiplier = 1;
                break;
            }
        }

        UINT iAs16Multiplier = 1;   // If we were to use AS_16, this would come into play

        switch( iFormatType )
        {
        case FORMAT_TYPE_32_32_FLOAT: 
            iIdealTextureCost += 8; // filtering not even supported here
            break;
        case FORMAT_TYPE_16_16_FLOAT: 
            iIdealTextureCost += 4 * iFilterMultiplier;
            break;
        case FORMAT_TYPE_16_16_FIXED: 
            iIdealTextureCost += 4 * iFilterMultiplier;
            break;
        case FORMAT_TYPE_11_11_10_FIXED: 
            iIdealTextureCost += 4 * iFilterMultiplier * iAs16Multiplier;
            break;
        case FORMAT_TYPE_8_8_FIXED: 
            iIdealTextureCost += 4 * iFilterMultiplier; // As16 not supported here 
            break;
        case FORMAT_TYPE_5_6_5_FIXED: 
            iIdealTextureCost += 4 * iFilterMultiplier; // As16 not supported here
            break;
        case FORMAT_TYPE_DXN: 
        case FORMAT_TYPE_CTX1: 
            iIdealTextureCost += 4 * iFilterMultiplier; // As16 free and automatic here
            break;
        case FORMAT_TYPE_DXT5: 
        case FORMAT_TYPE_DXT1: 
        default:
            iIdealTextureCost += 4 * iFilterMultiplier * iAs16Multiplier;
            break;
        }
    }

    if( ( iEnvironmentType != ENVIRONMENT_TYPE_NONE ) && ! bRenderNormals )
    {
        iIdealTextureCost += 8; // Cost of fetch from the environment map
    }

    // Cost of fetch from the trig lookup tables
    if( bUseLookupMapForTrig )
    {
        switch( iCoordType )
        {
        case COORD_TYPE_RECTANGULAR:
        default:
            break;

        case COORD_TYPE_CYLINDRICAL:
            iIdealTextureCost += 4;
            break;

        // These could become 4 rather than 8, using a 2D lookup map
        case COORD_TYPE_SPHERICAL:
        case COORD_TYPE_PSEUDOSPHERICAL:
            iIdealTextureCost += 8;
            break;
        }
    }

    return iIdealTextureCost;
}


//--------------------------------------------------------------------------------------
// Name: PerfCounterDebugRender()
// Desc: Print information from the perf counters to screen
//--------------------------------------------------------------------------------------
VOID Sample::PerfCounterDebugRender()
{
#ifndef _RELEASED3D
    WCHAR strText[256];
    D3DPERFCOUNTER_VALUES StartValues;
    m_pPerfCounterStart[ ( m_dwFrameCount + 1 ) % 3 ]->GetValues( &StartValues, 0, NULL );
    D3DPERFCOUNTER_VALUES EndValues;
    m_pPerfCounterEnd[ ( m_dwFrameCount + 1 ) % 3 ]->GetValues( &EndValues, 0, NULL );

    // Subtract start values from end values.
    UINT64* pStartValues = ( UINT64* )&StartValues;
    UINT64* pEndValues = ( UINT64* )&EndValues;
    const DWORD dwCount = sizeof( D3DPERFCOUNTER_VALUES ) / sizeof( UINT64 );
    for( DWORD i = 0; i < dwCount; ++i )
    {
        pEndValues[i] -= pStartValues[i];
    }

    FLOAT fYPos = 40.0f;

    m_Font.SetScaleFactors( 0.9f, 0.9f );
    //swprintf_s( strText, L"GPU Perf Counters" );
    //m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText );
    //fYPos += 20;

    // Display GPU busy cycle count for just the time of the test.
    //swprintf_s( strText, L"GPU Cycles: %I64d", EndValues.RBBM[0].QuadPart );
    //m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText );
    //fYPos += 20;
    m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"GPU MS:" );
    swprintf_s( strText, L"%3.3f", ( FLOAT )EndValues.RBBM[0].QuadPart / ( FLOAT )m_dwGpuCyclesPerMs );
    m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
    fYPos += 20;

    ++m_dwFrameCount;
#endif
}


//--------------------------------------------------------------------------------------
// Name: StaticAnalysisDebugRender()
// Desc: Render debug info from our static shader analysis and texture cost analysis.
//--------------------------------------------------------------------------------------
VOID Sample::StaticAnalysisDebugRender()
{
    BOOL bSpecializeShaders         = m_SpecializeShadersParam.GetValue();

    WCHAR strText[256];

    FLOAT fYPos = 60.0f;

    m_Font.SetScaleFactors( 0.9f, 0.9f );

    if( bSpecializeShaders )
    {
        m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"ALU:" );
        swprintf_s( strText, L"%2.2f cycles (%d instructions)", 
            m_ShaderCost.MaxAlu, (UINT) ( 0.75f * m_ShaderCost.MaxAlu ) );
        m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
        fYPos += 20;
    }
    else
    {
        m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"ALU:" );
        swprintf_s( strText, L"%2.2f-%2.2f cycles (%d-%d instructions)", 
            m_ShaderCost.MinAlu, m_ShaderCost.MaxAlu, 
            (UINT) ( 0.75f * m_ShaderCost.MinAlu ), 
            (UINT) ( 0.75f * m_ShaderCost.MaxAlu ) );
        m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
        fYPos += 20;
    }

    // The GPU must allocate a GPR for each interpolator, whether or not
    // the shader uses it.  The number of interpolants is generally the 
    // interpolant cost divided by 4 cycles (but there are exceptions).
    m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"GPRs:" );
    swprintf_s( strText, L"%d", Max( m_ShaderCost.MaxTempReg, (int) m_ShaderCost.Interpolator / 4 ) );
    m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
    fYPos += 20;

    // The estimated numbers are not good enough here, and the calculated 
    // numbers are hard to get from XGCalculateIdealShaderCost.
    // We attempt to reproduce them ourselves
    m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"Texture:" );
    UINT iIdealTextureCost = CalculateIdealTextureCost();
    swprintf_s( strText, L"%d cycles (ideal)", iIdealTextureCost );
    m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
    fYPos += 20;

    m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"Memory:" );
    swprintf_s( strText, L"%d bytes (%d bpp)", m_iCompressedSize, m_iCompressedBPP );
    m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
    fYPos += 20;
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Render the screen display for our custom menu.
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{

    PIXBeginNamedEvent( 0, "Render UI" );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else 
    {
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0.0f, 0.0f, 0xffff00ff, L"Normal Compression" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0.0f, 150.0f, 0xff00ffff, L"Uncompressed", ATGFONT_LEFT );
        m_Font.DrawText( 1152.0f, 150.0f, 0xff00ffff, L"Compressed", ATGFONT_RIGHT );

        StaticAnalysisDebugRender();

        PerfCounterDebugRender();

        FLOAT fParamX = 500.0f;
        FLOAT fParamY = 0.0f;
        FLOAT fParamYInc = 30.0f;

        UINT iVisibleUIStart = m_bBigMenu ? 0 : m_iVisibleUIStart;
        UINT iVisibleUICount = m_bBigMenu ? UI_PARAM_COUNT : m_iVisibleUICount;

        if( !m_bBigMenu )
        {
            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( fParamX + 40.0f, fParamY, 0xffff00ff, GLYPH_UP_ARROW ); 
            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( fParamX + 100.0f, fParamY, 0xffffffff, L"(" GLYPH_B_BUTTON L" to expand)" ); 
            fParamY += fParamYInc;
        }

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        for( UINT i = 0; i < iVisibleUICount; ++i )
        {
            UINT iParamIndex = ( iVisibleUIStart + i ) % UI_PARAM_COUNT;

            UIParam* Param = m_UIParamArray[iParamIndex];

            Param->RenderUI( &m_Font, fParamX, fParamY, ( iParamIndex == m_iActiveUIParameter ) );
            fParamY += fParamYInc;
        }

        if( !m_bBigMenu )
        {
            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( fParamX + 40.0f, fParamY, 0xffff00ff, GLYPH_DOWN_ARROW );
            fParamY += fParamYInc;
        }

        m_Font.End();
    }
}


//--------------------------------------------------------------------------------------
// Name: DrawGeometryToRenderTarget()
// Desc: Draw to either viewport.
//--------------------------------------------------------------------------------------
VOID Sample::DrawGeometryToRenderTarget(IDirect3DTexture9* pSrcTexture,
                                        UINT iFilterType, 
                                        IDirect3DPixelShader9* pPixelShader, 
                                        IDirect3DVertexBuffer9* pVB, 
                                        IDirect3DIndexBuffer9* pIB, 
                                        UINT iIBSize)
{
    // Make sure that the required resources exist
    assert( pSrcTexture );

    // Make sure that the required shaders and objects exist
    assert( pPixelShader );

    // Set all the right D3D state and resources
    m_pd3dDevice->SetIndices( pIB );
    m_pd3dDevice->SetStreamSource( 0, pVB, 0, sizeof( TestGeometryVertex ) );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    m_pd3dDevice->SetTexture( 0, pSrcTexture );
    switch( iFilterType )
    {
    case FILTER_TYPE_POINT:
    default:
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        break;

    case FILTER_TYPE_BILINEAR:
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        break;
    }
    m_pd3dDevice->SetPixelShader( pPixelShader );

    // Set up the float shader constants
    XMMATRIX matProj = XMMatrixOrthographicLH( 2.0f, 2.0f, 0.0f, 4.0f );

    XMMATRIX matWVP = m_matWorld * m_matView * matProj;
    
    XMMATRIX matWVPT = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPT, 4 );
    XMMATRIX matWorldT = XMMatrixTranspose( m_matWorld );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matWorldT, 4 );

    XMVECTOR vEyeWorldPos = -m_matView.r[3];
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vEyeWorldPos, 1 );

    // Draw the geometry
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0, iIBSize / 4 );
}

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, to render the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Recover all the relevant settings from the UI
    UINT iFormatType                = m_FormatTypeParam.GetValue();
    UINT iFilterType                = m_FilterTypeParam.GetValue();
    BOOL bSpecializeShaders         = m_SpecializeShadersParam.GetValue();
    UINT iTestGeometry              = m_TestGeometryParam.GetValue();

    IDirect3DTexture9* pRawTexture = m_NormalBuffer.m_pRawTexture;
    IDirect3DTexture9* pCompressedTexture = m_NormalBuffer.m_pCompressedTexture;
    IDirect3DPixelShader9 *pRawPixelShader = m_pRawPixelShader;
    IDirect3DPixelShader9 *pCompressedPixelShader = m_pCompressedPixelShader;

    // The combination of 32-bit float and bilinear filtering is not supported
    // by the hardware, and produces some artifacts.
    if( iFilterType != FILTER_TYPE_POINT && !IsFilterable( iFormatType ) )
    {
        iFilterType = FILTER_TYPE_POINT;
    }

    PIXBeginNamedEvent( 0, "Clear to pattern" );

    // Clear the viewport to a pattern (this is visible behind the non-full-screen test geometry)
    D3DCOLOR D3D_BLACK      = D3DCOLOR_ARGB( 0xff, 0x00, 0x00, 0x00 );
    D3DCOLOR D3D_GREEN      = D3DCOLOR_ARGB( 0xff, 0x00, 0xff, 0x00 );
    UINT iStripCount = 32;
    UINT iStripWidth = m_d3dpp.BackBufferWidth / iStripCount;
    UINT iStripHeight = m_d3dpp.BackBufferHeight;
    for( UINT iStrip = 0; iStrip < iStripCount; ++iStrip )
    {
        D3DRECT d3dRectBlack = { iStripWidth * iStrip, 0, iStripWidth * ( iStrip + 1 ), iStripHeight };
        m_pd3dDevice->Clear( 1, &d3dRectBlack, D3DCLEAR_TARGET, D3D_BLACK, 1.0f, 0L );
        ++iStrip;
        D3DRECT d3dRectGreen = { iStripWidth * iStrip, 0, iStripWidth * ( iStrip + 1 ), iStripHeight };
        m_pd3dDevice->Clear( 1, &d3dRectGreen, D3DCLEAR_TARGET, D3D_GREEN, 1.0f, 0L );
    }

    PIXEndNamedEvent();

    // With 512x512 source and 512x512 viewport, this causes exact texel-to-pixel
    // sampling, even with bilinear filtering enabled.
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    m_pd3dDevice->SetTexture( 1, m_pEnvironmentMap );
    m_pd3dDevice->SetTexture( 2, m_pSinCosLookupMapNegPiToPi );
    m_pd3dDevice->SetTexture( 3, m_pSinCosLookupMapZeroToPi );
    m_pd3dDevice->SetTexture( 4, m_pSinCosLookupMapZeroToHalfPi );

    // sin/cos wrap for 2Pi range, but not for smaller ranges
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSU,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 4, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 4, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 4, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    D3DVIEWPORT9 OldViewport;
    m_pd3dDevice->GetViewport( &OldViewport );

    {
        // Render uncompressed into left-hand viewport
        D3DVIEWPORT9 Viewport;
        Viewport.X = 64;
        Viewport.Y = 192;
        Viewport.Width = 512;
        Viewport.Height = 512;
        Viewport.MinZ = 0.0f;
        Viewport.MaxZ = 1.0f;
        m_pd3dDevice->SetViewport( &Viewport );

        PIXBeginNamedEvent( 0, "Render Left Viewport (Uncompressed)" );

        // Set up the bool shader constants for the raw texture
        // The raw texture isn't a filterable format, so must use point-sampling
        DrawGeometryToRenderTarget( pRawTexture, FILTER_TYPE_POINT, pRawPixelShader, 
            m_pVB[ iTestGeometry ], m_pIB[ iTestGeometry ], m_iIndexCount[ iTestGeometry ] );

        PIXEndNamedEvent();
    }

    {
        // Render compressed into right-hand viewport
        D3DVIEWPORT9 Viewport;
        Viewport.X = 704;
        Viewport.Y = 192;
        Viewport.Width = 512;
        Viewport.Height = 512;
        Viewport.MinZ = 0.0f;
        Viewport.MaxZ = 1.0f;
        m_pd3dDevice->SetViewport( &Viewport );

        // Flashing diffs cause righthand viewport to alternate between compressed and
        // raw each second
        if( m_bFlashingDiffs && ( m_fFlashingTimer > 0.5f ) )
        {
            PIXBeginNamedEvent( 0, "Render Right Viewport (Uncompressed)" );

            DrawGeometryToRenderTarget( pRawTexture, FILTER_TYPE_POINT, pRawPixelShader, 
                m_pVB[ iTestGeometry ], m_pIB[ iTestGeometry ], m_iIndexCount[ iTestGeometry ] );

            PIXEndNamedEvent();
        }
        else
        {
            PIXBeginNamedEvent( 0, "Render Right Viewport (Compressed)" );

            // If shaders are specialized then we already have a single shader which incorporates 
            // all our UI selections.
            //
            // Otherwise, we have a branching ubershader.  In that case, set up the bool and float4 
            // shader constants for the UI selections.
            if( !bSpecializeShaders )
            {
                BOOL bPixelShaderConstantB[PS_CONDITIONAL_BOOL_COUNT];
                GetPSConditionalsBool( bPixelShaderConstantB, FALSE );
                m_pd3dDevice->SetPixelShaderConstantB( 0, bPixelShaderConstantB, 
                    _countof( bPixelShaderConstantB ) );

                XMVECTOR vPixelShaderConstantF[PS_CONDITIONAL_INT_COUNT]; 
                GetPSConditionalsIntAsFloatVector( vPixelShaderConstantF, FALSE );
                m_pd3dDevice->SetPixelShaderConstantF( 1, (FLOAT*) vPixelShaderConstantF, 
                    _countof( vPixelShaderConstantF ) );
            }

#ifndef _RELEASED3D
        m_pd3dDevice->QueryPerfCounters( m_pPerfCounterStart[ m_dwFrameCount % 3 ], D3DPERFQUERY_WAITGPUIDLE );
#endif

        DrawGeometryToRenderTarget( pCompressedTexture, iFilterType, pCompressedPixelShader, 
            m_pVB[ iTestGeometry ], m_pIB[ iTestGeometry ], m_iIndexCount[ iTestGeometry ] );

#ifndef _RELEASED3D
        m_pd3dDevice->QueryPerfCounters( m_pPerfCounterEnd[ m_dwFrameCount % 3 ], D3DPERFQUERY_WAITGPUIDLE );
#endif

            PIXEndNamedEvent();
        }
    }

    m_pd3dDevice->SetViewport( &OldViewport );

    RenderUI();

    PIXEndNamedEvent();

    // Present the backbuffer contents to the display
    ATG::g_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    m_pd3dDevice->UnsetAll();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------

VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Use fixed back buffer resolution regardless of output dimensions
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


