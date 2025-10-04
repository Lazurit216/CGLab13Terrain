#include "Default.hlsl"

float4 WirePS(DS_OUTPUT pin) : SV_Target
{
    return float4(1.f, 1.f, 1.f, 1.0f);
    //return float4(0.87f, 0.f, 0.87f, 1.f); //fioletoviy
    //return float4(0.24f, 0.67f, 0.24f, 1.0f);//сетка цвета влюбленной жабы
}