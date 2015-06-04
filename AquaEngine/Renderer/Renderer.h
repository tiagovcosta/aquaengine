#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

//#include "..\DevTools\Profiler.h"

//#include "RendererInterfaces.h"
#include "ShaderManager.h"

#include "RenderDevice\RenderDevice.h"

#include "..\Core\Containers\HashMap.h"

#include "..\AquaMath.h"
#include "..\AquaTypes.h"

//TODO: ---------------------------------------------------------------
//Initialize members to nullptr to prevent crashes in shutdown function
//---------------------------------------------------------------------

struct lua_State;

namespace aqua
{
	struct RenderQueue;
	struct RenderItem;
	struct SortItem;
	struct Viewport;
	struct VisibilityData;

	class ResourceGenerator;
	class RenderQueueGenerator;

	class Camera;

	class Allocator;
	class LinearAllocator;
	class ProxyAllocator;
	class DynamicLinearAllocator;

	class Profiler;

	struct RenderTexture
	{
		RenderTargetH    render_target;
		UnorderedAccessH uav;
		u32			     width;
		u32			     height;
		u32			     depth;
	};

	struct RenderTargetView
	{
		RenderTargetH view;
		u32           width;
		u32           height;
		//u32           depth;
	};

	struct DepthStencilView
	{
		DepthStencilTargetH view;
		u32                 width;
		u32                 height;
		//u32                 depth;
	};
	
	struct RenderTextureX
	{
		RenderTargetH*       rtvs;
		DepthStencilTargetH* dsvs;
		ShaderResourceH*     srvs;
		UnorderedAccessH*    uavs;
		u32			         width;
		u32			         height;
		u32			         depth;
		u8                   num_rtvs;
		u8                   num_dsvs;
		u8                   num_srvs;
		u8                   num_uavs;
	};

	class TempBufferH
	{
		u32 handle; //do not modify

		friend class Renderer;
	};

	class Renderer
	{
	public:
		Renderer();
		~Renderer();

		bool init(u32 window_width, u32 window_height, WindowH wnd, bool windowed, lua_State* lua_state,
			      Allocator& main_allocator, LinearAllocator& temp_allocator);

		int shutdown();

		//void update(float dt);

		bool buildRenderQueues(u8 num_passes, const u32* passes_names, 
							   const VisibilityData* visibility_data, RenderQueue* out_queues);

		bool render(const Camera& camera, u32 generator_name, const void* generator_args);

		void setViewport(const Viewport& viewport, u32 render_target_width, u32 render_target_height);

		void setRenderTarget(u8 num_render_targets, const RenderTexture* render_targets,
							 DepthStencilTargetH depth_stencil_target);

		bool render(u8 pass_index, const RenderQueue& queue);

		bool render(const RenderItem& item);

		bool compute(u32 count_x, u32 count_y, u32 count_z, 
					 const ComputeKernelPermutation& kernel, const ParameterGroup& params);

		bool compute(BufferH args_buffer, u32 args_offset,
					 const ComputeKernelPermutation& kernel, const ParameterGroup& params);

		void addResourceGenerator(u32 name, ResourceGenerator* generator);
		//ResourceGenerator* getResourceGenetator(u32 name);

		void addRenderQueueGenerator(u32 name, RenderQueueGenerator* generator);

		bool generateResource(u32 resource_generator_name, const void* args_blob, const VisibilityData* visibility);

		void present();

		void setFrameParameter(u32 name, const void* data, u32 size);
		void setViewParameter(u32 name, const void* data, u32 size);

		void bindFrameParameters();
		void bindViewParameters();

		void setPassParameter(u8 pass_index, u32 name, const void* data, u32 size);

		u64 encodeSortKey(u32 context) const;

		TempBufferH createTemporaryBuffer(u32 size);
		void*       allocTemporaryData(const TempBufferH& buffer_handle, u32 size);

		RenderDevice*        getRenderDevice();
		const ShaderManager* getShaderManager() const;

		RenderTexture getBackBuffer() const;

		Profiler* getProfiler() const;
		/*
		enum class TempCBufferType : u8
		{
			MATERIAL,
			INSTANCE
		};

		BufferH getTemporaryCBuffer(TempCBufferType type, u8 index, u32 size)
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

			if(!_render_device.createConstantBuffer(desc, nullptr, temp))
				return nullptr;

			_temp_cbuffers->insert(cbuffer_id, temp);

			return temp;
		}
		*/
	private:

		struct TemporaryBuffer
		{
			static const u8 MAX_NUM_RETIRED_FRAMES = 3;

			BufferH buffer;
			u32     retired_frames[MAX_NUM_RETIRED_FRAMES];
			u32     size;
			u32     start;
			u32     end;
		};

		bool initLua(lua_State* lua_state);

		bool setupSamplers();

		bool createCommonParameterGroups();
		bool destroyCommonParameterGroups();

		static const u8 MAX_NUM_RESOURCE_GENERATORS     = 16;
		static const u8 MAX_NUM_RENDER_QUEUE_GENERATORS = 16;

		RenderDevice        _render_device;

		ShaderManager       _shader_manager;

		Allocator*          _main_allocator;
		LinearAllocator*    _temp_allocator;
		ProxyAllocator*     _shader_manager_allocator;

		u32                 _back_buffer_width;
		u32                 _back_buffer_height;

		ParameterGroup*     _frame_params;
		ParameterGroup*     _view_params;

		const CachedParameterGroup* _cached_frame_params;
		const CachedParameterGroup* _cached_view_params;

		ParameterGroup**    _passes_params;
		u8                  _num_passes;

		//Array of registered C++ resource generators
		u32                 _num_resource_generators;
		u32                 _resource_generators_names[MAX_NUM_RESOURCE_GENERATORS];
		ResourceGenerator*  _resource_generators[MAX_NUM_RESOURCE_GENERATORS];

		//Array of registered C++ render queue generators
		u32                    _num_render_queue_generators;
		u32					   _render_queue_generators_names[MAX_NUM_RENDER_QUEUE_GENERATORS];
		RenderQueueGenerator*  _render_queue_generators[MAX_NUM_RENDER_QUEUE_GENERATORS];

		Profiler* _profiler;

		struct CBufferID
		{
			u32 size  : 22;
			u32 index : 7;
			u32 type  : 3;
		};

		HashMap<u32, BufferH>* _temp_cbuffers;

		bool _ready;

		DynamicLinearAllocator* _current_frame_allocator;
	};
};