// A constant buffer that stores the model transform.
cbuffer ModelConstantBuffer : register(b0)
{
    float4x4 model;
};

// A constant buffer that stores each set of view and projection matrices in column-major format.
cbuffer ViewProjectionConstantBuffer : register(b1)
{
	float4x4 view[2];
    float4x4 viewProjection[2];
};

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
    min16float3 pos     : POSITION;
	min16float3 normal  : NORMAL0;
    min16float3 color   : COLOR0;
    uint        instId  : SV_InstanceID;
};

// Per-vertex data passed to the geometry shader.
// Note that the render target array index is set here in the vertex shader.
struct VertexShaderOutput
{
    min16float4 pos     : SV_POSITION;
    min16float3 color   : COLOR0;
	min16float3 normal_cameraspace : TEXCOORD0;
	min16float3 EyeDirection_cameraspace : TEXCOORD1;
    uint        rtvId   : SV_RenderTargetArrayIndex; // SV_InstanceID % 2
};

// Simple shader to do vertex processing on the GPU.
VertexShaderOutput main(VertexShaderInput input)
{
    int idx = input.instId % 2;

    VertexShaderOutput output;

    float4 pos = float4(input.pos, 1.0f);
    pos = mul(pos, model);
    pos = mul(pos, viewProjection[idx]);


	float4 pos_cameraspace = float4(input.pos, 1.0f);
	pos_cameraspace = mul(pos_cameraspace, model);
	pos_cameraspace = mul(pos_cameraspace, view[idx]);

	float3 eyeDirection_cameraspace = float3(0, 0, 0) - pos_cameraspace.xyz;

	float4 normal_cameraspace = float4(input.normal, 0);
	normal_cameraspace = mul(normal_cameraspace, model);
	normal_cameraspace = mul(normal_cameraspace, view[idx]);

    output.pos = (min16float4)pos;
	output.normal_cameraspace = normal_cameraspace.xyz;
	output.EyeDirection_cameraspace = eyeDirection_cameraspace;
    output.color = input.color;
    output.rtvId = idx;

    return output;
}
