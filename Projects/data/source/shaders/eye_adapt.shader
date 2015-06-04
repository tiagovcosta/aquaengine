type = "render"

include = [  ]

passes =
{
	luminance =
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
			last_mip_level = uint
		}
	}
	
	resources = 
	{
		luminance = { type = Texture2D }
		prev_luminance = { type = Texture2D }
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
				float4 position  : SV_Position;
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
    			output.position = float4(output.tex_coord * float2(2,-2) + float2(-1,1), 1, 1);

				return output;
			}
		"""
	}

	ps_func =
	{
		include = [samplers, ps_input]

		hlsl =
		"""
			static const float4 LUMINANCE = float4(0.2125f, 0.7154f, 0.0721f, 0.0f);
			static const float DELTA 	  = 0.0001f;

			float ps_main( PS_INPUT input) : SV_TARGET0
			{
				float lum = exp(luminance.Load(uint3(0,0,last_mip_level)).r);
				float prev_lum = prev_luminance.Load(uint3(0,0,0)).r;

				lum = prev_lum + (lum - prev_lum) * ( 1 - pow( 0.98f, 60 * delta_time ) );

    			return lum;
			}
		"""
	}
}