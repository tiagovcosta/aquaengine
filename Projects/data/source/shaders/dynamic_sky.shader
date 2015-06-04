type = "render"

include = [ "dynamic_sky_common" ]

passes =
{
	skydome =
	{
		vs = "vs_func"
		ps = "ps_func"
		//blend_state = alpha_blend
	}
}

mesh =
{
	streams =
	[
		[
			{ semantic = "POSITION", type = float3 }
			{ semantic = "TEXCOORD", type = float2 }
		]
	]

	options = 
	[

	]
}

material =
{
	resources =
	{
		//filter_texture = { type = Texture2D }
	}
}

instance = 
{
	constant_buffers = 
	{
		InstanceCB = 
		{
			//world_view_proj = float4x4
			world = float4x4
			sun_dir = float3
		}
	}
	
	resources = 
	{
		instance_data    = { type = Buffer }
		rayleigh_scatter = { type = Texture2D }
		mie_scatter      = { type = Texture2D }
		scattering       = { type = Texture2D, element_type = float3 }
	}
	/*
	options =
	[
		{ define = "INSTANCING" }
	]
	*/
}

snippets =
{
	ps_input =
	{
		hlsl =
		"""
			struct PS_INPUT
			{
				float4 position : SV_POSITION;
			    float2 tex_coord0 : TEXCOORD0;
				float3 tex_coord1 : TEXCOORD1;
			};
		"""
	}

	vs_func =
	{
		include = [ps_input]

		hlsl =
		"""
			struct VS_INPUT
			{
				float3 pos 			: POSITION;
				float2 tex_coord 	: TEXCOORD;
			};

			PS_INPUT vs_main(VS_INPUT input)
			{
				PS_INPUT output;

				// set z = w so that z/w = 1 (i.e., skydome always on far plane)
				//output.position   = mul(float4(input.pos, 1.0f), world_view_proj);
				output.position   = mul(mul(float4(input.pos * clip_distances.y * 0.9f, 1.0f), world) + float4(camera_position, 0.0f), view_proj);
				output.tex_coord0 = input.tex_coord;
				//vOut.tex_coord1   = -input.pos;
				output.tex_coord1   = -input.pos;

				return output;
			}
		"""
	}

	ps_func =
	{
		include = [samplers, ps_input, dynamic_sky_common]

		hlsl =
		"""
			static const float g = -0.990;
			static const float g2 = g * g;

			float getRayleighPhase(float fCos2)
			{
				return 0.75 * (1.0 + fCos2);
			}

			float getMiePhase(float fCos, float fCos2)
			{
				float3 v3HG;
				v3HG.x = 1.5f * ( (1.0f - g2) / (2.0f + g2) );
				v3HG.y = 1.0f + g2;
				v3HG.z = 2.0f * g;
				return v3HG.x * (1.0 + fCos2) / pow(abs(v3HG.y - v3HG.z * fCos), 1.5);
			}

			float4 ps_main( PS_INPUT input) : SV_TARGET0
			{	
				float fCos = dot( sun_dir, input.tex_coord1 ) / length( input.tex_coord1 );
				float fCos2 = fCos * fCos;
				
				float3 v3RayleighSamples = rayleigh_scatter.Sample(tri_linear_clamp_sampler, input.tex_coord0).xyz;
				float3 v3MieSamples = mie_scatter.Sample(tri_linear_clamp_sampler, input.tex_coord0).xyz;

				float3 color;
				color.rgb = getRayleighPhase(fCos2) * v3RayleighSamples.rgb + getMiePhase(fCos, fCos2) * v3MieSamples.rgb;

				color.rgb += max(0, 1-color.rgb)*float3(0.00f, 0.00f, 0.005f);

				color.rgb += scattering.Load( uint3(input.position.x/2, input.position.y/2,0) );

				return float4(pow(color.rgb,2.2f), 1);
			}
		"""
	}
}