const float4 lightColour1 : register( c0 );
const float4 lightDirection1 : register( c1 );
const float4 lightColour2 : register( c2 );
const float4 lightDirection2 : register( c3 );
const float4 ambientColour : register( c4 );
const float4 rimLightColour : register( c5 );
const float4 lightColour3 : register( c100 );
const float4 lightDirection3 : register( c101 );

float3	DiffuseLighting( float3 viewNormal, float2 screenCoord )
{
    float3	light1;
    float3	light2;
    float3	light3;
    float	light1Shadow;	
    
    light1 = saturate( dot( viewNormal, lightDirection1 ) ) * lightColour1;
    light2 = saturate( dot( viewNormal, lightDirection2 ) ) * lightColour2;
    light3 = saturate( dot( viewNormal, lightDirection3 ) ) * lightColour3;

    light1Shadow = 1;
     
    return light1 * light1Shadow + light2 + light3;
}

float3	RimLighting( float3 viewNormal, float3 viewPosition )
{
    float3	rimLight;
    float3	viewDirection;
    
    float rimLightPower = rimLightColour.w;
    
    viewDirection = -normalize( viewPosition );
    float NdotV = saturate( dot( viewNormal, viewDirection ) );
    
    rimLight = pow( 1 - NdotV, rimLightPower );
    rimLight *= rimLightColour;
    
    return rimLight;
}

float3	EnvironmentMap( sampler2D envmap, float3 normal )
{
    float2 coord;
    
    coord.x = 0.5 + normal.x * 0.5;
    coord.y = 0.5 - normal.y * 0.5;
    
    return tex2D( envmap, coord );
}

float4 ApplyLighting(
    float4	diffuseColour, 
    float3	viewNormal, 
    float3	viewPosition, 
    float3	reflection,
    float 	reflectivity, 
    float2  screenCoord,
    bool	lightTransparency = false )
{
    float4 result;
    float3 diffuseLighting;
    float3 rimLighting;
    
    diffuseLighting = DiffuseLighting( viewNormal, screenCoord );	
    rimLighting = RimLighting( viewNormal, viewPosition );
    
    result.xyz = diffuseColour.xyz * (diffuseLighting + ambientColour) + rimLighting;
    result.xyz += reflection * reflectivity;
    
    if( lightTransparency )
    {
        result.w = diffuseColour.w + diffuseLighting.z + rimLighting.z;
        result.w += reflection.z * reflectivity;
    }
    else
    {
        result.w = diffuseColour.w;
    }
    
    return result;
}
