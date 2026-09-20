//--------------------------------------------------------------------------------------
// AdvancedLighting.h
//
// Definitions for bindings of shader constants and samplers
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef ADVANCEDLIGHTING_H
#define ADVANCEDLIGHTING_H

// Try values between 3-9 and compare the visual and performance differences
#define RSM_SAMPLESIZE  4
#define RSM_NUMSAMPLES  ( RSM_SAMPLESIZE * RSM_SAMPLESIZE )


//--------------------------------------------------------------------------------------
// Vertex shader globals
//--------------------------------------------------------------------------------------
#define VSCONST_matCameraWVP                0
#define BIND_matCameraWVP                   c0
#define VSCONST_matWorld                    4
#define BIND_matWorld                       c4
#define VSCONST_matLightWVP                 8
#define BIND_matLightWVP                    c8
#define VSCONST_matSampleLightWVP           12
#define BIND_matSampleLightWVP              c12
#define VSCONST_vWorldSpaceCameraPosition   17
#define BIND_vWorldSpaceCameraPosition      c17
#define VSCONST_vWorldSpaceLightDirection   18
#define BIND_vWorldSpaceLightDirection      c18
#define VSCONST_vWorldScale                 19
#define BIND_vWorldScale                    c19
#define VSCONST_vAmbientLightColor          20
#define BIND_vAmbientLightColor             c20
#define VSCONST_vIndirectLightingRadius     21
#define BIND_vIndirectLightingRadius        c21
#define VSCONST_fSampling                   32
#define BIND_fSampling                      c32


//--------------------------------------------------------------------------------------
// Pixel shader globals
//--------------------------------------------------------------------------------------
#define PSCONST_vDiffuseLightColor          0
#define BIND_vDiffuseLightColor             c0
#define PSCONST_vSpecularLightColor         1
#define BIND_vSpecularLightColor            c1
#define PSCONST_vWorldSpaceLightPos         2
#define BIND_vWorldSpaceLightPos            c2
#define PSCONST_fEpsilonVSM                 3
#define BIND_fEpsilonVSM                    c3

#define PSCONST_matLightWVP                 VSCONST_matLightWVP                 // c8
#define PSCONST_matSampleLightWVP           VSCONST_matSampleLightWVP           // c12
#define PSCONST_vWorldSpaceCameraPosition   VSCONST_vWorldSpaceCameraPosition   // c17
#define PSCONST_vWorldSpaceLightDirection   VSCONST_vWorldSpaceLightDirection   // c18
#define PSCONST_vWorldScale                 VSCONST_vWorldScale                 // c19
#define PSCONST_vAmbientLightColor          VSCONST_vAmbientLightColor          // c20
#define PSCONST_vIndirectLightingRadius     VSCONST_vIndirectLightingRadius     // c21
#define PSCONST_fSampling                   VSCONST_fSampling                   // c32

#define PSCONST_fHammersleySampling         132
#define BIND_fHammersleySampling            c132

#define PSCONST_bDebugShowNoLighting        0
#define BIND_bDebugShowNoLighting           b0
#define PSCONST_bDebugShowDirectLighting    1
#define BIND_bDebugShowDirectLighting       b1
#define PSCONST_bDebugShowIndirectLighting  2
#define BIND_bDebugShowIndirectLighting     b2
#define PSCONST_bDebugReduceFlicker         3
#define BIND_bDebugReduceFlicker            b3


//--------------------------------------------------------------------------------------
// Sampler definitions
//--------------------------------------------------------------------------------------
#define SAMPLER_DiffuseTexture              0
#define BIND_DiffuseTexture                 s0
#define SAMPLER_NormalmapTexture            1
#define BIND_NormalmapTexture               s1
#define SAMPLER_ShadowmapTexture            2
#define BIND_ShadowmapTexture               s2

#define SAMPLER_RSMPositionTextureVS        D3DVERTEXTEXTURESAMPLER0
#define BIND_RSMPositionTextureVS           s0
#define SAMPLER_RSMLightDirTextureVS        D3DVERTEXTEXTURESAMPLER1
#define BIND_RSMLightDirTextureVS           s1
#define SAMPLER_RSMFluxTextureVS            D3DVERTEXTEXTURESAMPLER2
#define BIND_RSMFluxTextureVS               s2

#define SAMPLER_RSMPositionTexture          3
#define BIND_RSMPositionTexture             s3
#define SAMPLER_RSMLightDirTexture          4
#define BIND_RSMLightDirTexture             s4
#define SAMPLER_RSMFluxTexture              5
#define BIND_RSMFluxTexture                 s5

#define SAMPLER_RSMPositionSmallTexture     6
#define BIND_RSMPositionSmallTexture        s6
#define SAMPLER_RSMLightDirSmallTexture     7
#define BIND_RSMLightDirSmallTexture        s7
#define SAMPLER_RSMFluxSmallTexture         8
#define BIND_RSMFluxSmallTexture            s8
#define SAMPLER_RSMDepthTexture             9
#define BIND_RSMDepthTexture                s9

#define SAMPLER_HammersleyTexture           0
#define BIND_HammersleyTexture              s0

#define SAMPLER_RSMTexCoordTexture          0
#define BIND_RSMTexCoordTexture             s0



#endif  // ADVANCEDLIGHTING_H
