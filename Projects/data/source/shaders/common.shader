frame = 
{
	constant_buffers = 
	{
		FrameCB = 
		{
			delta_time = float
			runtime = float

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

	shadow_alpha_masked =
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

	normal_utilities =
	{
		hlsl = 
		"""
			float2 encode_normal(float3 n)
			{
			    float p = sqrt(-n.z*8+8);
			    return float2(n.xy/p + 0.5);
			}

			float3 decode_normal(float2 enc)
			{
				float2 fenc = enc*4-2;
				float f     = dot(fenc,fenc);
				float g     = sqrt(1-f/4);

				float3 n;
				n.xy = fenc*g;
				n.z  = 1-f/2;
				n.z = -n.z;

				return n;
			}
		"""
	}


	velocity_utilities =
	{
		hlsl = 
		"""
			// Based on CryEngine 3 Graphics Gems (slide 52)

			float2 encode_velocity(float2 v)
			{
			    return sqrt(abs(v)) * sign(v) * (127.0f/255.0f) + 0.5f;
			}

			float2 decode_velocity(float2 enc)
			{
				float2 v = (enc - 0.5f) *  1.0f/(127.0f/255.0f);

				return v*v * sign(v);
			}
		"""
	}

	gbuffer =
	{
		include = [ "normal_utilities" ]
		hlsl = 
		"""
			struct GBUFFER_OUT
			{
				float4 color_f0 : SV_TARGET0;
				float4 normal_roughness : SV_TARGET1;
			};

			#define GBUFFER_SET_COLOR(gbuffer, value) gbuffer.color_f0.rgb = value
			#define GBUFFER_SET_F0(gbuffer, value) gbuffer.color_f0.a = value

			//To prevent precision issues a RGBA16_UNORM texture must be used
			//If there's a need for A CHANNEL in GBUFFER switch to VIEW_SPACE_NORMAL and use normal_roughness.w channel
			#define WORLD_SPACE_NORMAL 1
			#define VIEW_SPACE_NORMAL 0

		#if WORLD_SPACE_NORMAL

			#define GBUFFER_SET_NORMAL(gbuffer, value) gbuffer.normal_roughness.rgb = 0.5f * value + 0.5f
			#define GBUFFER_GET_WS_NORMAL(value) 2.0f * value.xyz - 1.0f
			#define GBUFFER_GET_VS_NORMAL(value) normalize( mul(float4(2.0f * value.xyz - 1.0f, 0.0f), view).xyz )

			#define GBUFFER_SET_ROUGHNESS(gbuffer, value) gbuffer.normal_roughness.a = value
			#define GBUFFER_GET_ROUGHNESS(value) value.a

		#elif VIEW_SPACE_NORMAL

			#define GBUFFER_SET_NORMAL(gbuffer, value) gbuffer.normal_roughness.rga = float3(encode_normal(value), 0.0f)
			#define GBUFFER_GET_WS_NORMAL(value) normalize( mul(float4(decode_normal(value.xy), 0.0f), inv_view).xyz )
			#define GBUFFER_GET_VS_NORMAL(value) decode_normal(value.xy)

			#define GBUFFER_SET_ROUGHNESS(gbuffer, value) gbuffer.normal_roughness.b = value
			#define GBUFFER_GET_ROUGHNESS(value) value.b

		#endif
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