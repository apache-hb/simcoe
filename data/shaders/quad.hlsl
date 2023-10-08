struct Input {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer OffsetBuffer : register(b0) {
    float2 offset;
    float angle;
    float aspect;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float2 rotatePoint(float2 position, float2 origin) {
    float s = sin(angle);
    float c = cos(angle);

    float2 atOrigin = position - origin;
    float2 newPosition = float2(atOrigin.x * c - atOrigin.y * s, atOrigin.x * s + atOrigin.y * c);
    return newPosition + origin;
}

Input vsMain(float3 position : POSITION, float2 uv : TEXCOORD) {
    float2 rotated = rotatePoint(position.xy, float2(0.f, 0.f)) + offset;
    Input result;
    result.position = float4(rotated.x * aspect, rotated.y, 0.f, 1.0f);
    result.uv = uv;
    return result;
}

float4 psMain(Input input) : SV_TARGET {
    return gTexture.Sample(gSampler, input.uv);
}
