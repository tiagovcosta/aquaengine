type = "render"

include = [  ]

passes =
{
	tile_max =
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
			max_blur_radius = uint
		}
	}
	
	resources = 
	{
		tile_max_texture = { type = Texture2D }
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
    			output.position  = float4(output.tex_coord * float2(2,-2) + float2(-1,1), 1, 1);

				return output;
			}
		"""
	}

	ps_func =
	{
		include = [/*samplers,*/ ps_input, velocity_utilities]

		hlsl =
		"""
			float2 ps_main(PS_INPUT input) : SV_TARGET0
			{
				float2 max_velocity = 0.0f;
				float max_magnitude = -1.0f;

				int2 offset;

    			for(offset.y = -1; offset.y <= 1; ++offset.y)
    			{
        			for(offset.x = -1; offset.x <= 1; ++offset.x)
        			{
        				int3 pos = int3(clamp(input.position.xy + offset, 0, int2(127, 71)), 0);
        				
        				float2 velocity = tile_max_texture.Load(pos).xy;

        				float2 velocity_unpacked = decode_velocity(velocity);

						float magnitude = dot(velocity_unpacked, velocity_unpacked);

						if(magnitude > max_magnitude)
						{
							max_velocity  = velocity;
							max_magnitude = magnitude;
						}
        			}
        		}

				return max_velocity;
			}
		"""
	}
}