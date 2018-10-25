#pragma pack_matrix( row_major )

struct V_IN
{
	float2 UVL : UV;
	float3 posL : POSITION;
	float4 colL : COLOR;
};

struct V_OUT
{
	float2 UVH : UV;
	float4 posH : SV_POSITION;
	float4 colH : COLOR;
};

cbuffer OBJECT : register(b0)
{
	float4x4 worldMatrix;
}

cbuffer SCENE : register(b1)
{
	float4x4 viewMatrix;
	float4x4 projectionMatrix;
}

V_OUT main(V_IN input)
{
	V_OUT output = (V_OUT)0;
	// ensures translation is preserved during matrix multiply  
	float4 localH = float4(input.posL, 1.0f);
	// move local space vertex from vertex buffer into world space.
	localH = mul(localH, worldMatrix);

	//camera
	localH = mul(localH, viewMatrix);
	localH = mul(localH, projectionMatrix);

	//output.colH = input.colL;
	output.posH = localH;
	output.UVH = input.UVL;
	output.colH = input.colL;
	return output; // send projected vertex to the rasterizer stage
}
