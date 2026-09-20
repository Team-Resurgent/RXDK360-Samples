uniform float4 g_vFogColor          : register(c0); // Fog color
uniform float3 g_vAmbient           : register(c1); // Material ambient color
uniform float3 g_vLightDir          : register(c2); // Light direction

float4 ApplyLighting(
    float4	diffuseColour, 
    float4  causticColour,
    float3	viewNormal,
    float fFogValue )
{
    // Diffuse lighting
    float fDirectLight = max( dot( viewNormal, g_vLightDir ), 0.0f );
    float fIndirectLight = max( dot( viewNormal, -float3(g_vLightDir.x,g_vLightDir.z,g_vLightDir.y) ), 0.0f );
    float fLight = fDirectLight + fIndirectLight * 0.5f;
    float4 Color0 = diffuseColour * float4( fLight + g_vAmbient, 1 );

    // add caustics    
    float4 Color1 = causticColour * float4( fLight, fLight, fLight, 1 );

    // Return color blended with fog
    return float4( lerp( g_vFogColor.rgb, Color0.rgb + Color1.rgb, fFogValue ), Color0.a );
}
