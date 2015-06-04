type = "compute"

include = []

compute_kernels = 
{
	inject = 
	{
		header = "header"

		constant_buffers = 
		{
			ParamsCB = 
			{
				emitter_location = float4
				RandomVector = float4
			}
		}

		resources = 
		{
			particle_system = { type = AppendStructuredBuffer, element_type = Particle }
		}
	}

	copy_count = 
	{
		constant_buffers = 
		{
			CountCB = 
			{
				count = uint
			}
		}

		resources = 
		{
			indirect_args_buffer = { type = RWBuffer, element_type = uint }
		}
	}

	update = 
	{
		header = "header"

		options =
		[
		]

		constant_buffers = 
		{
			CountCB = 
			{
				num_particles = uint
			}
			/*
			ParamsCB = 
			{
				
			}
			*/
		}
		
		resources = 
		{
			prev_particle_system_state = { type = ConsumeStructuredBuffer, element_type = Particle }
			particle_system_state = { type = AppendStructuredBuffer, element_type = Particle }
		}
	}
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

	inject = 
	{
		hlsl =
		"""
			[numthreads(8,1,1)]
			void cs_main(uint3 dtid : SV_DispatchThreadID, uint3 GroupThreadID : SV_GroupThreadID)
			{
				Particle p;
				p.pad0 = 0.0f;
				/*
				// Initialize position to the current emitter location
				p.position = emitter_location.xyz;

				// Initialize direction to a randomly reflected vector
				p.direction = reflect( direction[GroupThreadID.x], RandomVector.xyz ) * 5.0f;
				*/

				float rand = frac(sin(dot((dtid.xy+1) * delta_time * 1000.0f ,float2(12.9898,78.233))) * 43758.5453);
				float rand_y = frac(sin(dot((dtid.xy+10) * delta_time * 1000.0f ,float2(12.9898,78.233))) * 43758.5453);
				float rand_z = frac(sin(dot((dtid.xy+100) * delta_time * 1000.0f ,float2(12.9898,78.233))) * 43758.5453);

				p.position = float3(0.0f, 0.0f, 0.0f);

				// Initialize direction to a randomly reflected vector
				p.direction = reflect( direction[GroupThreadID.x], float3(rand, rand_y, rand_z) ) * 5.0f;

				/*
				p.position = float3(rand*50.0f, 0.0f, 0.0f);
				p.position.x -=25.0f;

				// Initialize direction to a randomly reflected vector
				p.direction = float3(0.0f, 1.0f, 0.0f);
				*/

				// Initialize the lifetime of the particle in seconds
				p.time = 0.0f;

				// Append the new particle to the output buffer
				particle_system.Append( p );
			}
		"""
	}

	copy_count = 
	{
		hlsl =
		"""
			[numthreads(1,1,1)]
			void cs_main()
			{
				//Render
				indirect_args_buffer[0] = 6 * count;	// index_count_per_instance
				indirect_args_buffer[1] = 1;			// instance_count
				indirect_args_buffer[2] = 0;			// start_index_location
				indirect_args_buffer[3] = 0;			// base_vertex_location
				indirect_args_buffer[4] = 0;			// start_instance_location

				//Update
				indirect_args_buffer[5] = (count+511)/512;	// thread_group_count_x
				indirect_args_buffer[6] = 1;				// thread_group_count_y
				indirect_args_buffer[7] = 1;				// thread_group_count_z
			}
		"""
	}

	update = 
	{
		hlsl =
		"""
			#define GROUP_SIZE 512

			[numthreads(GROUP_SIZE,1,1)]
			void cs_main(uint3 DispatchThreadID : SV_DispatchThreadID)
			{
				uint myID = DispatchThreadID.x + DispatchThreadID.y * GROUP_SIZE + DispatchThreadID.z * GROUP_SIZE * GROUP_SIZE;

				if(myID < num_particles)
				{
					Particle p = prev_particle_system_state.Consume();
					p.position += p.direction * delta_time;
					p.time += delta_time;

					if(p.time < 4.0f)
						particle_system_state.Append( p );
				}
			}
		"""
	}
}