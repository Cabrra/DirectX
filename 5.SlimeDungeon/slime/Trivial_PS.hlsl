#pragma pack_matrix( row_major )

texture2D baseTexture : register(t0); // first texture

SamplerState filters[1] : register(s0); // filter 0 using CLAMP, filter 1 using WRAP

float4 main(float2 baseUV : UV, float4 position : SV_POSITION, float4 color : COLOR) : SV_TARGET
{
	float4 baseColor;
	baseColor = baseTexture.Sample(filters[0], baseUV) * color;

	clip(baseColor.w < 0.85f ? -1 : 1);
	return baseColor; // return a transition based on the detail alpha
}
