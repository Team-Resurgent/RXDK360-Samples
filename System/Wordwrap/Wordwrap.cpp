//--------------------------------------------------------------------------------------
// Wordwrap.cpp
//
// A simple application class that displays a re-sizable box on the screen which
// contains a text string that can be switched between English, Japanese, Korean and
// Traditional Chinese. 
//
// Developed by Microsoft Game Studios Tools and Technology Group
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Wordwrap.h"

//--------------------------------------------------------------------------------------
// Vertex shader
// We use the register semantic here to directly define the input register
// matWVP.  Conversely, we could let the HLSL compiler decide and check the
// constant table.
//--------------------------------------------------------------------------------------
const char g_strVertexShaderProgram[] =
    "struct VS_IN\n"
    "{\n"
        "float2 Pos : POSITION;\n"      // Object space position 
        "float4 Color : COLOR;\n"       // Vertex color                 
    "};\n"
    ""
    "struct VS_OUT\n"
    "{\n"
        "float4 Position : POSITION;\n"  // Projected space position 
        "float4 Color : COLOR;\n"
    "};\n"
    ""
    "uniform float2 PosScale : register(c0);\n"
    ""
    "VS_OUT main( VS_IN In )\n"
    "{\n"
        "VS_OUT Out;\n"
        "Out.Position.x = ( In.Pos.x * PosScale.x - 1.0 );\n"
        "Out.Position.y =-( In.Pos.y * PosScale.y - 1.0 );\n"
        "Out.Position.z = 0.0;\n"
        "Out.Position.w = 1.0;\n"
        "Out.Color = In.Color;\n"       // Projected space and 
        "return Out;\n"                 // Transfer color
    "}";

//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------
const char g_strPixelShaderProgram[] =
    " struct PS_IN                                 "
    " {                                            "
    "     float4 Color : COLOR;                    "  // Interpolated color from                      
    " };                                           "  // the vertex shader
    "                                              "
    " float4 main( PS_IN In ) : COLOR              "
    " {                                            "
    "     return In.Color;                         "  // Output color
    " }                                            ";

//-------------------------------------------------------------------------------------
// Structure to hold vertex data.
//-------------------------------------------------------------------------------------
struct COLORVERTEX
{
    float Position[2];
    DWORD Color;
};

//-------------------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------------------

// alphanumeric sample string
const WCHAR g_wszTestString[] =
    L"The Xbox console and controller are designed to capture the power and performance that Xbox will deliver and are based on the input of more than 5,000 gamers and games creators from around the world.\nSleek and powerful in its look, in a rich shade of black, the Xbox console is emblazoned with a large \"X\" and a signature green Xbox \"jewel\" positioned in its center.";

// Japanese sample string
const WCHAR g_wszTestStringJPN[] =
{
    0x300c, 0x0043, 0x0050, 0x0055, 0x300d, 0x306b, 0x0049, 0x006e,
    0x0074, 0x0065, 0x006c, 0x793e, 0x88fd, 0x306e, 0x0050, 0x0065,
    0x006e, 0x0074, 0x0069, 0x0075, 0x006d, 0x3001, 0x300c, 0x30b0,
    0x30e9, 0x30d5, 0x30a3, 0x30c3, 0x30af, 0x30c1, 0x30c3, 0x30d7,
    0x300d, 0x300c, 0x30b5, 0x30a6, 0x30f3, 0x30c9, 0x30c1, 0x30c3,
    0x30d7, 0x300d, 0x306b, 0x004e, 0x0076, 0x0069, 0x0064, 0x0069,
    0x0061, 0x88fd, 0x306e, 0x30c1, 0x30c3, 0x30d7, 0x3001, 0x305d,
    0x3057, 0x3066, 0x300c, 0x30cf, 0x30fc, 0x30c9, 0x30c7, 0x30a3,
    0x30b9, 0x30af, 0x300d, 0x3084, 0x300c, 0x30a4, 0x30fc, 0x30b5,
    0x30cd, 0x30c3, 0x30c8, 0x30dd, 0x30fc, 0x30c8, 0x300d, 0x3092,
    0x642d, 0x8f09, 0x3002, 0x000A, 0x3053, 0x306e, 0x30b9, 0x30da,
    0x30c3, 0x30af, 0x3060, 0x3051, 0x3092, 0x805e, 0x304f, 0x3068,
    0x3001, 0x307e, 0x308b, 0x3067, 0x30d1, 0x30bd, 0x30b3, 0x30f3,
    0x3068, 0x5909, 0x308f, 0x3089, 0x306a, 0x3044, 0x3088, 0x3046,
    0x306b, 0x601d, 0x3048, 0x308b, 0x304b, 0x3082, 0x3057, 0x308c,
    0x307e, 0x305b, 0x3093, 0x3002, 0x3057, 0x304b, 0x3057, 0x3001,
    0x0058, 0x0062, 0x006f, 0x0078, 0x306e, 0x6a5f, 0x80fd, 0x306f,
    0x5b8c, 0x5168, 0x306b, 0x30b2, 0x30fc, 0x30e0, 0x306e, 0x305f,
    0x3081, 0x306b, 0x7279, 0x5316, 0x3055, 0x308c, 0x3066, 0x3044,
    0x307e, 0x3059, 0x3002, 0x305d, 0x306e, 0x7279, 0x5316, 0x306e,
    0x30dd, 0x30a4, 0x30f3, 0x30c8, 0x306f, 0x5927, 0x304d, 0x304f,
    0x5206, 0x3051, 0x3066, 0x3075, 0x305f, 0x3064, 0x3002, 0x300c,
    0x30b7, 0x30b9, 0x30c6, 0x30e0, 0x306e, 0x69cb, 0x9020, 0x300d,
    0x3068, 0x300c, 0x30bd, 0x30d5, 0x30c8, 0x30a6, 0x30a7, 0x30a2,
    0x30fb, 0x30ec, 0x30a4, 0x30e4, 0x30fc, 0x306e, 0x8584, 0x3055,
    0x300d, 0x3067, 0x3059, 0x3002, 0x0000
};

// Korean sample string
const WCHAR g_wszTestStringKOR[] =
{
    0x0058, 0x0062, 0x006f, 0x0078, 0xb294, 0x0020, 0xd604, 0xc874,
    0xd558, 0xb294, 0x0020, 0xac00, 0xc7a5, 0x0020, 0xac15, 0xb825,
    0xd55c, 0x0020, 0xac8c, 0xc784, 0x0020, 0xc138, 0xacc4, 0xb97c,
    0x0020, 0xacbd, 0xd5d8, 0xd558, 0xac8c, 0x0020, 0xd558, 0xb294,
    0x0020, 0x004d, 0x0069, 0x0063, 0x0072, 0x006f, 0x0073, 0x006f,
    0x0066, 0x0074, 0xc0ac, 0xc758, 0x0020, 0xcc28, 0x0020, 0xc138,
    0xb300, 0x0020, 0xbe44, 0xb514, 0xc624, 0x0020, 0xac8c, 0xc784,
    0x0020, 0xc2dc, 0xc2a4, 0xd15c, 0xc785, 0xb2c8, 0xb2e4, 0x002e,
    0x0020, 0x0058, 0x0062, 0x006f, 0x0078, 0xb294, 0x0020, 0xac8c,
    0xc784, 0x0020, 0xc81c, 0xc791, 0xc790, 0xb4e4, 0xc5d0, 0xac8c,
    0x0020, 0xcd5c, 0xcd08, 0xb85c, 0x0020, 0xcc3d, 0xc758, 0xc801,
    0x0020, 0xbe44, 0xc804, 0xc744, 0x0020, 0xcda9, 0xc871, 0x0020,
    0xc2dc, 0xd0ac, 0x0020, 0xc218, 0x0020, 0xc788, 0xb294, 0x0020,
    0xae30, 0xc220, 0xc744, 0x0020, 0xc81c, 0xacf5, 0xd568, 0xc73c,
    0xb85c, 0xc368, 0x0020, 0xd658, 0xc0c1, 0xacfc, 0x0020, 0xd604,
    0xc2e4, 0xc758, 0x0020, 0xacbd, 0xacc4, 0xb97c, 0x0020, 0xd5c8,
    0xbb3c, 0xc5b4, 0xc90d, 0xb2c8, 0xb2e4, 0x002e, 0x0020, 0x000a,
    0x0058, 0x0062, 0x006f, 0x0078, 0x0020, 0xcf58, 0xc194, 0xacfc,
    0x0020, 0xcee8, 0xd2b8, 0xb864, 0xb7ec, 0xb294, 0x0020, 0xc804,
    0xc138, 0xacc4, 0x0020, 0x0035, 0x002c, 0x0030, 0x0030, 0x0030,
    0xc5ec, 0xba85, 0xc758, 0x0020, 0xac8c, 0xc774, 0xba38, 0x0020,
    0xbc0f, 0x0020, 0xac8c, 0xc784, 0x0020, 0xac1c, 0xbc1c, 0xc790,
    0xb4e4, 0xc758, 0x0020, 0xc870, 0xc5b8, 0xc5d0, 0x0020, 0xae30,
    0xcd08, 0xd558, 0xc5ec, 0x0020, 0xc6b0, 0xc218, 0xd55c, 0x0020,
    0xd30c, 0xc6cc, 0xc640, 0x0020, 0xc131, 0xb2a5, 0xc744, 0x0020,
    0xcd5c, 0xace0, 0xb85c, 0x0020, 0xbc1c, 0xd718, 0xd560, 0x0020,
    0xc218, 0x0020, 0xc788, 0xb3c4, 0xb85d, 0x0020, 0xc124, 0xacc4,
    0xb418, 0xc5c8, 0xc2b5, 0xb2c8, 0xb2e4, 0x002e, 0x0020, 0x0020,
    0xc9d9, 0xc740, 0x0020, 0xac80, 0xc740, 0xc0c9, 0xc758, 0x0020,
    0xb9e4, 0xb048, 0xd558, 0xace0, 0x0020, 0xac15, 0xd55c, 0x0020,
    0xc774, 0xbbf8, 0xc9c0, 0xc758, 0x0020, 0x0058, 0x0062, 0x006f,
    0x0078, 0x0020, 0xcf58, 0xc194, 0xc740, 0x0020, 0xcee4, 0xb2e4,
    0xb780, 0x0020, 0x0027, 0x0058, 0x0027, 0xc790, 0x0020, 0xc7a5,
    0xc2dd, 0xacfc, 0x0020, 0xd568, 0xaed8, 0x0020, 0xb179, 0xc0c9,
    0xc758, 0x0020, 0x0058, 0x0062, 0x006f, 0x0078, 0x0020, 0x0027,
    0xbcf4, 0xc11d, 0x0027, 0xc774, 0x0020, 0xc911, 0xc559, 0xc5d0,
    0x0020, 0xbc30, 0xce58, 0xb418, 0xc5b4, 0x0020, 0xc788, 0xc2b5,
    0xb2c8, 0xb2e4, 0x002e, 0x0020, 0x0000
};

// Traditional Chinese sample string
const WCHAR g_wszTestStringCHT[] =
{
    0x6700, 0x8212, 0x9069, 0x3001, 0x6700, 0x7cbe, 0x6e96, 0x7684,
    0x63a7, 0x5236, 0xff0c, 0x8b93, 0x60a8, 0x5b8c, 0x5168, 0x638c,
    0x63e1, 0x53ca, 0x53ef, 0x9760, 0x7684, 0x8a2d, 0x8a08, 0x3002,
    0x9019, 0x662f, 0x004d, 0x0069, 0x0063, 0x0072, 0x006f, 0x0073,
    0x006f, 0x0066, 0x0074, 0x91dd, 0x5c0d, 0x8ffd, 0x6c42, 0x9ad8,
    0x54c1, 0x8cea, 0x7684, 0x73a9, 0x5bb6, 0x6240, 0x505a, 0x7684,
    0x8a2d, 0x8a08, 0xff0c, 0x66f4, 0x7b26, 0x5408, 0x73a9, 0x5bb6,
    0x9032, 0x884c, 0x904a, 0x6232, 0x6642, 0x7684, 0x9700, 0x6c42,
    0x3002, 0x000a, 0x63a7, 0x5236, 0x5668, 0x0053, 0x8f03, 0x5c0f,
    0x3002, 0x9019, 0x500b, 0x5d84, 0x65b0, 0x7684, 0x8a2d, 0x8a08,
    0x8b93, 0x624b, 0x638c, 0x8f03, 0x5c0f, 0x7684, 0x73a9, 0x5bb6,
    0x4e5f, 0x80fd, 0x5920, 0x4e00, 0x624b, 0x638c, 0x63e1, 0x904a,
    0x6232, 0xff0c, 0x8b93, 0x73a9, 0x5bb6, 0x5728, 0x9032, 0x884c,
    0x904a, 0x6232, 0x7684, 0x6642, 0x5019, 0xff0c, 0x66f4, 0x52a0,
    0x7684, 0x8212, 0x9069, 0xff0c, 0x73a9, 0x518d, 0x4e45, 0x7684,
    0x904a, 0x6232, 0x4e5f, 0x4e0d, 0x6703, 0x7d2f, 0x3002, 0x0000
};

const WCHAR g_wszTestStringSimChin[] =
{
    0x611f, 0x8c22, 0x60a8, 0x8d2d, 0x4e70, 0x0020, 0x004d, 0x0069,
    0x0063, 0x0072, 0x006f, 0x0073, 0x006f, 0x0066, 0x0074, 0x0020,
    0x7684, 0x0020, 0x0058, 0x0062, 0x006f, 0x0078, 0x0020, 0x0033,
    0x0036, 0x0030, 0x2122, 0x0020, 0x89c6, 0x9891, 0x6e38, 0x620f,
    0x548c, 0x5a31, 0x4e50, 0x7cfb, 0x7edf, 0x3002, 0x4ece, 0x6b64,
    0x523b, 0x8d77, 0xff0c, 0x4f9d, 0x9760, 0x8fd9, 0x4e2a, 0x529f,
    0x80fd, 0x5f3a, 0x5927, 0x7684, 0x4ea7, 0x54c1, 0xff0c, 0x60a8,
    0x53ef, 0x4ee5, 0x73a9, 0x6e38, 0x620f, 0x3001, 0x4e0e, 0x670b,
    0x53cb, 0x4e92, 0x52a8, 0x5e76, 0x4eab, 0x53d7, 0x6570, 0x5b57,
    0x5a31, 0x4e50, 0xff0c, 0x83b7, 0x5f97, 0x968f, 0x5fc3, 0x6240,
    0x6b32, 0x7684, 0x4f7f, 0x7528, 0x4f53, 0x9a8c, 0x3002, 0x2022,
    0x0020, 0x6709, 0x4e86, 0x0020, 0x0058, 0x0062, 0x006f, 0x0078,
    0x0020, 0x0033, 0x0036, 0x0030, 0x0020, 0x89c6, 0x9891, 0x6e38,
    0x620f, 0x548c, 0x5a31, 0x4e50, 0x7cfb, 0x7edf, 0xff0c, 0x60a8,
    0x53ef, 0x4ee5, 0x73a9, 0x0020, 0x0058, 0x0062, 0x006f, 0x0078,
    0x0020, 0x0033, 0x0036, 0x0030, 0x0020, 0x6e38, 0x620f, 0x3001,
    0x64ad, 0x653e, 0x0020, 0x0044, 0x0056, 0x0044, 0x0020, 0x7535,
    0x5f71, 0x548c, 0x97f3, 0x9891, 0x0020, 0x0043, 0x0044, 0x3002,
    0x2022, 0x0020, 0x501f, 0x52a9, 0x9ad8, 0x901f, 0x0020, 0x0049,
    0x006e, 0x0074, 0x0065, 0x0072, 0x006e, 0x0065, 0x0074, 0x0020,
    0x670d, 0x52a1, 0xff0c, 0x53ef, 0x4ee5, 0x8fde, 0x901a, 0x4f7f,
    0x7528, 0x0020, 0x0058, 0x0062, 0x006f, 0x0078, 0x0020, 0x004c,
    0x0069, 0x0076, 0x0065, 0x00ae, 0x0020, 0x7684, 0x6570, 0x767e,
    0x4e07, 0x8ba1, 0x6e38, 0x620f, 0x73a9, 0x5bb6, 0x3002, 0x8bf7,
    0x7acb, 0x5373, 0x52a0, 0x5165, 0x60a8, 0x7684, 0x6e38, 0x620f,
    0x793e, 0x533a, 0xff0c, 0x4e0e, 0x670b, 0x53cb, 0x5206, 0x4eab,
    0x5fc3, 0x5f97, 0x5e76, 0x4e0b, 0x8f7d, 0x65b0, 0x5185, 0x5bb9,
    0x3002, 0x2022, 0x0020, 0x53ea, 0x9700, 0x8fde, 0x63a5, 0x5230,
    0x57fa, 0x4e8e, 0x0020, 0x004d, 0x0069, 0x0063, 0x0072, 0x006f,
    0x0073, 0x006f, 0x0066, 0x0074, 0x00ae, 0x0020, 0x0057, 0x0069,
    0x006e, 0x0064, 0x006f, 0x0077, 0x0073, 0x00ae, 0x0020, 0x7684,
    0x0020, 0x0050, 0x0043, 0x0020, 0x6216, 0x6570, 0x7801, 0x76f8,
    0x673a, 0x548c, 0x4fbf, 0x643a, 0x5f0f, 0x97f3, 0x4e50, 0x64ad,
    0x653e, 0x5668, 0x7b49, 0x5176, 0x4ed6, 0x8bbe, 0x5907, 0xff0c,
    0x56fe, 0x7247, 0x3001, 0x97f3, 0x4e50, 0x7b49, 0x5185, 0x5bb9,
    0x5c31, 0x4f1a, 0x4ee5, 0x6d41, 0x7684, 0x65b9, 0x5f0f, 0x4f20,
    0x9001, 0x5230, 0x0020, 0x0058, 0x0062, 0x006f, 0x0078, 0x0020,
    0x0033, 0x0036, 0x0030, 0x0020, 0x4e3b, 0x673a, 0x4e2d, 0x3002,
    0x2022, 0x0020, 0x6b23, 0x8d4f, 0x9884, 0x5148, 0x5b89, 0x88c5,
    0x5728, 0x0020, 0x0058, 0x0062, 0x006f, 0x0078, 0x0020, 0x0033,
    0x0036, 0x0030, 0x0020, 0x786c, 0x76d8, 0x4e2d, 0x7684, 0x97f3,
    0x4e50, 0x548c, 0x5176, 0x4ed6, 0x5185, 0x5bb9, 0x3002, 0x6709,
    0x5173, 0x0020, 0x0058, 0x0062, 0x006f, 0x0078, 0x0020, 0x0033,
    0x0036, 0x0030, 0x0020, 0x786c, 0x76d8, 0x7684, 0x8be6, 0x7ec6,
    0x4fe1, 0x606f, 0xff0c, 0x8bf7, 0x8bbf, 0x95ee, 0x0020, 0x0077,
    0x0077, 0x0077, 0x002e, 0x0078, 0x0062, 0x006f, 0x0078, 0x002e,
    0x0063, 0x006f, 0x006d, 0x3002, 0x0058, 0x0062, 0x006f, 0x0078,
    0x0020, 0x0033, 0x0036, 0x0030, 0x0020, 0x5177, 0x6709, 0x65b0,
    0x7684, 0x5bb6, 0x957f, 0x76d1, 0x62a4, 0x529f, 0x80fd, 0xff0c,
    0x5141, 0x8bb8, 0x7236, 0x6bcd, 0x548c, 0x76d1, 0x62a4, 0x4eba,
    0x901a, 0x8fc7, 0x81ea, 0x5b9a, 0x4e49, 0x8bbe, 0x7f6e, 0xff0c,
    0x63d0, 0x4f9b, 0x4e0e, 0x4f7f, 0x7528, 0x8005, 0x5e74, 0x9f84,
    0x76f8, 0x9002, 0x5e94, 0x7684, 0x5a31, 0x4e50, 0x5185, 0x5bb9,
    0x3002, 0x901a, 0x8fc7, 0x4e3b, 0x673a, 0x8bbe, 0x7f6e, 0xff0c,
    0x53ef, 0x4ee5, 0x9650, 0x5236, 0x5728, 0x4e3b, 0x673a, 0x4e0a,
    0x64ad, 0x653e, 0x7684, 0x6e38, 0x620f, 0x6216, 0x7535, 0x5f71,
    0x3002, 0x0058, 0x0062, 0x006f, 0x0078, 0x0020, 0x004c, 0x0069,
    0x0076, 0x0065, 0x0020, 0x8bbe, 0x7f6e, 0x53ef, 0x4ee5, 0x4e3a,
    0x6bcf, 0x540d, 0x513f, 0x7ae5, 0x5e10, 0x6237, 0x81ea, 0x5b9a,
    0x4e49, 0x0020, 0x0058, 0x0062, 0x006f, 0x0078, 0x0020, 0x004c,
    0x0069, 0x0076, 0x0065, 0x0020, 0x4f7f, 0x7528, 0x4f53, 0x9a8c,
    0xff0c, 0x5373, 0x4f7f, 0x4ed6, 0x4eec, 0x4e0d, 0x5728, 0x5bb6,
    0x91cc, 0x73a9, 0x3002, 0x0000
};


// Font object
static ATG::Font*   g_pFont;

// current string
StringType          g_CurrentString = StringType_Alphanumeric;


//--------------------------------------------------------------------------------------
// Callouts for help labeling
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_START_BUTTON,   ATG::HELP_PLACEMENT_1, L"Reset states" },
    { ATG::HELP_LEFTSTICK,      ATG::HELP_PLACEMENT_1, L"Move window" },
    { ATG::HELP_RIGHTSTICK,     ATG::HELP_PLACEMENT_1, L"Change window size" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Change text" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle PROHIBITION" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle\nNOHANGULWRAP" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

//--------------------------------------------------------------------------------------
// Name: Initialize
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize base member variables
    m_pVertexShader = NULL;
    m_pPixelShader = NULL;
    m_bDrawHelp = FALSE;
    m_bDrawHelp = FALSE;
    m_bEAString = FALSE;
    m_iOption = WW_PROHIBITION;
    Reset();

    //
    // initialize WordWrap lib.
    //
    WordWrap_SetOption( m_iOption );
    WordWrap_SetCallback( ( CB_GetWidthW )MyGetCharWidthW, NULL );

    // Create the font
    if( FAILED( m_Font.Create( "d:\\Media\\Fonts\\Arial_Unicode_MS_16.xpr" ) ) )
    {
        return ATGAPPERR_MEDIANOTFOUND;
    }
    g_pFont = &m_Font;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "d:\\media\\help\\Help.xpr" ) ) )
    {
        return ATGAPPERR_MEDIANOTFOUND;
    }
    // Create a vertex shader for doing the effect
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 8, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
        D3DDECL_END()
    };

    HRESULT hr = m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDeclaration );
    if( FAILED( hr ) )
    {
        return hr;
    }

    // Compile vertex shader.
    LPD3DXBUFFER pVertexShaderCode;
    LPD3DXBUFFER pVertexErrorMsg;
    hr = D3DXCompileShader( g_strVertexShaderProgram, sizeof( g_strVertexShaderProgram )-1,
                            NULL,
                            NULL,
                            "main",
                            "vs_2_0",
                            0,
                            &pVertexShaderCode,
                            &pVertexErrorMsg,
                            NULL );
    if( FAILED( hr ) )
    {
        if( pVertexErrorMsg )
        {
            OutputDebugString( ( char* )pVertexErrorMsg->GetBufferPointer() );
        }
        return E_FAIL;
    }

    // Create vertex shader.
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pVertexShaderCode->GetBufferPointer(),
                                      &m_pVertexShader );

    // Compile pixel shader.
    LPD3DXBUFFER pPixelShaderCode;
    LPD3DXBUFFER pPixelErrorMsg;
    hr = D3DXCompileShader( g_strPixelShaderProgram, sizeof( g_strPixelShaderProgram )-1,
                            NULL,
                            NULL,
                            "main",
                            "ps_2_0",
                            0,
                            &pPixelShaderCode,
                            &pPixelErrorMsg,
                            NULL );
    if( FAILED( hr ) )
    {
        if( pPixelErrorMsg )
        {
            OutputDebugString( ( char* )pPixelErrorMsg->GetBufferPointer() );
        }
        return E_FAIL;
    }

    // Create pixel shader.
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pPixelShaderCode->GetBufferPointer(),
                                     &m_pPixelShader );

    // Create vertex buffer and set up vertices
    COLORVERTEX aVertices[] =
    {
        { ( FLOAT )m_Rc.left,  ( FLOAT )m_Rc.top,    0x80202040 },
        { ( FLOAT )m_Rc.right, ( FLOAT )m_Rc.top,    0x80202040 },
        { ( FLOAT )m_Rc.right, ( FLOAT )m_Rc.bottom, 0x80202040 },
        { ( FLOAT )m_Rc.left,  ( FLOAT )m_Rc.bottom, 0x80202040 },
    };
    if( FAILED( m_pd3dDevice->CreateVertexBuffer( sizeof( aVertices ),
                                                  D3DUSAGE_WRITEONLY,
                                                  NULL,
                                                  D3DPOOL_MANAGED,
                                                  &m_pVB,
                                                  NULL ) ) )
    {
        return E_FAIL;
    }

    COLORVERTEX* pVertices;
    if( FAILED( m_pVB->Lock( 0, 0, ( void** )&pVertices, 0 ) ) )
    {
        return E_FAIL;
    }
    memcpy( pVertices, aVertices, sizeof( aVertices ) );
    m_pVB->Unlock();
    // Set the position scale factor as a vertex shader constant
    D3DSURFACE_DESC desc;
    LPDIRECT3DSURFACE9 pRenderTarget;
    m_pd3dDevice->GetRenderTarget( 0, &pRenderTarget );
    pRenderTarget->GetDesc( &desc );
    pRenderTarget->Release();
    m_fPosScale[0] = 2.0f / desc.Width;
    m_fPosScale[1] = 2.0f / desc.Height;

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: MySimpleTextOut
// Desc: draw string by using WordWrap functions.
//-----------------------------------------------------------------------------
void Sample::MySimpleTextOut( FLOAT fXPos, FLOAT fYPos, FLOAT fWidth, FLOAT fHeight, const WCHAR* wszInput )
{
    FLOAT fX = fXPos, fY = fYPos;
    FLOAT fW, fH;
    WCHAR wszDraw[1024];
    LPCWSTR wsz = wszInput;

    do
    {
        LPCWSTR wszEOL = NULL;
        LPCWSTR wszNext = WordWrap_FindNextLine( wsz, ( INT )fWidth, &wszEOL );
        int n = wszEOL ? ( ( wszEOL + 1 ) - wsz ) : 0;

        if( wszEOL )
        {
            wcsncpy_s( wszDraw, ( WCHAR* )wsz, n );

            m_Font.DrawText( fX, fY, 0xffffffff, wszDraw );
        }

        m_Font.GetTextExtent( wsz, &fW, &fH, TRUE );
        fY += fH;
        wsz = wszNext;
    } while( wsz );
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }
    // Set rectangle position
    ATG::GAMEPAD& m_DefaultGamepad = *pGamepad;

    const FLOAT STICKMARGIN = 0.2;
    if( m_DefaultGamepad.fX1 )
    {
        int shift = m_DefaultGamepad.fX1 > STICKMARGIN ? 1 : m_DefaultGamepad.fX1 < -STICKMARGIN ? -1 : 0;
        m_Rc.left += shift;
        m_Rc.right += shift;
    }

    if( m_DefaultGamepad.fY1 )
    {
        int shift = m_DefaultGamepad.fY1 > STICKMARGIN ? -1 : m_DefaultGamepad.fY1 < -STICKMARGIN ? 1 : 0;
        m_Rc.top += shift;
        m_Rc.bottom += shift;
    }

    if( m_DefaultGamepad.fX2 )
    {
        int adder = m_DefaultGamepad.fX2 > STICKMARGIN ? 1 : m_DefaultGamepad.fX2 < -STICKMARGIN ? -1 : 0;
        m_Rc.right += adder;
    }

    if( m_DefaultGamepad.fY2 )
    {
        int adder = m_DefaultGamepad.fY2 > STICKMARGIN ? -1 : m_DefaultGamepad.fY2 < -STICKMARGIN ? 1 : 0;
        m_Rc.bottom += adder;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        g_CurrentString = ( StringType )( g_CurrentString + 1 );

        if( g_CurrentString > StringType_SimplifiedChinese )
        {
            g_CurrentString = StringType_Alphanumeric;
        }
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        if( m_iOption & WW_PROHIBITION )
        {
            m_iOption &= ~WW_PROHIBITION;
        }
        else
        {
            m_iOption |= WW_PROHIBITION;
        }
        WordWrap_SetOption( m_iOption );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        if( m_iOption & WW_NOHANGULWRAP )
        {
            m_iOption &= ~WW_NOHANGULWRAP;
        }
        else
        {
            m_iOption |= WW_NOHANGULWRAP;
        }
        WordWrap_SetOption( m_iOption );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        Reset();
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff404040, 0xff404080 );
    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        RenderRect();
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Wordwrap" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffffff, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        if( m_iOption & WW_PROHIBITION )
        {
            m_Font.DrawText( 0, 50, 0xff00ffff, L"PROHIBITION" );
        }
        if( m_iOption & WW_NOHANGULWRAP )
        {
            m_Font.DrawText( 0, 90, 0xff00ffff, L"NOHANGULWRAP" );
        }

        FLOAT xPos = ( FLOAT )( ( m_Rc.left < m_Rc.right ) ? m_Rc.left : m_Rc.right );
        FLOAT yPos = ( FLOAT )( ( m_Rc.top < m_Rc.bottom ) ? m_Rc.top : m_Rc.bottom );
        FLOAT width = ( FLOAT )abs( m_Rc.right - m_Rc.left );
        FLOAT height = ( FLOAT )abs( m_Rc.bottom - m_Rc.top );

        LPCWSTR wsz = ( StringType_Alphanumeric == g_CurrentString ) ? g_wszTestString :
            ( StringType_Japanese == g_CurrentString ) ? g_wszTestStringJPN :
            ( StringType_Korean == g_CurrentString ) ? g_wszTestStringKOR : ( StringType_TraditionalChinese ==
                                                                              g_CurrentString ) ? g_wszTestStringCHT :
            g_wszTestStringSimChin;

        MySimpleTextOut( xPos, yPos, width, height, wsz );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderRect
//--------------------------------------------------------------------------------------
void Sample::RenderRect( DWORD dwColor )
{
    FLOAT left = ( FLOAT )( ( m_Rc.left < m_Rc.right ) ? m_Rc.left : m_Rc.right );
    FLOAT right = ( FLOAT )( ( m_Rc.left < m_Rc.right ) ? m_Rc.right : m_Rc.left );
    FLOAT top = ( FLOAT )( ( m_Rc.top < m_Rc.bottom ) ? m_Rc.top : m_Rc.bottom );
    FLOAT bottom = ( FLOAT )( ( m_Rc.top < m_Rc.bottom ) ? m_Rc.bottom : m_Rc.top );

    left += ( FLOAT )ATG::GetTitleSafeArea().x1;
    top += ( FLOAT )ATG::GetTitleSafeArea().y1;
    right += ( FLOAT )ATG::GetTitleSafeArea().x1;
    bottom += ( FLOAT )ATG::GetTitleSafeArea().y1;

    volatile COLORVERTEX* pVertices;

    if( FAILED( m_pVB->Lock( 0, 0, ( void** )&pVertices, 0 ) ) )
    {
        return;
    }

    pVertices[0].Position[0] = left;
    pVertices[0].Position[1] = top;
    pVertices[0].Color = dwColor;
    pVertices[1].Position[0] = right;
    pVertices[1].Position[1] = top;
    pVertices[1].Color = dwColor;
    pVertices[2].Position[0] = right;
    pVertices[2].Position[1] = bottom;
    pVertices[2].Color = dwColor;
    pVertices[3].Position[0] = left;
    pVertices[3].Position[1] = bottom;
    pVertices[3].Color = dwColor;
    m_pVB->Unlock();

    m_pd3dDevice->SetVertexDeclaration( m_pVertexDeclaration );
    m_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof( COLORVERTEX ) );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetPixelShader( m_pPixelShader );

    FLOAT fPosScale[4];
    memcpy( fPosScale, m_fPosScale, sizeof( m_fPosScale ) );
    ZeroMemory( fPosScale + 2, sizeof( FLOAT ) * 2 );
    m_pd3dDevice->SetVertexShaderConstantF( 0, fPosScale, 1 );

    // Draw the vertices in the vertex buffer
    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

    // Set stream source to null to allow us to modify the vertex buffer next time through
    m_pd3dDevice->SetStreamSource( NULL, NULL, NULL, NULL );

}

//-----------------------------------------------------------------------------
// Name: MyGetCharWidthW
// Desc: callback function which is needed by WordWrapLib.
//-----------------------------------------------------------------------------
INT MyGetCharWidthW( WCHAR c )
{
    FLOAT fW, fH;
    WCHAR wsz[2] = { c, L'\0' };
    g_pFont->GetTextExtent( wsz, &fW, &fH, TRUE );
    return ( INT )fW;
}

//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample xbApp;
    ATG::GetVideoSettings( &xbApp.m_d3dpp.BackBufferWidth, &xbApp.m_d3dpp.BackBufferHeight );
    xbApp.Run();
}
