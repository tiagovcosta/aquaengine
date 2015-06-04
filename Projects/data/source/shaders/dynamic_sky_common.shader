include = []

snippets =
{
	dynamic_sky_common =
	{
		hlsl =
		"""
			static const float InnerRadius                = 6356.7523142f;
			static const float OuterRadius                = InnerRadius * 1.0157313f;
			static const float PI                         = 3.1415159;
			static const float NumSamples                 = 10;
			static const float fScale                     = 1.0 / (OuterRadius - InnerRadius);
			static const float2 v2dRayleighMieScaleHeight = {0.25f, 0.1f};

			static const float ESun   = 20.0f;
			static const float Kr     = 0.0025f;
			static const float Km     = 0.0010f;
			static const float KrESun = Kr * ESun;
			static const float KmESun = Km * ESun;
			static const float Kr4PI  = Kr * 4.0f * PI;
			static const float Km4PI  = Km * 4.0f * PI;

			float HitOuterSphere( float3 O, float3 Dir ) 
			{
				float3 L = -O;

				float B = dot( L, Dir );
				float C = dot( L, L );
				float D = C - B * B; 
				float q = sqrt( OuterRadius * OuterRadius - D );
				float t = B;
				t += q;

				return t;
			}

			float2 GetDensityRatio( float fHeight )
			{
				const float fAltitude = (fHeight - InnerRadius) * fScale;
				return exp( -fAltitude / v2dRayleighMieScaleHeight.xy );
			}

			float2 t( float3 P, float3 Px )
			{
				float2 OpticalDepth = 0;

				float3 v3Vector =  Px - P;
				float fFar = length( v3Vector );
				float3 v3Dir = v3Vector / fFar;
						
				float fSampleLength = fFar / NumSamples;
				float fScaledLength = fSampleLength * fScale;
				float3 v3SampleRay = v3Dir * fSampleLength;
				P += v3SampleRay * 0.5f;
						
				for(int i = 0; i < NumSamples; i++)
				{
					float fHeight = length( P );
					OpticalDepth += GetDensityRatio( fHeight );
					P += v3SampleRay;
				}		

				OpticalDepth *= fScaledLength;
				return OpticalDepth;
			}
		"""
	}
}