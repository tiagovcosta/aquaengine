type = "render"

include = [  ]

passes =
{
	ssao =
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
		}
	}
	
	resources = 
	{
		normal_buffer = { type = Texture2D }
		depth_buffer  = { type = Texture2D }
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
				float4 position  : SV_Position;
				float2 tex_coord : TEXCOORD0;
				float3 view_ray  : TEXCOORD1;
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

    			//float4 position_ws = mul(output.position, inv_view_proj);
    			//position_ws /= position_ws.w;
    			//position_ws.xyz -= camera_position;

    			float3 position_ws = mul(output.position, inv_proj).xyz;

    			output.view_ray = float3(position_ws.xy / position_ws.z, 1.0f);

				return output;
			}
		"""
	}

	ps_func =
	{
		include = [ "samplers", "ps_input", "gbuffer" ]

		hlsl =
		"""
			static const float RADIUS = 0.5f;
			//static const float RADIUS = 1.2f;
/*
			static const uint SAMPLE_KERNEL_SIZE = 4;
			static const float3 SAMPLE_KERNEL[] = { float3(-1.0f, 0.0f, 1.0f),
													float3(1.0f, 0.0f, 1.0f),
													float3(0.0f, -1.0f, 1.0f),
													float3(0.0f, 1.0f, 1.0f) };
*/

			static const uint SAMPLE_KERNEL_SIZE = 8;
			static const float3 SAMPLE_KERNEL[] = 
			{ 
			float3( 0.251450823973f, 0.0871773986447f, 0.525009175087f ), 
			float3( -0.2916385311f, -0.181144893895f, 0.494842262899f ), 
			float3( -0.335602982577f, 0.673180335337f, 0.201479802776f ), 
			float3( -0.872496014491f, 0.316978854765f, 0.275931655948f ), 
			float3( 0.665311448175f, -0.244753343573f, 0.167157674677f ), 
			float3( 0.608843393328f, 0.615227753108f, 0.190684003857f ), 
			float3( 0.330306113081f, -0.264875180414f, 0.815237123271f ), 
			float3( 0.2138630048f, -0.919642471544f, 0.239081534152f ), 
			};

/*
			static const uint SAMPLE_KERNEL_SIZE = 16;
			static const float3 SAMPLE_KERNEL[SAMPLE_KERNEL_SIZE] = 
			{ 
			float3( 0.012161394446f, -0.424347671041f, 0.880196121696f ), 
			float3( -0.425954637765f, -0.0377561588967f, 0.889360067024f ), 
			float3( 0.387507603203f, 0.430330973305f, 0.111899092305f ), 
			float3( 0.341361964536f, -0.635618448304f, 0.369979550067f ), 
			float3( -0.426947104873f, -0.453487628396f, 0.124732338524f ), 
			float3( -0.751665665233f, 0.134719712855f, 0.0431492400871f ), 
			float3( 0.281225390523f, 0.0765525293121f, 0.902695036782f ), 
			float3( -0.282481323311f, 0.580414631982f, 0.0548133803405f ), 
			float3( 0.395813618527f, 0.11004966514f, 0.789623419118f ), 
			float3( 0.651128208425f, -0.189686799886f, 0.178211126601f ), 
			float3( 0.0775208321908f, -0.905737446768f, 0.376669825309f ), 
			float3( -0.556203174665f, -0.595727589002f, 0.551248489095f ), 
			float3( -0.164362409928f, -0.019628077486f, 0.189793740728f ), 
			float3( 0.509207878841f, 0.431773366038f, 0.509804557425f ), 
			float3( -0.730619214738f, 0.0573429482314f, 0.638370512231f ), 
			float3( -0.045208583521f, 0.433071964165f, 0.445899966801f ), 
			};
*/
			// hlsl array
			static const uint SAMPLE_NUM = 8;

			static const float3 POISSON_SAMPLES[SAMPLE_NUM] = 
			{ 
				float3( 0.835352229632f, -0.0837174886883f, 0.38444185945f ), 
				float3( -0.66642931683f, -0.305831990521f, -0.380433382251f ), 
				float3( 0.114573402593f, -0.365586760474f, 0.675984885662f ), 
				float3( -0.124132611992f, 0.924034848157f, -0.344905675893f ), 
				float3( -0.61393048916f, 0.16019921482f, 0.653224179437f ), 
				float3( 0.385299032702f, 0.15533075461f, -0.265999982056f ), 
				float3( 0.0400565394783f, 0.748991459201f, 0.458598305052f ), 
				float3( -0.0735425637235f, -0.885286700244f, 0.0167651072754f ), 
			};

			static const float BAYER_MATRIX[4][4] = { { 1.0f/17,  9.0f/17,  3.0f/17,  11.0f/17 },
													  { 13.0f/17, 5.0f/17,  15.0f/17, 7.0f/17  },
													  { 4.0f/17,  12.0f/17, 2.0f/17,  10.0f/17 },
													  { 16.0f/17, 8.0f/17,  14.0f/17, 6.0f/17  } };

			float ps_main( PS_INPUT input) : SV_TARGET0
			{
				//float3 normal = normal_buffer.Load(uint3(input.position.xy, 0)).rgb * 2.0f - 1.0f;
				float3 normal = GBUFFER_GET_VS_NORMAL(normal_buffer.Load(uint3(input.position.xy, 0)));
				float depth   = depth_buffer.Load(uint3(input.position.xy, 0)).r;

				depth = proj._43 / (depth - proj._33);

				//float3 position = camera_position + input.view_ray * depth;
				float3 position = input.view_ray * depth;
				//position = mul(float4(position, 1.0f), inv_view).xyz;

				//normal = mul(float4(normal, 0.0f), view).xyz;
				//normal = normalize(normal);

				//float3 rvec      = texture(uTexRandom, vTexcoord * uNoiseScale).xyz * 2.0 - 1.0;
				float bayer  = BAYER_MATRIX[input.position.x%4][input.position.y%4];
				float bayer2 = BAYER_MATRIX[input.position.y%4][input.position.x%4];
				float bayer3 = BAYER_MATRIX[input.position.x%4][input.position.x%4];
				float3 rvec  = float3(bayer, bayer2, 0.5f) * 2.0 - 1.0;
				//rvec         = float3(bayer, bayer3, bayer2) * 2.0 - 1.0;

				float rand = frac(sin(dot((input.position.xy+50.0f) ,float2(12.9898,78.233))) * 43758.5453);
				float rand2 = frac(sin(dot((input.position.xy*input.position.yx) ,float2(12.9898,78.233))) * 43758.5453);
				//rvec = float3(rand, rand, 0.5f) * 2.0 - 1.0;
				//rvec = float3(rand, 0.5f, rand2) * 2.0 - 1.0;
				//rvec = float3(rand, rand2, 0.5f) * 2.0 - 1.0;
				//rvec = float3(bayer * rand, 0.5f, bayer2 * rand2) * 2.0 - 1.0;

				rvec = normalize(rvec);

				//rvec = mul(float4(rvec, 0.0f), view).xyz;
				//rvec = normalize(rvec);

#if HEMISPHERICAL_SAMPLES
				float3 tangent   = normalize(rvec - normal * dot(rvec, normal));
				float3 bitangent = cross(normal, tangent);
				float3x3 tbn     = float3x3(tangent, bitangent, normal);
#endif
				float occlusion = 0.0f;

				for(uint i = 0; i < SAMPLE_KERNEL_SIZE; ++i) 
				{
					// get sample position:
					float scale = float(i) / SAMPLE_KERNEL_SIZE;
				   	scale = lerp(0.1f, 1.0f, scale * scale);

#if HEMISPHERICAL_SAMPLES
					float3 sample = mul(normalize(SAMPLE_KERNEL[i]) * scale, tbn);
					sample        = sample * RADIUS + position;
#else
					float3 sample = reflect(SAMPLE_KERNEL[i].xyz, rvec);
    
			        // Flip sample vector if it is behind the plane with normal 'normal'.
			        float flip = sign( dot(normal, sample) );

					sample = flip * sample * scale * RADIUS + position;
#endif

					// project sample position:
					float4 offset = float4(sample, 1.0);
					offset        = mul(offset, proj);
					//offset        = mul(offset, view_proj);
					offset.xy     /= offset.w;
					offset.xy     = offset.xy * 0.5 + 0.5;

					offset.y = 1.0f - offset.y;
				  
					// get sample depth:
					//float sample_depth = texture(uTexLinearDepth, offset.xy).r;
					float sample_depth  = depth_buffer.Sample(tri_linear_clamp_sampler, offset.xy).r;

					sample_depth = proj._43 / (sample_depth - proj._33);
/*
					// range check & accumulate:
					float range_check = max(0, depth - sample_depth) < RADIUS ? 1.0 : 0.0;
					//float range_check = abs(depth - sample_depth) < RADIUS ? 1.0 : 0.0;
					occlusion += (sample_depth <= sample.z ? 1.0 : 0.0) * range_check;
*/
/*
					float range_check = max(0, sample_depth - depth) < RADIUS ? 1.0 : 0.0;
					occlusion += (sample_depth <= sample.z ? 1.0 : 0.0) * range_check;
*/
					float range_check = smoothstep(0.0, 1.0, RADIUS / abs(position.z - sample_depth));
					occlusion += range_check * step(sample_depth, sample.z);
				}

				occlusion = 1.0 - (occlusion / SAMPLE_KERNEL_SIZE * 2);

				//occlusion = pow(occlusion, 4.0f);

				return occlusion;
			}
		"""
	}
}