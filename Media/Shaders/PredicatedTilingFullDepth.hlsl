//--------------------------------------------------------------------------------------
// Shaders for the Predicated Tiling Full Depth Sample
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Vertex shader constants and structures
//--------------------------------------------------------------------------------------
uniform float4x4 matWVP : register(c0);
uniform float4x4 matWorld : register(c4);
uniform float3 LightDirection : register(c8);
uniform float4 Color : register(c12);

struct VS_OUT
{
    float4 ProjPos  : POSITION;
    float4 Color    : COLOR;
};

struct VS_OUT_DEFERRED
{
    float4 ProjPos  : POSITION;
    float3 Normal   : NORMAL;
    float4 Color    : COLOR;
};

//--------------------------------------------------------------------------------------
// Name: ForwardRenderVS()
// Desc: Vertex shader for a simple forward renderer
//--------------------------------------------------------------------------------------
VS_OUT ForwardRenderVS( const float3 ObjPos : POSITION,
             const float3 Normal : NORMAL,
             const float2 TexCoord : TEXCOORD0 )
{
    VS_OUT Out;
    Out.ProjPos = mul( matWVP, float4( ObjPos, 1 ) );
    Out.Color = saturate( dot( Normal, -LightDirection ) );
    Out.Color += 0.1 * saturate( dot( Normal, LightDirection ) );
    Out.Color *= Color;
    Out.Color.a = Color.a;
    return Out;
}

//--------------------------------------------------------------------------------------
// Name: DeferredRenderVS()
// Desc: Vertex shader for a simple deferred renderer which outputs color and normals
//--------------------------------------------------------------------------------------
VS_OUT_DEFERRED DeferredRenderVS( const float3 ObjPos : POSITION,
             const float3 Normal : NORMAL,
             const float2 TexCoord : TEXCOORD0 )
{
    VS_OUT_DEFERRED Out;
    Out.ProjPos = mul( matWVP, float4( ObjPos, 1 ) );
    Out.Normal = mul( matWorld, Normal );
    Out.Color = Color;
    return Out;
}

//--------------------------------------------------------------------------------------
// Name: PostProcessVS()
// Desc: Vertex shader for a full screen quad for post processing
//--------------------------------------------------------------------------------------
float4 PostProcessVS( const float2 ObjPos: POSITION ) : POSITION
{
    return float4( ObjPos, 0, 1 );
}

//--------------------------------------------------------------------------------------
// Pixel shader constants and structures
//--------------------------------------------------------------------------------------
uniform sampler2D albedo_texture : register(s0);
uniform sampler2D normal_texture : register(s1);
uniform sampler2D depth_texture : register(s0);

static const float EDRAM_TILE_WIDTH = 80.0f;

struct PS_IN_DEFERRED
{
    float3 Normal   : NORMAL;
    float4 Color    : COLOR;
};

struct PS_OUT_DEFERRED
{
    float4 Albedo   : COLOR0;
    float4 Normal   : COLOR1;
};

//--------------------------------------------------------------------------------------
// Name: ForwardRenderPS()
// Desc: Pixel shader for a simple forward renderer
//--------------------------------------------------------------------------------------
float4 ForwardRenderPS( float4 Color : COLOR ) : COLOR
{
    return Color;
}

//--------------------------------------------------------------------------------------
// Name: DeferredRenderPS()
// Desc: Pixel shader for a simple deferred renderer which outputs color and normals
//--------------------------------------------------------------------------------------
PS_OUT_DEFERRED DeferredRenderPS( PS_IN_DEFERRED In )
{
    PS_OUT_DEFERRED Out;
    Out.Albedo = In.Color;
    Out.Normal = float4(0.5f * normalize(In.Normal) + 0.5f, 1.0f);
    return Out;
}

//--------------------------------------------------------------------------------------
// Name: DeferredLightingPS()
// Desc: Pixel shader for a simple deferred lighting calculstion which calculates
//       the final color from the color and normals textures
//--------------------------------------------------------------------------------------
float4 DeferredLightingPS( float2 TexCoord : VPOS ) : COLOR
{
    float4 albedo;
    float4 normal;
    asm { tfetch2D albedo, TexCoord, albedo_texture, UnnormalizedTextureCoords = true };
    asm { tfetch2D normal, TexCoord, normal_texture, UnnormalizedTextureCoords = true };
    normal = 2 * normal - 1;
    float4 color = saturate( dot( normal.xyz, -LightDirection ) );
    color += 0.1 * saturate( dot( normal.xyz, LightDirection ) );
    color.a = albedo.a;
    return (color * albedo);
}

//--------------------------------------------------------------------------------------
// Name: FastDepthRestorePS()
// Desc: Pixel shader to restore the depth buffer from a texture by treating
//       it as an A8R8G8B8 texture.  For more information, see the Xbox 360 GPU
//       Performance Update Presentation from Gamefest 2007.
//--------------------------------------------------------------------------------------
float4 FastDepthRestorePS( float2 ScreenPos : VPOS ) : COLOR
{
    float ColumnIndex = ScreenPos.x / EDRAM_TILE_WIDTH;
    float HalfColumn = frac( ColumnIndex );
    float2 TexCoord = ScreenPos;
    if( HalfColumn >= 0.5 )
        TexCoord.x -= ( EDRAM_TILE_WIDTH / 2 );
    else
        TexCoord.x += ( EDRAM_TILE_WIDTH / 2 );

    float4 DepthData;
    asm
    {
        tfetch2D DepthData,
                 TexCoord,
                 depth_texture,
                 UnnormalizedTextureCoords = true
    };
    return DepthData.zyxw;
}
