
texture2D baseTexture : register(t0); // first texture

SamplerState filters[1] : register(s0); // filter 0 using CLAMP, filter 1 using WRAP

float4 main(float2 baseUV : UV) : SV_TARGET
{
	float4 baseColor = baseTexture.Sample(filters[0], baseUV); // get base color
	
	return baseColor; // return a transition based on the detail alpha
}
