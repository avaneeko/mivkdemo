struct VSInput {
    float4 position : POSITION0;
    float3 color    : COLOR0;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 color    : COLOR;
};

[shader("vertex")]
VSOutput vs_main(VSInput input) {
    VSOutput output;
    output.position = input.position;
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target {
    return float4(input.color, 1.0);
}