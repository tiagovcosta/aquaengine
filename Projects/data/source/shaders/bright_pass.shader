type = "render"

include = [  ]

passes =
{
	bright_pass =
	{
		vs = "vs_func"
		ps = "ps_func"
	}
}

mesh =
{
	streams =
	[

	]

	options = 
	[

	]
}

material =
{
	resources =
	{
	}
}

instance = 
{
	constant_buffers = 
	{
		InstanceCB = 
		{
			threshold = float
		}
	}
	
	resources = 
	{
		scene_texture = { type = Texture2D }
		avg_luminance = { type = Texture2D }
	}

	options =
	[
		//{ define = "INSTANCING" }
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
				float4 position : SV_Position;
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
    			output.position = float4(output.tex_coord * float2(2,-2) + float2(-1,1), 0, 1);

				return output;
			}
		"""
	}

	ps_func =
	{
		include = [samplers, ps_input]

		hlsl =
		"""
			static const float3 LUMINANCE = float3(0.2125f, 0.7154f, 0.0721f);

			float3 Uncharted2Tonemap(float3 x, float A, float B, float C, float D, float E, float F)
			{
				return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
			}

			float3 FilmicToneMap(float3 color, float avg_lum, float middle_grey,
			                     float a, float b, float c, float d, float e, float f, float w)
			{
				//middle_grey = 1.03 - 2/(2+log10(avg_lum + 1));

				color              *= middle_grey/(avg_lum + 0.001f);
				float3 curr        = Uncharted2Tonemap(color, a, b, c, d, e, f);
				float3 white_scale = 1.0f/Uncharted2Tonemap(w, a, b, c, d, e, f);
				color.rgb          = curr*white_scale;

				return color;
			}

			float3 ReinhardToneMap(float3 color, float avg_lum, float middle_grey, float white)
			{
				float lum = dot(color, LUMINANCE);
				float lumScaled = (lum * middle_grey) / avg_lum;	
				float lumCompressed = (lumScaled * (1 + (lumScaled / (white * white)))) / (1 + lumScaled);
				color *= (lumCompressed/lum);

				return color;
			};
/*
			static const float A = 0.15f;
			static const float B = 0.50f;
			static const float C = 0.10f;
			static const float D = 0.20f;
			static const float E = 0.02f;
			static const float F = 0.30f;
			static const float W = 2.2f;
*/
			float4 ps_main( PS_INPUT input) : SV_TARGET0
			{
/*
				//float3 color = light_buffer.Sample(gTriPointSampler, pIn.texC).rgb;
				float3 color = light_buffer.Load(uint3(input.position.xy, 0)).rgb;

				color = pow(abs(color), 1/2.2f);

				return float4( color, 1.0f );
*/
				//float3 color  = scene_texture.Load(uint3(input.position.xy, 0)).rgb;

				//---------------------------------------------------------------------
/*
				// 4 taps average
				float3 color = scene_texture.Sample(tri_linear_clamp_sampler, input.tex_coord).rgb;
*/
				//float2 input_texel_size = float2(1.0f/1280, 1.0f/720);
				//float3 color = scene_texture.Sample(tri_linear_clamp_sampler, input.tex_coord + input_texel_size/2).rgb; <-- wrong offsets
				
				//---------------------------------------------------------------------
/*
				// 16 taps average
				float2 input_texel_dx = float2(1.0f/1280, 0.0f);
				float2 input_texel_dy = float2(0.0f, 1.0f/720);
				float2 input_texel_ds = input_texel_dx + input_texel_dy;

				//float3 color = scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord - input_texel_ds/2, 0).rgb;
				//color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord + input_texel_dx + input_texel_dx/2 - input_texel_dy/2, 0).rgb;
				//color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord + input_texel_dy - input_texel_dx/2 + input_texel_dy/2, 0).rgb;
				//color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord + input_texel_ds + input_texel_ds/2, 0).rgb;

				float3 color = scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord - input_texel_dx - input_texel_dy, 0).rgb;
				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord + input_texel_dx - input_texel_dy, 0).rgb;
				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord - input_texel_dx + input_texel_dy, 0).rgb;
				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord + input_texel_dx + input_texel_dy, 0).rgb;
				
				color *= 0.25f;
*/
				//---------------------------------------------------------------------

				// 36 taps custom (COD:AW)
				float3 color = float3(0.0f, 0.0f, 0.0f);

				float2 input_texel_dx = float2(1.0f/1280, 0.0f);
				float2 input_texel_dy = float2(0.0f, 1.0f/720);

				color = scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord - 2 * input_texel_dx - 2 * input_texel_dy, 0).rgb * 0.125f;
				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord - 2 * input_texel_dy, 0).rgb * 0.25f;
				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord + 2 * input_texel_dx - 2 * input_texel_dy, 0).rgb * 0.125f;

				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord - input_texel_dx - input_texel_dy, 0).rgb * 0.5f;
				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord + input_texel_dx - input_texel_dy, 0).rgb * 0.5f;

				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord - 2 * input_texel_dx, 0).rgb * 0.25f;
				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord, 0).rgb * 0.5f;
				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord + 2 * input_texel_dx, 0).rgb * 0.25f;

				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord - input_texel_dx + input_texel_dy, 0).rgb * 0.5f;
				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord + input_texel_dx + input_texel_dy, 0).rgb * 0.5f;

				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord - 2 * input_texel_dx + 2 * input_texel_dy, 0).rgb * 0.125f;
				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord + 2 * input_texel_dy, 0).rgb * 0.25f;;
				color += scene_texture.SampleLevel(tri_linear_clamp_sampler, input.tex_coord + 2 * input_texel_dx + 2 * input_texel_dy, 0).rgb * 0.125f;

				color *= 0.25f;

				//---------------------------------------------------------------------

				float avg_lum = avg_luminance.Load(uint3(0, 0, 0)).r;

				color = FilmicToneMap(color, avg_lum, K, A, B, C, D, E, F, W);

				float tone_mapped_lum = dot(color, LUMINANCE);

				color *= max(tone_mapped_lum - threshold, 0.0f)/tone_mapped_lum;

				return float4(color, 1.0f);
			}
		"""
	}
}