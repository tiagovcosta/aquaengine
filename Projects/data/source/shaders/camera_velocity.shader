type = "render"

include = [  ]

passes =
{
	camera_velocity =
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
			//curr_view = float4x4
			inv_view_prev_view_proj = float4x4
		}
	}
	
	resources = 
	{
		depth_texture = { type = Texture2D }
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
				float3 view_ray  : TEXCOORD1;
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

    			float3 position_ws = mul(output.position, inv_proj).xyz;

    			output.view_ray = float3(position_ws.xy / position_ws.z, 1.0f);

				return output;
			}
		"""
	}

	ps_func =
	{
		include = [/*samplers,*/ ps_input, depth_utilities, velocity_utilities]

		hlsl =
		"""
			static const float PI       = 3.1415159f;
			static const float DELTA    = 0.0001f;

			float2 ps_main( PS_INPUT input) : SV_TARGET0
			{
				float depth = depth_texture.Load(uint3(input.position.xy, 0)).r;
				depth 		= convertProjDepthToView(depth);

				//float3 position = camera_position + input.view_ray * depth;
				float3 position = input.view_ray * depth;

				float4 prev_position = mul(float4(position, 1.0f), inv_view_prev_view_proj);

				prev_position.xy /= prev_position.w;
				prev_position.y = -prev_position.y;
				prev_position.xy = prev_position.xy * 0.5f + 0.5f;

				return encode_velocity(prev_position.xy - input.tex_coord.xy);
			}
		"""
	}
}