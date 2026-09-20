const float4 lightColour1 : register( c0 );
const float4 lightDirection1 : register( c1 );
const float4 lightColour2 : register( c2 );
const float4 lightDirection2 : register( c3 );
const float4 ambientColour : register( c4 );
const float4 rimLightColour : register( c5 );
const float4 lightColour3 : register( c100 );
const float4 lightDirection3 : register( c101 );
const float4 coordRemap : register( c102 );
const float4 transTint : register( c103 );
const bool   transparentAvatar : register(b0);

sampler2D     s_backBuffer     : register(s7);

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
    float2  uvColour,
    sampler2D colourSampler,
    float   highlightAmount,
    bool	lightTransparency = false )
{
    float4 result;

    if ( transparentAvatar )
    {
        // Get the rim lighting amount.
        float3 viewDirection = -normalize( viewPosition );
        float NdotV = saturate( dot( viewNormal, viewDirection ) );
        
        // Colour and alpha are a different blend than the usual rim lighting.
        float darkening = saturate((NdotV * -2.0f) + 1.8f);
        float alpha = darkening * 0.6f;
        darkening = lerp(0.7f, 1.0f, darkening);
        result.xyz = darkening;
        

        // Use the colour texture as a bump map. Sample in texture space
        // to get a normal. Often not correct but good enough.
        float4 tex00, tex01, tex10, tex11;

        asm {
            tfetch2D tex00, uvColour, colourSampler, OffsetX=-1
            tfetch2D tex01, uvColour, colourSampler, OffsetX=1
            tfetch2D tex10, uvColour, colourSampler, OffsetY=-1
            tfetch2D tex11, uvColour, colourSampler, OffsetY=1
        };

        float4 toLum = { 0.3f, 0.59f, 0.11f, 0.0f };
        
        float h00 = dot( tex00, toLum );
        float h01 = dot( tex01, toLum );
        float h10 = dot( tex10, toLum );
        float h11 = dot( tex11, toLum );
        
        float xNormal = (h01 - h00) * 2.0f;
        float yNormal = (h11 - h10) * 2.0f;

        // Adjust the camera-space normals by the bump map.
        viewNormal.x -= xNormal;
        viewNormal.y += yNormal;
        viewNormal = normalize( viewNormal );



        // Get the magnitude of the bump map here.
        float bumpAlpha = saturate((sqrt(xNormal*xNormal + yNormal*yNormal) * 5.0f));
        bumpAlpha *= 0.5f;

        // Do diffuse lighting on the bump map and add to the colour and alpha.
        float bumpLum = abs(dot(viewNormal, lightDirection1));
        result.xyz += bumpAlpha;
        alpha += bumpAlpha * 0.2f;


        // Add specular lighting of the bump map. Abs so the back isn't flat.
        float3 refVecBump = -reflect(lightDirection1, viewNormal);
        float bumpSpecular = abs(dot(refVecBump, viewDirection));

        bumpSpecular = pow( bumpSpecular, 5.0f );
        result.xyz += bumpSpecular;
        alpha += bumpSpecular * 0.7f;

 
        // Blend with the back buffer.
        screenCoord.xy += coordRemap.xy;
        screenCoord.xy *= coordRemap.zw;

        float3 backColour = tex2D( s_backBuffer, screenCoord );
        backColour.xyz = lerp( backColour, transTint, 0.2f );

        alpha = diffuseColour.w * alpha;
        
        alpha = lerp( 0.2f, 1.0f, alpha );
        result.xyz = lerp(backColour, saturate(result.xyz), alpha);
                
        result.w = diffuseColour.w;
    }
    else
    {
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
    }

    float4 hlColour = { 1.0f, 1.0f, 0.0f, 1.0f };
    result = lerp( result, hlColour, highlightAmount );

    return result;
}
