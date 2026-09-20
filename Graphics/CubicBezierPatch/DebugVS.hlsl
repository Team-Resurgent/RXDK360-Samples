//-----------------------------------------------------------------------------
// Simple vertex shader that outputs position and color.
//-----------------------------------------------------------------------------

#pragma pack_matrix(row_major)

float4x4 matWorldViewProj : register(c0);

float4 vColor : register(c4);

void main( in  float4 vLocalPos  : POSITION,
           out float4 oScreenPos : POSITION,
           out float4 oColor     : COLOR )
{
    oScreenPos = mul( vLocalPos, matWorldViewProj );

    oColor = vColor;
}
