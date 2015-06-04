type = "render"

include = [  ]

passes =
{
	ssao =
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
		lower_level_mip = { type = Texture2D }
	}

	options =
	[
		{ define = "VERTICAL" }
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
			static const float offset[3] = { 0.0, 1.3846153846, 3.2307692308 };
			static const float weight[3] = { 0.2270270270, 0.3162162162, 0.0702702703 };

			float4 ps_main( PS_INPUT input) : SV_TARGET0
			{
				float4 result = input_texture.Load(uint3(input.position.xy, 0)) * weight[0];

			    for (int i = 1; i < 3; i++)
			    {
			    	#if VERTICAL

			        result += input_texture.Sample(tri_linear_clamp_sampler, (input.position.xy + float2(0.0f, offset[i]))*one_over_texture_size) * weight[i];
			        result += input_texture.Sample(tri_linear_clamp_sampler, (input.position.xy - float2(0.0f, offset[i]))*one_over_texture_size) * weight[i];

			        #else

			        result += input_texture.Sample(tri_linear_clamp_sampler, (input.position.xy + float2(offset[i], 0.0f))*one_over_texture_size) * weight[i];
			        result += input_texture.Sample(tri_linear_clamp_sampler, (input.position.xy - float2(offset[i], 0.0f))*one_over_texture_size) * weight[i];

			        #endif
			    }

			    #if VERTICAL
			    	// add lower level mip bloom
			        result += lower_level_mip.Sample(tri_linear_clamp_sampler, input.tex_coord);

			    #endif

				return result;
			}
		"""
	}
}