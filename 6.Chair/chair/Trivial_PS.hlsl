#pragma pack_matrix( row_major )

texture2D baseTexture : register(t0); // first texture

SamplerState filters[1] : register(s0); // filter 0 using CLAMP, filter 1 using WRAP

cbuffer POINTLIGHT : register(b0)
{
	float3 lightPos;
	float padding;
	float3 lightDir;
	float radiusPoint;
	float4 lightCol;
}

cbuffer SPOTLIGHT : register(b1)
{
	float3 spotPos;
	float pad;
	float3 spotlightDir;
	float radius;
	float4 spotCol;
	float inner;
	float outer;
	float2 morepadding;
}

float4 main(float2 baseUV : UV, float4 position : SV_POSITION, float3 normals : NORMAL, float3 unpos : POSITION) : SV_TARGET
{
	float4 baseColor;

	baseColor = baseTexture.Sample(filters[0], baseUV);

	clip(baseColor.w < 0.85f ? -1 : 1);

	//Point light
	float3 dir = normalize(lightPos - unpos);
		float ratio = saturate(dot(dir, normals));
	float atten = 1.0 - saturate(length(lightPos - unpos) / radiusPoint);
	atten *= atten;

	//spot light
	float3 sDir = normalize(spotPos - unpos);
		float surfaceRat = saturate(dot(-sDir, spotlightDir));
	//float spotFactor = (surfaceRat > CONERATIO) ? 1 : 0;
	float spotRatio = saturate(dot(sDir, normals));
	//RESULT = SPOTFACTOR * LIGHTRATIO * LIGHTCOLOR * SURFACECOLOR
	float innerAttenua = 1.0f - saturate(length(lightPos - unpos) / radius);
	innerAttenua *= innerAttenua;
	float spotAtten = 1.0f - saturate((inner - surfaceRat) / (inner - outer));

	return (ratio * lightCol * baseColor * atten) + (spotRatio * spotCol * baseColor * innerAttenua * spotAtten);
}
