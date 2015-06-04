frame = 
{
	constant_buffers = 
	{
		FrameCB = 
		{
			delta_time = float

			A = float
			B = float
			C = float
			D = float
			E = float
			F = float
			W = float
			K = float
		}
	}
	
	resources = 
	{
		//test_texture = { type = RWTexture2D, element_type = float4 }
		//test_texture2 = { type = Texture2D }
	}
}

view = 
{
	constant_buffers = 
	{
		ViewCB = 
		{
			view = float4x4
			proj = float4x4
			view_proj = float4x4
			inv_view = float4x4
			inv_proj = float4x4
			inv_view_proj = float4x4
			camera_position = float3
			clip_distances = float2
		}
	}
	
	resources = 
	{
		//test_texture4 = { type = Texture2D }
		//test_texture3 = { type = RWTexture2D, element_type = float4 }
	}
}

passes =
{
	gbuffer =
	{

	}

	gbuffer_alpha_masked =
	{

	}

	shadow =
	{

	}

	debug =
	{
		constant_buffers = 
		{
			PassCB = 
			{
				aspect = float
			}
		}
		
		resources = 
		{
		}
	}
}

blend_states =
{
	alpha_blend =
	{
        //blend_enable 				= true
        src_blend 					= src_alpha
  		dest_blend 					= inv_src_alpha   // Change to "One" and use a color that's not fully saturated
 		blend_op 					= add             // to show overlapping areas more strongly.
		src_blend_alpha 			= one
		dest_blend_alpha 			= zero
 		blend_op_alpha				= add
		render_target_write_mask 	= all
	}
}

//loool
/*loool*/
snippets = 
{
	depth_utilities =
	{
		hlsl =
		"""
			// convert a depth value from post-projection space into view space
			float convertProjDepthToView( float z )
			{
			    z = 1.0f / (z*inv_proj._34 + inv_proj._44);
			    return z;
			}

		"""
	}

	gbuffer =
	{
		hlsl = 
		"""
			struct GBUFFER_OUT
			{
				float4 color_f0 : SV_TARGET0;
				float4 normal_roughness : SV_TARGET1;
			};

			#define GBUFFER_SET_COLOR(gbuffer, value) gbuffer.color_f0.rgb = value
			#define GBUFFER_SET_F0(gbuffer, value) gbuffer.color_f0.a = value
			#define GBUFFER_SET_NORMAL(gbuffer, value) gbuffer.normal_roughness.rgb = value
			#define GBUFFER_SET_ROUGHNESS(gbuffer, value) gbuffer.normal_roughness.a = value
		"""
	}

	samplers = 
	{
		hlsl = 
		"""
		//loool
		/*loool*/
			SamplerState tri_point_clamp_sampler  : register(s0);
			SamplerState tri_linear_clamp_sampler : register(s1);
			SamplerState aniso_clamp_sampler      : register(s2);

			SamplerState tri_point_mirror_sampler  : register(s3);
			SamplerState tri_linear_mirror_sampler : register(s4);
			SamplerState aniso_mirror_sampler      : register(s5);

			SamplerState tri_point_wrap_sampler  : register(s6);
			SamplerState tri_linear_wrap_sampler : register(s7);
			SamplerState aniso_wrap_sampler      : register(s8);

			SamplerComparisonState shadow_sampler : register(s9);
		"""
	}
}