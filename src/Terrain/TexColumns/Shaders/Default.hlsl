//***************************************************************************************
// Default.hlsl - ������������ ������
//***************************************************************************************


// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D gDiffuseMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gDispMap : register(t2);
Texture2D gDecalDiffMap : register(t3);
Texture2D gDecalNormMap : register(t4);
Texture2D gDecalDispMap : register(t5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    float g_TessellationFactor;
    float g_Scale;
};

// Constant data that varies per material.
cbuffer cbMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

// Constant data that varies per pass.
cbuffer cbPass : register(b2)
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
    
    float gTessFactorMin; // ����������� ������ ���������� �����
    float gTessFactorMax; // ������������ ������ ���������� �����
    int gTessLevel; // ������ ���������� ������ ����� (����� ���� ������� ������������)
    float gMaxTessDistance; // ����������, �� ������� ����������� ���. ����������
    float gDisplacementScale; // ������� ��������
    int fixTessLevel;
    float DecalRadius; // 4 ����� (����� 16 ����)
    float DecalFalloffRadius; // 4 ����� (���������� � 16 ����)
    float DecalPadding; // 4 ����� (���������� � 20 ���� - ����� ���������?)
    float3 decalPosition;
    float4x4 decalViewProj;
    float4x4 DecalTexTransform;
    float DecalPadding1; // 4 ����� (���������� � 20 ���� - ����� ���������?)
    int isDecalVisible;
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
    //float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
    float3 Tan : TANGENT;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // �������������� �������, �������, ����������� � ������� ����������
    vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
    // ��� �������/����������� ���������� gWorld (����������� uniform scale).
    // ���� ���� non-uniform scale, ����� ��������-����������������� ������� ���� (����� (float3x3)gInvWorld).
    // �� ��� �������� ���� ���������� gWorld.
    vout.NormalW = normalize(mul(vin.NormalL, (float3x3) gWorld));
    vout.Tan = normalize(mul(vin.Tan, (float3x3) gWorld));

    // �������������� ���������� ���������� (� ������ ������������� ������� � ���������)
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, gMatTransform).xy;

    return vout;
}


struct HS_CONSTANT_DATA_OUTPUT
{
    float Edges[3] : SV_TessFactor;
    float Inside : SV_InsideTessFactor;
};
//Called once per patch. The patch and an index to the patch (patch ID) are passed in
HS_CONSTANT_DATA_OUTPUT ConstantsHS(InputPatch<VertexOut, 3> p, uint PatchID : SV_PrimitiveID)
{
    HS_CONSTANT_DATA_OUTPUT Out;
    
    // ��������� ����� ����� � ������� �����������
    float3 patchCenter = (p[0].PosW + p[1].PosW + p[2].PosW) / 3.0f;
    
    // ��������� ���������� �� ������ �� ������ �����
    float distanceToCamera = distance(patchCenter, gEyePosW);
    
    // ������� ������ ���������� �� ������������ ������
    float baseTessFactor = g_TessellationFactor;
    
    // ������������ ��������� �� ������ ����������
    // ��� ������ �� ������, ��� ������ ����������
    float distanceMultiplier = 1.0f;
    
    if (distanceToCamera < 10.0f)
        distanceMultiplier = 4.0f;
    else if (distanceToCamera < 20.0f)
        distanceMultiplier = 2.f;
    else if (distanceToCamera < 30.0f)
        distanceMultiplier = 1.f;
    else
        distanceMultiplier = 1.f;
    
    // �������� ������ ����������
    float finalTessFactor = baseTessFactor * distanceMultiplier;
    
    // ������������ �������� (�������� 64, ������� 1)
    finalTessFactor = clamp(finalTessFactor, 1.0f, 64.0f);
    
    // ��������� ������� ����������
    Out.Edges[0] = finalTessFactor;
    Out.Edges[1] = finalTessFactor;
    Out.Edges[2] = finalTessFactor;
    Out.Inside = finalTessFactor;
    
    // ���������� ����� (����������������� ��� ��������)
    //if (PatchID == 0) {
    //printf(distanceToCamera, baseTessFactor, distanceMultiplier, finalTessFactor);
                //distanceToCamera, baseTessFactor, distanceMultiplier, finalTessFactor);
    //}
    
    return Out;
}

struct HS_CONTROL_POINT_OUTPUT
{
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD0;
    float3 TanW : TANGENT;
};

[domain("tri")] // indicates a triangle patch (3 verts)
[partitioning("fractional_odd")] // available options: fractional_even, fractional_odd, integer, pow2
[outputtopology("triangle_cw")] // vertex ordering for the output triangles
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantsHS")] // name of the patch constant hull shader
[maxtessfactor(64.0)] //hint to the driver � the lower the better
// Pass in the input patch and an index for the control point
HS_CONTROL_POINT_OUTPUT HS(InputPatch<VertexOut, 3> patch, uint i :
SV_OutputControlPointID)
{
    HS_CONTROL_POINT_OUTPUT hout;

    // ������ �������� ������ ����������� �����
    hout.PosW = patch[i].PosW;
    hout.NormalW = patch[i].NormalW;
    hout.TexC = patch[i].TexC;
    hout.TanW = patch[i].Tan;

    return hout;
}



struct DS_OUTPUT
{
    float4 PosH : SV_POSITION; // ������� � Clip Space (!!!)
    float3 PosW : POSITION; // ������� � ���� (��� ���������)
    float3 NormalW : NORMAL; // ������� � ���� (����� ��������!)
    float2 TexC : TEXCOORD0; // ���������� ����������
    float2 decalUV : TEXCOORD1; // ���������� ����������
    float3 TanW : TANGENT; // ����������� � ���� (��� normal mapping)
    bool isInDecal : ISINDECAL;
};

[domain("tri")]
DS_OUTPUT DS(HS_CONSTANT_DATA_OUTPUT input,
             float3 domainLoc : SV_DomainLocation,
             const OutputPatch<HS_CONTROL_POINT_OUTPUT, 3> patch)
{
    DS_OUTPUT dout;

    // 1. ������������ ��������� ����������� �����
    dout.PosW = domainLoc.x * patch[0].PosW + domainLoc.y * patch[1].PosW + domainLoc.z * patch[2].PosW;
    dout.NormalW = domainLoc.x * patch[0].NormalW + domainLoc.y * patch[1].NormalW + domainLoc.z * patch[2].NormalW;
    dout.TexC = domainLoc.x * patch[0].TexC + domainLoc.y * patch[1].TexC + domainLoc.z * patch[2].TexC;
    dout.TanW = domainLoc.x * patch[0].TanW + domainLoc.y * patch[1].TanW + domainLoc.z * patch[2].TanW;
    // --- 2. ��������� ������� ������ �� ��� ������� ---
    float distToDecalCenter = distance(dout.PosW, decalPosition);
    float decalInfluence = smoothstep(DecalFalloffRadius, DecalRadius, distToDecalCenter);
     // --- 3. ��������� ��������, ���� ���� ������� ������ ---
    float finalDisplacementOffset = 0.0f; // �� ��������� �������� ���
    
    float3 unitNormalW = normalize(dout.NormalW);
    dout.TanW = normalize(dout.TanW);
    
    if (decalInfluence > 0.0f && isDecalVisible==1)
    {
        // --- 3a. ��������� UV ���������� ��� ������ ---
        float4 decalClipPos = mul(float4(dout.PosW, 1.0f), decalViewProj);
        decalClipPos.xyz /= (decalClipPos.w + 1e-6f);
        
        float4 texTransform = mul(float4(decalClipPos.xy, 0.0f, 1.0f), DecalTexTransform);
        float2 texC = texTransform.xy;
        
        //texC *= .f; // ��������� �������� � 2 ����

        dout.decalUV = texC;
        float decalDispValue = gDecalDispMap.SampleLevel(gsamLinearClamp, texC, 0.0f).r;
        
        finalDisplacementOffset = (decalDispValue - 0.5f) * gDisplacementScale * decalInfluence;
        dout.PosW += finalDisplacementOffset * dout.NormalW;
        dout.isInDecal = true;
    }
    else
    {
        float displacement = gDispMap.SampleLevel(gsamAnisotropicWrap, dout.TexC, 0).r;
        displacement *= g_Scale;
        dout.PosW += unitNormalW * displacement;
        dout.isInDecal = false;
    }
    
    // 4. �������� �������/����������� (�� ���������, ��� ��� �������� ���)
    // ����������� ����������������� ������� (����� ��� �������� � �����������)
    dout.NormalW = normalize(dout.NormalW);
    dout.TanW = normalize(dout.TanW);
  
    // 5. ������������� ����������������� ������� ������� � Clip Space
    dout.PosH = mul(float4(dout.PosW, 1.0f), gViewProj);

    // ���������� ��������� ��� ����������� �������
    return dout;
}

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;

	// Build orthonormal basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}

float4 PS(DS_OUTPUT pin) : SV_Target
{
    float4 diffuseAlbedo;
    float3 normalSample;
    if (pin.isInDecal && isDecalVisible==1)
    {
        diffuseAlbedo = gDecalDiffMap.Sample(gsamAnisotropicWrap, pin.decalUV) * gDiffuseAlbedo;
        normalSample = gDecalNormMap.Sample(gsamAnisotropicWrap, pin.decalUV).rgb; // ��������� ����� �������
    }
    else
    {
        diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
        normalSample = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb; // ��������� ����� �������
        
    }
    if (diffuseAlbedo.r == 1 && diffuseAlbedo.g == 1 && diffuseAlbedo.b == 1)
    {
        diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    }
    // ���������� �������� � ������� ��� ������

    // ���������� normal map, ���� ����
    // ������� NormalSampleToWorldSpace ������ ������������ pin.NormalW � pin.TanW �� DS
 
    float3 bumpedNormalW = NormalSampleToWorldSpace(normalSample, pin.NormalW, pin.TanW); // ��������� ��������� �������

    // ������� ��� ������ ���� ������������� � DS, �� �� ������ ������:
    bumpedNormalW = normalize(bumpedNormalW); // ���������� bumpedNormalW ��� ���������

    // ������ � ������
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    // ������ ��������� (��������� bumpedNormalW � pin.PosW)
    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess }; // �������� ����������� diffuseAlbedo

    // ��������� ������ ��������� ��� ���� ���������� �����
    float3 directLight = ComputeLighting(gLights, mat, pin.PosW, bumpedNormalW, toEyeW, 1.f); // �������� bumpedNormalW
    float4 litColor = ambient + float4(directLight, 0.0f);

    // ��������� �����, ���� ����� (fog logic...)

    // �����-��������� � �.�.
    litColor.a = diffuseAlbedo.a; // ��������� ����� �� ��������

    return litColor;
}