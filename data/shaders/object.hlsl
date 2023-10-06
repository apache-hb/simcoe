struct Input {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer CameraBuffer : register(b0) {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 perspective(float4 position) {
    float4 result = mul(position, model);
    result = mul(position, view);
    result = mul(result, projection);
    return result;
}

Input vsMain(float3 position : POSITION, float2 uv : TEXCOORD) {
    Input result;
    result.position = perspective(float4(position, 1.0f));
    result.uv = uv;
    return result;
}

float4 psMain(Input input) : SV_TARGET {
    return gTexture.Sample(gSampler, input.uv);
}
