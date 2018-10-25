float4 main( /*float4 positionFromRasterizer : SV_POSITION,*/ float4 colorFromRasterizer : COLOR ) : SV_TARGET
{
	return colorFromRasterizer;
}