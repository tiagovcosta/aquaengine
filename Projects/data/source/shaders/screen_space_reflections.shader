type = "render"

include = [ "shadow_mapping" ]

passes =
{
	screen_space_reflections =
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
			project_to_pixel = float4x4
			z_thickness		 = float
		}
	}
	
	resources = 
	{
		color_texture = { type = Texture2D }
		normal_texture = { type = Texture2D }
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
    			output.position  = float4(output.tex_coord * float2(2,-2) + float2(-1,1), 1, 1);

    			float3 position_ws = mul(output.position, inv_proj).xyz;

    			output.view_ray = float3(position_ws.xy / position_ws.z, 1.0f);

				return output;
			}
		"""
	}

	ps_func =
	{
		include = ["samplers", "ps_input", "depth_utilities", "gbuffer"]

		hlsl =
		"""
			static const float PI       = 3.1415159f;
			static const float DELTA    = 0.0001f;
			static const uint NUM_STEPS = 10;

			static const float BAYER_MATRIX[4][4] = { { 1.0f/17,  9.0f/17,  3.0f/17,  11.0f/17 },
													  { 13.0f/17, 5.0f/17,  15.0f/17, 7.0f/17  },
													  { 4.0f/17,  12.0f/17, 2.0f/17,  10.0f/17 },
													  { 16.0f/17, 8.0f/17,  14.0f/17, 6.0f/17  } };

			void swap(in out float a, in out float b)
			{
				float temp = a;
				a = b;
				b = temp;
			}

			float distanceSquared(float2 A, float2 B)
			{
				A -= B;
				return dot(A, A);
			}

			bool traceScreenSpaceRay1(float3 origin, float3 direction, float stride, float jitter_fraction,
									  float max_steps, float max_ray_trace_distance, 
									  out float2 hit_pixel, out float3 hit_point, out float step_count)
			{

				// Clip ray to a near plane in 3D (doesn't have to be *the* near plane, although that would be a good idea)
				float ray_length = ((origin.z + direction.z * max_ray_trace_distance) < clip_distances.x) ?
				                    (clip_distances.x - origin.z) / direction.z :  max_ray_trace_distance;

				float3 end_point = direction * ray_length + origin;

				// Project into screen space
				float4 H0 = mul(float4(origin, 1.0), project_to_pixel);
				float4 H1 = mul(float4(end_point, 1.0), project_to_pixel);

				// There are a lot of divisions by w that can be turned into multiplications
				// at some minor precision loss...and we need to interpolate these 1/w values
				// anyway.
				//
				// Because the caller was required to clip to the near plane,
				// this homogeneous division (projecting from 4D to 2D) is guaranteed 
				// to succeed. 
				float k0 = 1.0f / H0.w;
				float k1 = 1.0f / H1.w;

				// Switch the original points to values that interpolate linearly in 2D
				float3 Q0 = origin * k0; 
				float3 Q1 = end_point * k1;

				// Screen-space endpoints
				float2 P0 = H0.xy * k0;
				float2 P1 = H1.xy * k1;

				// [Optional clipping to frustum sides here]

				// Initialize to off screen
				hit_pixel = float2(-1.0, -1.0);

				// If the line is degenerate, make it cover at least one pixel
				// to avoid handling zero-pixel extent as a special case later
				P1 += (distanceSquared(P0, P1) < 0.0001) ? 0.01 : 0.0;

				float2 delta = P1 - P0;

				// Permute so that the primary iteration is in x to reduce large branches later
				bool permute = false;

				if (abs(delta.x) < abs(delta.y))
				{
					// More-vertical line. Create a permutation that swaps x and y in the output
					permute = true;

				    // Directly swizzle the inputs
					delta = delta.yx;
					P1    = P1.yx;
					P0    = P0.yx;
				}

				// From now on, "x" is the primary iteration direction and "y" is the secondary one

				float step_direction = sign(delta.x);
				float invdx          = step_direction / delta.x;
				float2 dP            = float2(step_direction, invdx * delta.y);

				// Track the derivatives of Q and k
				float3 dQ = (Q1 - Q0) * invdx;
				float  dk = (k1 - k0) * invdx;

				// Scale derivatives by the desired pixel stride
				dP *= stride; 
				dQ *= stride; 
				dk *= stride;

				// Offset the starting values by the jitter fraction
				P0 += dP * jitter_fraction;
				Q0 += dQ * jitter_fraction;
				k0 += dk * jitter_fraction;

				// Slide P from P0 to P1, (now-homogeneous) Q from Q0 to Q1, and k from k0 to k1
				float3 Q = Q0;
				float  k = k0;

				// We track the ray depth at +/- 1/2 pixel to treat pixels as clip-space solid 
				// voxels. Because the depth at -1/2 for a given pixel will be the same as at 
				// +1/2 for the previous iteration, we actually only have to compute one value 
				// per iteration.
				float prev_z_max_estimate = origin.z;
				//float step_count          = 0.0;
				step_count          	  = 0.0;
				float ray_z_max           = prev_z_max_estimate;
				float ray_z_min           = prev_z_max_estimate;
				float scene_z_max         = ray_z_max - 1e4;

				// P1.x is never modified after this point, so pre-scale it by 
				// the step direction for a signed comparison
				float end = P1.x * step_direction;

				// We only advance the z field of Q in the inner loop, since
				// Q.xy is never used until after the loop terminates.

				for (float2 P = P0;
				    ((P.x * step_direction) <= end) && 
				    (step_count < max_steps) &&
				    ((ray_z_min > scene_z_max + z_thickness) || (ray_z_max < scene_z_max)) &&
				    (scene_z_max != 0.0);
				    P += dP, Q.z += dQ.z, k += dk, step_count += 1.0)
				{     
					hit_pixel = permute ? P.yx : P;

				    // The depth range that the ray covers within this loop
				    // iteration. Assume that the ray is moving in increasing z
				    // and swap if backwards.  Because one end of the interval is
				    // shared between adjacent iterations, we track the previous
				    // value and then swap as needed to ensure correct ordering
				    ray_z_min = prev_z_max_estimate;

				    // Compute the value at 1/2 pixel into the future
				    ray_z_max = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
					prev_z_max_estimate = ray_z_max;

					// Since we don't know if the ray is stepping forward or backward in depth,
        			// maybe swap. Note that we preserve our original z "max" estimate first.
				    if (ray_z_min < ray_z_max)
				    	swap(ray_z_min, ray_z_max);

				    // Camera-space z of the background
				    scene_z_max = convertProjDepthToView(depth_texture.Load(uint3(hit_pixel, 0)).r);
				} // pixel on ray

				Q.xy += dQ.xy * step_count;
				hit_point = Q * (1.0 / k);

				// Matches the new loop condition:
				return (ray_z_min <= scene_z_max + z_thickness) && (ray_z_max >= scene_z_max);
			}

			float4 ps_main( PS_INPUT input) : SV_TARGET0
			{
				float3 normal = GBUFFER_GET_VS_NORMAL( normal_texture.Load(uint3(input.position.xy, 0)) );

				if(!any(normal))
					return float4(0.0f, 0.0f, 0.0f, 0.0f);

				float depth = depth_texture.Load(uint3(input.position.xy, 0)).x;

				depth = convertProjDepthToView(depth);

				float3 position = input.view_ray * depth;

				float3 ray_dir = normalize( reflect(normalize(position), normalize(normal)) );
/*
				float4 p = float4(position + ray_dir, 1.0f);
				p = mul(p, proj);
				p /= p.w;

				p.xy = p.xy * float2(0.5f, -0.5f) + 0.5f;

				return color_texture.Sample(tri_linear_clamp_sampler, p.xy).rgb;
*/
				float2 hit_pixel;
				float3 hit_point;
				float step_count;

				float dither = BAYER_MATRIX[input.position.x%4][input.position.y%4];

				if(traceScreenSpaceRay1(position, ray_dir, 10, dither, 50, 16, hit_pixel, hit_point, step_count))
				{
					//step_count /= 10;
					//return step_count;

					//fade reflections near screen edges
					float dist = distance(hit_pixel*one_over_texture_size, float2(0.5f, 0.5f)) * 2.0f;

					static const float FADE_START = 0.5f;
					static const float FADE_END = 1.0f;

					float alpha = 1.0f - saturate( (dist - FADE_START) / (FADE_END - FADE_START) );

					return float4(color_texture.Load(uint3(hit_pixel, 0)).rgb, alpha);
				}
				else
					//return float3(0.01f, 0.01f, 0.01f) * color_texture.Load(uint3(0, 0, 0)).rgb;
					return float4(0.0f, 0.0f, 0.0f, 0.0f);
			}
		"""
	}
}