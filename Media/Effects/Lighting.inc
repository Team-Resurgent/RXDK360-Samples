
float3 RecoverXYZFromNormalMapSample( float2 NormalMapSample )
{
    // Expand normal map sample to -1..1 range.
    NormalMapSample = NormalMapSample * 2.0 - 1.0;
    float3 result = float3( NormalMapSample.x, NormalMapSample.y, saturate( 1 - dot( NormalMapSample, NormalMapSample ) ) );
    return result;
}

float3 BumpNormalToObjectSpace( float3 Normal, float3 Binormal, float3 Tangent, float3 SampledNormal )
{
    float3 obj_binormal = normalize( Binormal );
    float3 obj_tangent = normalize( Tangent );
    float3 obj_normal = normalize( Normal );
    float3 obj_bumpnormal = obj_tangent * SampledNormal.x +
                            obj_binormal * SampledNormal.y +
                            obj_normal * SampledNormal.z;
                            
    return normalize(obj_bumpnormal);
}




float2 ComputePointLight( float3 pixel_obj_pos, 
                          float3 pixel_obj_normal, 
                          float3 obj_view_direction,
                          float4 light_obj_pos_range,
                          float SpecularExponent )
{
    // compute direction in object space to the point light
    float3 dir = light_obj_pos_range.xyz - pixel_obj_pos;
    float len = rsqrt( dot( dir, dir ) );
    float2 result = 0;
    
    if( len >= light_obj_pos_range.w )
    {   
        // normalize direction
        dir *= len;
        
        // result.x is diffuse illumination
        result.x = saturate( dot( pixel_obj_normal, dir) );
        
        // result.y is specular illumination
        result.y = pow( saturate( dot( obj_view_direction, dir ) ), SpecularExponent );
        
        // compute light intensity - linear falloff
        float Attenuation = saturate( 1.0f - (1.0f/len) * light_obj_pos_range.w );
        
        result *= Attenuation;
    }
    
    return result;
}


float2 ComputeDirLight( float3 pixel_obj_pos,
                        float3 pixel_obj_normal, 
                        float3 obj_view_direction,
                        float3 light_obj_dir,
                        float SpecularExponent,
                        float4x4 light_shadow_proj_matrix_tight,
                        sampler2D shadow_texture_tight,
                        float4x4 light_shadow_proj_matrix_scene,
                        sampler2D shadow_texture_scene )
{
    float2 result = 0;
    
    // result.x is diffuse illumination
    result.x = saturate( dot( pixel_obj_normal, -light_obj_dir ) );
    
    // result.y is specular illumination
    //result.y = pow( saturate( dot( obj_view_direction, pixel_obj_normal ) ), SpecularExponent );        
    result.y = 0;

    float4 LightPos = mul( float4( pixel_obj_pos, 1 ), light_shadow_proj_matrix_tight );
    LightPos.xyz /= LightPos.w;
    float2 LightPosMinMax = abs( LightPos.xy * 2 - 1 );
    asm
    {
        max4 LightPosMinMax.x, LightPosMinMax.xy
    };
    float ShadowValue = 0;
    if( LightPosMinMax.x <= 1.0 )
    {
        ShadowValue = ShadowMapSample( LightPos.z, LightPos.xy, shadow_texture_tight );
    }
    else
    {
        LightPos = mul( float4( pixel_obj_pos, 1 ), light_shadow_proj_matrix_scene );
        LightPos.xyz /= LightPos.w;
        ShadowValue = ShadowMapSample( LightPos.z, LightPos.xy, shadow_texture_scene );
    }
    result *= ShadowValue;
    
    return result;
}


float2 ComputeSpotLight( float3 pixel_obj_pos, 
                         float3 pixel_obj_normal, 
                         float3 obj_view_direction,
                         float4 light_obj_pos_range,
                         float3 light_obj_dir,
                         float3 light_inner_outer_angles,
                         float SpecularExponent,
                         float4x4 light_shadow_proj_matrix,
                         sampler2D shadow_texture )
{
    // compute direction in object space to the point light
    float3 dir = light_obj_pos_range - pixel_obj_pos;
    float len = rsqrt( dot( dir, dir ) );
    float2 result = 0;
    if( len >= light_obj_pos_range.w )
    {
        // normalize direction
        dir *= len;
        float AngleToAxis = dot( -dir, light_obj_dir );
        if( AngleToAxis >= light_inner_outer_angles.x )
        {
            result.x = saturate( dot( pixel_obj_normal, dir ) );
                
            result.y = pow( saturate( dot( obj_view_direction, dir ) ), SpecularExponent );
            
            float Attenuation = saturate( 1.0f - (1.0f/len) * light_obj_pos_range.w );    
                        
            float SpotLight = 1.0f - saturate( ( light_inner_outer_angles.y - AngleToAxis ) * light_inner_outer_angles.z );
            
            float4 LightPos = mul( float4( pixel_obj_pos, 1 ), light_shadow_proj_matrix );
            LightPos /= LightPos.w;
            float ShadowValue = ShadowMapSample( LightPos.z, LightPos.xy, shadow_texture );
            
            result *= SpotLight * ShadowValue * Attenuation;
        }
    }
    return result;
}
