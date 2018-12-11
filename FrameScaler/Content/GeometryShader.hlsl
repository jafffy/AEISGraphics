struct GeometryShaderInput
{
    min16float4 pos     : SV_POSITION;
    min16float3 color   : COLOR0;
	min16float3 normal_cameraspace : TEXCOORD0;
	min16float3 EyeDirection_cameraspace : TEXCOORD1;
    uint        instId  : TEXCOORD2;
};

struct GeometryShaderOutput
{
    min16float4 pos     : SV_POSITION;
    min16float3 color   : COLOR0;
	min16float3 normal_cameraspace : TEXCOORD0;
	min16float3 EyeDirection_cameraspace : TEXCOORD1;
    uint        rtvId   : SV_RenderTargetArrayIndex;
};

[maxvertexcount(3)]
void main(triangle GeometryShaderInput input[3], inout TriangleStream<GeometryShaderOutput> outStream)
{
    GeometryShaderOutput output;
    [unroll(3)]
    for (int i = 0; i < 3; ++i)
    {
        output.pos   = input[i].pos;
        output.color = input[i].color;
		output.normal_cameraspace = input[i].normal_cameraspace;
		output.EyeDirection_cameraspace = input[i].EyeDirection_cameraspace;
        output.rtvId = input[i].instId;
        outStream.Append(output);
    }
}
