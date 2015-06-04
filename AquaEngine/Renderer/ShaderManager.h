#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015              
/////////////////////////////////////////////////////////////////////////////////////////////


#include "RendererUtilities.h"

#include "RenderDevice\RenderDeviceTypes.h"
#include "RenderDevice\RenderDeviceEnums.h"

#include "..\AquaTypes.h"

//TODO
// CHANGE CONSTANTS OFFSET HANDLING SO IT SUPPORTS PARAM GROUPS WHERE NOT ALL CBUFFERS ARE DYNAMIC
// OR NOT
// BECAUSE IT WOULD REQUIRE KNOWLEDGE OF WHICH CBUFFER THE CONSTANT IS IN

static const uint64_t COMMON_MAGIC_NUMBER  = 0xC072C072C072C072;
static const uint64_t RENDER_MAGIC_NUMBER  = COMMON_MAGIC_NUMBER + 1;
static const uint64_t COMPUTE_MAGIC_NUMBER = RENDER_MAGIC_NUMBER + 1;

namespace aqua
{
	class RenderDevice;
	class Allocator;
	class LinearAllocator;

	class ParameterGroupDesc;

	/*
	struct Permutation
	{
		Permutation(u64 v = 0) : value(v) {}
		u64 value; //do not modify
	};
	*/
	struct ShaderFile
	{
		char*  data;
		size_t size;
		u32    name;
	};

	/*
	struct MapCommand
	{
		void* data;
		u32   data_size;
	};
	*/

	struct ShaderPermutationPass
	{
		const PipelineState* pipeline_state;
		const u8*			 vertex_buffers_bind_info;
		const u8*			 material_params_bind_info;
		const u8*			 instance_params_bind_info;
	};
	
	using ShaderPermutation = const ShaderPermutationPass* const *;

	struct MeshDesc
	{
		Permutation mesh_options_mask;
		u32         elements_names_offset;		//u32 array
		u32         elements_indices_offset;	//u8 array
		u32			elements_streams_offset;	//u8 array
		u32			elements_sizes_offset;		//u8 array
		u32         elements_bits_offset;		//Permutation array
		u8          num_elements;
	};

	struct MeshElement
	{
		u32 name;
		u8  index;
	};

	struct ElementDesc
	{
		u8 stream;
		u8 offset;
	};

	struct MeshPermutationDesc
	{
		Permutation permutation;
		u8          streams_strides[7];
		u8          num_streams;
	};

	struct InputElementDesc;

	struct InputLayoutDesc
	{
		InputElementDesc* elements;
		u8                num_elements;
	};

	namespace mesh_desc
	{
		MeshPermutationDesc getMeshPermutationDesc(const MeshDesc& desc, u8 num_elements, const MeshElement* elements,
												   ElementDesc* out_elements_descs);

		//ElementDesc getElementDesc(const MeshDesc& desc, Permutation permutation, MeshElement element);

		InputLayoutDesc getPermutationInputLayoutDesc(const MeshDesc& desc, const char* const * elements_names,
													  const RenderResourceFormat* elements_formats,
													  Permutation permutation, LinearAllocator& temp_allocator);

		void fillVertexBuffers(const MeshDesc& desc, u8 num_elements, const ElementDesc* elements_descs,
							   u32 num_vertices, const void* const * vertex_data, void* const * vertex_buffers);
	}
	
	/////////////////////////////////////////////////////////////////////////////////////
	/*
	class ParameterGroupDesc
	{
	public:

		const u32* getCBuffersSizes() const;

		u8 getSRVIndex(u32 name) const;
		u8 getUAVIndex(u32 name) const;
		u32 getConstantOffset(u32 name) const;

		u8 getNumCBuffers() const
		{
			return _num_cbuffers;
		}

		u8 getNumSRVs() const
		{
			return _num_srvs;
		}

		u8 getNumUAVs() const
		{
			return _num_uavs;
		}

	private:

		ParameterGroupDesc(const ParameterGroupDesc&) = delete;
		ParameterGroupDesc& operator=(ParameterGroupDesc) = delete;

		//---------------------------------
		//SAME STRUCT AS IN SHADER COMPILER
		//---------------------------------

		u32 _cbuffers_offset;	//u32 array (names) + u32 array (sizes)
		u32 _srvs_names_offset;	//u32 array
		u32 _uavs_names_offset;	//u32 array
		u32 _constants_offset;	//u32 array (names) + u32 array (offsets)

		u8  _num_cbuffers;
		u8  _num_srvs;
		u8  _num_uavs;
		u8  _num_constants;
	};
	*/

	struct ParameterGroupDescSet
	{
		Permutation options_mask;

		u32         num_permutations;
		u32         permutations_nums_offset;	//Permutation array
		u32         permutations_offset;		//ParameterGroupDesc array

		u32         options_names_offset;		//u32 array
		u32		    options_offset;				//Option array
		u8          num_options;
	};

	Permutation enableOption(const ParameterGroupDescSet& group, u32 name, Permutation permutation);
	Permutation disableOption(const ParameterGroupDescSet& group, u32 name, Permutation permutation);

	const ParameterGroupDesc* getParameterGroupDesc(const ParameterGroupDescSet& group, Permutation permutation);
	

	/*
	ParameterGroup* createParameterGroup(const ParameterGroupDesc& desc, u32 dynamic_cbuffers, Allocator& allocator);
	void destroyParameterGroup(ParameterGroup& group, Allocator& allocator);
	
	struct DynamicBuffer
	{
		BufferH buffer;
		void*   data;
		u32     data_size;
	};

	struct DynamicShaderResource
	{
		ShaderResourceH srv;
		ResourceH       resource;
		void*           data;
		u32             data_size;
	};

	struct DynamicParameterGroup
	{
		BufferH*          cbuffers;
		ShaderResourceH*  srvs;
		UnorderedAccessH* uavs;
		MapCommand*       map_commands;

	private:
		DynamicParameterGroup() {}; //Create using 'createParameterGroup' function
	};

	DynamicParameterGroup* createDynamicParameterGroup(const ParameterGroupDesc& desc, Allocator& allocator);
	void destroyDynamicParameterGroup(DynamicParameterGroup& group, Allocator& allocator);
	*/
	/////////////////////////////////////////////////////////////////////////////////////

	class RenderShader
	{
	public:
		
		const MeshDesc* getMeshDesc() const;

		const ParameterGroupDescSet* getMaterialParameterGroupDescSet() const;
		const ParameterGroupDescSet* getInstanceParameterGroupDescSet() const;

		ShaderPermutation getPermutation(Permutation permutation) const;

		u8         getNumPasses() const;
		const u32* getPassesNames() const;

	private:
		
		RenderShader(const ShaderFile& file, RenderDevice& render_device, 
					 Allocator& allocator, LinearAllocator& temp_allocator);

		void destroy();

		struct RenderShaderDesc;

		Allocator&              _allocator;
		RenderDevice&           _render_device;

		RenderShaderDesc*             _desc;
		DeviceChildH*                 _functions;
		InputLayoutH*                 _input_layouts;
		PipelineState*                _pipeline_states; //Store shaders separatly because multiple permutations might use the same shader
		ShaderPermutationPass*		  _passes;
		const ShaderPermutationPass** _permutations; //size = num_permutations * num_passes

		friend class ShaderManager;
	};

	/////////////////////////////////////////////////////////////////////////////////////

	struct ComputeKernelPermutation
	{
		ComputeShaderH shader;
		u8*            params_bind_info;
	};

	class ComputeShader
	{
	public:

		u8 getKernel(u32 name) const;

		const ParameterGroupDescSet* getKernelParameterGroupDescSet(u8 kernel) const;

		const ComputeKernelPermutation* getPermutation(u8 kernel, Permutation permutation) const;

	private:
		ComputeShader(const ShaderFile& file, Allocator& allocator, RenderDevice& render_device);

		void destroy();

		struct ComputeShaderDesc;

		Allocator&                _allocator;
		RenderDevice&             _render_device;

		ComputeShaderDesc*        _desc;
		ComputeKernelPermutation* _kernels_permutations;
		
		friend class ShaderManager;
	};

	class ShaderManager
	{
	public:
		ShaderManager();
		~ShaderManager();

		bool init(const ShaderFile& common_file, u8 num_render_shaders, const ShaderFile* render_shaders_files, 
			      u8 num_compute_shaders, const ShaderFile* compute_shaders_files,
				  RenderDevice& render_device, Allocator& allocator, LinearAllocator& temp_allocator);

		static const u8 INVALID_SHADER = UINT8_MAX;

		const RenderShader* getRenderShader(u32 name) const;
		u8            getRenderShaderIndex(u32 name) const;
		//u8            getShaderPassIndex(u8 render_shader_index, u32 pass_name) const;
		//const Shader* getShader(u8 render_shader_index, u8 pass_index, Permutation permutation) const;

		Permutation   getOptionBits(u8 render_shader_index, u32 permutation_name) const;

		const ComputeShader*  getComputeShader(u32 name) const;

		const ParameterGroupDesc* getFrameParametersDesc() const;
		const ParameterGroupDesc* getViewParametersDesc() const;

		const ParameterGroupDesc* getPassParametersDesc(u8 pass_index) const;
		const ParameterGroupDesc* getPassesParametersDescs() const;
		u8						  getPassIndex(u32 name) const;
		u8						  getNumPasses() const;

		const u8* getFrameParametersBindInfo() const;
		const u8* getViewParametersBindInfo() const;
		const u8* getPassParametersBindInfo(u32 pass_index) const;

		//const ParameterGroupPermutation* getMaterialParametersDesc(u32 shader_pack_name, Permutation permutation, u32 pass_name) const;

		bool          reloadShaderPack(u32 name, const char* data, size_t size);

		int           shutdown();

	private:
		ShaderManager(const ShaderManager&);
		ShaderManager& operator=(ShaderManager);

		struct CommonDesc;

		RenderDevice*             _render_device;
		Allocator*                _allocator;
		LinearAllocator*          _temp_allocator;

		CommonDesc*               _common_desc;

		u32*                      _render_shaders_names;
		RenderShader*             _render_shaders;

		u32*                      _compute_shaders_names;
		ComputeShader*            _compute_shaders;

		u8                        _num_render_shaders;
		u8                        _num_compute_shaders;
	};
};