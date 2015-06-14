type = "render"

include = [ ]

passes =
{
	composite =
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
			project_to_pixel = float4x4
			z_thickness		 = float
		}
	}
	
	resources = 
	{
		color_texture = { type = Texture2D }
		reflection_texture = { type = Texture2D }
		normal_texture = { type = Texture2D }
		depth_texture = { type = Texture2D }
		material_texture = { type = Texture2D }
		rayleigh_texture = { type = Texture2D }
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
			float3 ps_main( PS_INPUT input) : SV_TARGET0
			{
				float3 color = color_texture.Load(uint3(input.position.xy, 0)).rgb;

				float4 normal_roughness = normal_texture.Load(uint3(input.position.xy, 0));

				float3 normal   = GBUFFER_GET_VS_NORMAL( normal_roughness );
				float roughness = GBUFFER_GET_ROUGHNESS( normal_roughness );

				float4 reflection = reflection_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord, roughness * 5);

				float4 color_f0 = material_texture.Load(uint3(input.position.xy, 0));
				color_f0.rgb    = pow(color_f0.rgb, 2.2);
				
				float metal          = saturate(color_f0.a*2-1);
				float non_metal_spec = saturate(color_f0.a*2)*0.2;
				
				float3 Ks = lerp( non_metal_spec.rrr, color_f0.rgb, metal );

				float depth = depth_texture.Load(uint3(input.position.xy, 0)).x;

				depth = convertProjDepthToView(depth);

				float3 position_vs = input.view_ray * depth;

			    float3 view_dir = normalize( position_vs );

			    float3 fresnel_specular = Ks + (1 - Ks) * pow(1-dot(-view_dir, normal), 5);

			    //fresnel_specular = saturate(fresnel_specular * 1000);

			    float3 ray_dir = normalize( reflect(view_dir, normal) );

			    // (3.14f / 1.5f) should be (3.14f / 2) however this "hack" makes reflections look better
			    float2 spherical_coord = float2(acos(ray_dir.y) / (3.14f / 1.5f), atan2(ray_dir.z, ray_dir.x) / (2 * 3.14f));

			    float3 rayleigh = rayleigh_texture.Sample(tri_linear_clamp_sampler, spherical_coord).rgb;

				return color + (reflection.rgb * reflection.a + rayleigh * (1-reflection.a)) * fresnel_specular;
			}
		"""
	}
}