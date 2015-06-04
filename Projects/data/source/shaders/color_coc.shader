type = "render"

include = [  ]

passes =
{
	color_coc =
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
			nearBlurryPlaneZ = float
			nearSharpPlaneZ = float
			farSharpPlaneZ = float
			farBlurryPlaneZ = float

			/** Scales the near-field z distance to the fraction of the maximum
			    blur radius in either field. */
			nearNormalize = float

			/** Scales the far-field z distance to the fraction of the maximum
			    blur radius in either field. */
			farNormalize = float
		}
	}
	
	resources = 
	{
		color_texture = { type = Texture2D }
		depth_texture = { type = Texture2D }
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
		include = [samplers, ps_input, depth_utilities]

		hlsl =
		"""
			static const float DELTA = 0.0001f;

			float4 ps_main( PS_INPUT input) : SV_TARGET0
			{
				float4 output = 0.0f;

				//output.rgb = input_texture.Sample(tri_point_clamp_sampler, input.tex_coord).rgb;
				output.rgb = color_texture.Load(uint3(input.position.xy, 0)).rgb;

				float z = convertProjDepthToView(depth_texture.Load(uint3(input.position.xy, 0)).r);

				// Fractional radius on [0, 1]
			    float radius;
			    
			    // Note that the radius is negative in the far field.

			    if (z < nearSharpPlaneZ)
			    {
			        output.a = (nearSharpPlaneZ - max(z, nearBlurryPlaneZ)) * nearNormalize;
			    }
			    else if (z < farSharpPlaneZ)
			    {
			        output.a = 0.0; // In the focus field
			    }
			    else
			    {
			        // Produce a negative value
			        output.a = (farSharpPlaneZ - min(z, farBlurryPlaneZ)) * farNormalize;
			    }

				return output;
			}
		"""
	}
}