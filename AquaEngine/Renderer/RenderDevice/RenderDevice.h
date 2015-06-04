#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "RenderDeviceTypes.h"
#include "RenderDeviceEnums.h"
#include "RenderDeviceDescs.h"

#include "ParameterGroup.h"

#include "..\..\Core\Containers\HashMap.h"
#include "..\..\Utilities\Debug.h"
#include "..\..\Utilities\Logger.h"

#include "..\..\AquaTypes.h"

//#define RENDER_INLINE __forceinline

#define RENDER_INLINE inline

/*
-------------------------------------------------------------------------------------------------
|||||||||||||| RESOURCE BINDING OVERVIEW ||||||||||||||||||||||||||||||||||||||||||||||||||||||||
-------------------------------------------------------------------------------------------------

	Resource binding is handled by 4 classes:

		- CachedParameterGroup - Immutable list of resources to bind.
								 The implementation is platform specific (eg: In D3D12 this is simply a DescriptorTable)

		- ParameterGroup - Dynamic list of resources to bind. 
						   List of resources inside ParamterGroup can be changed at runtime.
						   ParameterGroup are "cached" in the RenderDevice which returns a CachedParameterGroup.

		- ParameterGroupDesc - Contains a descripton of what parameters a shader needs.
							   Used by the RenderDevice to create ParameterGroups.

		- RenderDevice - 

		Example usage:

			- During loading:

				- Load a shader;
				- Calculate the required permutation;
				- Get the ParameterGroupDesc of that permutation;
				- Call RenderDevice::createParameterGroup(...);
				- Set constant parameters

			- Every frame:

				- Update the ParameterGroup (set dynamic constants/textures/etc)
				- Cache the ParameterGroup
				- Bind the CachedParameterGroup created in the previous step
				- Set the PipelineState and other dynamic state
				- Execute draw call


*/

namespace aqua
{
	class Allocator;
	class LinearAllocator;
	class DynamicLinearAllocator;

	struct MappedSubresource
	{
		void* data;
		u32   row_pitch;
		u32   depth_pitch;
	};

	//Implementation details are kept private
	//because CachedParameterGroups can't be modified after they're created

	class CachedParameterGroup;

	class RenderDevice
	{
	public:

		enum class ParameterGroupType : u8
		{
			FRAME,
			VIEW,
			PASS,
			MATERIAL,
			INSTANCE
		};

		RenderDevice();
		~RenderDevice();

		int init(u32 width, u32 height, WindowH wnd, bool windowed,
				 bool back_buffer_render_target, bool back_buffer_uav,
				 Allocator& allocator, LinearAllocator& temp_allocator);

		int shutdown();

		//--------------------------------------------------------------------------------------------------------

#ifdef _WIN32
		ID3D11Device* getLowLevelDevice();
#endif

		//--------------------------------------------------------------------------------------------------------

		bool createBuffer(const BufferDesc& desc, const void* initial_data,
						  BufferH& out_buffer, ShaderResourceH* out_srv, UnorderedAccessH* out_uav);

		bool createConstantBuffer(const ConstantBufferDesc& desc, const void* initial_data, BufferH& out_buffer);

		bool createTexture1D(const Texture1DDesc& desc, const void** initial_data,
							 u8 num_srvs, const TextureViewDesc* srvs_descs,
							 u8 num_rtvs, const TextureViewDesc* rtvs_descs,
							 u8 num_uavs, const TextureViewDesc* uavs_descs,
							 ShaderResourceH* out_srs, RenderTargetH* out_rts, UnorderedAccessH* out_uavs);

		bool createTexture2D(const Texture2DDesc& desc, const SubTextureData* initial_data,
							 u8 num_srvs, const TextureViewDesc* srvs_descs,
							 u8 num_rtvs, const TextureViewDesc* rtvs_descs,
							 u8 num_dsvs, const TextureViewDesc* dsvs_descs,
							 u8 num_uavs, const TextureViewDesc* uavs_descs,
							 ShaderResourceH* out_srs, RenderTargetH* out_rts,
							 DepthStencilTargetH* out_dsvs, UnorderedAccessH* out_uavs);

		int createTextureCube(const TextureCubeDesc& desc, ShaderResourceH* out_sr,
							  RenderTargetH& out_rt, UnorderedAccessH* out_uav);

		int createTexture(const TextureDesc& desc, u8 num_srvs, const int* srvs_descs,
						  u8 num_rtvs, const int* rtvs_descs, u8 num_uavs, const int* uavs_descs,
						  ShaderResourceH* out_srs, RenderTargetH* out_rts, UnorderedAccessH* out_uavs);

		int createRenderTarget(const RenderTargetDesc& desc,
							   RenderTargetH& out_render_target, ShaderResourceH& out_shader_resource);

		int createDepthStencilTarget(const DepthStencilTargetDesc& desc,
									 DepthStencilTargetH& out_depth_stencil_target, ShaderResourceH* out_shader_resource);

		int createBlendState(const BlendStateDesc& desc, BlendStateH& out_blend_state);
		int createRasterizerState(const RasterizerStateDesc& desc, RasterizerStateH& out_rasterizer_state);
		int createDepthStencilState(const DepthStencilStateDesc& desc, DepthStencilStateH& out_depth_stencil_state);
		int createSamplerState(const SamplerStateDesc& desc, SamplerStateH& out_sampler_state);

		int createInputLayout(const InputElementDesc* input_elements_descs, u32 num_elements,
							  const void* shader_bytecode, u32 bytecode_length, InputLayoutH& out_input_layout);

		int createVertexShader(const void* shader_bytecode, size_t bytecode_length, VertexShaderH& out_vertex_shader);
		int createHullShader(const void* shader_bytecode, size_t bytecode_length, HullShaderH& out_hull_shader);
		int createDomainShader(const void* shader_bytecode, size_t bytecode_length, DomainShaderH& out_domain_shader);
		int createGeometryShader(const void* shader_bytecode, size_t bytecode_length, GeometryShaderH& out_geometry_shader);
		int createPixelShader(const void* shader_bytecode, size_t bytecode_length, PixelShaderH& out_pixel_shader);
		int createComputeShader(const void* shader_bytecode, size_t bytecode_length, ComputeShaderH& out_compute_shader);

		int createQuery(QueryType type, QueryH& out_query);

		/////////////////////////////////////////////////////////////////////////////

		ParameterGroup* createParameterGroup(Allocator& allocator, ParameterGroupType type, const ParameterGroupDesc& desc,
											 u32 dynamic_cbuffers_mask, u32 dynamic_srvs_mask, const u32* srvs_sizes);

		void deleteParameterGroup(Allocator& allocator, ParameterGroup& param_group);

		CachedParameterGroup* cacheParameterGroup(const ParameterGroup& param_group);
		const CachedParameterGroup* cacheTemporaryParameterGroup(const ParameterGroup& param_group);

		void deleteCachedParameterGroup(CachedParameterGroup& param_group);

		//---------------------------------------------------------------------------

		const PipelineState* createPipelineState(const PipelineStateDesc& desc);
		const ComputePipelineState* createComputePipelineState();

		//---------------------------------------------------------------------------

		void setPipelineState(const PipelineState* pipeline_state);
		//void bindParameterGroup(const void* parameter_group, const void* binding_info);
		void bindParameterGroup(const CachedParameterGroup& parameter_group, const void* binding_info);

		void setComputePipelineState(const ComputePipelineState* pipeline_state);
		void bindComputeParameterGroup(const void* parameter_group, const void* binding_info);

		void bindSamplerGroup(const void* sampler_group, const void* binding_info);

		/////////////////////////////////////////////////////////////////////////////

		void bindVertexBuffer(BufferH vertex_buffer, u32 stride, u32 offset, u8 slot);
		void bindIndexBuffer(BufferH buffer, IndexBufferFormat format, u32 offset);
		void bindInputLayout(InputLayoutH input_layout);
		void bindPrimitiveTopology(PrimitiveTopology primitive_topology);

		void bindRenderTarget(RenderTargetH render_target, u8 slot);
		void bindUnorderedAccessTarget(UnorderedAccessH uav, u8 slot);
		void bindDepthStencilTarget(DepthStencilTargetH depth_stencil_target);

		void bindViewport(const ViewportH& viewport);

		//--------------------------------------------------------------------------

		void draw(u32 vertex_count, u32 start_vertex);
		void drawInstanced(u32 vertex_count, u32 num_instances, u32 start_vertex, u32 start_instance);
		void drawIndexed(u32 index_count, u32 start_index, u32 base_vertex);
		void drawIndexedInstanced(u32 index_count, u32 num_instances, u32 start_index, u32 base_vertex, u32 start_instance);

		void dispatch(u32 count_x, u32 count_y, u32 count_z);

		//--------------------------------------------------------------------------

		void drawInstancedIndirect(BufferH args, u32 offset);
		void drawIndexedInstancedIndirect(BufferH args, u32 offset);

		void dispatchIndirect(BufferH args, u32 offset);

		//--------------------------------------------------------------------------

		void copyStructureCount(BufferH dest_buffer, u32 offset, UnorderedAccessH src_uav);

		void generateMips(ShaderResourceH srv);

		void applyStateChanges();
		void applySamplerChanges();
		void applyComputeChanges();

		MappedSubresource map(ResourceH resource, u32 subresource, MapType map_type);
		void              unmap(ResourceH resource, u32 subresource);

		void clearRenderTarget(RenderTargetH render_target, const float clear_color[4]);
		void clearDepthStencilTarget(DepthStencilTargetH depth_stencil_target, float depth);

		void present();

		void clearFrameAllocator();

		void unbindResources();
		void unbindComputeResources();

		void begin(const QueryH& query);
		void end(const QueryH& query);

		bool checkStatus(const QueryH& query);
		void getData(const QueryH& query, void* data, u32 data_size);

		RenderTargetH    getBackBufferRT() const;
		UnorderedAccessH getBackBufferUAV() const;

		static void release(const DeviceChildH& device_child)
		{
			if(device_child != nullptr)
				device_child->Release();
		}

#if _DEBUG
		static void setDebugName(const DeviceChildH& device_child, const char* name)
		{
			device_child->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
		}
#endif

		//-------------------------------------------------------------------------------------
		//-------------------------------------------------------------------------------------
		//-------------------------------------------------------------------------------------

		BufferH getTemporaryCBuffer(ParameterGroupType type, u8 index, u32 size)
		{
			u32 cbuffer_id = size;
			cbuffer_id |= index << 22;
			cbuffer_id |= (u8)type << 29;

			auto temp = _temp_cbuffers->lookup(cbuffer_id, nullptr);

			if(temp != nullptr)
				return temp;

			ConstantBufferDesc desc;
			desc.size        = size;
			desc.update_mode = UpdateMode::CPU;

			if(!createConstantBuffer(desc, nullptr, temp))
				return nullptr;

			_temp_cbuffers->insert(cbuffer_id, temp);

			return temp;
		}

		//----------------------------------------------------------------------
		//----------------------------------------------------------------------
		//----------------------------------------------------------------------

	private:

		Allocator*              _main_allocator;
		LinearAllocator*        _temp_allocator;
		DynamicLinearAllocator* _frame_allocator;

#ifdef _WIN32

		struct ShaderStage;

		void bindConstantBuffer(ShaderStage& stage, BufferH cbuffer, u8 slot);
		void bindShaderResource(ShaderStage& stage, ShaderResourceH resource, u8 slot);
		void bindSamplerState(ShaderStage& stage, SamplerStateH sampler, u8 slot);
		void bindUAV(ShaderStage& stage, UnorderedAccessH uav, u8 slot);

		void VSBindShader(VertexShaderH vertex_shader);
		void VSBindConstantBuffer(BufferH constant_buffer, u8 slot);
		void VSBindShaderResource(ShaderResourceH shader_resource, u8 slot);
		void VSBindSampler(SamplerStateH sampler, u8 slot);

		void HSBindShader(HullShaderH hull_shader);
		void HSBindConstantBuffer(BufferH constant_buffer, u8 slot);
		void HSBindShaderResource(ShaderResourceH shader_resource, u8 slot);
		void HSBindSampler(SamplerStateH sampler, u8 slot);

		void DSBindShader(DomainShaderH domain_shader);
		void DSBindConstantBuffer(BufferH constant_buffer, u8 slot);
		void DSBindShaderResource(ShaderResourceH shader_resource, u8 slot);
		void DSBindSampler(SamplerStateH sampler, u8 slot);

		void GSBindShader(GeometryShaderH geometry_shader);
		void GSBindConstantBuffer(BufferH constant_buffer, u8 slot);
		void GSBindShaderResource(ShaderResourceH shader_resource, u8 slot);
		void GSBindSampler(SamplerStateH sampler, u8 slot);

		void PSBindShader(PixelShaderH pixel_shader);
		void PSBindConstantBuffer(BufferH constant_buffer, u8 slot);
		void PSBindShaderResource(ShaderResourceH shader_resource, u8 slot);
		void PSBindSampler(SamplerStateH sampler, u8 slot);

		void CSBindShader(ComputeShaderH compute_shader);
		void CSBindConstantBuffer(BufferH constant_buffer, u8 slot);
		void CSBindShaderResource(ShaderResourceH shader_resource, u8 slot);
		void CSBindSampler(SamplerStateH sampler, u8 slot);
		void CSBindUAV(UnorderedAccessH sampler, u8 slot);

		void bindBlendState(BlendStateH state);
		void bindRasterizerState(RasterizerStateH state);
		void bindDepthStencilState(DepthStencilStateH state);

		ID3D11Device*                _device;
		ID3D11DeviceContext*         _immediate_context;
		IDXGISwapChain*              _swap_chain;
		ID3D11Debug*				 _debugger;
		ID3D11RenderTargetView*      _back_buffer_rt;
		ID3D11UnorderedAccessView*   _back_buffer_uav;

		BlendStateH        _blend_states[8];
		RasterizerStateH   _rasterizer_states[8];
		DepthStencilStateH _depth_stencil_states[8];

#elif (AQUA_D3D12_RHI)

#elif (METAL)

#endif

		//Device State

		static const u32 NUM_VERTEX_BUFFERS = 4;
		static const u32 NUM_TEXTURE_SLOTS = 16;
		static const u32 NUM_CBUFFER_SLOTS = 8;
		static const u32 NUM_SAMPLER_SLOTS = 16;
		static const u32 NUM_RENDER_TARGET_SLOTS = 8;
		static const u32 NUM_UAV_SLOTS = 8;

		struct ShaderStage
		{
			ShaderStage() : check_textures(false),
				check_cbuffers(false),
				check_samplers(false)
			{
				for(u32 i = 0; i < NUM_TEXTURE_SLOTS; i++)
					textures[i] = nullptr;

				for(u32 i = 0; i < NUM_CBUFFER_SLOTS; i++)
					cbuffers[i] = nullptr;

				for(u32 i = 0; i < NUM_SAMPLER_SLOTS; i++)
					samplers[i] = nullptr;
			}

			ShaderResourceH textures[NUM_TEXTURE_SLOTS];
			BufferH         cbuffers[NUM_CBUFFER_SLOTS];
			SamplerStateH   samplers[NUM_SAMPLER_SLOTS];

			bool check_textures;
			bool check_cbuffers;
			bool check_samplers;
		};

		BufferH             _vertex_buffers[NUM_VERTEX_BUFFERS];
		u32                 _vertex_buffers_strides[NUM_VERTEX_BUFFERS];
		u32                 _vertex_buffers_offsets[NUM_VERTEX_BUFFERS];
		BufferH             _last_vertex_buffers[NUM_VERTEX_BUFFERS];
		u32                 _last_vertex_buffers_strides[NUM_VERTEX_BUFFERS];
		u32                 _last_vertex_buffers_offsets[NUM_VERTEX_BUFFERS];
		bool                _check_vertex_buffers;

		BufferH             _index_buffer;
		IndexBufferFormat   _index_format;
		u32                 _index_offset;

		InputLayoutH        _input_layout;

		PrimitiveTopology   _primitive_topology;

		RasterizerStateH    _rasterizer_state;
		ViewportH           _viewport;

		BlendStateH         _blend_state;
		DepthStencilStateH  _depth_stencil_state;

		RenderTargetH       _render_targets[NUM_RENDER_TARGET_SLOTS];
		UnorderedAccessH    _uavs[NUM_UAV_SLOTS];
		DepthStencilTargetH _depth_stencil_target;
		bool                _update_output_merger;

		VertexShaderH       _vertex_shader;
		ShaderStage         _vertex_shader_stage;
		ShaderStage         _last_vertex_shader_stage;

		HullShaderH         _hull_shader;
		ShaderStage         _hull_shader_stage;
		ShaderStage         _last_hull_shader_stage;

		DomainShaderH       _domain_shader;
		ShaderStage         _domain_shader_stage;
		ShaderStage         _last_domain_shader_stage;

		GeometryShaderH     _geometry_shader;
		ShaderStage         _geometry_shader_stage;
		ShaderStage         _last_geometry_shader_stage;

		PixelShaderH        _pixel_shader;
		ShaderStage         _pixel_shader_stage;
		ShaderStage         _last_pixel_shader_stage;

		ComputeShaderH      _compute_shader;
		ShaderStage         _compute_shader_stage;
		ShaderStage         _last_compute_shader_stage;
		UnorderedAccessH    _compute_uavs[NUM_UAV_SLOTS];
		UnorderedAccessH    _last_compute_uavs[NUM_UAV_SLOTS];
		bool                _compute_check_uavs;

		HashMap<u32, BufferH>* _temp_cbuffers;

		struct TempSRVBuffer
		{
			BufferH         buffer;
			ShaderResourceH srv;
		};

		HashMap<u32, TempSRVBuffer>* _temp_srv_buffer;
	};

#if _WIN32
#include "RenderDeviceD3D11.inl"
#endif
};