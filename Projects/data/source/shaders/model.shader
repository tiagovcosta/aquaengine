type = "render"

include = []

passes =
{
	gbuffer =
	{
		condition = "!ALPHA_MASKED"

		vs = "vs_gbuffer"
		ps = "ps_gbuffer"

		rasterizer_state = { condition = "TWO_SIDED" value = "no_cull" }
	}

	gbuffer_alpha_masked =
	{
		condition = "ALPHA_MASKED"
		
		vs = "vs_gbuffer"
		ps = "ps_gbuffer"

		rasterizer_state = { condition = "TWO_SIDED" value = "no_cull" }
	}

	shadow =
	{
		vs = "vs_shadow"
		//ps = "ps_shadow"
		ps = { condition = "ALPHA_MASKED", value = "ps_shadow" }

		rasterizer_state = { condition = "TWO_SIDED" value = "no_cull" }
	}
}

mesh =
{
	streams =
	[
		[
			{ semantic = "POSITION",type = float3 }
			{ semantic = "BONEIDS", type = uint4, define = "VERTEX_BONES" }
			{ semantic = "BONEWEIGHTS", type = uint4, define = "VERTEX_BONES" }
		]
		[
			{ semantic = "NORMAL",type = float3 }
			{ semantic = "TANGENT", type = float3, define = "VERTEX_TANGENT" }
			{ semantic = "TEXCOORD", index = 0, type = float2, define = "VERTEX_TEXCOORD0" }
			{ semantic = "TEXCOORD", index = 1, type = float2, define = "VERTEX_TEXCOORD1" }
		]
	]

	options = 
	[
		{ define = "VERTEX_BONES" }
		{ define = "VERTEX_TANGENT" }
		{ define = "VERTEX_TEXCOORD0" }
		{ define = "VERTEX_TEXCOORD1" }
	]
}

material =
{
	resources =
	{
		diffuse_map = { type = Texture2D }
		normal_map = { type = Texture2D }
		specular_map = { type = Texture2D }
	}

	constant_buffers = 
	{
		MaterialCB = 
		{
			diffuse_color = { name = "Diffuse Color" type = float3 condition = "!DIFFUSE_MAP" }
			f0 = { name = "F0" type = float condition = "!SPECULAR_MAP" }
			roughness = { name = "Roughness" type = float condition = "!SPECULAR_MAP" }
			tiling_scale = { name = "Tiling scale" type = float2 condition = "TILING" }
		}
	}

	options =
	[
		{ define = "CHECKBOARD" condition = "VERTEX_TEXCOORD0" }
		{ define = "DIFFUSE_MAP" condition = "VERTEX_TEXCOORD0" }
		{ define = "NORMAL_MAP" condition = "VERTEX_TANGENT && VERTEX_TEXCOORD0" }
		{ define = "SPECULAR_MAP" condition = "VERTEX_TEXCOORD0" }
		{ define = "TWO_SIDED" }
		{ define = "ALPHA_MASKED" condition = "DIFFUSE_MAP" }
		{ define = "TILING" condition = "DIFFUSE_MAP || NORMAL_MAP || SPECULAR_MAP" }
	]
}

instance = 
{
	constant_buffers = 
	{
		InstanceCB = 
		{
			//world_view = float4x4
			world = float4x4
			tint = { name = "Tint" type = float3 condition = "!TINT" }
		}
	}
	
	resources = 
	{
		instance_data = { type = Buffer }
	}

	options =
	[
		{ define = "INSTANCING" }
		{ define = "TINT" }
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
				float4 pos_hs     : SV_POSITION;
				float3 pos_vs     : POSITION;
			    float3 normal_vs  : NORMAL;
				#if NORMAL_MAP
				float3 tangent_vs : TANGENT;
				#endif
				#if VERTEX_TEXCOORD0
			    float2 tex_c0     : TEXCOORD0;
				#endif
			};
		"""
	}

	vs_gbuffer =
	{
		include = [ps_input]

		hlsl =
		"""
			struct VS_INPUT
			{
				float3 position     	: POSITION;
				#if VERTEX_BONES
					uint4 bones_ids     : BONEIDS;
					uint4 bones_weights : BONEWEIGHTS;
				#endif
				float3 normal       	: NORMAL;
				#if VERTEX_TANGENT
					float3 tangent      : TANGENT;
				#endif
				#if VERTEX_TEXCOORD0
					float2 tex_c0       : TEXCOORD0;
				#endif
				#if VERTEX_TEXCOORD1
					float2 tex_c1       : TEXCOORD1;
				#endif
			};
			
			PS_INPUT vs_main(VS_INPUT input)
			{
				PS_INPUT output;
				/*
				float4 pos_vs = mul(float4(input.position,1.0f), world_view);

				output.pos_vs = pos_vs.xyz;
				output.pos_hs = mul(pos_vs, proj);
				
				output.normal_vs = mul(float4(input.normal, 0.0f), world_view).xyz;
				
				#if NORMAL_MAP
					output.tangent_vs = mul(float4(input.tangent, 0.0f), world_view).xyz;
				#endif
				
				#if VERTEX_TEXCOORD0
					output.tex_c0 = input.tex_c0;
				#endif
				*/

				float4 pos_vs = mul(float4(input.position,1.0f), world);
				//float4 pos_vs = float4(input.position,1.0f);

				output.pos_vs = pos_vs.xyz;
				output.pos_hs = mul(pos_vs, view_proj);
				
				output.normal_vs = mul(float4(input.normal, 0.0f), world).xyz;
				
				#if NORMAL_MAP
					output.tangent_vs = mul(float4(input.tangent, 0.0f), world).xyz;
				#endif
				
				#if VERTEX_TEXCOORD0
					output.tex_c0 = input.tex_c0;
				#endif

				return output;
			}
		"""
	}

	ps_gbuffer =
	{
		include = [samplers, ps_input, gbuffer]

		hlsl =
		"""
			GBUFFER_OUT ps_main( PS_INPUT input, bool is_front_face : SV_IsFrontFace )
			{
				GBUFFER_OUT output;

				#if TILING
					input.tex_c0 *= tiling_scale;
				#endif

				#if DIFFUSE_MAP
					//output.diffuse = pow(abs(diffuse_map.Sample(aniso_clamp_sampler, input.tex_c0)), 2.2);
					float4 color = diffuse_map.Sample(aniso_wrap_sampler, input.tex_c0);
					
					#if ALPHA_MASKED
						clip(color.a - 0.10f);
					#endif

					GBUFFER_SET_COLOR(output, color.rgb);

				#else
					//output.diffuse = float4(pow(diffuse_color, 2.2), 1.0f);

					GBUFFER_SET_COLOR(output, diffuse_color);

				#endif
				
				#if SPECULAR_MAP
					float2 spec = specular_map.Sample(tri_linear_wrap_sampler, input.tex_c0).rg;

					GBUFFER_SET_F0(output, spec.r);
					GBUFFER_SET_ROUGHNESS(output, spec.g);

				#else
					GBUFFER_SET_F0(output, f0);
					GBUFFER_SET_ROUGHNESS(output, roughness);
				#endif

				#if CHECKBOARD
				
					float2 checkboard = input.tex_c0 * 250;

					if(floor(checkboard.x) % 2 == floor(checkboard.y) % 2)
					{
						GBUFFER_SET_COLOR(output, float3(1.0f, 1.0f, 1.0f));
						GBUFFER_SET_F0(output, 0.5);
						GBUFFER_SET_ROUGHNESS(output, 0.0f);
					}

				#endif
				
				#if !NORMAL_MAP
					//output.normal.rgb = 0.5f * (normalize(input.normal_vs) + 1.0f);

					//input.normal_vs *= !is_front_face * -1.0f;

					if (!is_front_face)
						input.normal_vs *= -1.0f; // rendering the back face, so invert the normal

					GBUFFER_SET_NORMAL(output, 0.5f * normalize(input.normal_vs) + 0.5f);
				#else
					float3 normalT = normal_map.Sample(tri_linear_wrap_sampler, input.tex_c0).xyz;

					normalT = 2.0f*normalT - 1.0f;

					float3 N = normalize(input.normal_vs);
					float3 T = normalize(input.tangent_vs - dot(input.tangent_vs, N)*N);
					float3 B = cross(N,T);
					
					float3x3 TBN = float3x3(T, B, N);
					float3 bumped_normal_vs = normalize(mul(normalT, TBN));
				    
					//output.normal.rgb = 0.5f * (normalize(bumped_normal_vs) + 1.0f);

					GBUFFER_SET_NORMAL(output, 0.5f * normalize(bumped_normal_vs) + 0.5f);

				#endif
				
				//output.depth = input.pos_vs.z/clip_distances.y;
				
				return output;
			}
		"""
	}

	ps_shadow_input =
	{
		hlsl =
		"""
			struct PS_INPUT
			{
				float4 pos_hs     : SV_POSITION;
				//#if ALPHA_MASKED
				#if VERTEX_TEXCOORD0 && ALPHA_MASKED
			    	float2 tex_c0     : TEXCOORD0;
				#endif
			};
		"""
	}

	vs_shadow =
	{
		include = [ps_shadow_input]

		hlsl =
		"""
			struct VS_INPUT
			{
				float3 position     	: POSITION;
				#if VERTEX_BONES
					uint4 bones_ids     : BONEIDS;
					uint4 bones_weights : BONEWEIGHTS;
				#endif
				/*
				float3 normal       	: NORMAL;
				#if VERTEX_TANGENT
					float3 tangent      : TANGENT;
				#endif
				#if VERTEX_TEXCOORD0
					float2 tex_c0       : TEXCOORD0;
				#endif
				#if VERTEX_TEXCOORD1
					float2 tex_c1       : TEXCOORD1;
				#endif
				*/

				//#if ALPHA_MASKED
				#if VERTEX_TEXCOORD0
					float2 tex_c0       : TEXCOORD0;
				#endif
			};
			
			//float4 vs_main(VS_INPUT input) : SV_POSITION
			PS_INPUT vs_main(VS_INPUT input)
			{
				//return mul(mul(float4(input.position,1.0f), world), view_proj);

				PS_INPUT output;

				output.pos_hs = mul(mul(float4(input.position,1.0f), world), view_proj);

				#if ALPHA_MASKED
					output.tex_c0 = input.tex_c0;
				#endif

				return output;
			}
		"""
	}

	ps_shadow =
	{
		include = [samplers, ps_shadow_input]

		hlsl =
		"""
			void ps_main(PS_INPUT input)
			{
				#if TILING
					input.tex_c0 *= tiling_scale;
				#endif
				
				#if ALPHA_MASKED
					//float4 color = diffuse_map.Sample(aniso_clamp_sampler, input.tex_c0);
					float4 color = diffuse_map.Sample(tri_linear_wrap_sampler, input.tex_c0);
					
					clip(color.a - 0.10f);
				#endif

				return;
			}
		"""
	}
}