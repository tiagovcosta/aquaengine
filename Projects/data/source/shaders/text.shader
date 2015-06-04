type = "render"

include = []

passes =
{
	debug =
	{
		vs = "vs_func"
		ps = "ps_func"
		blend_state = alpha_blend
	}
}

mesh =
{
	streams =
	[
		[
			{ semantic = "POSITION", type = float2 }
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
		sprite_font = { type = Texture2D }
	}
}

instance = 
{
	constant_buffers = 
	{
		InstanceCB = 
		{
			color = float3
		}
	}
	
	resources = 
	{
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
				float4 position  : SV_POSITION;
				float2 tex_coord : TEXCOORD;
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
				float2 position 	: POSITION;
				float2 tex_coord 	: TEXCOORD;
			};
			
			PS_INPUT vs_main(VS_INPUT input)
			{
				PS_INPUT output;

				output.position = float4(input.position, 0.0f, 1.0f);
				output.tex_coord = input.tex_coord;

				return output;
			}
		"""
	}

	ps_func =
	{
		include = [samplers, ps_input]

		hlsl =
		"""
			float4 ps_main( PS_INPUT input ) : SV_TARGET0
			{
				//return float4(1.0f, 1.0f, 1.0f);
				return sprite_font.Sample(tri_point_clamp_sampler, input.tex_coord);
			}
		"""
	}
}