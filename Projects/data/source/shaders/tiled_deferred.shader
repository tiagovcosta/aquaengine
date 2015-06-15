type = "compute"

include = [ "shadow_mapping" ]

compute_kernels = 
{
	tiled_deferred = 
	{
		options =
		[
		]

		constant_buffers = 
		{
			KernelCB = 
			{
				viewport_inv_view_proj = float4x4
				num_tiles_x            = uint
				num_tiles_y            = uint
				num_directional_lights = uint
				num_point_lights       = uint
				num_spot_lights        = uint

				shadow_matrices = { type = float4x4, array_size = 4 }
				cascade_splits  = float4
			}
		}
		
		resources = 
		{
			directional_light_buffer_direction = { type = Buffer, element_type = float3 }
			directional_light_buffer_color     = { type = Buffer, element_type = float4 }

			point_light_buffer_center_radius = { type = Buffer, element_type = float4 }
			point_light_buffer_color         = { type = Buffer, element_type = float4 }

			spot_light_buffer_center_radius = { type = Buffer, element_type = float4 }
			spot_light_buffer_color         = { type = Buffer, element_type = float4 }
			spot_light_buffer_params        = { type = Buffer, element_type = float4 }

			output_lighting_buffer = { type = RWTexture2D, element_type = float4 }
			color_buffer           = { type = Texture2D, element_type = float4 }
			normal_buffer          = { type = Texture2D, element_type = float4 }
			depth_buffer           = { type = Texture2D, element_type = float }
			ssao_buffer            = { type = Texture2D, element_type = float }
			shadow_map             = { type = Texture2D, element_type = float }

			scattering             = { type = Texture2D, element_type = float3 }
		}
	}
}

snippets =
{
	lighting =
	{
		hlsl =
		"""
			#define PI 3.1415159f

			float3 lighting(float3 diffuse, float3 f0, float3 one_minus_f0, float roughness_squared, float roughness_power_4_minus_1,
							float3 normal, float3 view_dir,
							float3 light_dir, float3 light_color_times_pi)
			{  
		        float3 H = normalize(light_dir + view_dir);
		        float NdotH = saturate(dot(normal, H));
		        float LdotH = saturate(dot(light_dir, H));
		       
		        float NdotH_2 = NdotH * NdotH;
		       
		        float D = pow(roughness_squared/(NdotH_2 * (roughness_power_4_minus_1) + 1), 2)/PI;
		       
		        float3 fresnel_specular = f0 + one_minus_f0 * pow(1-LdotH, 5);
		       
		        float3 Rs = fresnel_specular * D / 4;
		       
		       	return (diffuse + Rs) * light_color_times_pi * saturate(dot(light_dir, normal));
			}
		"""
	}

	tiled_deferred = 
	{
		include = [ /*"normal_utilities",*/ "gbuffer", "depth_utilities", "lighting", "cascaded_shadow_mapping" ]

		hlsl =
		"""
			#define SPHEREMAP_ENCODE 1

			#define TILED_CULLING 1
			#define LIGHTS_PER_TILE_MODE 0

			#define TILE_RES 16
			#define NUM_THREADS_PER_TILE (TILE_RES*TILE_RES)
			#define MAX_NUM_LIGHTS_PER_TILE 272

			static const float4 kRadarColors[14] = 
			{
			    {0,0.9255,0.9255,1},   // cyan
			    {0,0.62745,0.9647,1},  // light blue
			    {0,0,0.9647,1},        // blue
			    {0,1,0,1},             // bright green
			    {0,0.7843,0,1},        // green
			    {0,0.5647,0,1},        // dark green
			    {1,1,0,1},             // yellow
			    {0.90588,0.75294,0,1}, // yellow-orange
			    {1,0.5647,0,1},        // orange
			    {1,0,0,1},             // bright red
			    {0.8392,0,0,1},        // red
			    {0.75294,0,0,1},       // dark red
			    {1,0,1,1},             // magenta
			    {0.6,0.3333,0.7882,1}, // purple
			};

			#define PI 3.1415159f

			float4 ConvertNumberOfLightsToRadarColor(uint nNumLightsInThisTile, uint uMaxNumLightsPerTile)
			{
			    // black for no lights
			    if( nNumLightsInThisTile == 0 ) return float4(0,0,0,1);
			    // light purple for reaching the max
			    else if( nNumLightsInThisTile == uMaxNumLightsPerTile ) return float4(0.847,0.745,0.921,1);
			    // white for going over the max
			    else if ( nNumLightsInThisTile > uMaxNumLightsPerTile ) return float4(1,1,1,1);
			    // else use weather radar colors
			    else
			    {
			        // use a log scale to provide more detail when the number of lights is smaller

			        // want to find the base b such that the logb of uMaxNumLightsPerTile is 14
			        // (because we have 14 radar colors)
			        float fLogBase = exp2(0.07142857f*log2((float)uMaxNumLightsPerTile));

			        // change of base
			        // logb(x) = log2(x) / log2(b)
			        uint nColorIndex = floor(log2((float)nNumLightsInThisTile) / log2(fLogBase));
			        return kRadarColors[nColorIndex];
			    }
			}

			//-----------------------------------------------------------------------------------------
			// Group Shared Memory (aka local data share, or LDS)
			//-----------------------------------------------------------------------------------------
			// min and max depth per tile
			groupshared uint ldsZMax;
			groupshared uint ldsZMin;

			// per-tile light list
			groupshared uint ldsLightIdxCounterA;
			groupshared uint ldsLightIdxCounterB;
			groupshared uint ldsLightIdx[2*MAX_NUM_LIGHTS_PER_TILE];

			// per-tile spot light list
			groupshared uint ldsSpotIdxCounterA;
			groupshared uint ldsSpotIdxCounterB;
			groupshared uint ldsSpotIdx[2*MAX_NUM_LIGHTS_PER_TILE];

			// this creates the standard Hessian-normal-form plane equation from three points, 
			// except it is simplified for the case where the first point is the origin
			float4 CreatePlaneEquation( float4 b, float4 c )
			{
			    float4 n;

			    // normalize(cross( b.xyz-a.xyz, c.xyz-a.xyz )), except we know "a" is the origin
			    n.xyz = normalize(cross( b.xyz, c.xyz ));

			    // -(n dot a), except we know "a" is the origin
			    n.w = 0;

			    return n;
			}

			// point-plane distance, simplified for the case where 
			// the plane passes through the origin
			float GetSignedDistanceFromPlane( float4 p, float4 eqn )
			{
			    // dot( eqn.xyz, p.xyz ) + eqn.w, , except we know eqn.w is zero 
			    // (see CreatePlaneEquation above)
			    return dot( eqn.xyz, p.xyz );
			}

			void CalculateMinMaxDepthInLds( uint3 globalIdx )
			{
			    float opaqueDepth = depth_buffer.Load( uint3(globalIdx.x,globalIdx.y,0) ).x;
			    float opaqueViewPosZ = convertProjDepthToView( opaqueDepth );
			    uint opaqueZ = asuint( opaqueViewPosZ );

			    if( opaqueDepth != 0.f )
			    {
			        InterlockedMax( ldsZMax, opaqueZ );
			        InterlockedMin( ldsZMin, opaqueZ );
			    }
			}

			float4 ConvertProjToView( float4 p )
			{
			    p = mul( p, inv_proj );
			    p /= p.w;
			    return p;
			}

			void DoLightCulling( in uint3 globalIdx, in uint localIdxFlattened, in uint3 groupIdx, out float fHalfZ )
			{
			    if( localIdxFlattened == 0 )
			    {
			        ldsZMin = 0x7f7fffff;  // FLT_MAX as a uint
			        ldsZMax = 0;
			        ldsLightIdxCounterA = 0;
			        ldsLightIdxCounterB = MAX_NUM_LIGHTS_PER_TILE;
			        ldsSpotIdxCounterA = 0;
			        ldsSpotIdxCounterB = MAX_NUM_LIGHTS_PER_TILE;
			    }

			    float4 frustumEqn[4];
			    {
			    	// construct frustum for this tile
			        uint pxm = TILE_RES*groupIdx.x;
			        uint pym = TILE_RES*groupIdx.y;
			        uint pxp = TILE_RES*(groupIdx.x+1);
			        uint pyp = TILE_RES*(groupIdx.y+1);

			        uint uWindowWidthEvenlyDivisibleByTileRes  = TILE_RES*num_tiles_x;
			        uint uWindowHeightEvenlyDivisibleByTileRes = TILE_RES*num_tiles_y;

			        // four corners of the tile, clockwise from top-left
			        float4 frustum[4];
			        frustum[0] = ConvertProjToView( float4( pxm/(float)uWindowWidthEvenlyDivisibleByTileRes*2.f-1.f, (uWindowHeightEvenlyDivisibleByTileRes-pym)/(float)uWindowHeightEvenlyDivisibleByTileRes*2.f-1.f,1.f,1.f) );
			        frustum[1] = ConvertProjToView( float4( pxp/(float)uWindowWidthEvenlyDivisibleByTileRes*2.f-1.f, (uWindowHeightEvenlyDivisibleByTileRes-pym)/(float)uWindowHeightEvenlyDivisibleByTileRes*2.f-1.f,1.f,1.f) );
			        frustum[2] = ConvertProjToView( float4( pxp/(float)uWindowWidthEvenlyDivisibleByTileRes*2.f-1.f, (uWindowHeightEvenlyDivisibleByTileRes-pyp)/(float)uWindowHeightEvenlyDivisibleByTileRes*2.f-1.f,1.f,1.f) );
			        frustum[3] = ConvertProjToView( float4( pxm/(float)uWindowWidthEvenlyDivisibleByTileRes*2.f-1.f, (uWindowHeightEvenlyDivisibleByTileRes-pyp)/(float)uWindowHeightEvenlyDivisibleByTileRes*2.f-1.f,1.f,1.f) );

			        // create plane equations for the four sides of the frustum, 
			        // with the positive half-space outside the frustum (and remember, 
			        // view space is left handed, so use the left-hand rule to determine 
			        // cross product direction)
			        for(uint i=0; i<4; i++)
			            frustumEqn[i] = CreatePlaneEquation( frustum[i], frustum[(i+1)&3] );
			    }

			    GroupMemoryBarrierWithGroupSync();

			    // calculate the min and max depth for this tile, 
			    // to form the front and back of the frustum

			    CalculateMinMaxDepthInLds( globalIdx );

			    GroupMemoryBarrierWithGroupSync();

			    float maxZ = asfloat( ldsZMax );
			    float minZ = asfloat( ldsZMin );
			    fHalfZ 	   = (minZ + maxZ) / 2.0f;

			    // loop over the lights and do a sphere vs. frustum intersection test
			    for(uint i = localIdxFlattened; i < num_point_lights; i += NUM_THREADS_PER_TILE)
			    {
			        float4 center = point_light_buffer_center_radius[i];
			        float r = center.w;
			        center.xyz = mul( float4(center.xyz, 1), view ).xyz;

			        // test if sphere is intersecting or inside frustum
			        if( ( GetSignedDistanceFromPlane( center, frustumEqn[0] ) < r ) &&
			            ( GetSignedDistanceFromPlane( center, frustumEqn[1] ) < r ) &&
			            ( GetSignedDistanceFromPlane( center, frustumEqn[2] ) < r ) &&
			            ( GetSignedDistanceFromPlane( center, frustumEqn[3] ) < r ) )
			        {
			            if( -center.z + minZ < r && center.z - fHalfZ < r )
			            {
			                // do a thread-safe increment of the list counter 
			                // and put the index of this light into the list
			                uint dstIdx = 0;
			                InterlockedAdd( ldsLightIdxCounterA, 1, dstIdx );
			                ldsLightIdx[dstIdx] = i;
			            }
			            if( -center.z + fHalfZ < r && center.z - maxZ < r )
			            {
			                // do a thread-safe increment of the list counter 
			                // and put the index of this light into the list
			                uint dstIdx = 0;
			                InterlockedAdd( ldsLightIdxCounterB, 1, dstIdx );
			                ldsLightIdx[dstIdx] = i;
			            }
			        }
			    }

			    // loop over the spot lights and do a sphere vs. frustum intersection test
			    for(uint j = localIdxFlattened; j < num_spot_lights; j += NUM_THREADS_PER_TILE)
			    {
					float4 center = spot_light_buffer_center_radius[j];
					float r       = center.w;
					center.xyz    = mul( float4(center.xyz, 1), view ).xyz;

			        // test if sphere is intersecting or inside frustum
			        if( ( GetSignedDistanceFromPlane( center, frustumEqn[0] ) < r ) &&
			            ( GetSignedDistanceFromPlane( center, frustumEqn[1] ) < r ) &&
			            ( GetSignedDistanceFromPlane( center, frustumEqn[2] ) < r ) &&
			            ( GetSignedDistanceFromPlane( center, frustumEqn[3] ) < r ) )
			        {
			            if( -center.z + minZ < r && center.z - fHalfZ < r )
			            //if( center.z - r < minZ && center.z + r > fHalfZ )
			            {
			                // do a thread-safe increment of the list counter 
			                // and put the index of this light into the list
			                uint dstIdx = 0;
			                InterlockedAdd( ldsSpotIdxCounterA, 1, dstIdx );
			                ldsSpotIdx[dstIdx] = j;
			            }
			            if( -center.z + fHalfZ < r && center.z - maxZ < r )
			            //if( center.z - r < fHalfZ && center.z + r > maxZ )
			            {
			                // do a thread-safe increment of the list counter 
			                // and put the index of this light into the list
			                uint dstIdx = 0;
			                InterlockedAdd( ldsSpotIdxCounterB, 1, dstIdx );
			                ldsSpotIdx[dstIdx] = j;
			            }
			        }
			    }

			    GroupMemoryBarrierWithGroupSync();
			}

			[numthreads(TILE_RES,TILE_RES,1)]
			void cs_main( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
			{
				uint localIdxFlattened = localIdx.x + localIdx.y*TILE_RES;

			#if TILED_CULLING
				float fHalfZ;

   				DoLightCulling( globalIdx, localIdxFlattened, groupIdx, fHalfZ );
   			#endif

				float4 normal_roughness = normal_buffer.Load( uint3(globalIdx.x,globalIdx.y,0) );

				float3 normal = GBUFFER_GET_WS_NORMAL(normal_roughness);

				float depth_buffer_depth = depth_buffer.Load( uint3(globalIdx.x,globalIdx.y,0) );

				float4 color_f0 = color_buffer.Load( uint3(globalIdx.x,globalIdx.y,0) );
				color_f0.rgb = pow(color_f0.rgb, 2.2);

				float4 position_ws_aux = mul(float4((float)globalIdx.x+0.5, (float)globalIdx.y+0.5, depth_buffer_depth, 1.0), viewport_inv_view_proj);
			    float3 position_ws = position_ws_aux.xyz / position_ws_aux.w;

			    float3 view_dir = normalize( camera_position - position_ws );

			    float depth_vs = convertProjDepthToView(depth_buffer_depth);

			    //lighting

		        float metal = saturate(color_f0.a*2-1);
		        float non_metal_spec = saturate(color_f0.a*2)*0.2;
		       
		        float3 Ks = lerp( non_metal_spec.rrr, color_f0.rgb, metal );
		        float3 Kd = lerp( color_f0.rgb, float3(0,0,0), metal );
		        float3 fresnel_diffuse = 1.0 - Ks;
		       
		        //float M = normal_roughness.w * normal_roughness.w + 0.0001f;
		        float M = GBUFFER_GET_ROUGHNESS(normal_roughness) * GBUFFER_GET_ROUGHNESS(normal_roughness) + 0.001f;
		        float M_2 = M * M;

		        float3 Rd = Kd * fresnel_diffuse * 1.0f/PI;

		       	//----------------------------------------

		       	//float3 one_minus_f0 = 1 - Ks;
		       	float roughness_power_4_minus_1 = M_2 - 1;

		       	float3 result = float3(0.0f, 0.0f, 0.0f);

		       	{
			       	for(uint i = 0; i < num_directional_lights; i++)
			       	{
			       		float4 color = directional_light_buffer_color[i];

			       		float cos_light_angle = dot(normal, directional_light_buffer_direction[i]);

			       		float3 pos_shadow = position_ws + normal * 0.3f * saturate(1.0f - cos_light_angle + 0.3f);
			       		//float3 pos_shadow = position_ws + normal * 1.0f / SHADOW_MAP_HEIGHT;

			       		result += lighting(Rd, Ks, fresnel_diffuse, M, roughness_power_4_minus_1, normal, view_dir, 
			       							directional_light_buffer_direction[i], color.rgb * color.a * 255 * PI) * 
			       							shadowVisibility(pos_shadow, depth_vs);
			       	}
		       	}

		       	{
		       	#if TILED_CULLING
		       		uint uStartIdx = (depth_vs < fHalfZ) ? 0 : MAX_NUM_LIGHTS_PER_TILE;
        			uint uEndIdx = (depth_vs < fHalfZ) ? ldsLightIdxCounterA : ldsLightIdxCounterB;

			       	for(uint i = uStartIdx; i < uEndIdx; i++)
			       	{
			       		uint nLightIndex = ldsLightIdx[i];
			    #else
			    	for(uint i = 0; i < num_point_lights; i++)
			       	{
			       		uint nLightIndex = i;
			    #endif
			       		float4 center_radius = point_light_buffer_center_radius[nLightIndex];

						float3 to_light      = center_radius.xyz - position_ws;
						float3 light_dir     = normalize(to_light);
						float light_distance = length(to_light);

					    float radius = center_radius.w;

					    if( light_distance < radius )
					    {
					        float x = light_distance / radius;

					        // fake inverse squared falloff:
					        // -(1/k)*(1-(k+1)/(1+k*x^2))
					        // k=20: -(1/20)*(1 - 21/(1+20*x^2))
					        float falloff = -0.05 + 1.05/(1+20*x*x);
					        //falloff = 1.0f;

					        float4 color = point_light_buffer_color[nLightIndex];

			       			result += lighting(Rd, Ks, fresnel_diffuse, M, roughness_power_4_minus_1, normal, view_dir, 
			       						       light_dir, color.rgb * color.a * 255 * PI) * falloff;
			       		}
			       	}
		       	}

		       	{
		       	#if TILED_CULLING
		       		uint uStartIdx = (depth_vs < fHalfZ) ? 0 : MAX_NUM_LIGHTS_PER_TILE;
        			uint uEndIdx = (depth_vs < fHalfZ) ? ldsSpotIdxCounterA : ldsSpotIdxCounterB;

			       	for(uint i = uStartIdx; i < uEndIdx; i++)
			       	{
			       		uint nLightIndex = ldsSpotIdx[i];
			    #else
			    	for(uint i = 0; i < num_spot_lights; i++)
			       	{
			       		uint nLightIndex = i;
			    #endif
			       		float4 bounding_sphere_center_radius = spot_light_buffer_center_radius[nLightIndex];

			       		float4 spot_params = spot_light_buffer_params[nLightIndex];

			       		float3 spot_light_dir;

						#if !SPHEREMAP_ENCODE
							spot_light_dir.xy = spot_params.xy;
						    // reconstruct z component of the light dir from x and y
						    spot_light_dir.z = sqrt(max(0.0f, 1.0f - spot_light_dir.x*spot_light_dir.x - spot_light_dir.y*spot_light_dir.y));

						    // the sign bit for cone angle is used to store the sign for the z component of the light dir
						    spot_light_dir.z = (spot_params.z > 0) ? spot_light_dir.z : -spot_light_dir.z;
						    //spot_light_dir = normalize(spot_light_dir);
						#else
						    float2 fenc = spot_params.xy*4-2;
						    float f = dot(fenc,fenc);
						    float g = sqrt(1-f/4);
						    //half3 n;
						    spot_light_dir.xy = fenc*g;
						    spot_light_dir.z = 1-f/2;
						#endif
					    // calculate the light position from the bounding sphere (we know the top of the cone is 
					    // r_bounding_sphere units away from the bounding sphere center along the negated light direction)
					    float3 light_position = bounding_sphere_center_radius.xyz - bounding_sphere_center_radius.w*spot_light_dir;

						float3 to_light      = light_position - position_ws;
						float3 light_dir     = normalize(to_light);
						float light_distance = length(to_light);
						float cosine_current_cone_angle = dot(-light_dir, spot_light_dir);

					    float radius = spot_params.w;

					    #if !SPHEREMAP_ENCODE
					    	float cosine_cone_angle = (spot_params.z > 0) ? spot_params.z : -spot_params.z;
					    #else
					    	float cosine_cone_angle = spot_params.z;
					    #endif

					    if( light_distance < radius && cosine_current_cone_angle > cosine_cone_angle )
					    {
					    	float radial_attenuation = (cosine_current_cone_angle - cosine_cone_angle) / (1.0 - cosine_cone_angle);
        					radial_attenuation = radial_attenuation * radial_attenuation;
        					//radial_attenuation = 1.0f;

					        float x = light_distance / radius;

					        // fake inverse squared falloff:
					        // -(1/k)*(1-(k+1)/(1+k*x^2))
					        // k=20: -(1/20)*(1 - 21/(1+20*x^2))
					        float falloff = -0.05 + 1.05/(1+20*x*x);
					        //falloff = 1.0f;

					        float4 color = spot_light_buffer_color[nLightIndex];

			       			result += lighting(Rd, Ks, fresnel_diffuse, M, roughness_power_4_minus_1, normal, view_dir, 
			       						       light_dir, color.rgb * color.a * 255 * PI) * falloff * radial_attenuation;

			       			//result = float3(0.0f, 1.0f, 1.0f);
			       		}
			       	}
		       	}

		       	//ambient light
		       	//result += float3(0.01f, 0.01f, 0.01f) * Rd * ssao_buffer.Load( uint3(globalIdx.x,globalIdx.y,0) );
		       	result += float3(0.1f, 0.1f, 0.1f) * Rd * ssao_buffer.Load( uint3(globalIdx.x,globalIdx.y,0) );
		       	
		       	result += scattering.Load( uint3(globalIdx.x/2,globalIdx.y/2,0) );

	       	#if LIGHTS_PER_TILE_MODE
				uint uStartIdx            = (depth_vs < fHalfZ) ? 0 : MAX_NUM_LIGHTS_PER_TILE;
				uint uEndIdx              = (depth_vs < fHalfZ) ? ldsLightIdxCounterA : ldsLightIdxCounterB;
				uint nNumLightsInThisTile = uEndIdx-uStartIdx;
				uEndIdx                   = (depth_vs < fHalfZ) ? ldsSpotIdxCounterA : ldsSpotIdxCounterB;
				nNumLightsInThisTile      += uEndIdx-uStartIdx;
				uint uMaxNumLightsPerTile = 2*MAX_NUM_LIGHTS_PER_TILE;  // max for points plus max for spots

				nNumLightsInThisTile += num_directional_lights;
				uMaxNumLightsPerTile += num_directional_lights;  // max for points plus max for spots

				uMaxNumLightsPerTile = 10;

			    result *= ConvertNumberOfLightsToRadarColor(nNumLightsInThisTile, uMaxNumLightsPerTile).rgb;
			#endif

		       	uint cascade_index = 0;

				// Figure out which cascade to sample from
				#if NUM_CASCADES > 1
				[unroll]
				for(uint i = 0; i < NUM_CASCADES - 1; ++i)
				{
					[flatten]
					if(depth_vs > cascade_splits[i])
						cascade_index = i + 1;
				}
				#endif

				float4 cascade_color = float4(1.0f, 1.0f, 1.0f, 1.0f);

				if(cascade_index == 0)
					cascade_color = float4(1.0f, 0.0f, 0.0f, 1.0f);
				else if(cascade_index == 1)
					cascade_color = float4(0.0f, 1.0f, 0.0f, 1.0f);
				else if(cascade_index == 2)
					cascade_color = float4(0.0f, 0.0f, 1.0f, 1.0f);
				else if(cascade_index == 3)
					cascade_color = float4(1.0f, 1.0f, 0.0f, 1.0f);

			    //output_lighting_buffer[globalIdx.xy] = float4(pow(result, 1/2.2),1);
			    output_lighting_buffer[globalIdx.xy] = float4(result,1);
			    //output_lighting_buffer[globalIdx.xy] = float4(result,1) * depth_vs/10000;
			    //output_lighting_buffer[globalIdx.xy] = float4(result,1) * cascade_color;
			    //output_lighting_buffer[globalIdx.xy] = (float4(result,1) + 1 - float4(result,1)) * shadowVisibility(position_ws, depth_vs) * depth_vs/10000;
			    //output_lighting_buffer[globalIdx.xy] = (float4(result,1) + 1 - float4(result,1)) * shadowVisibility(position_ws, depth_vs) * depth_vs/10000 * cascade_color;
			    //output_lighting_buffer[globalIdx.xy] = (float4(result,1) + 1 - float4(result,1)) * shadowVisibility(position_ws, depth_vs);
			
			    /*float maxZ = asfloat( ldsZMax );
			    float minZ = asfloat( ldsZMin );
			    float k = maxZ - minZ;

			    output_lighting_buffer[globalIdx.xy] = float4(k,k,k,1.0f);*/
			}
		"""
	}
}