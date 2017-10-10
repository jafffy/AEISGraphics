cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
}

float4 main( float4 pos : POSITION ) : SV_POSITION
{
	float4 output;
	output = mul(pos, World);
	output = mul(output, View);
	output = mul(output, Projection);

	return output;
}