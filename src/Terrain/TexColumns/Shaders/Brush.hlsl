// Brush.hlsl - только compute shader

// Константные буферы для compute shader
cbuffer cbTerrainTile : register(b1)
{
    // 16 bytes
    float3 gTilePosition;
    float gTileSize;
    
    // 16 bytes  
    float3 gTerrainOffset;
    float gMapSize;
    
    // 16 bytes
    float gHeightScale;
    int showBoundingBox;
    float Padding1;
    float Padding2;
};

cbuffer cbBrush : register(b0)
{
    float4 BrushColors;

    float3 BrushWPos;
    int isBrushMode;
    int isPainting;
    float BrushRadius;
    float BrushFalofRadius;


}

// SRV для карты высот
Texture2D gTerrDispMap : register(t0);

// UAV для текстуры кисти
RWTexture2D<float4> gBrushTexture : register(u0);

// Функция для рисования круга с плавным краем
float DrawCircle(float2 pixelPos, float2 center, float radius, float feather)
{
    float dist = distance(pixelPos, center);
    
    // Если внутри радиуса - полная интенсивность
    if (dist <= radius - feather * 0.5f)
    {
        return 1.0f;
    }
    // Если в области затухания - плавный переход
    else if (dist <= radius + feather * 0.5f)
    {
        float alpha = 1.0f - smoothstep(radius - feather * 0.5f, radius + feather * 0.5f, dist);
        return alpha;
    }
    // Вне круга
    else
    {
        return 0.0f;
    }
}

[numthreads(16, 16, 1)]
void BrushCS(uint3 threadID : SV_DispatchThreadID)
{
    // Размеры текстуры (должны соответствовать mBrushTextureWidth/Height)
    const uint textureWidth = 1024;
    const uint textureHeight = 1024;
    
    // Проверяем выход за границы текстуры
    if (threadID.x >= textureWidth || threadID.y >= textureHeight)
        return;
    
    // Если не рисуем - выходим
    if (isPainting == 0)
        return;
    
    // Координаты текущего пикселя в UV пространстве [0, 1]
    float2 pixelUV = float2(threadID.xy) / float2(textureWidth, textureHeight);
    
    // ===== ВАЖНО: Преобразуем мировые координаты кисти в UV пространство текстуры =====
    // Мировые координаты кисти -> координаты на террейне -> UV координаты текстуры
    
    // 1. Вычисляем положение кисти относительно тайла
    float2 brushRelativePos = BrushWPos.xz - gTilePosition.xz;
    
    // 2. Преобразуем в UV координаты террейна [0, 1]
    float2 brushTerrainUV = float2(
        brushRelativePos.x / gMapSize,
        brushRelativePos.y / gMapSize
    );
    
    // 3. Теперь brushTerrainUV и pixelUV в одном пространстве
    
    // Вычисляем расстояние от текущего пикселя до кисти
    float distanceToBrush = distance(pixelUV, brushTerrainUV);
    
    // Радиусы в UV пространстве (делим на размер карты)
    float brushRadiusUV = BrushRadius / gMapSize;
    float brushFalloffUV = BrushFalofRadius / gMapSize;
    
    // Вычисляем интенсивность кисти
    float intensity = 0.0f;
    
    if (distanceToBrush <= brushRadiusUV)
    {
        // Внутри основного радиуса - полная интенсивность
        intensity = 1.0f;
    }
    else if (distanceToBrush <= brushRadiusUV + brushFalloffUV)
    {
        // В области затухания
        float normalizedDist = (distanceToBrush - brushRadiusUV) / brushFalloffUV;
        intensity = 1.0f - smoothstep(0.0f, 1.0f, normalizedDist);
    }
    
    // Если есть интенсивность - рисуем
    if (intensity > 0.0f)
    {
        // Читаем текущий цвет
        float4 currentColor = gBrushTexture[threadID.xy];
        
        // Цвет кисти с учетом интенсивности
        float4 brushColor = float4(BrushColors.rgb, intensity * BrushColors.a);
        
        // Аддитивное смешивание (для накопления)
        float4 newColor = currentColor + brushColor;
        
        // Или альфа-блендинг (для замены)
        // float4 newColor = lerp(currentColor, brushColor, brushColor.a);
        
        // Ограничиваем значения
        newColor = saturate(newColor);
        
        // Записываем результат
        gBrushTexture[threadID.xy] = newColor;
    }
}