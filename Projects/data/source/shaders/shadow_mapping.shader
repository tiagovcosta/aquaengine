include = []

snippets =
{
	cascaded_shadow_mapping =
	{
		include = [ "samplers" ]
		
		hlsl =
		"""
		#define CASCADE_SIZE 2048
		#define NUM_CASCADES 4
		#define SHADOW_MAP_WIDTH CASCADE_SIZE * NUM_CASCADES
		#define SHADOW_MAP_HEIGHT CASCADE_SIZE
		#define SHADOW_MAP_SIZE float2(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT)

		#define WITNESS_PCF 1
		#define FilterSize_ 3

		float sampleShadowCascade(in float3 position_shadow)
		{
			float2 shadow_tex_coord = position_shadow.xy;
			float shadow_depth      = position_shadow.z;

			//return shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_tex_coord, shadow_depth).x;			
			return shadow_depth <= shadow_map.SampleLevel(tri_point_clamp_sampler, shadow_tex_coord, 0).x + 0.000f;
			//return saturate(abs(shadow_depth - shadow_map.SampleLevel(tri_point_clamp_sampler, shadow_tex_coord, 0)));
		}

		//--------------------------------------------------------------------------------------
		// Samples the shadow map cascades based on the world-space position, using edge-tap
		// smoothing PCF for filtering
		//--------------------------------------------------------------------------------------
		float sampleShadowCascadePCF(in float3 position_shadow)
		{
			float2 shadow_tex_coord = position_shadow.xy;
			float shadow_depth      = position_shadow.z;

			// Edge tap smoothing
			const int Radius     = 2;
			const int NumSamples = (Radius * 2 + 1) * (Radius * 2 + 1);

			float2 fracs      = frac(shadow_tex_coord.xy * SHADOW_MAP_SIZE);
			float left_edge   = 1.0f - fracs.x;
			float right_edge  = fracs.x;
			float top_edge    = 1.0f - fracs.y;
			float bottom_edge = fracs.y;

			float shadow_visibility = 0.0f;

			[unroll(NumSamples)]
			for (int y = -Radius; y <= Radius; y++)
			{
				[unroll(NumSamples)]
				for (int x = -Radius; x <= Radius; x++)
				{
					float2 offset       = float2(x, y) * (1.0f / SHADOW_MAP_SIZE);
					float2 sample_coord = shadow_tex_coord + offset;
					//float sample        = clamp(shadow_map.SampleCmpLevelZero(shadow_sampler, sample_coord, shadow_depth).x, 0.0f, 1.0f);
					float sample        = shadow_map.SampleCmpLevelZero(shadow_sampler, sample_coord, shadow_depth - 0.000f	).x;

					float xWeight = 1;
					float yWeight = 1;

					if(x == -Radius)
						xWeight = left_edge;
					else if(x == Radius)
						xWeight = right_edge;

					if(y == -Radius)
						yWeight = top_edge;
					else if(y == Radius)
						yWeight = bottom_edge;

					shadow_visibility += sample * xWeight * yWeight;
				}
			}

			shadow_visibility /= NumSamples;

			shadow_visibility = shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_tex_coord, shadow_depth).x;

			return shadow_visibility;
		}

		//-------------------------------------------------------------------------------------------------
		// Helper function for SampleShadowMapOptimizedPCF
		//-------------------------------------------------------------------------------------------------
		float SampleShadowMap(in float2 base_uv, in float u, in float v, in float2 shadowMapSizeInv, in float depth)
		{
		    float2 uv = base_uv + float2(u, v) * shadowMapSizeInv;

		    return shadow_map.SampleCmpLevelZero(shadow_sampler, uv, depth);
		}

		//-------------------------------------------------------------------------------------------------
		// The method used in The Witness
		//-------------------------------------------------------------------------------------------------
		float SampleShadowMapOptimizedPCF(in float3 shadowPos)
		{
		    float lightDepth = shadowPos.z;

		    //const float bias = Bias;
		    const float bias = 0.0f;

		    lightDepth -= bias;

		    float2 uv = shadowPos.xy * SHADOW_MAP_SIZE; // 1 unit - 1 texel

		    float2 shadowMapSizeInv = 1.0 / SHADOW_MAP_SIZE;

		    float2 base_uv;
		    base_uv.x = floor(uv.x + 0.5);
		    base_uv.y = floor(uv.y + 0.5);

		    float s = (uv.x + 0.5 - base_uv.x);
		    float t = (uv.y + 0.5 - base_uv.y);

		    base_uv -= float2(0.5, 0.5);
		    base_uv *= shadowMapSizeInv;

		    float sum = 0;

		    #if FilterSize_ == 2
		        return shadow_map.SampleCmpLevelZero(shadow_sampler, shadowPos.xy, lightDepth);
		    #elif FilterSize_ == 3

		        float uw0 = (3 - 2 * s);
		        float uw1 = (1 + 2 * s);

		        float u0 = (2 - s) / uw0 - 1;
		        float u1 = s / uw1 + 1;

		        float vw0 = (3 - 2 * t);
		        float vw1 = (1 + 2 * t);

		        float v0 = (2 - t) / vw0 - 1;
		        float v1 = t / vw1 + 1;

		        sum += uw0 * vw0 * SampleShadowMap(base_uv, u0, v0, shadowMapSizeInv, lightDepth);
		        sum += uw1 * vw0 * SampleShadowMap(base_uv, u1, v0, shadowMapSizeInv, lightDepth);
		        sum += uw0 * vw1 * SampleShadowMap(base_uv, u0, v1, shadowMapSizeInv, lightDepth);
		        sum += uw1 * vw1 * SampleShadowMap(base_uv, u1, v1, shadowMapSizeInv, lightDepth);

		        return sum * 1.0f / 16;

		    #elif FilterSize_ == 5

		        float uw0 = (4 - 3 * s);
		        float uw1 = 7;
		        float uw2 = (1 + 3 * s);

		        float u0 = (3 - 2 * s) / uw0 - 2;
		        float u1 = (3 + s) / uw1;
		        float u2 = s / uw2 + 2;

		        float vw0 = (4 - 3 * t);
		        float vw1 = 7;
		        float vw2 = (1 + 3 * t);

		        float v0 = (3 - 2 * t) / vw0 - 2;
		        float v1 = (3 + t) / vw1;
		        float v2 = t / vw2 + 2;

		        sum += uw0 * vw0 * SampleShadowMap(base_uv, u0, v0, shadowMapSizeInv, lightDepth);
		        sum += uw1 * vw0 * SampleShadowMap(base_uv, u1, v0, shadowMapSizeInv, lightDepth);
		        sum += uw2 * vw0 * SampleShadowMap(base_uv, u2, v0, shadowMapSizeInv, lightDepth);

		        sum += uw0 * vw1 * SampleShadowMap(base_uv, u0, v1, shadowMapSizeInv, lightDepth);
		        sum += uw1 * vw1 * SampleShadowMap(base_uv, u1, v1, shadowMapSizeInv, lightDepth);
		        sum += uw2 * vw1 * SampleShadowMap(base_uv, u2, v1, shadowMapSizeInv, lightDepth);

		        sum += uw0 * vw2 * SampleShadowMap(base_uv, u0, v2, shadowMapSizeInv, lightDepth);
		        sum += uw1 * vw2 * SampleShadowMap(base_uv, u1, v2, shadowMapSizeInv, lightDepth);
		        sum += uw2 * vw2 * SampleShadowMap(base_uv, u2, v2, shadowMapSizeInv, lightDepth);

		        return sum * 1.0f / 144;

		    #else // FilterSize_ == 7

		        float uw0 = (5 * s - 6);
		        float uw1 = (11 * s - 28);
		        float uw2 = -(11 * s + 17);
		        float uw3 = -(5 * s + 1);

		        float u0 = (4 * s - 5) / uw0 - 3;
		        float u1 = (4 * s - 16) / uw1 - 1;
		        float u2 = -(7 * s + 5) / uw2 + 1;
		        float u3 = -s / uw3 + 3;

		        float vw0 = (5 * t - 6);
		        float vw1 = (11 * t - 28);
		        float vw2 = -(11 * t + 17);
		        float vw3 = -(5 * t + 1);

		        float v0 = (4 * t - 5) / vw0 - 3;
		        float v1 = (4 * t - 16) / vw1 - 1;
		        float v2 = -(7 * t + 5) / vw2 + 1;
		        float v3 = -t / vw3 + 3;

		        sum += uw0 * vw0 * SampleShadowMap(base_uv, u0, v0, shadowMapSizeInv, lightDepth);
		        sum += uw1 * vw0 * SampleShadowMap(base_uv, u1, v0, shadowMapSizeInv, lightDepth);
		        sum += uw2 * vw0 * SampleShadowMap(base_uv, u2, v0, shadowMapSizeInv, lightDepth);
		        sum += uw3 * vw0 * SampleShadowMap(base_uv, u3, v0, shadowMapSizeInv, lightDepth);

		        sum += uw0 * vw1 * SampleShadowMap(base_uv, u0, v1, shadowMapSizeInv, lightDepth);
		        sum += uw1 * vw1 * SampleShadowMap(base_uv, u1, v1, shadowMapSizeInv, lightDepth);
		        sum += uw2 * vw1 * SampleShadowMap(base_uv, u2, v1, shadowMapSizeInv, lightDepth);
		        sum += uw3 * vw1 * SampleShadowMap(base_uv, u3, v1, shadowMapSizeInv, lightDepth);

		        sum += uw0 * vw2 * SampleShadowMap(base_uv, u0, v2, shadowMapSizeInv, lightDepth);
		        sum += uw1 * vw2 * SampleShadowMap(base_uv, u1, v2, shadowMapSizeInv, lightDepth);
		        sum += uw2 * vw2 * SampleShadowMap(base_uv, u2, v2, shadowMapSizeInv, lightDepth);
		        sum += uw3 * vw2 * SampleShadowMap(base_uv, u3, v2, shadowMapSizeInv, lightDepth);

		        sum += uw0 * vw3 * SampleShadowMap(base_uv, u0, v3, shadowMapSizeInv, lightDepth);
		        sum += uw1 * vw3 * SampleShadowMap(base_uv, u1, v3, shadowMapSizeInv, lightDepth);
		        sum += uw2 * vw3 * SampleShadowMap(base_uv, u2, v3, shadowMapSizeInv, lightDepth);
		        sum += uw3 * vw3 * SampleShadowMap(base_uv, u3, v3, shadowMapSizeInv, lightDepth);

		        return sum * 1.0f / 2704;

		    #endif
		}

		//--------------------------------------------------------------------------------------
		// Computes the visibility term by performing the shadow test
		//--------------------------------------------------------------------------------------
		float shadowVisibility(in float3 position_ws, in float depth_vs)
		{
			float shadow_visibility = 1.0f;
			uint cascade_index      = 0;

			uint i;

			// Figure out which cascade to sample from
			#if NUM_CASCADES > 1
				[unroll]
				for(i = 0; i < NUM_CASCADES - 1; ++i)
				{
					[flatten]
					if(depth_vs > cascade_splits[i])
						cascade_index = i + 1;
				}
			#endif

			float4x4 shadow_matrix  = shadow_matrices[cascade_index];
			float3 shadow_position  = mul(float4(position_ws, 1.0f), shadow_matrix).xyz;

#if WITNESS_PCF
			shadow_visibility = SampleShadowMapOptimizedPCF(shadow_position);
#else
			shadow_visibility = sampleShadowCascadePCF(shadow_position);
#endif

			// Sample the next cascade, and blend between the two results to
			// smooth the transition
			const float BLEND_THRESHOLD = 0.1f;
			float next_split            = cascade_splits[cascade_index];
			float split_size            = i == 0 ? next_split : next_split - cascade_splits[cascade_index - 1];
			float split_dist            = (next_split - depth_vs) / split_size;

			shadow_matrix  = shadow_matrices[cascade_index + 1];
			shadow_position  = mul(float4(position_ws, 1.0f), shadow_matrix).xyz;

			#if WITNESS_PCF
				float next_split_visibility = SampleShadowMapOptimizedPCF(shadow_position);
			#else
				float next_split_visibility = sampleShadowCascadePCF(shadow_position);
			#endif

			float lerp_amt              = smoothstep(0.0f, BLEND_THRESHOLD, split_dist);
			shadow_visibility           = lerp(next_split_visibility, shadow_visibility, lerp_amt);

			return shadow_visibility;
		}

		float shadowVisibilityNoFiltering(in float3 position_ws, in float depth_vs)
		{
			float shadow_visibility = 1.0f;
			uint cascade_index      = 0;

			uint i;

			// Figure out which cascade to sample from
			#if NUM_CASCADES > 1
				[unroll]
				for(i = 0; i < NUM_CASCADES - 1; ++i)
				{
					[flatten]
					if(depth_vs > cascade_splits[i])
						cascade_index = i + 1;
				}
			#endif

			float4x4 shadow_matrix = shadow_matrices[cascade_index];
			float3 shadow_position = mul(float4(position_ws, 1.0f), shadow_matrix).xyz;

			return sampleShadowCascade(shadow_position);
		}

		"""
	}
}