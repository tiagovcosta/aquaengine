type = "render"

include = [  ]

passes =
{
	blur =
	{
		vs = "vs_func"
		ps = "ps_func"
	}
}

instance = 
{
	constant_buffers = 
	{
		InstanceCB = 
		{
			one_over_texture_size = float2
		}
	}
	
	resources = 
	{
		input_texture = { type = Texture2D }
	}

	options =
	[
	]
}

snippets =
{
	ps_input =
	{
		hlsl =
		"""
			struct PS_INPUT
			{
				float4 position  : SV_Position;
				float2 tex_coord : TEXCOORD0;
			};
		"""
	}

	vs_func =
	{
		include = [ps_input]

		hlsl =
		"""
			
			PS_INPUT vs_main(uint vertex_id : SV_VERTEXID)
			{
				PS_INPUT output;
				output.tex_coord =  float2((vertex_id << 1) & 2, vertex_id & 2);
    			output.position = float4(output.tex_coord * float2(2,-2) + float2(-1,1), 1, 1);

				return output;
			}
		"""
	}

	ps_func =
	{
		include = [samplers, ps_input]

		hlsl =
		"""
			float4 ps_main( PS_INPUT input) : SV_TARGET0
			{
				float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);

				static const int COUNT = 4;

				[unroll]
				for(int i = 0; i < COUNT; i++)
				{
					[unroll]
					for(int j = 0; j < COUNT; j++)
					{
						result += input_texture.Load(uint3(input.position.xy, 0), int2(i-COUNT/2, j-COUNT/2));
					}
				}


				result /= COUNT*COUNT;
/*
				if(result.r < 0.9f)
					result /= 2.0f;
*/
				return result;
			}
		"""
	}
}