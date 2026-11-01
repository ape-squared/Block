#include "ReShade.fxh"

void VS_Pass(in uint id : SV_VertexID, out float4 pos : SV_Position, out float2 uv : TEXCOORD)
{
    PostProcessVS(id, pos, uv);
}

float4 PS_Red(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return float4(1.0, 0.0, 0.0, 1.0);
}

float4 PS_Green(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return float4(0.0, 1.0, 0.0, 1.0);
}

technique RedPass < ui_label = "Red"; >
{
    pass
    {
        VertexShader = VS_Pass;
        PixelShader = PS_Red;
    }
}

technique GreenPass < ui_label = "Green"; >
{
    pass
    {
        VertexShader = VS_Pass;
        PixelShader = PS_Green;
    }
}
