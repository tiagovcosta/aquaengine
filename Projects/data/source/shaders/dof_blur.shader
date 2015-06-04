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
			//input_texel_size = float2
			max_coc_radius_pixels       = int
			near_blur_radius_pixels     = int
			inv_near_blur_radius_pixels = float
			blur_source_size            = int2
			near_source_size            = int2
		}
	}
	
	resources = 
	{
		blur_source_texture = { type = Texture2D }
		near_source_texture = { type = Texture2D }
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

			#if !VERTICAL
				static const int2 direction = int2(1, 0);
			#else
				static const int2 direction = int2(0, 1);
			#endif

			bool inNearField(float radius_pixels)
			{
			    return radius_pixels > 0.25;
			}

			struct PS_OUTPUT
			{
				float4 near_result : SV_TARGET0;
				float4 blur_result : SV_TARGET1;
			};

			PS_OUTPUT ps_main(PS_INPUT input)
			{
				PS_OUTPUT output;

				//output.rgb = input_texture.Sample(tri_point_clamp_sampler, input.tex_coord).rgb;
				//output.rgb = color_texture.Load(uint3(input.position.xy, 0)).rgb;

				const int GAUSSIAN_TAPS = 6;
				float gaussian[GAUSSIAN_TAPS + 1];

				// 11 x 11 separated Gaussian weights.  This does not dictate the 
				// blur kernel for depth of field; it is scaled to the actual
				// kernel at each pixel.
				gaussian[6] = 0.00000000000000;  // Weight applied to outside-radius values
				gaussian[5] = 0.04153263993208;
				gaussian[4] = 0.06352050813141;
				gaussian[3] = 0.08822292796029;
				gaussian[2] = 0.11143948794984;
				gaussian[1] = 0.12815541114232;
				gaussian[0] = 0.13425804976814;

				// Accumulate the blurry image color
				output.blur_result.rgb = float3(0.0f, 0.0f, 0.0f);
				float blur_weight_sum  = 0.0f;

				// Accumulate the near-field color and coverage
				output.near_result    = float4(0.0f, 0.0f, 0.0f, 0.0f);
				float near_weight_sum = 0.000f;

				// Location of the central filter tap (i.e., "this" pixel's location)
				// Account for the scaling down by 50% during blur
				int2 A = int2(input.position.xy) * (direction + int2(1,1));

				float packedA = blur_source_texture.Load(uint3(A, 0)).a;
				//float r_A = (packedA * 2.0 - 1.0) * max_coc_radius_pixels;
				float r_A = packedA * max_coc_radius_pixels;

				// Map r_A << 0 to 0, r_A >> 0 to 1
				float near_fieldness_A = saturate(r_A * 4.0);

				for (int delta = -max_coc_radius_pixels; delta <= max_coc_radius_pixels; ++delta)
				{
			        // Tap location near A
			        int2 B = A + (direction * delta);

			        // Packed values
			        float4 blur_input = blur_source_texture.Load(uint3(clamp(B, int2(0, 0), blur_source_size - int2(1, 1)), 0));

			        // Signed kernel radius at this tap, in pixels
			        //float r_B = (blur_input.a * 2.0 - 1.0) * float(max_coc_radius_pixels);
			        float r_B = blur_input.a * float(max_coc_radius_pixels);
			        
			        /////////////////////////////////////////////////////////////////////////////////////////////
			        // Compute blurry buffer

			        float weight = 0.0;
			        
			        float wNormal  = 
			            // Only consider mid- or background pixels (allows inpainting of the near-field)
			            float(! inNearField(r_B)) * 
			            
			            // Only blur B over A if B is closer to the viewer (allow 0.5 pixels of slop, and smooth the transition)
			            saturate(abs(r_A) - abs(r_B) + 1.5) *
			            
			            // Stretch the Gaussian extent to the radius at pixel B.
			            gaussian[clamp(int(float(abs(delta) * (GAUSSIAN_TAPS - 1)) / (0.001 + abs(r_B * 0.5))), 0, GAUSSIAN_TAPS)];

			        weight = lerp(wNormal, 1.0, near_fieldness_A);

			        // far + mid-field output 
			        blur_weight_sum += weight;
			        output.blur_result.rgb  += blur_input.rgb * weight;
			        
			        ///////////////////////////////////////////////////////////////////////////////////////////////
			        // Compute near-field super blurry buffer
			        
			        float4 near_input;

					#if !VERTICAL
			            // On the first pass, extract coverage from the near field radius
			            // Note that the near field gets a box-blur instead of a Gaussian 
			            // blur; we found that the quality improvement was not worth the 
			            // performance impact of performing the Gaussian computation here as well.

			            // Curve the contribution based on the radius.  We tuned this particular
			            // curve to blow out the near field while still providing a reasonable
			            // transition into the focus field.
			            near_input.a = float(abs(delta) <= r_B) * saturate(r_B * inv_near_blur_radius_pixels * 4.0);
			            near_input.a *= near_input.a;
			            near_input.a *= near_input.a;

			            // Compute premultiplied-alpha color
			            near_input.rgb = blur_input.rgb * near_input.a;
			            
					#else
			            // On the second pass, use the already-available alpha values
			            near_input = near_source_texture.Load(uint3(clamp(B, int2(0, 0), near_source_size - int2(1, 1)), 0));
					#endif
			                    
			        // We subsitute the following efficient expression for the more complex: weight = gaussian[clamp(int(float(abs(delta) * (GAUSSIAN_TAPS - 1)) * inv_near_blur_radius_pixels), 0, GAUSSIAN_TAPS)];
					weight             = float(abs(delta) < near_blur_radius_pixels);
					output.near_result += near_input * weight;
					near_weight_sum    += weight;
			    }
			    
				#if !VERTICAL
			        // Retain the packed radius on the first pass.  On the second pass it is not needed.
			        output.blur_result.a = packedA;
				#else
			        output.blur_result.a = 1.0;
				#endif

			    // Normalize the blur
			    output.blur_result.rgb /= blur_weight_sum;

			    // The taps are already normalized, but our Gaussian filter doesn't line up 
			    // with them perfectly so there is a slight error.
			    output.near_result /= near_weight_sum;  

				return output;
			}
		"""
	}
}