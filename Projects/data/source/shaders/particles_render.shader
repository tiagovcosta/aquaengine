type = "render"

include = []

header = "header"

passes =
{
	debug =
	{
		vs = "vs_func"
		ps = "ps_func"
		blend_state = alpha_blend
		depth_stencil_state = "no_depth_write"
	}
}
/*
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
*/
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
			color = float3
		}
	}
	
	resources = 
	{
		particle_system_state = { type = StructuredBuffer, element_type = Particle }
		particle_texture = { type = Texture2D }
	}

	options =
	[
	]
}

snippets =
{
	header =
	{
		hlsl =
		"""
			// TEST EFFECT OF PADDING TO 32 BYTES
			struct Particle
			{
			    float3 position;
			    float  pad0;
				float3 direction;
				float  time;
			};

			static const float3 direction[8] =
			{
				normalize( float3(  1.0f,  1.0f,  1.0f ) ),
				normalize( float3( -1.0f,  1.0f,  1.0f ) ),
				normalize( float3( -1.0f, -1.0f,  1.0f ) ),
				normalize( float3(  1.0f, -1.0f,  1.0f ) ),
				normalize( float3(  1.0f,  1.0f, -1.0f ) ),
				normalize( float3( -1.0f,  1.0f, -1.0f ) ),
				normalize( float3( -1.0f, -1.0f, -1.0f ) ),
				normalize( float3(  1.0f, -1.0f, -1.0f ) )
			};
		"""
	}

	ps_input =
	{
		hlsl =
		"""
			struct PS_INPUT
			{
				float4 position  : SV_POSITION;
				float2 tex_coord : TEXCOORD;
				nointerpolation float alpha : ALPHA;
			};
		"""
	}

	vs_func =
	{
		include = [ps_input]

		hlsl =
		"""
			PS_INPUT vs_main(uint id : SV_VERTEXID)
			{
				PS_INPUT output;

				uint particle_index = id / 4;
				uint vertex_in_quad = id % 4; 

				// calculate the position of the vertex
				float3 position;
				position.x = (vertex_in_quad % 2) ? 1.0 : -1.0;
				position.y = (vertex_in_quad & 2) ? -1.0 : 1.0;
				position.z = 0.0;

				//position.xy *= PARTICLE_RADIUS;
				position.xy *= 0.2f;

			    position = mul( position, (float3x3)inv_view ) + particle_system_state[particle_index].position.xyz;
			    //position += particle_system_state[particle_index].position.xyz;

			    //output.position = mul( float4(position,1.0), g_mWorldViewProj );
			    output.position = mul( float4(position,1.0), view_proj );

			    output.alpha = 1 - particle_system_state[particle_index].time / 2;

				// texture coordinates
				output.tex_coord.x = (vertex_in_quad % 2) ? 1.0 : 0.0;
				output.tex_coord.y = (vertex_in_quad & 2) ? 1.0 : 0.0; 

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
				//return float4(1.0f, 0.0f, 0.0f, 1.0f) * input.alpha;
				return float4(1.0f, 0.0f, 0.0f, particle_texture.Sample(tri_point_clamp_sampler, input.tex_coord).r) * input.alpha;
			}
		"""
	}
}