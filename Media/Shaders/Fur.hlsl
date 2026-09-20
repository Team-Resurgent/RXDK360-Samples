//-----------------------------------------------------------------------------
// File: Fur.hlsl
//
// Desc: HLSL file for the Fur sample. 
// 
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Copyright (C) Tomohide Kano. All rights reserved.
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

// Vertex shader constants
uniform float4x4 mMODELVIEWPROJ : register(c0);       // 
uniform float3   vCAMERA_POS    : register(c4);       // 
uniform float3   vLIGHT_DIR     : register(c5);       // 
uniform float    fFUR_LENGTH    : register(c6);       // 
uniform float    fOFFSET_SCALE  : register(c7);       // 

uniform float3   vTRANSLATIONALFORCE : register(c8);  // 
uniform float3   vOMEGA              : register(c9);  // 
uniform float    fDAMPING            : register(c10); // 

// Pixel shader constants
uniform float    fFUR_BRIGHTNESS     : register(c0);       // 
uniform float    fFUR_LAYER          : register(c1);       // 


//-----------------------------------------------------------------------------
// Name: SkinVS/PS()
// Desc: Shaders for the surface of the mesh under the fur
//-----------------------------------------------------------------------------
struct FUR_VERTEX
{
    float3 Position  : POSITION;
    float2 Tex0      : TEXCOORD0;
    float2 Tex1      : TEXCOORD1;
    float2 Scale     : TEXCOORD2;
    float2 Radius    : TEXCOORD3;
    float3 DS        : TEXCOORD4;
    float3 DT        : TEXCOORD5;
    float3 Normal    : NORMAL;
};

struct VSOUT
{
    float4 Position  : POSITION;
    float4 Color     : COLOR0;
};

VSOUT SkinVS( FUR_VERTEX Input )
{
    VSOUT Output;
    
    // Transform the vertex into homogeneous clip space
    Output.Position = mul( float4( Input.Position, 1.0f ), mMODELVIEWPROJ );

    // Calculate diffuse color (hemisphere lighting)
    float4 vDIFFUSE = float4( 0.09, 0.06, 0.03, 0.0 );
    float4 vAMBIENT = float4( 0.09, 0.06, 0.03, 0.0 );

    float fLight = dot( Input.Normal, vLIGHT_DIR );

    Output.Color = fLight * vDIFFUSE + vAMBIENT;

    return Output;
}

float4 SkinPS( VSOUT Input ) : COLOR
{
    return Input.Color;
}


//-----------------------------------------------------------------------------
// Testing: CreateUVMapping shaders
//-----------------------------------------------------------------------------
VSOUT CreateUVMappingVS( FUR_VERTEX Input )
{
    VSOUT Output;
    
    Output.Position = float4( Input.Tex0 * 2.0f - 1.0f, 0, 1 );
    Output.Color    = float4( Input.Position, 1 );
    
    return Output;
}

float4 CreateUVMappingPS( VSOUT Input ) : COLOR
{
    return Input.Color;
}

//-----------------------------------------------------------------------------
// Name: FurVS/PS()
// Desc: Shaders for the fur layers
//-----------------------------------------------------------------------------
struct VSOUT2
{
    float4 Position  : POSITION;  // xyzw = Clip-space position
    float4 TexCoord0 : TEXCOORD0; // xy-- = Texcoord for fur texture
                                  // --zw = Texcoord for the normal map
    float4 TexCoord1 : TEXCOORD3; // xyz- = Light vector
                                  // ---w = Offset scale factor .x
    float4 TexCoord2 : TEXCOORD4; // xyz- = Half-angle vector
                                  // ---w = Offset scale factor .y
};

VSOUT2 FurVS( FUR_VERTEX Input )
{
    VSOUT2 Output;

    // Add fur length to vertex position
    float3 Pos = Input.Position + Input.Normal * fFUR_LENGTH;

    // Transform vertex into homogeneous clip space
    Output.Position = mul( float4( Pos, 1.0f), mMODELVIEWPROJ );

    // Pass thru tex coords to TEXCOORD0
    Output.TexCoord0.xy = Input.Tex1; // Assign texcoord for fur texture
    Output.TexCoord0.zw = Input.Tex0; // Assign texcoord for normal map

    // Calculate light vector and store it into TEXCOORD1
    {
        // Transform light vector into local texture space
        float3 vLight;
        vLight.x = dot( vLIGHT_DIR, Input.DS );
        vLight.y = dot( vLIGHT_DIR, Input.DT );
        vLight.z = dot( vLIGHT_DIR, Input.Normal );

        // Store normalized light vector (since texture space is not necessarily orthonormal)
        Output.TexCoord1.xyz = normalize( vLight );
    }

    // Calculate halfangle vector and store it into TEXCOORD2
    {
        float3 vHalf;
        float3 vTemp;
        
        // Calculate halfangle vector in model space
        vHalf = normalize( vCAMERA_POS - Pos ) + vLIGHT_DIR;
        vHalf = normalize( vHalf );

        // Transform halfangle vector into local texture space
        vTemp.x = dot( vHalf, Input.DS );
        vTemp.y = dot( vHalf, Input.DT );
        vTemp.z = dot( vHalf, Input.Normal );

        // Store normalized halfangle vector
        Output.TexCoord2.xyz = normalize( vTemp );
    }

    // Calculate offset scale factor and store it into TEXCOORD1/2
    {
        float2 OffsetScale;
        OffsetScale = Input.Scale * fOFFSET_SCALE * Input.Radius / ( Input.Radius + fFUR_LENGTH );
OffsetScale = Input.Scale * fOFFSET_SCALE;

        // Store offset scale
        Output.TexCoord1.w = OffsetScale.x;
        Output.TexCoord2.w = OffsetScale.y;
    }

    return Output;
}


sampler OffsetMap        : register(s0);
sampler LightingTexture  : register(s1);
sampler FurTexture       : register(s2);
sampler FurColorTexture  : register(s3);


float4 FurPS( VSOUT2 Input ) : Color
{
    float2 vBaseTexCoord   = Input.TexCoord0.xy;
    float2 vOffsetTexCoord = Input.TexCoord0.zw;
    float3 vLight          = Input.TexCoord1.xyz;
    float3 vHalf           = Input.TexCoord2.xyz;
    float2 vOffsetScale    = float2( Input.TexCoord1.w, Input.TexCoord2.w );
    float2 vOffset         = tex2D( OffsetMap, vOffsetTexCoord ).xy;
    float3 vNormal         = normalize( float3( vOffset, 1 ) );
    vOffset = 2*vOffset - 1;
    vNormal = 2*vNormal - 1;

    // Lookup diffuse and specular/shadow terms from a 3D texture
    float4 LightingTexel;
    {
        float3 vLightingTexCoord;
        vLightingTexCoord.x = dot( vNormal, vLight );
        vLightingTexCoord.y = dot( vNormal, vHalf );
        vLightingTexCoord.z = vLight.z * 0.5 + 0.5;
        LightingTexel = tex3D( LightingTexture, vLightingTexCoord ); // Diffuse, specular
    }
    
    // Lookup fur texel using offset from base fur texture
    float4 FurTexel;
    {
        float2 vFurTexCoord = vBaseTexCoord + vOffsetScale * vOffset;
        FurTexel = tex2D( FurTexture, vFurTexCoord ); // Fur_brightness, Fur_transparency
    }

    float3 Diffuse  = LightingTexel.rgb * FurTexel.rgb;
    float  Specular = LightingTexel.a   * FurTexel.r;

    float4 Output;
    Output.rgb = saturate( float3( 0.5, 0.5, 0.4 ) * Specular + Diffuse );
    Output.a   = saturate( FurTexel.a );
    return Output;
}


//-----------------------------------------------------------------------------
// Name: UpdateOffsetMapVS/PS
// Desc: Shaders for updating the offset map
//-----------------------------------------------------------------------------
struct OFFSET_VERTEX
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float2 Force    : TEXCOORD1;
    float  Damping  : TEXCOORD2;
};

OFFSET_VERTEX UpdateOffsetMapVS( FUR_VERTEX Input )
{
    OFFSET_VERTEX Output;  
    
    // Use texcoord to generate position
    Output.Position = float4( (Input.Tex0.x*2-1), -(Input.Tex0.y*2-1), 1, 1 );
    
    // Output tex coords for previous offset map
    Output.TexCoord = Input.Tex0;
    
    // Force
    float3 Force = vTRANSLATIONALFORCE - cross( vOMEGA, Input.Position );
    
    // Transform force vector into local texture space
    float FS = dot( Force,    Input.DS );
    float FT = dot( Force,    Input.DT );
    float ST = dot( Input.DS, Input.DT );
    
    float Det = 1.0f / ( 1.0f - ST * ST );
    
    Output.Force.x = ( FS - FT * ST ) * Det; 
    Output.Force.y = ( FT - FS * ST ) * Det; 
    
    // Damping
    Output.Damping = fDAMPING;
    
    return Output;
}


sampler OffsetTexture : register(s0);

float4 UpdateOffsetMapPS( OFFSET_VERTEX Input ) : COLOR
{
    float2 vOffset  = 2 * tex2D( OffsetTexture, Input.TexCoord ).xy - 1;
    float2 vForce   = Input.Force;
    float  fDamping = Input.Damping;

    // Update the offset
    vOffset = fDamping * vForce + (1-fDamping)*vOffset;

    // Clamp the result
    float len = length( vOffset );
    if( len > 1.0f )
        vOffset /= len;
        
    // Output the updated offset
    return float4( (vOffset+1)/2, 0, 0 );
}


//-----------------------------------------------------------------------------
// Name: ShowTextureVS/PS()
// Desc: Shaders for displaying a texture in a screenspace quad
//-----------------------------------------------------------------------------
uniform float4   vSCREENSPACESCALE  : register(c0);
uniform float4   vSCREENSPACEOFFSET : register(c1);

sampler CurrentTexture : register(s0);

struct VSOUT4
{
    float4 Position  : POSITION;
    float2 TexCoord0 : TEXCOORD0;
};

VSOUT4 ShowTextureVS( float4 Position  : POSITION,
                      float2 TexCoord0 : TEXCOORD0 )
{
    VSOUT4 Output;
    Output.Position  = Position * vSCREENSPACESCALE + vSCREENSPACEOFFSET;
    Output.TexCoord0 = TexCoord0;
    return Output;
}

float4 ShowTexturePS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    return tex2D( CurrentTexture, TexCoord0 );
}
