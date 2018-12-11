struct PixelShaderInput
{
    min16float4 pos   : SV_POSITION;
    min16float3 color : COLOR0;
	min16float3 normal_cameraspace : TEXCOORD0;
	min16float3 EyeDirection_cameraspace : TEXCOORD1;
};

float3 main(PixelShaderInput input) : SV_TARGET
{
	float3 LightColor = float3(1, 1, 1);
	float LightPower = 1.0f;

	float3 MaterialDiffuseColor = float3(1, 1, 1);
	float3 MaterialAmbientColor = float3(0.1, 0.1, 0.1) * MaterialDiffuseColor;
	float3 MaterialSpecularColor = float3(0.3, 0.3, 0.3);

	float3 n = normalize(input.normal_cameraspace);
	float3 l = normalize(float3(1, 1, -1));
	float cosTheta = clamp(dot(n, l), 0, 1);

	float3 E = normalize(input.EyeDirection_cameraspace);
	float3 R = reflect(-l, n);
	float cosAlpha = clamp(dot(E, R), 0, 1);

    return MaterialAmbientColor + 
		MaterialDiffuseColor * LightColor * LightPower * cosTheta + 
		MaterialSpecularColor * LightColor * LightPower * pow(cosAlpha, 5);
}
