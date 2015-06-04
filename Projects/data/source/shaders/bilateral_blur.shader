type = "render"

include = [  ]

passes =
{
	bilateral_blur =
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
			//input_texel_size = float2
			one_over_texture_size = float2
		}
	}
	
	resources = 
	{
		input_texture = { type = Texture2D, element_type = float3 }
		depth_texture = { type = Texture2D, element_type = float }
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
		include = [samplers, ps_input, depth_utilities]

		hlsl =
		"""
			static const float DELTA = 0.0001f;

			/*
			static const uint NUM_SAMPLES = 3;

			static const float offset[NUM_SAMPLES] = { 0.0, 1.3846153846, 3.2307692308 };
			static const float weight[NUM_SAMPLES] = { 0.2270270270, 0.3162162162, 0.0702702703 };
			*/
			
			static const uint NUM_SAMPLES = 4;
			static const float weight[NUM_SAMPLES] = { 0.285375187f, 0.222250419f, 0.104983664f, 0.030078323f };
			
			float3 ps_main( PS_INPUT input) : SV_TARGET0
			{
				float center_depth = depth_texture.Load(uint3(input.position.xy, 0)).r;
				center_depth = convertProjDepthToView(center_depth);

				float3 result = input_texture.Load(uint3(input.position.xy, 0)) * weight[0];

				float total_weight = weight[0];

			    for (uint i = 1; i < NUM_SAMPLES; i++)
			    {
			    	#if VERTICAL

				    	//float2 sample_offset = float2(0.0f, offset[i]) * one_over_texture_size;
				    	float2 sample_offset = float2(0.0f, i * one_over_texture_size.y);

			    	#else

			    		//float2 sample_offset = float2(offset[i], 0.0f) * one_over_texture_size;
				    	float2 sample_offset = float2(i * one_over_texture_size.x, 0.0f);

			    	#endif

					float3 sample_color = input_texture.Sample(tri_linear_clamp_sampler, input.tex_coord.xy + sample_offset);
					float sample_depth  = depth_texture.Sample(tri_linear_clamp_sampler, input.tex_coord.xy + sample_offset);
					sample_depth        = convertProjDepthToView(sample_depth);

			    	float sample_weight = weight[i] * max(0.0f, 1.0f - (0.05f) * abs(sample_depth - center_depth));

			    	result += sample_color * sample_weight;
			    	total_weight += sample_weight;

			    	//------------

			    	sample_color = input_texture.Sample(tri_linear_clamp_sampler, input.tex_coord.xy - sample_offset);
			    	sample_depth = depth_texture.Sample(tri_linear_clamp_sampler, input.tex_coord.xy - sample_offset);
			    	sample_depth = convertProjDepthToView(sample_depth);

			    	sample_weight = weight[i] * max(0.0f, 1.0f - (0.05f) * abs(sample_depth - center_depth));

			    	result += sample_color * sample_weight;
			    	total_weight += sample_weight;
			    }

				return result / (total_weight);
			}
		"""
	}
}