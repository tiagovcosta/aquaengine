type = "render"

include = [ "dynamic_sky_common" ]

passes =
{
	update =
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
			//InvWavelength = float3
			//WavelengthMie = float3
			//v3SunDir = float3

			inv_wavelength = float3
			wavelength_mie = float3
			sun_dir = float3
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
		include = [samplers, ps_input, dynamic_sky_common]

		hlsl =
		"""
			float4 ps_main( PS_INPUT input) : SV_TARGET0
			{	
				float2 Tex0 = input.tex_coord;
				 
				const float3 v3PointPv = float3( 0.0f, InnerRadius + 0.001f, 0.0f );
				const float AngleY = 100.0 * Tex0.x * PI / 180.0;
				const float AngleXZ = PI * Tex0.y;
				
				float3 v3Dir;
				v3Dir.x = sin( AngleY ) * cos( AngleXZ  );
				v3Dir.y = cos( AngleY );
				v3Dir.z = sin( AngleY ) * sin( AngleXZ  );
				v3Dir = normalize( v3Dir );

				float fFarPvPa = HitOuterSphere( v3PointPv , v3Dir );
				float3 v3Ray = v3Dir;

				float3 v3PointP = v3PointPv;
				float fSampleLength = fFarPvPa / NumSamples;
				float fScaledLength = fSampleLength * fScale;
				float3 v3SampleRay = v3Ray * fSampleLength;
				v3PointP += v3SampleRay * 0.5f;
							
				float3 v3RayleighSum = 0;

				for( int k = 0; k < NumSamples; k++ )
				{
					float PointPHeight = length( v3PointP );

					float2 DensityRatio = GetDensityRatio( PointPHeight );
					DensityRatio *= fScaledLength;

					float2 ViewerOpticalDepth = t( v3PointP, v3PointPv );
									
					float dFarPPc = HitOuterSphere( v3PointP, sun_dir );
					float2 SunOpticalDepth = t( v3PointP, v3PointP + sun_dir * dFarPPc );

					float2 OpticalDepthP = SunOpticalDepth.xy + ViewerOpticalDepth.xy;
					float3 v3Attenuation = exp( - Kr4PI * inv_wavelength * OpticalDepthP.x - Km4PI /** wavelength_mie*/ * OpticalDepthP.y );

					v3RayleighSum += DensityRatio.x * v3Attenuation;

					v3PointP += v3SampleRay;
				}

				float3 RayLeigh = v3RayleighSum * KrESun;
				RayLeigh *= inv_wavelength;
				
				return float4( RayLeigh, 1.0f );
			}
		"""
	}
}