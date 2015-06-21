type = "render"

include = [  ]

passes =
{
	gather =
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
			num_samples = uint
			screen_size = uint2
			screen_size_times_two = uint2
			screen_size_minus_one = uint2
		}
	}
	
	resources = 
	{
		neighbor_max_texture = { type = Texture2D }
		velocity_texture     = { type = Texture2D }
		color_texture        = { type = Texture2D }
		depth_texture        = { type = Texture2D }
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
		include = [samplers, ps_input, velocity_utilities]

		hlsl =
		"""
			float cone(float dist, float r)
			{
			    return saturate(1.0 - abs(dist) / r);
			}


			float fastCone(float dist, float invR)
			{
			    return saturate(1.0 - abs(dist) * invR);
			}


			// A cone filter with maximum weight 1 at dist = 0 and min weight 0 at |v|=dist.
			float cylinder(float dist, float r)
			{
			    //return 1.0 - smoothstep(r * 0.95, r * 1.05, abs(dist));

			    // Alternative: (marginally faster on GeForce, comparable quality)
			    return sign(r - abs(dist)) * 0.5 + 0.5;

			    // The following gives nearly identical results and may be faster on some hardware,
			    // but is slower on GeForce
			    //    return (abs(dist) <= r) ? 1.0 : 0.0;
			}


			/** 0 if depth_A << depth_B, 1 if depth_A >> z_depth, fades between when they are close */
			float softDepthCompare(float depth_A, float depth_B)
			{
			    // World space distance over which we are conservative about the classification
			    // of "foreground" vs. "background".  Must be > 0.  
			    // Increase if slanted surfaces aren't blurring enough.
			    // Decrease if the background is bleeding into the foreground.
			    // Fairly insensitive
			    const float SOFT_DEPTH_EXTENT = 0.01;

			    return saturate(1.0 - (depth_B - depth_A) / SOFT_DEPTH_EXTENT);
			}


			// For linear Z values where more negative = farther away from camera
			float softZCompare(float z_A, float z_B)
			{
			    // World space distance over which we are conservative about the classification
			    // of "foreground" vs. "background".  Must be > 0.  
			    // Increase if slanted surfaces aren't blurring enough.
			    // Decrease if the background is bleeding into the foreground.
			    // Fairly insensitive
			    const float SOFT_Z_EXTENT = 0.1;

			    return saturate(1.0 - (z_A - z_B) / SOFT_Z_EXTENT);
			}

			static const float BAYER_MATRIX[4][4] = { { 1.0f/17,  9.0f/17,  3.0f/17,  11.0f/17 },
													  { 13.0f/17, 5.0f/17,  15.0f/17, 7.0f/17  },
													  { 4.0f/17,  12.0f/17, 2.0f/17,  10.0f/17 },
													  { 16.0f/17, 8.0f/17,  14.0f/17, 6.0f/17  } };

			float3 ps_main(PS_INPUT input) : SV_TARGET0
			{
				float2 neighbor_velocity       = decode_velocity(neighbor_max_texture.Load(int3(input.position.xy/max_blur_radius, 0)).xy) * screen_size;
				float length_neighbor_velocity = length(neighbor_velocity);

				float3 sum = color_texture.Load(int3(input.position.xy, 0)).rgb;

				if(length_neighbor_velocity <= 0.505f)
					return sum;

				float2 center_velocity       = decode_velocity(velocity_texture.Load(int3(input.position.xy, 0)).xy) * screen_size;
				float length_center_velocity = length(center_velocity);

				float depth_center = depth_texture.Load(int3(input.position.xy, 0)).x;

				float radius_neighbor   = length_neighbor_velocity * 0.5f;
				float radius_center     = length_center_velocity * 0.5f;
				float inv_radius_center = 1.0f / radius_center;

				float2 dir_center   = center_velocity/length_center_velocity;
				float2 dir_neighbor = neighbor_velocity/length_neighbor_velocity;

				//float total_weight = inv_radius_center * 2; // == 1.0f / length_center_velocity
				float total_weight = ((float)num_samples/40.0f) * inv_radius_center;

				sum *= total_weight;

				//float jitter = frac(sin(dot((input.position.xy), float2(12.9898,78.233))) * 43758.5453) - 0.5f;
				float jitter = BAYER_MATRIX[input.position.x%4][input.position.y%4];

				for(uint i = 0; i < num_samples; ++i)
				{
					// The original algorithm ignores the center sample, but we include it because doing so
					// produces better results for thin objects at the expense of adding a slight amount of grain.
					// That is because the jitter will bounce this slightly off the actual center
					#if SMOOTHER
						if (i == num_samples / 2) { continue; }
					#endif

					// Signed step distance from X to Y.
					// Because cone(r_Y) = 0, we need this to never reach +/- radius_neighbor, even with jitter.
					// If this value is even slightly off then gaps and bands will appear in the blur.
					// This pattern is slightly different than the original paper.
					float t = clamp(2.4 * (float(i) + 1.0 + jitter) / (num_samples + 1.0) - 1.2, -1, 1);

					float dist = t * radius_neighbor;

					float2 sampling_direction = (((i & 1) == 1) ? dir_center : dir_neighbor);

					float2 offset =
					// Alternate between the neighborhood direction and this pixel's direction.
					// This significantly helps avoid tile boundary problems when other are
					// two large velocities in a tile. Favor the neighborhood velocity on the farthest 
					// out taps (which also means that we get slightly more neighborhood taps, as we'd like)
					dist * sampling_direction;

					// Point being considered; offset and round to the nearest pixel center.
					// Then, clamp to the screen bounds
					int3 other = int3(clamp(int2(input.position.xy + offset), 0, (int2)screen_size_minus_one), 0);
					//int3 other = int3(input.position.xy + offset, 0);

					float depth_sample = depth_texture.Load(other).x;

					// is other in the foreground or background of me?
					float in_front = softDepthCompare(depth_center, depth_sample);
					float in_back  = softDepthCompare(depth_sample, depth_center);

					// Relative contribution of sample to the center
					float coverage_sample = 0.0f;

					// Blurry me, estimate background
					coverage_sample += in_back * fastCone(dist, inv_radius_center);

					float radius_sample = length(decode_velocity(velocity_texture.Load(other).xy) * screen_size) * 0.5f;

					float3 color_sample = color_texture.Load(other).rgb;

					// Blurry other over any me
					coverage_sample += in_front * cone(dist, radius_sample);

					// Mutually blurry me and other
					coverage_sample += 
					// Optimized implementation
					cylinder(dist, min(radius_center, radius_sample)) * 2.0f;

					// coverage_sample = saturate(coverage_sample * abs(dot(normalize(velocity_sample), sampling_direction)));
					// coverage_sample = saturate(dot(normalize(velocity_sample), sampling_direction));
					// Code from paper:
					// cylinder(dist, radius_center) * cylinder(dist, radius_sample) * 2.0;

					// Accumulate (with premultiplied coverage)
					sum 		 += color_sample * coverage_sample;
					total_weight += coverage_sample;
				}

				return sum/total_weight;
			}
		"""
	}
}