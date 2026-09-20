//------------------------------------------------------------------------------
// Shaders for doing shadow-volumes on the GPU
//------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
uniform float4x4 g_matWorldViewProj   : register(c0);  // World-view-projection matrix
uniform float4   g_vLightPos          : register(c4);  // Light position (may be infinite: w=0)
uniform float    g_fOffsetScale       : register(c20);  // Offset scale
uniform float    g_fOffsetScale2      : register(c21);  // Offset scale for back-facing polys


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
uniform float4   g_vConstantColor   : register(c0);
uniform float4   g_vDiffuseColor    : register(c8);

uniform sampler  DiffuseTexture     : register(s0);


//--------------------------------------------------------------------------------------
// Name: MeshVS()
// Desc: Vertex shader for a simple mesh
//--------------------------------------------------------------------------------------
struct VSOUT_MESH
{
    float4 vPosition : POSITION;
    float4 vDiffuse  : COLOR;
    float2 vTexcoord : TEXCOORD;
};

VSOUT_MESH MeshVS( float3 vPosition : POSITION,
                   float3 vNormal   : NORMAL,
                   float2 vTexcoord : TEXCOORD )
{
    VSOUT_MESH Output;
    Output.vPosition = mul( float4( vPosition, 1 ), g_matWorldViewProj );
    Output.vDiffuse  = saturate( dot( vNormal, g_vLightPos ) );
    Output.vTexcoord = vTexcoord;
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: TexModDiffusePS()
// Desc: Pixel shader for a simple mesh
//--------------------------------------------------------------------------------------
float4 TexModDiffusePS( float4 vDiffuse  : COLOR,
                        float2 vTexcoord : TEXCOORD ) : COLOR
{
    return vDiffuse * tex2D( DiffuseTexture, vTexcoord );
}


//--------------------------------------------------------------------------------------
// Name: DiffuseOnlyPS()
// Desc: Pixel shader for a simple mesh
//--------------------------------------------------------------------------------------
float4 DiffuseOnlyPS( float4 vDiffuse  : COLOR ) : COLOR
{
    return vDiffuse * g_vDiffuseColor;
}


//--------------------------------------------------------------------------------------
// Name: ShadowVolumeCpuVS()
// Desc: Vertex shader for shadow volumes that were preprocessed on the CPU
//--------------------------------------------------------------------------------------
float4 ShadowVolumeCpuVS( float3 vPosition : POSITION,
                          float  fOffset   : NORMAL ) : POSITION
{
    fOffset *= g_fOffsetScale;
    
    // Offset vertex along light direction.
    float3 vLightDir    = normalize( vPosition * g_vLightPos.w - g_vLightPos.xyz ); // V * Lw - L

    float3 vExtrudedPos = vLightDir * fOffset + vPosition; // Add offset to pos

    // Transform and output the extruded position
    return mul( float4(vExtrudedPos,1.0f), g_matWorldViewProj );
}


//--------------------------------------------------------------------------------------
// Name: ShadowVolumeCpuVS()
// Desc: Vertex shader for shadow volumes that were preprocessed on the CPU
//--------------------------------------------------------------------------------------
float4 ShadowVolumeGpuVS( float3 vPosition : POSITION, 
                          float4 vPlane    : NORMAL ) : POSITION
{
    float fOffset = ( dot( vPlane, g_vLightPos ) > 0.0f ) ? g_fOffsetScale2 : g_fOffsetScale;

    // Offset vertex along light direction.
    float3 vLightDir    = normalize( vPosition * g_vLightPos.w - g_vLightPos.xyz ); // V * Lw - L
    
    float3 vExtrudedPos = vLightDir * fOffset + vPosition; // Add offset to pos

    // Transform and output the extruded position
    return mul( float4(vExtrudedPos,1.0f), g_matWorldViewProj );
}


//-----------------------------------------------------------------------------
// Name: ScreenspaceVS()
// Desc: Passthru position-only for simple screenspace primitives
//-----------------------------------------------------------------------------
float4 ScreenspaceVS( float4 vPosition : POSITION ) : POSITION
{
    return vPosition;
}


//-----------------------------------------------------------------------------
// Name: ConstantColorPS()
// Desc: Just passes through the color specified via shader constant
//-----------------------------------------------------------------------------
float4 ConstantColorPS() : COLOR
{
    return g_vConstantColor;
}

