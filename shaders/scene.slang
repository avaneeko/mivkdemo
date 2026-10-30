// UNDONE: Make this combined mvp and put separate world view projection into cbuffer.
struct PushConstants {
    column_major float4x4 modelView;
    column_major float4x4 proj;
    uint baseColorTexture;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> push;

[[vk::binding(0, 0)]] Texture2D<float4> Textures[];
[[vk::binding(1, 0)]] SamplerState TextureSampler;

struct VSInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 texcoord : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

[shader("vertex")]
VSOutput vs_main(VSInput input) {
    VSOutput output;
    output.position = mul(push.proj, mul(push.modelView, float4(input.position, 1.0)));
    output.texcoord = input.texcoord;
    return output;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target {
    return Textures[push.baseColorTexture].Sample(TextureSampler, input.texcoord);
}
