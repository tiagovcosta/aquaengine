type = "render"

include = []

passes =
{
	gbuffer =
	{
		condition = "!ALPHA_MASK"
		vs = "vs_func"
		ps = "ps_func"
		//blend_state = alpha_blend
		rasterizer_state = { condition = "WIREFRAME" value = "wireframe" }
	}

	gbuffer_alpha_masked =
	{
		condition = "ALPHA_MASK"
		vs = "vs_func"
		ps = "ps_func"
		//blend_state = alpha_blend
		rasterizer_state = { condition = "WIREFRAME" value = "wireframe" }
	}

	shadow =
	{
		vs = "vs_func_shadow"
		rasterizer_state = [
								{ condition = "WIREFRAME" value = "wireframe" }
								"no_cull"
							]
	}
}

mesh =
{
	streams =
	[
		[
			{ semantic = "POSITION", type = float3 }
		]
	]

	options = 
	[

	]
}

material =
{
	constant_buffers = 
	{
		MaterialCB = 
		{
			//levels_morph_consts = { type = float2, array_size = "NUM_LEVELS" }
			levels_morph_consts = { type = float4, array_size = 16 }
			eye_pos = float3
			height_scale = float
			height_offset = float
			height_map_size = float
		}
	}

	resources =
	{
		height_map = { type = Texture2D }
		color_map = { type = Texture2D }
	}
}

instance = 
{
	constant_buffers = 
	{
		InstanceCB = 
		{
			position_scale = float4
		}
	}
	
	resources = 
	{
		instance_data = { type = Buffer }
	}
	
	options =
	[
		{ define = "INSTANCING" }
		{ define = "WIREFRAME" }
		{ define = "USE_LOD_COLOR" }
		{ define = "HEIGHT_MAP" }
		{ define = "VERTEX_NORMAL" }
		{ define = "ALPHA_MASK" }
	]
	
}

snippets =
{
	height =
	{
		hlsl =
		"""
			static const float HALF_GRID_DIM = 8;
			static const float ONE_OVER_HALF_GRID_DIM = 1/HALF_GRID_DIM;

			float height(float x, float z, float2 scale_level)
			{
				#if !HEIGHT_MAP

					//return sin(x/32/scale * 2 * 3.14) * sin(z/32/scale * 2 * 3.14) * scale * 2;
					return sin(x/32 * 2 * 3.14) * sin(z/32 * 2 * 3.14) * 3;

				#else

					//x /= scale_level.x;
					//z /= scale_level.x;

					float h = height_map.Load(int3(x, z, 0)).r * height_scale + height_offset;

					if(x == 0 || x == height_map_size-1 || z == 0 || z == height_map_size-1)
						h = -1000.0f;

					//return height_map.Load(int3(x, z, 0)).r * height_scale + height_offset;
					return h;

				#endif
			}

			float2 morphVertex(float2 inPos, float2 vertex, float morphLerpK, float scale)
			{
			   float2 fracPart = (frac( inPos * float2(HALF_GRID_DIM, HALF_GRID_DIM) ) * float2(ONE_OVER_HALF_GRID_DIM, ONE_OVER_HALF_GRID_DIM) ) * scale*16;
			   return vertex.xy - fracPart * morphLerpK;
			}

			float3 world_position(float2 in_pos, float4 position_scale_level)
			{
				float3 position_world;
				position_world.xz = in_pos * position_scale_level.z + position_scale_level.xy; //scale and translate
				position_world.y = 0.0f;

				float dist = distance(eye_pos, position_world);

				int level = (int)position_scale_level.w;

				float morph = 1.0f - clamp( levels_morph_consts[level].x - dist * levels_morph_consts[level].y, 0.0, 1.0 );

				//morph = 0;

				/*
				float end = 40;
				float start = 20;
				float kk = end / (end-start);
   				float kk2 = 1.0f / (end-start);

				float morph = 1.0f - clamp( kk - dist * kk2, 0.0, 1.0 );
				*/

				position_world.xz = morphVertex(in_pos/16, position_world.xz, morph, position_scale_level.z);

				position_world.y = height(position_world.x, position_world.z, position_scale_level.zw);

				return position_world;
			}
		"""
	}

	ps_input =
	{
		hlsl =
		"""
			struct PS_INPUT
			{
				float4 position : SV_POSITION;
				#if VERTEX_NORMAL
					float3 normal   : NORMAL;
				#endif
				float2 tex   : TEXCOORD0;
				#if USE_LOD_COLOR
					float3 color : COLOR;
				#endif
			    //float2 tex_coord0 : TEXCOORD0;
			};
		"""
	}

	vs_func =
	{
		include = [height, ps_input]

		hlsl =
		"""
			struct VS_INPUT
			{
				float3 pos 			: POSITION;
				//float2 tex_coord 	: TEXCOORD;
			};

			PS_INPUT vs_main(VS_INPUT input)
			{
				PS_INPUT output;

				float3 position_world = world_position(input.pos.xz, position_scale);

				output.position = mul(float4(position_world, 1.0f), view_proj);

				#if VERTEX_NORMAL
					float h01 = height(position_world.x - position_scale.z, position_world.z, position_scale.z);
					float h10 = height(position_world.x, position_world.z - position_scale.z, position_scale.z);
					float h21 = height(position_world.x + position_scale.z, position_world.z, position_scale.z);
					float h12 = height(position_world.x, position_world.z + position_scale.z, position_scale.z);
					/*
					float3 p0 = input.pos;

					float3 p1 = input.pos;
					p1.x += position_scale.z;
					input.pos.y = height(p1.x, p1.z, position_scale.z);

					float3 p2 = input.pos;
					p2.z += position_scale.z;
					input.pos.y = height(p2.x, p2.z, position_scale.z);
					*/
					float3 va = normalize(float3(2 * position_scale.z, h21 - h01, 0));
	    			float3 vb = normalize(float3(0, h12 - h10, 2 * position_scale.z));

					output.normal = normalize(cross(vb, va));

					output.normal.x = h01 - h21;
					output.normal.z = h10 - h12;

					output.normal.y = 2;

					output.normal = normalize(output.normal);
				#endif

				output.tex = position_world.xz;

				#if USE_LOD_COLOR

					int level = (int)position_scale.w;

					if(level == 0)
					{
						output.color = float3(1.0f, 0.0f, 0.0f);
					} 
					else if(level == 1)
					{
						output.color = float3(0.0f, 1.0f, 0.0f);
					} 
					else// if(level == 2)
					{
						output.color = float3(0.0f, 0.0f, 1.0f);
					}
				#endif

				return output;
			}
		"""
	}

	vs_func_shadow =
	{
		include = [height]

		hlsl =
		"""
			struct VS_INPUT
			{
				float3 pos : POSITION;
			};

			float4 vs_main(VS_INPUT input) : SV_POSITION
			{
				return mul(float4(world_position(input.pos.xz, position_scale), 1.0f), view_proj);
			}
		"""
	}

	ps_func =
	{
		include = [samplers, height, ps_input, gbuffer]

		hlsl =
		"""
			float height2(float x, float z, float2 scale_level)
			{
				return height_map.SampleLevel(tri_linear_clamp_sampler, float2(x, z)/height_map_size, 0).r * height_scale + height_offset;
			}

			float height3(float2 tc)
			{
				return height_map.SampleLevel(tri_linear_clamp_sampler, tc/height_map_size, 0).r * height_scale + height_offset;
			}

			float3 Sobel( float2 tc )
			{
				// Useful aliases
				//float2 pxSz = float2( 1.0f / heightMapDimensions.x, 1.0f / heightMapDimensions.y );
				float2 pxSz = float2(1.0f, 1.0f);
				
				// Compute the necessary offsets:
				float2 o00 = tc + float2( -pxSz.x, -pxSz.y );
				float2 o10 = tc + float2(    0.0f, -pxSz.y );
				float2 o20 = tc + float2(  pxSz.x, -pxSz.y );

				float2 o01 = tc + float2( -pxSz.x, 0.0f    );
				float2 o21 = tc + float2(  pxSz.x, 0.0f    );

				float2 o02 = tc + float2( -pxSz.x,  pxSz.y );
				float2 o12 = tc + float2(    0.0f,  pxSz.y );
				float2 o22 = tc + float2(  pxSz.x,  pxSz.y );

				// Use of the sobel filter requires the eight samples
				// surrounding the current pixel:
				float h00 = height3(o00);
				float h10 = height3(o10);
				float h20 = height3(o20);

				float h01 = height3(o01);
				float h21 = height3(o21);

				float h02 = height3(o02);
				float h12 = height3(o12);
				float h22 = height3(o22);

				// Evaluate the Sobel filters
				float Gx = h00 - h20 + 2.0f * h01 - 2.0f * h21 + h02 - h22;
				float Gy = h00 + 2.0f * h10 + h20 - h02 - 2.0f * h12 - h22;

				// Generate the missing Z
				float Gz = 0.01f * sqrt( max(0.0f, 1.0f - Gx * Gx - Gy * Gy ) );

				// Make sure the returned normal is of unit length
				return normalize( float3( 2.0f * Gx, Gz, 2.0f * Gy ) );
			}

			GBUFFER_OUT ps_main(PS_INPUT input)
			{	
				GBUFFER_OUT output;

				float offset = 1.0f;

				float3 normal;

				#if VERTEX_NORMAL

					normal = input.normal;

				#else

					#if !HEIGHT_MAP

						float h01 = height(input.tex.x - offset, input.tex.y, position_scale.zw);
						float h10 = height(input.tex.x, input.tex.y - offset, position_scale.zw);
						float h21 = height(input.tex.x + offset, input.tex.y, position_scale.zw);
						float h12 = height(input.tex.x, input.tex.y + offset, position_scale.zw);

					#else

						//offset *= position_scale.w + 1;

						float h01 = height2(input.tex.x - offset, input.tex.y, position_scale.zw);
						float h10 = height2(input.tex.x, input.tex.y - offset, position_scale.zw);
						float h21 = height2(input.tex.x + offset, input.tex.y, position_scale.zw);
						float h12 = height2(input.tex.x, input.tex.y + offset, position_scale.zw);

					#endif

					normal.x = h01 - h21;
					normal.z = h10 - h12;
					normal.y = 2;

					//input.normal = Sobel(input.tex.xy);

				#endif

				#if VIEW_SPACE_NORMAL
				normal = mul(float4(normal, 0.0f), view).xyz;
				#endif
				
				//float NdL = saturate(dot(normal, normalize(float3(1.0f, 1.0f, 0.0f))));

				//float3 color = float3(0.3f, 0.8f, 0.0f);
				float4 color = color_map.SampleLevel(tri_linear_clamp_sampler, input.tex/height_map_size, 0);

				#if ALPHA_MASK
				clip(color.a - 0.1f);
				#endif

				#if USE_LOD_COLOR
					color.rgb *= input.color * 2.0f;
				#endif

				GBUFFER_SET_COLOR(output, color.rgb /* (NdL + 0.3f)*/);
				//GBUFFER_SET_F0(output, 0.18f);
				GBUFFER_SET_F0(output, 0.04f / 0.2 / 2);
				GBUFFER_SET_NORMAL(output, normalize(normal));
				//GBUFFER_SET_NORMAL(output, 0.5f * normal + 0.5f);
				GBUFFER_SET_ROUGHNESS(output, 0.8f);

				return output;
			}
		"""
	}
}