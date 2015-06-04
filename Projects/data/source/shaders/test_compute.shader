type = "compute"

include = []

compute_kernels = 
{
	chess_table = 
	{
		options =
		[
			[ NO_TINT, TINT_RED, TINT_BLUE ]
			[ ASPECT_16_9, ASPECT_16_10 ]
		]
		
		resources = 
		{
			output_texture = { type = RWTexture2D, element_type = float4 }
		}
	}

	noise_one_liner = 
	{
		options =
		[
		]
		
		resources = 
		{
			output_texture = { type = RWTexture2D, element_type = float4 }
		}
	}
}

snippets =
{
	chess_table = 
	{
		hlsl =
		"""
			//#if ASPECT_16_9
			//[numthreads(32,32,1)]
			//#elif ASPECT_16_10
			[numthreads(32,32,1)]
			//#endif
			void cs_main(uint3 dtid : SV_DispatchThreadID, 
						 uint3 gid : SV_GroupID)
			{
				#if NO_TINT
				float4 color = float4(0,0,0,1);
				#elif TINT_RED
				float4 color = float4(1,0,0,1);
				#elif TINT_BLUE
				float4 color = float4(0,0,1,1);
				#endif
				
				bool p = (gid.x%2.0) == 0;
				bool q = (gid.y%2.0) == 0;
				
				//both even or both odd
				bool k = (p && q) || !(p || q);
				
				output_texture[dtid.xy] = float4(k.xxx,1) + (!k)*color;
			}
		"""
	}

	noise_one_liner = 
	{
		hlsl =
		"""
			[numthreads(32,32,1)]
			void cs_main(uint3 dtid : SV_DispatchThreadID, 
						 uint3 gid : SV_GroupID)
			{
				float rand = frac(sin(dot((dtid.xy+1),float2(12.9898,78.233))) * 43758.5453);
				//float rand = frac(sin(dot((dtid.xy+1) * delta_time * 10000.0f ,float2(12.9898,78.233))) * 43758.5453);

  				output_texture[dtid.xy] = float4(rand, rand, rand, 1.0f);
  			}
		"""
	}
}