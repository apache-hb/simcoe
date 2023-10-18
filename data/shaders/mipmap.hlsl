// we only handle power of 2 textures
struct Input {
    uint3 groudId : SV_GroupID;
    uint3 dispatchThreadId : SV_DispatchThreadID;
};

cbuffer MipMapInfo : register(b0) {
    uint sourceLevel;
    uint totalMips;
    float2 texelSize;
};

Texture2D<float> gSource : register(t0);

RWTexture2D<float> gOutMip : register(u0);

SamplerState gSampler : register(s0);

[numthreads(16, 16, 1)]
void csMain(Input input) {
    float2 uv = texelSize * (input.dispatchThreadId.xy + 0.5f);
    gOutMip[input.dispatchThreadId.xy] = gSource.SampleLevel(gSampler, uv, sourceLevel);
}
