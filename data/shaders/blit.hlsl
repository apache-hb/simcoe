struct Input {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

Input vsMain(float3 position : POSITION, float2 uv : TEXCOORD) {
    Input result = { float4(position, 1.f), uv };
    return result;
}

float4 psMain(Input input) : SV_TARGET {
    return gTexture.Sample(gSampler, input.uv);
}
