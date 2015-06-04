type = "render"

include = [  ]

passes =
{
	apply_gamma =
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
		}
	}
	
	resources = 
	{
		light_buffer = { type = Texture2D }
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
			float4 ps_main( PS_INPUT input) : SV_TARGET0
			{	
				//float3 color = light_buffer.Sample(gTriPointSampler, pIn.texC).rgb;
				float3 color = light_buffer.Load(uint3(input.position.xy, 0)).rgb;

				color = pow(abs(color), 1/2.2f);

				return float4( color, 1.0f );
			}
		"""
	}
}