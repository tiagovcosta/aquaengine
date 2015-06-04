type = "render"

include = [ ]

passes =
{
	update =
	{
		vs = "vs_func"
		ps = "ps_func"
		//blend_state = alpha_blend
	}
}
/*
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
		//filter_texture = { type = Texture2D }
	}
}
*/
instance = 
{
	constant_buffers = 
	{
		InstanceCB = 
		{
			//InvWavelength = float3
		}
	}
	
	resources = 
	{
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
				output.position  = float4(output.tex_coord * float2(2,-2) + float2(-1,1), 0, 1);

				return output;
			}
		"""
	}

	ps_func =
	{
		include = [samplers, ps_input]

		hlsl =
		"""
			float4 ps_main( PS_INPUT input) : SV_TARGET0
			{	
				//float rand = frac(sin(dot((input.position.xy+1),float2(12.9898,78.233))) * 43758.5453);
				float rand = frac(sin(dot((input.position.xy+1) * delta_time ,float2(12.9898,78.233))) * 43758.5453);

  				return float4(rand, rand, rand, 1.0f);
			}
		"""
	}
}