#include "ShaderManager.h"

#include "RenderDevice\RenderDevice.h"
#include "RenderDevice\RenderDeviceDescs.h"

#include "..\Utilities\BinaryReader.h"
//#include "..\Utilities\File.h"
#include "..\Core\Allocators\ScopeStack.h"
#include "..\Core\Allocators\LinearAllocator.h"
#include "..\Core\Allocators\Allocator.h"

#include "..\Utilities\StringID.h"

#include "RenderDevice\ParameterGroup.h"

namespace aqua
{
	namespace mesh_desc
	{
		MeshPermutationDesc getMeshPermutationDesc(const MeshDesc& desc, u8 num_elements, const MeshElement* elements,
												   ElementDesc* out_elements_descs)
		{
			auto elements_names   = (const u32*)pointer_math::add(&desc, desc.elements_names_offset);
			auto elements_indices = (const u8*)pointer_math::add(&desc, desc.elements_indices_offset);
			auto elements_streams = (const u8*)pointer_math::add(&desc, desc.elements_streams_offset);
			auto elements_sizes   = (const u8*)pointer_math::add(&desc, desc.elements_sizes_offset);
			auto elements_bits    = (const Permutation*)pointer_math::add(&desc, desc.elements_bits_offset);

			MeshPermutationDesc out;
			out.num_streams = 0;

			u8 current_stream = UINT8_MAX;

			for(u8 i = 0; i < num_elements; i++)
			{
				u8 index = UINT8_MAX;

				//find element index
				for(index = 0; index < desc.num_elements; index++)
				{
					if(elements_names[index] == elements[i].name && elements_indices[index] == elements[i].index)
						break;
				}

				if(index == UINT8_MAX)
				{
					out_elements_descs[i].stream = UINT8_MAX; //element not found
					out_elements_descs[i].offset = UINT8_MAX;
					continue;
				}

				//Add element bits
				out.permutation.value |= elements_bits[index].value;

				if(current_stream != elements_streams[index])
				{
					out.num_streams++;
					current_stream = elements_streams[index];

					out_elements_descs[i].stream = current_stream;
					out_elements_descs[i].offset = 0;

					out.streams_strides[out.num_streams - 1] = elements_sizes[index];

				}
				else
				{
					out_elements_descs[i].stream = current_stream;
					out_elements_descs[i].offset = out.streams_strides[out.num_streams - 1];

					out.streams_strides[out.num_streams - 1] += elements_sizes[index];
				}
			}

			return out;
		}

		InputLayoutDesc getPermutationInputLayoutDesc(const MeshDesc& desc, const char* const * elements_names,
			const RenderResourceFormat* elements_formats,
			Permutation permutation, LinearAllocator& temp_allocator)
		{
			auto elements_indices = (const u8*)pointer_math::add(&desc, desc.elements_indices_offset);
			auto elements_streams = (const u8*)pointer_math::add(&desc, desc.elements_streams_offset);
			auto elements_sizes   = (const u8*)pointer_math::add(&desc, desc.elements_sizes_offset);
			auto elements_bits    = (const Permutation*)pointer_math::add(&desc, desc.elements_bits_offset);

			InputLayoutDesc out;
			out.num_elements = 0;

			for(u8 i = 0; i < desc.num_elements; i++)
			{
				if((elements_bits[i].value & permutation.value) != elements_bits[i].value)
					continue;

				out.num_elements++;
			}

			out.elements = allocator::allocateArray<InputElementDesc>(temp_allocator, out.num_elements);

			u8 current_out_element = 0;

			for(u8 i = 0; i < desc.num_elements; i++)
			{
				if((elements_bits[i].value & permutation.value) != elements_bits[i].value)
					continue;

				out.elements[current_out_element].name                    = elements_names[i];
				out.elements[current_out_element].index                   = elements_indices[i];;
				out.elements[current_out_element].input_slot              = elements_streams[i];
				out.elements[current_out_element].instance_data_step_rate = 0;
				out.elements[current_out_element].format                  = elements_formats[i];
				out.elements[current_out_element].input_class             = InputClassification::PER_VERTEX_DATA;

				current_out_element++;
			}

			return out;
		}

		void fillVertexBuffers(const MeshDesc& desc, u8 num_elements, const ElementDesc* elements_descs,
			u32 num_vertices, const void* const * vertex_elements, const u8* elements_sizes,
			const u8* streams_strides, void* const * vertex_buffers)
		{
			for(u8 i = 0; i < num_elements; i++)
			{
				u8 stream_stride = streams_strides[elements_descs[i].stream];

				for(u32 j = 0; j < num_vertices; j++)
				{
					u8 offset = j * stream_stride + elements_descs[i].offset;

					memcpy(pointer_math::add(vertex_buffers[elements_descs[i].stream], offset),
							pointer_math::add(vertex_elements[i], j * elements_sizes[i]),
							i);
				}
			}
		}
	}

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------

	/*
	const u32* ParameterGroupDesc::getCBuffersSizes() const
	{
		if(_num_cbuffers == 0)
			return nullptr;

		return (const u32*)pointer_math::add(this, _cbuffers_offset + _num_cbuffers*sizeof(u32));
	}

	u8 ParameterGroupDesc::getSRVIndex(u32 name) const
	{
		auto srvs_names = (const u32*)pointer_math::add(this, _srvs_names_offset);

		for(u32 i = 0; i < _num_srvs; i++)
		{
			if(srvs_names[i] == name)
				return i;
		}

		return UINT8_MAX;
	}

	u8 ParameterGroupDesc::getUAVIndex(u32 name) const
	{
		auto uavs_names = (const u32*)pointer_math::add(this, _uavs_names_offset);

		for(u32 i = 0; i < _num_uavs; i++)
		{
			if(uavs_names[i] == name)
				return i;
		}

		return UINT8_MAX;
	}

	u32 ParameterGroupDesc::getConstantOffset(u32 name) const
	{
		auto constants_names = (const u32*)pointer_math::add(this, _constants_offset);
		auto constants_offsets = (const u32*)pointer_math::add(this, _constants_offset + _num_constants*sizeof(u32));

		for(u32 i = 0; i < _num_constants; i++)
		{
			if(constants_names[i] == name)
				return constants_offsets[i];
		}

		return UINT32_MAX;
	}
	*/

	struct Option
	{
		Permutation bits;
		Permutation group_clear_mask;
	};

	Permutation enableOption(const ParameterGroupDescSet& group, u32 name, Permutation permutation)
	{
		auto options_names = (const u32*)pointer_math::add(&group, group.options_names_offset);
		auto options       = (const Option*)pointer_math::add(&group, group.options_offset);

		for(u32 i = 0; i < group.num_options; i++)
		{
			if(options_names[i] == name)
			{
				permutation.value &= options[i].group_clear_mask.value;
				permutation.value |= options[i].bits.value;
				return permutation;
			}
		}

		Logger::get().write(MESSAGE_LEVEL::WARNING_MESSAGE, CHANNEL::RENDERER, "Shader Option %u not found!", name);

		return permutation;
	}

	Permutation disableOption(const ParameterGroupDescSet& group, u32 name, Permutation permutation)
	{
		auto options_names = (const u32*)pointer_math::add(&group, group.options_names_offset);
		auto options       = (const Option*)pointer_math::add(&group, group.options_offset);

		for(u32 i = 0; i < group.num_options; i++)
		{
			if(options_names[i] == name)
			{
				permutation.value &= options[i].group_clear_mask.value;
				return permutation;
			}
		}

		return permutation;
	}

	const ParameterGroupDesc* getParameterGroupDesc(const ParameterGroupDescSet& group, Permutation permutation)
	{
		permutation.value &= group.options_mask.value;

		auto permutations_nums = (const Permutation*)pointer_math::add(&group, group.permutations_nums_offset);
		auto permutations      = (const ParameterGroupDesc*)pointer_math::add(&group, group.permutations_offset);

		for(u32 i = 0; i < group.num_permutations; i++)
		{
			if(permutations_nums[i].value == permutation.value)
				return &permutations[i];
		}

		return nullptr;
	}
	/*
	ParameterGroup* createParameterGroup(const ParameterGroupDesc& desc, u32 dynamic_cbuffers, Allocator& allocator)
	{
		u8 num_cbuffers = desc.getNumCBuffers();
		u8 num_srvs     = desc.getNumSRVs();
		u8 num_uavs     = desc.getNumUAVs();

		//Calculate memory block size
		u32 size = sizeof(ParameterGroup) + num_cbuffers*sizeof(BufferH) + num_srvs*sizeof(ShaderResourceH) +
				   num_uavs*sizeof(UnorderedAccessH);

		if(dynamic_cbuffers != 0 && num_cbuffers > 0)
		{
			size += num_cbuffers*sizeof(MapCommand);

			//Make sure that constants will be 16 byte aligned
			//Check https://developer.nvidia.com/content/constant-buffers-without-constant-pain-0

			u8 diff = size % 16;

			if(diff != 0)
			{
				size += 16 - diff;
			}

			auto cbuffers_sizes = desc.getCBuffersSizes();

			for(u8 i = 0; i < num_cbuffers; i++)
				size += cbuffers_sizes[i];
		}

		//allocate continuous block to store all parameters
		ParameterGroup* group = (ParameterGroup*)allocator.allocate(size, 16);

		ASSERT(group != nullptr);

		void* offset = group + 1;

		if(num_cbuffers > 0)
		{
			group->cbuffers = (BufferH*)offset;

			ASSERT(pointer_math::isAligned(group->cbuffers, __alignof(BufferH)));

			offset = group->cbuffers + num_cbuffers;
		}
		else
			group->cbuffers = nullptr;

		if(num_srvs > 0)
		{
			group->srvs = (ShaderResourceH*)offset;

			ASSERT(pointer_math::isAligned(group->srvs, __alignof(ShaderResourceH)));

			offset = group->srvs + num_srvs;
		}
		else
			group->srvs = nullptr;

		if(num_uavs > 0)
		{
			group->uavs = (UnorderedAccessH*)offset;

			ASSERT(pointer_math::isAligned(group->uavs, __alignof(UnorderedAccessH)));

			offset = group->uavs + num_uavs;
		}
		else
			group->uavs = nullptr;

		if(dynamic_cbuffers != 0 && num_cbuffers > 0)
		{
			group->map_commands = (MapCommand*)offset;

			ASSERT(pointer_math::isAligned(group->map_commands, __alignof(MapCommand)));

			offset = group->map_commands + num_cbuffers;
			offset = pointer_math::alignForward(offset, 16);

			auto cbuffers_sizes = desc.getCBuffersSizes();

			for(u8 i = 0; i < num_cbuffers; i++)
			{
				if(dynamic_cbuffers & 1 << i)
				{
					group->map_commands[i].data      = offset;
					group->map_commands[i].data_size = cbuffers_sizes[i];

					ASSERT(pointer_math::isAligned(group->map_commands[i].data, 16));
				}
				else
				{
					group->map_commands[i].data = nullptr;
				}

				offset = pointer_math::add(offset, cbuffers_sizes[i]);
			}
		}
		else
			group->map_commands = nullptr;

		return group;
	}

	void destroyParameterGroup(ParameterGroup& group, Allocator& allocator)
	{
		allocator.deallocate(&group);
	}
	*/

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------

	struct RenderShader::RenderShaderDesc
	{
		MeshDesc			  mesh_desc;
		ParameterGroupDescSet material_params;
		ParameterGroupDescSet instance_params;

		u32                   passes_names_offset;		//u32 array
		u32                   params_bind_info_offset;	//u8 array

		u32                   permutations_nums_offset;	//Permutation array
		u32                   num_permutations;

		u32                   num_functions;
		u32                   num_input_layouts;
		u32                   num_shaders;
		u32                   num_shader_passes;

		u8                    num_passes;
	};

	RenderShader::RenderShader(const ShaderFile& file, RenderDevice& render_device,
		Allocator& allocator, LinearAllocator& temp_allocator)
		: _allocator(allocator), _render_device(render_device)
	{
		BinaryReader br(file.data, file.size);

		u64 magic_number = br.next<u64>();

		ASSERT("Invalid Render shader file" && magic_number == RENDER_MAGIC_NUMBER);

		u64 desc_size = br.next<u64>();

		_desc = (RenderShaderDesc*)_allocator.allocate(desc_size, 8);

		const void* source = br.nextRawData(desc_size);

		memcpy(_desc, source, desc_size);

		_functions = allocator::allocateArrayNoConstruct<DeviceChildH>(_allocator, _desc->num_functions);

		size_t render_shader_desc_end_offset = br.getPosition();

		u64 num_functions = br.next<u64>();

		u32 current_function = 0;

		for(u64 i = 0; i < num_functions; i++)
		{
			const char* function_name = br.nextString();

			u8 type = br.next<u8>();

			size_t num_permutations = static_cast<size_t>(br.next<u32>());

			//Switch on the shader function type
			switch(type)
			{
			case (u8)ShaderType::VERTEX_SHADER:
				for(size_t k = 0; k < num_permutations; k++)
				{
					size_t length = static_cast<size_t>(br.next<u32>());

					_render_device.createVertexShader(br.nextRawData(length), length, (VertexShaderH&)_functions[current_function]);

#if _DEBUG
					RenderDevice::setDebugName(_functions[current_function], function_name);
#endif

					current_function++;
				}
				break;
			case (u8)ShaderType::HULL_SHADER:
				for(size_t k = 0; k < num_permutations; k++)
				{
					size_t length = static_cast<size_t>(br.next<u32>());

					_render_device.createHullShader(br.nextRawData(length), length, (HullShaderH&)_functions[current_function]);

#if _DEBUG
					RenderDevice::setDebugName(_functions[current_function], function_name);
#endif

					current_function++;
				}
				break;
			case (u8)ShaderType::DOMAIN_SHADER:
				for(size_t k = 0; k < num_permutations; k++)
				{
					size_t length = static_cast<size_t>(br.next<u32>());

					_render_device.createDomainShader(br.nextRawData(length), length, (DomainShaderH&)_functions[current_function]);

#if _DEBUG
					RenderDevice::setDebugName(_functions[current_function], function_name);
#endif

					current_function++;
				}
				break;
			case (u8)ShaderType::GEOMETRY_SHADER:
				for(size_t k = 0; k < num_permutations; k++)
				{
					size_t length = static_cast<size_t>(br.next<u32>());

					_render_device.createGeometryShader(br.nextRawData(length), length, (GeometryShaderH&)_functions[current_function]);

#if _DEBUG
					RenderDevice::setDebugName(_functions[current_function], function_name);
#endif

					current_function++;
				}
				break;
			case (u8)ShaderType::PIXEL_SHADER:
				for(size_t k = 0; k < num_permutations; k++)
				{
					size_t length = static_cast<size_t>(br.next<u32>());

					_render_device.createPixelShader(br.nextRawData(length), length, (PixelShaderH&)_functions[current_function]);

#if _DEBUG
					RenderDevice::setDebugName(_functions[current_function], function_name);
#endif

					current_function++;
				}
				break;
			default:
				ASSERT("Invalid shader type" && false);
			}
		}

		ASSERT((_desc->mesh_desc.num_elements > 0 && _desc->num_input_layouts > 0) || 
			   (_desc->mesh_desc.num_elements == 0 && _desc->num_input_layouts == 0));

		if(_desc->mesh_desc.num_elements > 0 && _desc->num_input_layouts > 0)
		{
			ScopeStack scope(temp_allocator);

			const char**		  input_elements_names = scope.newPODArray<const char*>(_desc->mesh_desc.num_elements);
			RenderResourceFormat* input_elements_types =
				scope.newPODArray<RenderResourceFormat>(_desc->mesh_desc.num_elements);

			for(u8 i = 0; i < _desc->mesh_desc.num_elements; i++)
			{
				input_elements_names[i] = br.nextString();

				u8 type = br.next<u8>();

				switch(type)
				{
				case 0:
					input_elements_types[i] = RenderResourceFormat::R32_FLOAT;
					break;
				case 1:
					input_elements_types[i] = RenderResourceFormat::RG32_FLOAT;
					break;
				case 2:
					input_elements_types[i] = RenderResourceFormat::RGB32_FLOAT;
					break;
				case 3:
					input_elements_types[i] = RenderResourceFormat::RGBA32_FLOAT;
					break;
				case 4:
					input_elements_types[i] = RenderResourceFormat::R32_UINT;
					break;
				case 5:
					input_elements_types[i] = RenderResourceFormat::RG32_UINT;
					break;
				case 6:
					input_elements_types[i] = RenderResourceFormat::RGB32_UINT;
					break;
				case 7:
					input_elements_types[i] = RenderResourceFormat::RGBA32_UINT;
					break;
				default:
					ASSERT("Invalid input element type!" && false);
					break;
				}
			}

			_input_layouts = allocator::allocateArrayNoConstruct<InputLayoutH>(_allocator, _desc->num_input_layouts);

			BinaryReader bytecode_reader(br.getData(), br.getSize());

			for(u32 i = 0; i < _desc->num_input_layouts; i++)
			{
				Permutation mesh_permutation = br.next<Permutation>();
				u64         bytecode_offset = br.next<u64>();

				InputLayoutDesc il_desc = mesh_desc::getPermutationInputLayoutDesc(_desc->mesh_desc, input_elements_names,
					input_elements_types, mesh_permutation,
					temp_allocator);

				bytecode_reader.setPosition(render_shader_desc_end_offset + (size_t)bytecode_offset);

				u32 byte_code_length = bytecode_reader.next<u32>();

				_render_device.createInputLayout(il_desc.elements, il_desc.num_elements,
					bytecode_reader.nextRawData(static_cast<size_t>(byte_code_length)),
					byte_code_length, _input_layouts[i]);
			}
		}
		else
		{
			_input_layouts = nullptr;
		}

		struct RuntimeShader
		{
			u32 input_layout_index;
			u32 vs_function_index;
			u32 hs_function_index;
			u32 ds_function_index;
			u32 gs_function_index;
			u32 ps_function_index;
			u8  rasterizer_state_index;
			u8  blend_state_index;
			u8  depth_stencil_state_index;
		};

		_pipeline_states = allocator::allocateArrayNoConstruct<PipelineState>(_allocator, _desc->num_shaders);

		for(u32 i = 0; i < _desc->num_shaders; i++)
		{
			RuntimeShader k = br.next<RuntimeShader>();

			if(k.input_layout_index != UINT32_MAX)
				_pipeline_states[i].input_layout = (InputLayoutH)_input_layouts[k.input_layout_index];
			else
				_pipeline_states[i].input_layout = nullptr;

			if(k.vs_function_index != UINT32_MAX)
				_pipeline_states[i].vertex_shader = (VertexShaderH)_functions[k.vs_function_index];
			else
				_pipeline_states[i].vertex_shader = nullptr;

			if(k.hs_function_index != UINT32_MAX)
				_pipeline_states[i].hull_shader = (HullShaderH)_functions[k.hs_function_index];
			else
				_pipeline_states[i].hull_shader = nullptr;

			if(k.ds_function_index != UINT32_MAX)
				_pipeline_states[i].domain_shader = (DomainShaderH)_functions[k.ds_function_index];
			else
				_pipeline_states[i].domain_shader = nullptr;

			if(k.gs_function_index != UINT32_MAX)
				_pipeline_states[i].geometry_shader = (GeometryShaderH)_functions[k.gs_function_index];
			else
				_pipeline_states[i].geometry_shader = nullptr;

			if(k.ps_function_index != UINT32_MAX)
				_pipeline_states[i].pixel_shader = (PixelShaderH)_functions[k.ps_function_index];
			else
				_pipeline_states[i].pixel_shader = nullptr;

			_pipeline_states[i].blend_state         = k.blend_state_index;
			_pipeline_states[i].depth_stencil_state = k.depth_stencil_state_index;
			_pipeline_states[i].rasterizer_state    = k.rasterizer_state_index;
		}

		_passes = allocator::allocateArrayNoConstruct<ShaderPermutationPass>(_allocator, _desc->num_shader_passes);

		auto params_bind_info = (const u8*)pointer_math::add(_desc, _desc->params_bind_info_offset);

		struct RuntimeShaderPermutationPass
		{
			u64 shader_index;
			u32 vertex_buffers_bind_info_index;
			u32 material_params_bind_info_index;
			u32 instance_params_bind_info_index;
		};

		for(u32 i = 0; i < _desc->num_shader_passes; i++)
		{
			RuntimeShaderPermutationPass k = br.next<RuntimeShaderPermutationPass>();

			ShaderPermutationPass& permutation = _passes[i];

			permutation.pipeline_state = &_pipeline_states[k.shader_index];

			if(k.vertex_buffers_bind_info_index != UINT32_MAX)
				permutation.vertex_buffers_bind_info = &params_bind_info[k.vertex_buffers_bind_info_index];
			else
				permutation.vertex_buffers_bind_info = nullptr;

			if(k.material_params_bind_info_index != UINT32_MAX)
				permutation.material_params_bind_info = &params_bind_info[k.material_params_bind_info_index];
			else
				permutation.material_params_bind_info = nullptr;

			if(k.instance_params_bind_info_index != UINT32_MAX)
				permutation.instance_params_bind_info = &params_bind_info[k.instance_params_bind_info_index];
			else
				permutation.instance_params_bind_info = nullptr;
		}

		u32 num_permutations = /*_desc->num_shaders **/ _desc->num_permutations;

		_permutations = allocator::allocateArrayNoConstruct<const ShaderPermutationPass*>(_allocator, num_permutations);

		u64* indices = br.nextArray<u64>(num_permutations);

		for(u32 i = 0; i < num_permutations; i++)
		{
			u64 index = indices[i];

			if(index == UINT64_MAX)
				_permutations[i] = nullptr;
			else
				_permutations[i] = &_passes[index];
		}
	}

	void RenderShader::destroy()
	{
		allocator::deallocateArray(_allocator, _permutations);
		allocator::deallocateArray(_allocator, _passes);
		allocator::deallocateArray(_allocator, _pipeline_states);

		if(_desc->num_input_layouts > 0)
		{
			for(u32 i = 0; i < _desc->num_input_layouts; i++)
				RenderDevice::release(_input_layouts[i]);

			allocator::deallocateArray(_allocator, _input_layouts);
		}

		for(u32 i = 0; i < _desc->num_functions; i++)
			RenderDevice::release(_functions[i]);

		allocator::deallocateArray(_allocator, _functions);

		_allocator.deallocate(_desc);
	}

	const MeshDesc* RenderShader::getMeshDesc() const
	{
		return &_desc->mesh_desc;
	}

	const ParameterGroupDescSet* RenderShader::getMaterialParameterGroupDescSet() const
	{
		return &_desc->material_params;
	}

	const ParameterGroupDescSet* RenderShader::getInstanceParameterGroupDescSet() const
	{
		return &_desc->instance_params;
	}

	ShaderPermutation RenderShader::getPermutation(Permutation permutation) const
	{
		auto permutations_nums = (const Permutation*)pointer_math::add(_desc, _desc->permutations_nums_offset);

		u64 num = _desc->num_permutations / _desc->num_passes;

		for(u32 i = 0; i < num; i++)
		{
			if(permutations_nums[i].value == permutation.value)
				return &_permutations[i * _desc->num_passes];
		}

		return nullptr;
	}

	u8 RenderShader::getNumPasses() const
	{
		return _desc->num_passes;
	}

	const u32* RenderShader::getPassesNames() const
	{
		return (const u32*)pointer_math::add(_desc, _desc->passes_names_offset);
	}

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------

	struct ComputeShader::ComputeShaderDesc
	{
		u32 kernels_names_offset;				//u32 array
		u32 kernels_offset;						//ParameterGroup array
		u32 kernels_permutations_start_offset;	//u32 array
		u32 params_bind_info_offset;			//u8 array
		u32 num_kernel_permutations;
		u8  num_kernels;
	};

	ComputeShader::ComputeShader(const ShaderFile& file, Allocator& allocator, RenderDevice& render_device)
		: _allocator(allocator), _render_device(render_device)
	{
		BinaryReader br(file.data, file.size);

		u64 magic_number = br.next<u64>();

		ASSERT("Invalid Compute shader file" && magic_number == COMPUTE_MAGIC_NUMBER);

		u64 desc_size = br.next<u64>();

		_desc = (ComputeShaderDesc*)_allocator.allocate(desc_size, 8);

		const void* source = br.nextRawData(desc_size);

		memcpy(_desc, source, desc_size);

		_kernels_permutations =
			allocator::allocateArrayNoConstruct<ComputeKernelPermutation>(_allocator, _desc->num_kernel_permutations);

		u32 current_permutation = 0;

		u8* params_bind_info = (u8*)pointer_math::add(_desc, _desc->params_bind_info_offset);

		for(u32 i = 0; i < _desc->num_kernels; i++)
		{
			const char* kernel_name = br.nextString();

			u8 type = br.next<u8>();

			ASSERT("Invalid Compute shader type" && type == (u8)ShaderType::COMPUTE_SHADER);

			u32 num_permutations = br.next<u32>();

			u32 current_permutation2 = current_permutation;

			for(u32 j = 0; j < num_permutations; j++)
			{
				size_t length = static_cast<size_t>(br.next<u32>());

				_render_device.createComputeShader(br.nextRawData(length), length,
					_kernels_permutations[current_permutation].shader);

#if _DEBUG
				RenderDevice::setDebugName(_kernels_permutations[current_permutation].shader, kernel_name);
#endif

				current_permutation++;
			}

			for(u32 j = 0; j < num_permutations; j++)
			{
				_kernels_permutations[current_permutation2].params_bind_info = &params_bind_info[br.next<u32>()];

				current_permutation2++;
			}
		}
	}

	void ComputeShader::destroy()
	{
		for(u32 i = 0; i < _desc->num_kernel_permutations; i++)
		{
			RenderDevice::release(_kernels_permutations[i].shader);
		}

		allocator::deallocateArray(_allocator, _kernels_permutations);
		_allocator.deallocate(_desc);
	}

	u8 ComputeShader::getKernel(u32 name) const
	{
		auto kernels_names = (const u32*)pointer_math::add(_desc, _desc->kernels_names_offset);

		for(u8 i = 0; i < _desc->num_kernels; i++)
		{
			if(kernels_names[i] == name)
				return i;
		}

		return UINT8_MAX;
	}

	const ParameterGroupDescSet* ComputeShader::getKernelParameterGroupDescSet(u8 kernel) const
	{
		auto kernels_names = (const u32*)pointer_math::add(_desc, _desc->kernels_names_offset);

		auto kernels_params = (const ParameterGroupDescSet*)pointer_math::add(_desc, _desc->kernels_offset);

		return &kernels_params[kernel];
	}

	const ComputeKernelPermutation* ComputeShader::getPermutation(u8 kernel, Permutation permutation) const
	{
		auto params_desc_array = (const ParameterGroupDescSet*)pointer_math::add(_desc, _desc->kernels_offset);
		auto& params_desc      = params_desc_array[kernel];

		auto permutations_nums = (const Permutation*)pointer_math::add(&params_desc, params_desc.permutations_nums_offset);

		u32 index;

		for(index = 0; index < params_desc.num_permutations; index++)
		{
			if(permutations_nums[index].value == permutation.value)
				break;
		}

		if(index == params_desc.num_permutations)
			return nullptr;

		auto kernels_permutations_start = (const u32*)pointer_math::add(_desc, _desc->kernels_permutations_start_offset);

		return &_kernels_permutations[kernels_permutations_start[kernel] + index];
	}

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------

	struct ShaderManager::CommonDesc
	{
		u32 passes_names_offset;				//u32 array
		u32 params_offset;						//ParameterGroupDesc array
		u32 params_bind_info_offsets_offset;	//u32 array
		u8  num_passes;
	};

	ShaderManager::ShaderManager() : _render_device(nullptr), _allocator(nullptr), _temp_allocator(nullptr),
		_num_render_shaders(0), _num_compute_shaders(0)
	{

	}

	ShaderManager::~ShaderManager()
	{

	}

	bool ShaderManager::init(const ShaderFile& common_file, u8 num_render_shaders, const ShaderFile* render_shaders_files,
		u8 num_compute_shaders, const ShaderFile* compute_shaders_files,
		RenderDevice& render_device, Allocator& allocator, LinearAllocator& temp_allocator)
	{
		_render_device  = &render_device;
		_allocator      = &allocator;
		_temp_allocator = &temp_allocator;

		//Load common desc
		/*
		size_t common_size = file::getFileSize("data/shaders/common.cshader");
		_common_desc = (CommonDesc*)_allocator->allocate(common_size, 8);

		file::readFile("data/shaders/common.cshader", false, (char*)_common_desc);
		*/

		_common_desc = (CommonDesc*)_allocator->allocate(common_file.size, 8);
		memcpy(_common_desc, common_file.data, common_file.size);

		//Create render shaders
		_num_render_shaders = num_render_shaders;

		if(_num_render_shaders > 0)
		{
			_render_shaders_names = allocator::allocateArray<u32>(*_allocator, _num_render_shaders);

			ASSERT(_render_shaders_names != nullptr);

			_render_shaders = allocator::allocateArrayNoConstruct<RenderShader>(*_allocator, _num_render_shaders);

			ASSERT(_render_shaders != nullptr);

			for(u8 i = 0; i < _num_render_shaders; i++)
			{
				_render_shaders_names[i] = render_shaders_files[i].name;
				new (&_render_shaders[i]) RenderShader(render_shaders_files[i], *_render_device,
					*_allocator, *_temp_allocator);
				/*
				BinaryReader br(render_shaders_files[i].data, render_shaders_files[i].size);

				loadRenderShader(br, _render_shaders[i]);
				*/
			}
		}

		//Create compute shaders
		_num_compute_shaders = num_compute_shaders;

		if(_num_compute_shaders > 0)
		{
			_compute_shaders_names = allocator::allocateArrayNoConstruct<u32>(*_allocator, _num_compute_shaders);

			ASSERT(_compute_shaders_names != nullptr);

			_compute_shaders = allocator::allocateArrayNoConstruct<ComputeShader>(*_allocator, _num_compute_shaders);

			ASSERT(_compute_shaders != nullptr);

			for(u8 i = 0; i < _num_compute_shaders; i++)
			{
				_compute_shaders_names[i] = compute_shaders_files[i].name;
				new (&_compute_shaders[i]) ComputeShader(compute_shaders_files[i], *_allocator, *_render_device);
			}
		}

		return true;
	}

	int ShaderManager::shutdown()
	{
		//Deallocate compute shaders
		for(u8 i = 0; i < _num_compute_shaders; i++)
			_compute_shaders[i].destroy();

		if(_num_compute_shaders > 0)
		{
			allocator::deallocateArray(*_allocator, _compute_shaders);
			allocator::deallocateArray(*_allocator, _compute_shaders_names);
		}

		//Deallocate render shaders
		for(u8 i = 0; i < _num_render_shaders; i++)
			_render_shaders[i].destroy();

		if(_num_render_shaders > 0)
		{
			allocator::deallocateArray(*_allocator, _render_shaders);
			allocator::deallocateArray(*_allocator, _render_shaders_names);
		}

		_allocator->deallocate(_common_desc);

		return 1;
	}

	const RenderShader* ShaderManager::getRenderShader(u32 name) const
	{
		for(u8 i = 0; i < _num_render_shaders; i++)
		{
			if(name == _render_shaders_names[i])
				return &_render_shaders[i];
		}

		return nullptr;
	}

	u8 ShaderManager::getRenderShaderIndex(u32 name) const
	{
		for(u8 i = 0; i < _num_render_shaders; i++)
		{
			if(name == _render_shaders_names[i])
				return i;
		}

		return MAXUINT8;
	}

	const ComputeShader* ShaderManager::getComputeShader(u32 name) const
	{
		for(u8 i = 0; i < _num_compute_shaders; i++)
		{
			if(_compute_shaders_names[i] == name)
			{
				return &_compute_shaders[i];
			}
		}

		return nullptr;
	}

	const ParameterGroupDesc* ShaderManager::getFrameParametersDesc() const
	{
		auto params = (const ParameterGroupDesc*)pointer_math::add(_common_desc, _common_desc->params_offset);

		return &params[0];
	};

	const ParameterGroupDesc* ShaderManager::getViewParametersDesc() const
	{
		auto params = (const ParameterGroupDesc*)pointer_math::add(_common_desc, _common_desc->params_offset);

		return &params[1];
	};

	const ParameterGroupDesc* ShaderManager::getPassParametersDesc(u8 pass_index) const
	{
		auto params = (const ParameterGroupDesc*)pointer_math::add(_common_desc, _common_desc->params_offset);

		return &params[pass_index + 2];
	};

	const ParameterGroupDesc* ShaderManager::getPassesParametersDescs() const
	{
		auto params = (const ParameterGroupDesc*)pointer_math::add(_common_desc, _common_desc->params_offset);

		return &params[2];
	};

	u8 ShaderManager::getPassIndex(u32 pass_name) const
	{
		const u32* passes_names = (const u32*)pointer_math::add(_common_desc, _common_desc->passes_names_offset);

		for(u8 i = 0; i < _common_desc->num_passes; i++)
		{
			if(passes_names[i] == pass_name)
				return i;
		}

		return UINT8_MAX;
	};

	u8 ShaderManager::getNumPasses() const
	{
		return _common_desc->num_passes;
	};

	const u8* ShaderManager::getFrameParametersBindInfo() const
	{
		u32* params_bind_info_offsets =
			(u32*)pointer_math::add(_common_desc, _common_desc->params_bind_info_offsets_offset);

		return (u8*)pointer_math::add(_common_desc, params_bind_info_offsets[0]);
	};

	const u8* ShaderManager::getViewParametersBindInfo() const
	{
		u32* params_bind_info_offsets =
			(u32*)pointer_math::add(_common_desc, _common_desc->params_bind_info_offsets_offset);

		return (u8*)pointer_math::add(_common_desc, params_bind_info_offsets[1]);
	};

	const u8* ShaderManager::getPassParametersBindInfo(u32 pass_index) const
	{
		auto params_bind_info_offsets =
			(const u32*)pointer_math::add(_common_desc, _common_desc->params_bind_info_offsets_offset);

		return (u8*)pointer_math::add(_common_desc, params_bind_info_offsets[pass_index + 2]);
	};
}