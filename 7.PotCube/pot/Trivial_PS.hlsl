#pragma pack_matrix( row_major )

textureCUBE baseTexture : register(t0); // first texture
texture2D base2 : register(t1); // first texture

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
cbuffer BRANCH : register(b2)
{
	bool type;
	bool mypad;
	bool mypadd2;
	bool padd;
	float3 mypad3;
}

float4 main(float3 baseUVW : UV, float4 position : SV_POSITION, float3 normals : NORMAL, 
	float3 unpos : POSITION, float3 normUVW : NORMALUVW) : SV_TARGET
{
	if (type == true)
		return base2.Sample(filters[0], baseUVW);
	else
		return baseTexture.Sample(filters[0], normUVW);
}
