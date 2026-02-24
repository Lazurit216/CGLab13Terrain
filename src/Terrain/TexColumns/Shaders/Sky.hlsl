#include "LightingUtil.hlsl"

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
    float4x4 gJitteredViewProj;
    float4x4 prevViewProj;
    Light gLights[MaxLights];
    float gScale;
    float gTessellationFactor;
};

cbuffer AtmosphereCB : register(b1)
{
    float3 RayleighScattering;
    float RayleighHeight;

    float3 MieScattering;
    float MieHeight;

    float3 SunDirection;
    float SunIntensity;

    float3 SunColor;
    float AtmosphereRadius;
};

struct VSOut
{
    float4 PosH : SV_POSITION;
    float3 ViewDir : TEXCOORD0;
};

VSOut VS(uint id : SV_VertexID)
{
    VSOut o;
    float2 pos[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };

    float2 p = pos[id];
    o.PosH = float4(p, 0.0f, 1.0f);

    // Восстанавливаем направление взгляда
    float4 worldH = mul(float4(p, 1.0f, 1.0f), gInvViewProj);
    o.ViewDir = normalize(worldH.xyz / worldH.w - gEyePosW);

    return o;
}

static const float PI = 3.14159265359f;

float RayleighPhase(float mu)
{
    return 3.0f * (1.0f + mu * mu) / (16.0f * PI);
}

float MiePhase(float mu)
{
    float g = 0.76f;
    float denom = 1.0f + g * g - 2.0f * g * mu;
    denom = max(denom, 0.001f); // Защита от деления на 0
    return (1.0f - g * g) / (4.0f * PI * pow(denom, 1.5f));
}

float4 PS(VSOut input) : SV_Target
{
    float3 viewDir = normalize(input.ViewDir);
    float3 sunDir = normalize(SunDirection);

    float mu = dot(viewDir, -sunDir);
    mu = saturate(mu);

    // Фазовая функция
    float rayleigh = RayleighPhase(mu);
    float mie = MiePhase(mu);

    // Цвет неба (Rayleigh + Mie)
    float3 skyColor =
        RayleighScattering * rayleigh * 0.5f +
        MieScattering * mie * 0.5f;

    // Солнечный диск (без переполнения)
    float sunFactor = pow(saturate(mu), 64.0f);
    float3 sunDisc = SunColor * SunIntensity * sunFactor;

    // Финальный цвет (без HDR перегрузки)
    float3 finalColor = skyColor + sunDisc;
    
    // Тонкая настройка яркости
    finalColor = finalColor * 0.8f;
    
    float exposure = 1.2f;
    finalColor *= exposure;
    finalColor = finalColor / (1.0f + finalColor);
    
    return float4(finalColor, 1.0f);
}

// Тестовый шейдер для отладки
float4 fPS(VSOut input) : SV_Target
{
    float3 viewDir = normalize(input.ViewDir);
    
    // Визуализируем направление взгляда
    return float4(viewDir * 0.5f + 0.5f, 1.0f);
}