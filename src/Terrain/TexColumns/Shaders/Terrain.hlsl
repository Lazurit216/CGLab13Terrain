#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

Texture2D gTerrDiffMap : register(t0);
Texture2D gTerrNormMap : register(t1);
Texture2D gTerrDispMap : register(t2);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

// Constant data that varies per pass.
cbuffer cbPass : register(b1)
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
    Light gLights[MaxLights];
    
    float gScale;
    float gTessellationFactor;
};

// Constant data that varies per material.
cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

cbuffer cbTerrainTile : register(b3) 
{
    float3 gTilePosition;
    float gTileSize;
    float gMapSize;
    float gHeightScale;
    int showBoundingBox;
};

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 Tan : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
    float2 TexCl : TEXCOORD2;
    float height : HEIGHT;
};


VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
  
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, gMatTransform).xy;
    float coeff = gTileSize / gMapSize;
    vout.TexC *= coeff;
    vout.TexC += gTilePosition.xz / gMapSize;
    vout.TexCl = vin.TexC;

    float height = gTerrDispMap.SampleLevel(gsamLinearClamp, vout.TexC, 0).r;
    vout.height = height;
    
    float3 posL = vin.PosL;
    posL.y = posL.y + height * gHeightScale;

    float4 posW = mul(float4(posL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    
    // ИСПРАВЛЕННОЕ вычисление нормали
    float2 texelSize = float2(1.0f / gMapSize, 1.0f / gMapSize);
    
    // Семплируем высоты соседних точек
    float hL = gTerrDispMap.SampleLevel(gsamLinearClamp, vout.TexC + float2(-texelSize.x, 0.0f), 0).r;
    float hR = gTerrDispMap.SampleLevel(gsamLinearClamp, vout.TexC + float2(texelSize.x, 0.0f), 0).r;
    float hD = gTerrDispMap.SampleLevel(gsamLinearClamp, vout.TexC + float2(0.0f, -texelSize.y), 0).r;
    float hU = gTerrDispMap.SampleLevel(gsamLinearClamp, vout.TexC + float2(0.0f, texelSize.y), 0).r;
    
    // Вычисляем градиенты
    float dX = (hR - hL) * gHeightScale; // градиент по X
    float dZ = (hU - hD) * gHeightScale; // градиент по Z
    
    // Создаем нормаль напрямую из градиентов
    // Формула: normal = normalize((-dX, 1, -dZ))
    float3 normal = normalize(float3(-dX, 1.0f, -dZ));
    
    // Создаем тангент
    float3 tangent = normalize(float3(1.0f, dX, 0.0f));
    
    // Трансформируем в мировое пространство
    vout.NormalW = normalize(mul(normal, (float3x3) gWorld));
    vout.TangentW = normalize(mul(tangent, (float3x3) gWorld));
    
    // Трансформируем в clip space
    vout.PosH = mul(posW, gViewProj);
    
    return vout;
}
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    // Распаковываем нормаль из [0,1] в [-1,1]
    float3 normalT = 2.0f * normalMapSample - 1.0f;
    
    // Строим TBN матрицу
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);
    
    float3x3 TBN = float3x3(T, B, N);
    
    // Трансформируем нормаль в мировое пространство
    float3 bumpedNormalW = mul(normalT, TBN);
    
    return bumpedNormalW;
}

float4 ShowBoundainBoxes(VertexOut pin) : SV_Target
{
    if (showBoundingBox == 1)
    {
        // Вычисляем границы тайла как в C++ коде
        float3 bboxMin = float3(gTilePosition.x, 0.0f, gTilePosition.z);
        float3 bboxMax = float3(gTilePosition.x + gTileSize, gHeightScale, gTilePosition.z + gTileSize);
    
        // Толщина границы (можно настроить)
        float borderThickness = 0.02f * gTileSize;
    
        // Проверяем, находится ли пиксель близко к любой границе
        bool nearLeft = (pin.PosW.x - bboxMin.x) < borderThickness;
        bool nearRight = (bboxMax.x - pin.PosW.x) < borderThickness;
        bool nearBottom = (pin.PosW.z - bboxMin.z) < borderThickness;
        bool nearTop = (bboxMax.z - pin.PosW.z) < borderThickness;
    
        // Если пиксель близко к любой из четырех границ - рисуем границу
        if (nearLeft || nearRight || nearBottom || nearTop)
        {
            return float4(0.0f, 1.0f, 0.0f, 1.0f); // Зеленая граница
        }
    }
    
    // Если не рисуем границу или showBoundingBox == 0, возвращаем прозрачный
    // Это позволит основной функции WirePS продолжить выполнение
    return float4(0, 0, 0, 0);
}

float4 PS(VertexOut pin) : SV_Target
{
        // Сначала проверяем границы
    float4 borderColor = ShowBoundainBoxes(pin);
    if (borderColor.a > 0.5)  return borderColor;
    
    float4 diffuseAlbedo = gTerrDiffMap.Sample(gsamAnisotropicWrap, pin.TexC);
    diffuseAlbedo *= gDiffuseAlbedo;

    float3 normalMapSample = gTerrNormMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb;
    
    pin.NormalW = normalize(pin.NormalW);
    pin.TangentW = normalize(pin.TangentW);
    
    float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);

    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess }; 

    float3 directLight = ComputeLighting(gLights, mat, pin.PosW, bumpedNormalW, toEyeW, 1.f); 
    float4 litColor = ambient + float4(directLight, 0.0f);

    litColor.a = diffuseAlbedo.a; 

    return litColor;
}

float4 WirePS(VertexOut pin) : SV_Target
{
    // Сначала проверяем границы
    float4 borderColor = ShowBoundainBoxes(pin);
    if (borderColor.a > 0.5)  return borderColor;
    
    // Основной цвет тайла
    float minSize = 4;
    float normalizedSize = saturate((gTileSize - minSize) / (gMapSize - minSize));

    // Нелинейное преобразование для большей контрастности
    normalizedSize = smoothstep(0.0, 1.0, normalizedSize);

    // Очень насыщенные цвета
    float4 smallColor = float4(0.1f, 0.1f, 0.8f, 1.0f); // Темно-синий
    float4 largeColor = float4(0.9f, 0.1f, 0.1f, 1.0f); // Темно-красный
    return lerp(smallColor, largeColor, normalizedSize);
}