type = "render"

include = []

passes =
{
	debug =
	{
		vs = "vs_func"
		ps = "ps_func"
		blend_state = alpha_blend
		//depth_stencil_state = no_depth_test
	}
}

mesh =
{
	streams =
	[
		[
			{ semantic = "POSITION", index = 0, type = float3 }
			{ semantic = "POSITION", index = 1, type = float3 }
			//{ semantic = "WEIGHTS", type = float3 }
		]
	]

	options = 
	[

	]
	/*[
		{ "semantic" = "WORLDVIEWPROJ", "type" = "float4x4", "per_instance" = true, "options" = ["INSTANCING"] }
		{ "semantic" = "COLOR", "type" = "float3", "per_instance" = true, "options" = ["INSTANCING"] }
		{ "semantic" = "RADIUS", "type" = "float", "per_instance" = true, "options" = ["INSTANCING"] }
	]*/
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
			world_view_projection = float4x4
			color = float3
			line_width = float
		}
	}
	
	resources = 
	{
		instance_data = { type = Buffer }
	}

	options =
	[
		{ define = "INSTANCING" }
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
				float4 position              : SV_POSITION;
				nointerpolation float3 color : COLOR;
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
				float3   pos0                  : POSITION0;
				float3   pos1                  : POSITION1;
				//float3   weights               : WEIGHTS;
			};
			
			PS_INPUT vs_main(VS_INPUT input, uint id:SV_VERTEXID)
			{
				PS_INPUT output;
				
				output.color = color;

				// Transform the input points.
				float4 p0 = mul( float4( input.pos0, 1.0f ), world_view_projection);
				float4 p1 = mul( float4( input.pos1, 1.0f ), world_view_projection);
				/*
				// Warp transformed points by aspect ratio.
				float4 w0 = p0;
				float4 w1 = p1;
				w0.y /= aspect;
				w1.y /= aspect;

				// Calc vectors between points in screen space.
				float2  delta2 = w1.xy / w1.z - w0.xy / w0.z;
				float3  delta_p;

				delta_p.xy = delta2;
				delta_p.z = w1.z - w0.z;

				//
				// Calc UV basis vectors.
				//
				// Calc U
				float   len = length( delta2 );
				float3  U = delta_p / len;

				// Create V orthogonal to U.
				float3  V;
				V.x = U.y;
				V.y = -U.x;
				V.z = 0;

				//Expand to quad

				// 0      1
				//  ------
				//  |    |
				//  ------
				// 2      3

				// Calculate output position based on this
				// vertex's id.

				bool k = id % 2 == 1;

				output.position = p0 * k + p1 * !k;

				// Calc offset part of position

				int p = id & 2;

				//float3 offset = U * input.weights.z + V * (-p * 2 + 1);
				float3 offset = V * (-p * 2 + 1);

				// Apply line thickness.
				offset.xy *= line_width;

				// Unwarp by inverse of aspect ratio.
				offset.y *= aspect;

				// Undo perspective divide since the hardware will do it.
				output.position.xy += offset.xy * output.position.z;
				*/

				// Warp transformed points by aspect ratio.
				float4 w0 = p0;
				float4 w1 = p1;
				w0.y /= aspect;
				w1.y /= aspect;

				// Calc vectors between points in screen space.
				float2  delta = w1.xy/w1.w - w0.xy/w0.w;
/*
				if((w1.w < 0 && w0.w > 0) || (w1.w > 0 && w0.w < 0))
				{
					delta = -delta;
				}
*/
				float2 V;
				V.x = delta.y;
				V.y = -delta.x;

				if((w1.w < 0 && w0.w > 0) || (w1.w > 0 && w0.w < 0))
				{
					V = -V;
				}

				//Expand to quad

				// 0      1
				//  ------
				//  |    |
				//  ------
				// 2      3

				// Calculate output position based on this
				// vertex's id.

				bool k = id % 2 == 1;

				output.position = p0 * k + p1 * !k;

				// Calc offset part of position

				int p = id & 2;

				float2 offset = V * (-p * 2 + 1);

				offset = normalize(offset);

				// Apply line thickness.
				offset *= line_width;

				// Unwarp by inverse of aspect ratio.
				offset.y *= aspect;

				// Undo perspective divide since the hardware will do it.
				output.position.xy += offset * output.position.w;

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
				return float4(input.color, 1.0f);
			}
		"""
	}
}