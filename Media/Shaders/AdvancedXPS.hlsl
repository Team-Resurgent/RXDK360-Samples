//--------------------------------------------------------------------------------------
// Shaders for the AdvancedXPS sample
//--------------------------------------------------------------------------------------

// Vertex shader constants for transform.
float4x4    g_matWorldViewProj  : register(c0);
float3      g_vWindDirection    : register(c4);
float       g_fTime             : register(c5);
float3      g_vCameraPosWorld   : register(c6);

// Pixel shader constants for directional light.
float3      g_vDirLightInvDir   : register(c0);
float4      g_vDirLightColor    : register(c1);

// Pixel shader texture samplers for surface materials.
#define TRILINEAR_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; }
sampler2D   g_DiffuseTexture    : register(s0) = TRILINEAR_SAMPLER;

struct VS_IN
{
    float4 Position : POSITION0;
    float3 Normal   : NORMAL;
    float2 Tex0     : TEXCOORD0;
};

struct VS_OUT
{
    float4 Position     : POSITION;
    float2 Tex0         : TEXCOORD0;
    float3 PosToCamera  : TEXCOORD1;
};

struct PS_IN
{
    float2 Tex0         : TEXCOORD0;
    float3 PosToCamera  : TEXCOORD1;
};


float ComputeWind( float3 WorldPosition )
{
    // Compute the wind phase based on the world position along the wind direction
    // This makes the wind appear to blow across the scene
    float WindPhase = dot( WorldPosition.xz, g_vWindDirection.xz );
    // The wind time is the global time combined with the wind phase at this location
    float WindTime = WindPhase + g_fTime;
    
    // Compute wind amount based on the wind time
    // This function is simply some layered sine waves, it's pretty cheap but effective
    float WindAmount = sin( WindTime * 0.7f ) + sin( WindTime * 2.2f ) * 0.5f + sin( WindTime * 4.7f ) * 0.25f;
    
    // The wind deflection is scaled with the height of the position, so the root of the
    // plant will not deflect at all
    float WindScale = 0.1f;
    return WindAmount * WindScale * WorldPosition.y;
}


VS_OUT VSWindWave( VS_IN In )
{
    VS_OUT Out;
    [isolate]
    {
        float3 Position = In.Position.xyz + ( ComputeWind( In.Position.xyz ) * g_vWindDirection );
        Out.Position = mul( float4( Position, 1 ), g_matWorldViewProj );
        Out.PosToCamera = g_vCameraPosWorld - Position;
    }
    Out.Tex0 = In.Tex0;
    return Out;
}

VS_OUT VSGroundPlane( VS_IN In )
{
    VS_OUT Out;
    [isolate]
    {
        float3 Position = In.Position.xyz;
        Out.Position = mul( float4( Position, 1 ), g_matWorldViewProj );
        Out.PosToCamera = g_vCameraPosWorld - Position;
    }
    Out.Tex0 = In.Tex0;
    return Out;
}

float4 PSBillboard( PS_IN In ) : COLOR
{
    // Sample from texture
    float4 vDiffuseColor = tex2D( g_DiffuseTexture, In.Tex0 );
    
    // Normalize the interpolated position to camera vector
    float3 vPosToCamera = normalize( In.PosToCamera );
    
    // Compute directional light factor
    float DirLight = dot( vPosToCamera, g_vDirLightInvDir );
    
    // The "edge light" is how much light bleeds through the edges of the blades
    float ThruLight = pow( saturate( -DirLight ), 300.0f );
    float EdgeLight = ThruLight * ( 1.0f - vDiffuseColor.a );
    
    // Clamp the directional light to a positive number, so the back face is still lit
    DirLight = DirLight * 0.15f + 0.85f;
    
    // Apply lighting factors to texture
    vDiffuseColor.rgb *= ( ( DirLight + EdgeLight ) * g_vDirLightColor );
    return vDiffuseColor;
}

float4 PSGroundPlane( PS_IN In ) : COLOR
{
    float4 vDiffuseColor = tex2D( g_DiffuseTexture, In.Tex0 );
    float3 vPosToCamera = normalize( In.PosToCamera );
    float DirLight = dot( vPosToCamera, g_vDirLightInvDir );
    DirLight = DirLight * 0.15f + 0.85f;
    vDiffuseColor.rgb *= ( DirLight * g_vDirLightColor );
    return vDiffuseColor;
}
