//--------------------------------------------------------------------------------------
// Shaders for the Edge-Based Anti-aliasing sample
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
uniform float4x4 g_matWorldViewProj   : register(c0);  // World-view-projection matrix
uniform float4   g_vLightDir          : register(c4);  // Light position
uniform float2   g_vScreenSize        : register(c9);  // Screen size in pixels

//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
uniform sampler  DiffuseTexture     : register(s0);

texture2D frameBufferTex : register(t1);
uniform sampler  FrameBuffer      = sampler_state
{
    texture = frameBufferTex;
    minfilter = LINEAR;
    magfilter = LINEAR;
};

struct VSOUT_MESH
{
    float4 vPosition : POSITION;
    float4 vDiffuse  : COLOR;
    float2 vTexcoord : TEXCOORD;
};

//--------------------------------------------------------------------------------------
// Name: MeshVS()
// Desc: Vertex shader for a simple mesh
//--------------------------------------------------------------------------------------
VSOUT_MESH MeshVS( float3 vPosition : POSITION,
                   float3 vNormal   : NORMAL,
                   float2 vTexcoord : TEXCOORD )
{
    VSOUT_MESH Output;
    Output.vPosition = mul( float4( vPosition, 1 ), g_matWorldViewProj );
    Output.vDiffuse  = saturate( dot( vNormal, g_vLightDir ) );
    Output.vTexcoord = vTexcoord;
    return Output;
}

struct VSOUT_EDGEAA
{
    float4 vPosition  : POSITION;
    float2 vDeltaPos  : TEXCOORD0;
    float2 vTex       : TEXCOORD1;
};

//--------------------------------------------------------------------------------------
// Name: EdgeAA_VS()
// Desc: Vertex shader AA Edges.  Expands 4 vertices into a 1 pixel wide quad centered
//       around the edge.  The texture coordinates of the quad are set in such a way 
//       as to report in the pixel shader the distance along each horizontal or vertical
//       segment of the line
//--------------------------------------------------------------------------------------
VSOUT_EDGEAA EdgeAA_VS( int index : INDEX )
{
    VSOUT_EDGEAA Output;
    float4 vPosition;
    float4 vNextPos;
    asm
    {
        vfetch vPosition, index, position0, UseTextureCache=true;
        vfetch vNextPos, index, position1, UseTextureCache=true;
    };

    Output.vPosition   = mul( vPosition, g_matWorldViewProj );
    float4 vNextPosOut = mul( vNextPos,  g_matWorldViewProj );

    index = index % 4;

    if( index == 0 || index == 1 )
    {
        Output.vDeltaPos = vNextPosOut/vNextPosOut.w - Output.vPosition/Output.vPosition.w;
    }
    else
    {
         Output.vDeltaPos =  Output.vPosition/Output.vPosition.w - vNextPosOut/vNextPosOut.w;
    }   

    // Keep the AA Edge in front of the mesh
    Output.vPosition.z -= .001f;
    
    // Expand the edge into a quad
    float fScale = 1.0f;
    float2 vQuadDelta;
    if( abs( Output.vDeltaPos.y ) > abs( Output.vDeltaPos.x ) )
    {
        vQuadDelta = float2( 1.0f / g_vScreenSize.x, 0.0f );
        if( Output.vDeltaPos.y < 0.0f )
        {
            fScale = -1.0f;
        }
    }
    else
    {
        vQuadDelta = float2( 0.0f, 1.0f / g_vScreenSize.y );
        if( Output.vDeltaPos.x > 0 )
        {
            fScale = -1.0f;
        }
    }

    if( index == 1 || index == 2)
    {
        Output.vPosition.xy -= fScale * vQuadDelta;
        Output.vTex = -.5f * fScale + .5f;
    }
    else
    {
        Output.vPosition.xy += fScale * vQuadDelta;
        Output.vTex = .5f * fScale + .5f;
    }
        
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: EdgeAA_PS()
// Desc: Pixel shader for AA Edges.  Uses the distance along the horizontal or vertical
//       segment of the line, as reported by the perpendicular texture coordinate, to 
//       blend between adjacent values in the frame buffer.  The final result is an 
//       anti-aliased edge.
//--------------------------------------------------------------------------------------
float4 EdgeAA_PS( VSOUT_EDGEAA In, float2 vPos : VPOS ) : COLOR
{
    float2 uv = ( vPos + .5f ) / g_vScreenSize;
    float fSubPixelPos = In.vTex.y;
    
    [flatten]
    // If X is the major axis
    if( abs( In.vDeltaPos.x ) > abs( In.vDeltaPos.y ) )
    {
        uv.y -= ( (fSubPixelPos - step( .5f, fSubPixelPos) )/ g_vScreenSize.y );
    }
    else // Y is major axis
    {
        uv.x += ( (fSubPixelPos - step( .5f, fSubPixelPos) )/ g_vScreenSize.x );
    }

    float4 t0;
    asm
    {            
        tfetch2D t0, FrameBuffer, uv, MinFilter = linear, MagFilter = linear;
    };
    return t0;
}


//--------------------------------------------------------------------------------------
// Name: TexModDiffusePS()
// Desc: Simple pixel shader with textured meshes
//--------------------------------------------------------------------------------------
float4 TexModDiffusePS( float4 vDiffuse  : COLOR,
                        float2 vTexcoord : TEXCOORD_centroid ) : COLOR
{
    return vDiffuse * tex2D( DiffuseTexture, vTexcoord );
}


//--------------------------------------------------------------------------------------
// Name: DiffuseOnlyPS()
// Desc: Pixel shader for a simple mesh
//--------------------------------------------------------------------------------------
float4 DiffuseOnlyPS( float4 vDiffuse  : COLOR ) : COLOR
{
    return vDiffuse;
}

