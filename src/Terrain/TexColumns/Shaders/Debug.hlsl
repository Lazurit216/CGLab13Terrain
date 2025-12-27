Texture2D gDebugTexture : register(t0);
SamplerState gsamPointClamp : register(s0);

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // Просто передаем позицию (уже в NDC пространстве для полноэкранного квада)
    vout.PosH = float4(vin.PosL, 1.0f);
    vout.TexC = vin.TexC;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    // Просто выводим текстуру как есть
    return gDebugTexture.Sample(gsamPointClamp, pin.TexC);
}