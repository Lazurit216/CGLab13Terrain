cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
};

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
   
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return float4(0.0f, 1.0f, 0.0f, 1.0f); // Зеленый
}