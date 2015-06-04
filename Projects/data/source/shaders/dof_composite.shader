type = "render"

include = [  ]

passes =
{
	dof_blur =
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
			/*
			max_coc_radius_pixels       = int
			near_blur_radius_pixels     = int
			inv_near_blur_radius_pixels = float
			blur_source_size            = int2
			near_source_size            = int2
			*/
		}
	}
	
	resources = 
	{
		packedBuffer = { type = Texture2D }
		blurBuffer = { type = Texture2D }
		nearBuffer = { type = Texture2D }
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
			// Boost the coverage of the near field by this factor.  Should always be >= 1
			//
			// Make this larger if near-field objects seem too transparent
			//
			// Make this smaller if an obvious line is visible between the near-field blur and the mid-field sharp region
			// when looking at a textured ground plane.
			static const float coverageBoost = 1.5f;

			float grayscale(float3 c)
			{
			    return (c.r + c.g + c.b) / 3.0;
			}

			float4 ps_main(PS_INPUT input) : SV_TARGET0
			{
				float3 output;

				int2 A = int2(input.position.xy);

			    float4 pack    = packedBuffer.Load(uint3(A, 0));
				float3 sharp   = pack.rgb;
				float3 blurred = blurBuffer.Sample(tri_linear_clamp_sampler, input.tex_coord).rgb;
				float4 near    = nearBuffer.Sample(tri_linear_clamp_sampler, input.tex_coord);

				// Normalized Radius
			    float normRadius = pack.a;

			    // Fix the far field scaling factor so that it remains independent of the 
			    // near field settings
			    //normRadius *= (normRadius < 0.0) ? farRadiusRescale : 1.0;

			    // Boost the blur factor
			    //normRadius = clamp(normRadius * 2.0, -1.0, 1.0);

			    if (coverageBoost != 1.0)
			    {
			        float a = saturate(coverageBoost * near.a);
			        near.rgb = near.rgb * (a / max(near.a, 0.001f));
			        near.a = a;
			    }

			    // Decrease sharp image's contribution rapidly in the near field
			    if (normRadius > 0.1)
			    {
			        normRadius = min(normRadius * 1.5, 1.0);
			    }
/*
			    if (normRadius < 1.0f && normRadius > 0.0f)
			    {
			        normRadius = 0.0f;
			    }
*/
/*
			    if (normRadius > 0.1)
			    {
			        normRadius = 0.0f;
			    }
*/
   				output = lerp(sharp, blurred, abs(normRadius)) * (1.0 - near.a) + near.rgb;

				return float4(output, 1.0f);
			}
		"""
	}
}