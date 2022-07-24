// glslangValidator -V -e main $(ProjectDir)\HLSLComputeShader.comp.hlsl -o $(ProjectDir)\HLSLComputeShader.comp.spv

[numthreads(512, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if (DTid[0] < 16)
	{
		printf("HLSL GI ID X value is: %d", DTid[0]);
	}
}
