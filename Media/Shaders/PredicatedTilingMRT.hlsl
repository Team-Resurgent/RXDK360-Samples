uniform float4x4 g_matWVP : register(c0);
uniform float4x4 g_matWorld : register(c4);
                                                                               
struct VS_OUT                                                                
{                                                                            
    float4 ProjPos  : POSITION;
    float4 WorldPos : TEXCOORD0;
    float4 WorldNormal : TEXCOORD1;
};

float4 CompressWorldPos( float4 WorldPos )
{
    return ( WorldPos * 0.5 ) + 0.5;
}

float4 CompressNormal( float4 Normal )
{
    return ( normalize( Normal ) * 0.5 ) + 0.5;
}
                                                                             
VS_OUT vs_main( const float3 ObjPos : POSITION,                                
                const float3 Normal : NORMAL,                                  
                const float2 TexCoord : TEXCOORD0 )                              
{                                                                             
    VS_OUT Out;
    
    // compute world-space normal
    Out.WorldNormal = mul( float3( Normal ), g_matWorld );
    
    // compute projected position                                                              
    Out.ProjPos = mul( float4( ObjPos, 1 ), g_matWVP );

    // world-space position    
    Out.WorldPos = mul( float4( ObjPos, 1 ), g_matWorld );
    Out.WorldPos.w = 0; 
    
    return Out;                                                               
}                                                                           

struct PS_OUT_RT
{
    float4 Color[3] : COLOR0;
};                                          
                                               
uniform float4 g_lightPosWorld[4] : register(c0);
uniform float4 g_lightColor[4] : register(c4);

// The purpose of this pixel shader is to output interesting colors to three
// rendertargets.  In this case, target 0 gets some simple lighting, target 1 gets
// a colorful display of the pixel position, and target 2 gets a colorful display of
// the pixel normal.

PS_OUT_RT ps_main( VS_OUT In )               
{
    PS_OUT_RT Out;
    
    // iterate over the point lights
    float4 pointLighting = 0;
    for( int i = 0; i < 4; i++ )
    {
        // compute vector from pixel to light and get its distance
        float3 lightVector = In.WorldPos - g_lightPosWorld[i];
        float distsq = max( dot( lightVector, lightVector ), 0.001 );
        // compute a 1/dist^4 falloff
        distsq *= distsq;
        float falloff = 1 / distsq;

        // perform dot product, exclude light from the back of the pixel using saturate        
        lightVector = normalize( lightVector );
        float d = dot( In.WorldNormal, -lightVector );
        pointLighting += g_lightColor[i] * falloff * saturate( d );
    }
    
    // a little fake directional light from the top, to fill in the shape of the scene
    float4 fakeDirectionalLight = 0.2 * max( 0, dot( In.WorldNormal, float3( 0, 1, 0 ) ) );

    // copy lighting into rendertarget 0
    Out.Color[0] = pointLighting + fakeDirectionalLight;
    
    // compress and copy world position into rendertarget 1
    Out.Color[1] = CompressWorldPos( In.WorldPos );
    
    // bias and copy world normal into rendertarget 2
    float4 normal = In.WorldNormal;
    normal.w = 0;
    Out.Color[2] = CompressNormal( normal );
        
    return Out;                           
}                                            
