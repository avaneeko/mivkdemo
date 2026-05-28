// UNDONE: Make this combined mvp and put separate world view projection into cbuffer.
struct PushConstants {
    column_major float4x4 modelView;
    column_major float4x4 proj;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> push;

struct VSInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 texcoord : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 viewNormal : COLOR0;
};

[shader("vertex")]
VSOutput vs_main(VSInput input) {
    VSOutput output;

    // View-space normal
    float3x3 normalMatrix = (float3x3)push.modelView;
    output.viewNormal = normalize(mul(normalMatrix, input.normal));

    // Clip-space
    output.position = mul(push.proj, mul(push.modelView, float4(input.position, 1.0)));

    return output;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target {
    // Normal visualization: map normal from [-1,1] to [0,1]
    return float4(input.viewNormal * 0.5 + 0.5, 1.0);
}