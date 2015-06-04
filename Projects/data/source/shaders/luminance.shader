type = "render"

include = [  ]

passes =
{
	luminance =
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
			input_texel_size = float2
		}
	}
	
	resources = 
	{
		input_texture = { type = Texture2D }
	}

	options =
	[
		//{ define = "INSTANCING" }
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
			static const float4 LUMINANCE = float4(0.2125f, 0.7154f, 0.0721f, 0.0f);
			static const float DELTA 	  = 0.0001f;

			float ps_main( PS_INPUT input) : SV_TARGET0
			{
				float luminance = 0.0f;

				static const int COUNT = 2;

				[unroll]
				for(int i = 0; i < COUNT; i++)
				{
					[unroll]
					for(int j = 0; j < COUNT; j++)
					{
						//luminance += input_texture.Load(uint3(input.position.xy, 0), int2(i-COUNT/2, j-COUNT/2));

						float4 value = input_texture.Sample(tri_point_clamp_sampler, input.tex_coord + float2(i, j) * input_texel_size);

						luminance += log(dot(value, LUMINANCE) + DELTA);
					}
				}

				luminance /= COUNT*COUNT;

				return luminance;
			}
		"""
	}
}