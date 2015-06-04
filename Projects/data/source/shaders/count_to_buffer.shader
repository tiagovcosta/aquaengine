type = "compute"

include = []

compute_kernels = 
{
	count_to_buffer = 
	{
		constant_buffers = 
		{
			ParamsCB = 
			{
				count = uint
			}
		}

		resources = 
		{
			indirect_args_buffer = { type = RWBuffer, element_type = uint }
		}
	}
}

snippets =
{
	count_to_buffer = 
	{
		hlsl =
		"""
			[numthreads(1,1,1)]
			void cs_main()
			{
				indirect_args_buffer[0] = 6 * count;	// vertexCountPerInstance
				indirect_args_buffer[1] = 1;			// instanceCount
				indirect_args_buffer[2] = 0;			// startVertexLocation
				indirect_args_buffer[3] = 0;			// startInstanceLocation
			}
		"""
	}
}