type = "render"

include = [ "shadow_mapping" ]

passes =
{
	accumulation =
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
			light_dir = float3
			light_color = float3
			shadow_matrices = { type = float4x4, array_size = 4 }
			cascade_splits  = float4
		}
	}
	
	resources = 
	{
		depth_texture = { type = Texture2D }
		shadow_map = { type = Texture2D }
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
		include = [/*samplers,*/ ps_input, depth_utilities, cascaded_shadow_mapping]

		hlsl =
		"""
			static const float PI       = 3.1415159f;
			static const float DELTA    = 0.0001f;
			static const uint NUM_STEPS = 10;

			static const float G_SCATTERING = 0.76f;

			static const float BAYER_MATRIX[4][4] = { { 1.0f/17,  9.0f/17,  3.0f/17,  11.0f/17 },
													  { 13.0f/17, 5.0f/17,  15.0f/17, 7.0f/17  },
													  { 4.0f/17,  12.0f/17, 2.0f/17,  10.0f/17 },
													  { 16.0f/17, 8.0f/17,  14.0f/17, 6.0f/17  } };

			float phase(float LdotV)
			{
				float result = 1.0f - G_SCATTERING;
				result *= result;
				result /= (4.0f * PI * pow(1.0f + G_SCATTERING * G_SCATTERING - (2.0f * G_SCATTERING) * LdotV, 1.5f));
				return result;
			}

			float3 ps_main( PS_INPUT input) : SV_TARGET0
			{
				float depth = depth_texture.Load(uint3(input.position.xy, 0)).r;

				//depth = proj._43 / (depth - proj._33);
				depth = convertProjDepthToView(depth);

				//float3 position = camera_position + input.view_ray * depth;
				float3 position = input.view_ray * depth;

				position = mul(float4(position, 1.0f), inv_view).xyz;
/*
				if(distance(position, float3(0.0f, 0.0f, 0.0f)) < 2.0f)
					return float4(1.0f, 0.0f, 0.0f, 1.0f);
				else
					return float4(0.0f, 0.0f, 0.0f, 1.0f);
*/
				float3 dir = position - camera_position;

				float len = length(dir);

				dir /= len;

				len = min(100.0f, len);

				const float LdotV = dot(light_dir, dir);

				float step_length = len / NUM_STEPS;

				float3 step = dir * step_length;

				float3 accum = 0.0f;

				float dither = BAYER_MATRIX[input.position.x%4][input.position.y%4];

				position = camera_position + step * dither;

				//[loop]
				for(uint i = 0; i < NUM_STEPS; i++)
				{
					depth = mul(float4(position, 1.0f), view).z;

					float shadow_map_depth = shadow_map.Load(uint3(input.position.xy, 0)).r;

					//accum += exp( -0.5f * step_length * i) * light_color * phase(LdotV) * shadowVisibility(position, depth) * step_length;
					accum += light_color * phase(LdotV) * shadowVisibilityNoFiltering(position, depth) * step_length;
					//accum += light_color * phase(LdotV) * shadowVisibility(position, depth);

					position += step;
				}

				//accum /= NUM_STEPS;

				return accum;
			}
		"""
	}
}