struct Input {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 colour : COLOUR;
};

cbuffer Camera : register(b0) {
    float4x4 projection;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 perspective(float2 pos) {
    return mul(projection, float4(pos, 0.f, 1.f));
}

Input vsMain(float2 position : POSITION, float2 uv : TEXCOORD0, float4 colour : COLOUR) {
    Input result = { perspective(position), uv, colour };
    return result;
}

float4 psMain(Input input) : SV_TARGET {
    return input.colour * gTexture.Sample(gSampler, input.uv);
}
