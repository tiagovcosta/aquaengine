#include "Renderer.h"

#include "..\DevTools\Profiler.h"

#include "RendererStructs.h"
#include "RendererInterfaces.h"

#include "Camera.h"

#include "..\Utilities\File.h"

#include "..\Core\Allocators\ProxyAllocator.h"
#include "..\Core\Allocators\DynamicLinearAllocator.h"
#include "..\Core\Allocators\LinearAllocator.h"
#include "..\Core\Allocators\Allocator.h"

#include "..\Utilities\ScriptUtilities.h"
#include "..\Utilities\StringID.h"
#include "..\Utilities\Logger.h"

#include <lua.hpp>

#include <bitset>
#include <algorithm>

using namespace aqua;

Renderer::Renderer()// : _profiler(nullptr)
{

}

Renderer::~Renderer()
{

}

bool Renderer::init(u32 window_width, u32 window_height, WindowH wnd, bool windowed, lua_State* lua_state,
	                Allocator& main_allocator, LinearAllocator& temp_allocator)
{
	_main_allocator = &main_allocator;
	_temp_allocator = &temp_allocator;

	_temp_cbuffers = allocator::allocateNew<HashMap<u32, BufferH>>(*_main_allocator, *_main_allocator);

	void* temp_mark = _temp_allocator->getMark();

	ASSERT(lua_state != nullptr);

	initLua(lua_state);

	double init_lua_memory = script_utilities::getMemoryUsage(lua_state);

	lua_newtable(lua_state);

	lua_getglobal(lua_state, "Renderer");
	lua_setfield(lua_state, -2, "Renderer");

	lua_pushinteger(lua_state, window_width);
	lua_setfield(lua_state, -2, "window_width");
	lua_pushinteger(lua_state, window_height);
	lua_setfield(lua_state, -2, "window_height");

	if(!script_utilities::loadScript(lua_state, "data/render_config.lua"))
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Error loading Renderer config!");
		return false;
	}

	script_utilities::getScriptTable(lua_state, "data/render_config.lua");

	lua_getfield(lua_state, -1, "render_settings");
	
	lua_getfield(lua_state, -1, "back_buffer_width");
	lua_getfield(lua_state, -2, "back_buffer_height");
	lua_getfield(lua_state, -3, "back_buffer_render_target");
	lua_getfield(lua_state, -4, "back_buffer_uav");

	_back_buffer_width             = (u32)lua_tointeger(lua_state, -4);
	_back_buffer_height            = (u32)lua_tointeger(lua_state, -3);
	bool back_buffer_render_target = lua_toboolean(lua_state, -2) != 0;
	bool back_buffer_uav           = lua_toboolean(lua_state, -1) != 0;

	lua_pop(lua_state, 5); //pop render_settings

	if(!_render_device.init(_back_buffer_width, _back_buffer_height, wnd, windowed,
							back_buffer_render_target, back_buffer_uav, 
							*_main_allocator, *_temp_allocator))
		return false;

	///////////////////////////////////////////////////////////////////////////////
	//Load render shaders
	///////////////////////////////////////////////////////////////////////////////

	lua_getfield(lua_state, -1, "render_shaders");

	u8 num_render_shaders            = (u8)lua_objlen(lua_state, -1);
	ShaderFile* render_shaders_files = nullptr;

	u8 num_loaded_render_shaders = 0;

	if(num_render_shaders > 0)
	{
		render_shaders_files = allocator::allocateArray<ShaderFile>(*_temp_allocator, num_render_shaders);

		ASSERT(render_shaders_files != nullptr);

		for(u8 i = 0; i < num_render_shaders; i++)
		{
			lua_rawgeti(lua_state, -1, i + 1);
			const char* render_shader_filename = lua_tostring(lua_state, -1);

			size_t render_shader_size = file::getFileSize(render_shader_filename);

			if(render_shader_size == 0)
			{
				Logger::get().write(MESSAGE_LEVEL::WARNING_MESSAGE, CHANNEL::RENDERER, "Render shader not found: %s", render_shader_filename);
				continue;
			}

			char* shader_pack = (char*)_temp_allocator->allocate(render_shader_size);

			ASSERT(shader_pack != nullptr);

			file::readFile(render_shader_filename, false, shader_pack);

			render_shaders_files[num_loaded_render_shaders].name = getStringID(render_shader_filename);
			render_shaders_files[num_loaded_render_shaders].data = shader_pack;
			render_shaders_files[num_loaded_render_shaders].size = render_shader_size;

			num_loaded_render_shaders++;

			lua_pop(lua_state, 1); //pop render shader filename
		}
	}

	lua_pop(lua_state, 1); //pop shader packs array

	///////////////////////////////////////////////////////////////////////////////
	//Load compute shaders
	///////////////////////////////////////////////////////////////////////////////

	lua_getfield(lua_state, -1, "compute_shaders");

	u8 num_compute_shaders            = (u8)lua_objlen(lua_state, -1);
	ShaderFile* compute_shaders_files = nullptr;

	u8 num_loaded_compute_shaders = 0;

	if(num_compute_shaders > 0)
	{
		compute_shaders_files = allocator::allocateArray<ShaderFile>(*_temp_allocator, num_compute_shaders);

		ASSERT(compute_shaders_files != nullptr);

		for(u8 i = 0; i < num_compute_shaders; i++)
		{
			lua_rawgeti(lua_state, -1, i + 1);
			const char* compute_shader_filename = lua_tostring(lua_state, -1);

			size_t compute_shader_size = file::getFileSize(compute_shader_filename);

			if(compute_shader_size == 0)
			{
				Logger::get().write(MESSAGE_LEVEL::WARNING_MESSAGE, CHANNEL::RENDERER, "Compute shader not found: %s", compute_shader_filename);
				continue;
			}

			char* compute_shader = (char*)_temp_allocator->allocate(compute_shader_size);

			ASSERT(compute_shader != nullptr);

			file::readFile(compute_shader_filename, false, compute_shader);

			compute_shaders_files[num_loaded_compute_shaders].name = getStringID(compute_shader_filename);
			compute_shaders_files[num_loaded_compute_shaders].data = compute_shader;
			compute_shaders_files[num_loaded_compute_shaders].size = compute_shader_size;

			num_loaded_compute_shaders++;

			lua_pop(lua_state, 1); //pop compute shader filename
		}
	}

	lua_pop(lua_state, 1); //pop shader packs array

	//Load shader common

	ShaderFile shader_common;
	shader_common.size = file::getFileSize("data/shaders/common.cshader");
	shader_common.data = (char*)_temp_allocator->allocate(shader_common.size);

	file::readFile("data/shaders/common.cshader", false, shader_common.data);

	_shader_manager_allocator = allocator::allocateNew<ProxyAllocator>(*_main_allocator, *_main_allocator);

	if(!_shader_manager.init(shader_common, num_loaded_render_shaders, render_shaders_files, 
							 num_loaded_compute_shaders, compute_shaders_files,
							 _render_device, *_shader_manager_allocator, *_temp_allocator))
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Error initializing Shader Manager!");
		return false;
	}

	_temp_allocator->rewind(temp_mark); //Free temporary shader memory

	lua_pop(lua_state, 1); // pop render_config table
	script_utilities::unloadScript(lua_state, "data/render_config.lua");

	#if _DEBUG
		if(script_utilities::getMemoryUsage(lua_state) - init_lua_memory > 0)
			Logger::get().write(MESSAGE_LEVEL::WARNING_MESSAGE, CHANNEL::RENDERER, "Lua memory leak in Renderer");
	#endif

	_temp_allocator->rewind(temp_mark);

	setupSamplers();

	if(!createCommonParameterGroups())
		return false;

	_profiler = allocator::allocateNew<Profiler>(*_main_allocator, _render_device);

	_current_frame_allocator = allocator::allocateNew<DynamicLinearAllocator>(*_main_allocator, *_main_allocator,
																			  4096, 16);

	_num_resource_generators     = 0;
	_num_render_queue_generators = 0;

	return true;
}

int Renderer::shutdown()
{
	allocator::deallocateDelete(*_main_allocator, _current_frame_allocator);
	
	if(_profiler != nullptr)
		allocator::deallocateDelete(*_main_allocator, _profiler);

	destroyCommonParameterGroups();

	/*
	for(u32 i = 0; i < _num_render_targets; i++)
	{
		_render_device.release(_render_targets[i].render_target);
		_render_device.release(_render_targets[i].shader_resource);
	}

	for(u32 i = 0; i < _num_depth_stencil_targets; i++)
	{
		_render_device.release(_depth_stencil_targets[i].depth_stencil_target);
		_render_device.release(_depth_stencil_targets[i].shader_resource);
	}

	if(_depth_stencil_targets != nullptr)
		allocator::deallocateArray(*_main_allocator, _depth_stencil_targets);

	if(_render_targets != nullptr)
		allocator::deallocateArray(*_main_allocator, _render_targets);
	*/
	_shader_manager.shutdown();

	if(_shader_manager_allocator != nullptr)
		allocator::deallocateDelete(*_main_allocator, _shader_manager_allocator);

	if(_temp_cbuffers != nullptr)
		allocator::deallocateDelete(*_main_allocator, _temp_cbuffers);

	_render_device.shutdown();

	return 1;
}
/*
void Renderer::update(float dt)
{
}
*/
void Renderer::setViewport(const Viewport& viewport, u32 render_target_width, u32 render_target_height)
{
	ViewportH vp;
	vp.TopLeftX = viewport.x * render_target_width;
	vp.TopLeftY = viewport.y * render_target_height;
	vp.Width    = viewport.width * render_target_width;
	vp.Height   = viewport.height * render_target_height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	_render_device.bindViewport(vp);
}

void Renderer::setRenderTarget(u8 num_render_targets, const RenderTexture* render_targets,
							   DepthStencilTargetH depth_stencil_target)
{
	ASSERT((num_render_targets > 0 && render_targets != nullptr) || depth_stencil_target != nullptr);

	for(u8 i = 0; i < num_render_targets; i++)
	{
		_render_device.bindRenderTarget(render_targets[i].render_target, i);
	}

	_render_device.bindDepthStencilTarget(depth_stencil_target);
}

/*
using DrawCallFunc = void(*)(RenderDevice&, const RenderItem&);

static const DrawCallFunc draw_calls_funcs[] =
{
	//Draw
	[](RenderDevice& render_device, const RenderItem& ri)
	{
		render_device.draw(ri.draw_call->basic.vertex_count, ri.draw_call->basic.start_vertex);
	},

	//DrawInstanced
	[](RenderDevice& render_device, const RenderItem& ri)
	{
		render_device.drawInstanced(ri.draw_call->basic.vertex_count, ri.num_instances,
									ri.draw_call->basic.start_vertex, 0);
	},

	//DrawInstancedIndirect
	[](RenderDevice& render_device, const RenderItem& ri)
	{
		render_device.drawInstancedIndirect(ri.draw_call->indirect.args_buffer, ri.draw_call->indirect.args_offset);
	},

	//DrawIndexed
	[](RenderDevice& render_device, const RenderItem& ri)
	{
		render_device.drawIndexed(ri.draw_call->indexed.index_count, ri.draw_call->indexed.start_index, 
								  ri.draw_call->indexed.base_vertex);
	},

	//DrawIndexedInstanced
	[](RenderDevice& render_device, const RenderItem& ri)
	{
		render_device.drawIndexedInstanced(ri.draw_call->indexed.index_count, ri.num_instances, 
										   ri.draw_call->indexed.index_count,ri.draw_call->indexed.base_vertex, 0);
	},

	//DrawIndexedInstancedIndirect
	[](RenderDevice& render_device, const RenderItem& ri)
	{
		render_device.drawIndexedInstancedIndirect(ri.draw_call->indirect.args_buffer, ri.draw_call->indirect.args_offset);
	}
};
*/

bool Renderer::render(const RenderItem& item)
{
	_render_device.setPipelineState(item.shader->pipeline_state);

	const u8* vertex_buffers_bind_info = item.shader->vertex_buffers_bind_info;

	if(item.shader->vertex_buffers_bind_info != nullptr)
	{
		while(*vertex_buffers_bind_info != UINT8_MAX)
		{
			VertexBuffer vb = item.mesh->getVertexBuffer(*vertex_buffers_bind_info);

			_render_device.bindVertexBuffer(vb.buffer, vb.stride, vb.offset, *vertex_buffers_bind_info);

			vertex_buffers_bind_info++;
		}
	}

	_render_device.bindPrimitiveTopology(item.mesh->topology);

	/*
	if(item.shader->material_params_bind_info != nullptr)
		ASSERT(item.material_params != nullptr)
	*/

	if(item.material_params != nullptr)
		_render_device.bindParameterGroup(*item.material_params, item.shader->material_params_bind_info);

	if(item.instance_params != nullptr)
		_render_device.bindParameterGroup(*item.instance_params, item.shader->instance_params_bind_info);

	if(!item.draw_call->is_indirect)
	{
		if(item.draw_call->is_indexed)
		{
			_render_device.bindIndexBuffer(item.mesh->index_buffer, item.mesh->index_format,
										   item.mesh->index_buffer_offset);

			if(item.num_instances <= 1)
			{
				_render_device.drawIndexed(item.draw_call->direct.count,
										   item.draw_call->direct.start_index, item.draw_call->direct.start_vertex);
			}
			else
			{
				_render_device.drawIndexedInstanced(item.draw_call->direct.count, item.num_instances,
													item.draw_call->direct.start_index,
													item.draw_call->direct.start_vertex, 0);
			}
		}
		else
		{
			if(item.num_instances <= 1)
			{
				_render_device.draw(item.draw_call->direct.count, item.draw_call->direct.start_vertex);
			}
			else
			{
				_render_device.drawInstanced(item.draw_call->direct.count, item.num_instances,
											 item.draw_call->direct.start_vertex, 0);
			}
		}
	}
	else
	{
		if(item.draw_call->is_indexed)
		{
			_render_device.bindIndexBuffer(item.mesh->index_buffer, item.mesh->index_format,
										   item.mesh->index_buffer_offset);

			_render_device.drawIndexedInstancedIndirect(item.draw_call->indirect.args_buffer,
														item.draw_call->indirect.args_offset);
		}
		else
			_render_device.drawInstancedIndirect(item.draw_call->indirect.args_buffer,
												 item.draw_call->indirect.args_offset);
	}

	return true;
}

bool Renderer::render(u8 pass_index, const RenderQueue& queue)
{
	const CachedParameterGroup* pass_params;

	if(pass_index != UINT8_MAX)
	{
		pass_params = _render_device.cacheTemporaryParameterGroup(*_passes_params[pass_index]);

		_render_device.bindParameterGroup(*pass_params, _shader_manager.getPassParametersBindInfo(pass_index));
	}

	for(u32 i = 0; i < queue.size; i++)
	{

		if(pass_index != UINT8_MAX)
		{
			_render_device.bindParameterGroup(*pass_params, _shader_manager.getPassParametersBindInfo(pass_index));
		}

		const RenderItem& item = *queue.sort_items[i].item;

		_render_device.setPipelineState(item.shader->pipeline_state);

		const u8* vertex_buffers_bind_info = item.shader->vertex_buffers_bind_info;

		if(item.shader->vertex_buffers_bind_info != nullptr)
		{
			while(*vertex_buffers_bind_info != UINT8_MAX)
			{
				VertexBuffer vb = item.mesh->getVertexBuffer(*vertex_buffers_bind_info);

				_render_device.bindVertexBuffer(vb.buffer, vb.stride, vb.offset, *vertex_buffers_bind_info);

				vertex_buffers_bind_info++;
			}
		}

		_render_device.bindPrimitiveTopology(item.mesh->topology);

		if(item.material_params != nullptr)
			_render_device.bindParameterGroup(*item.material_params, item.shader->material_params_bind_info);

		if(item.instance_params != nullptr)
			_render_device.bindParameterGroup(*item.instance_params, item.shader->instance_params_bind_info);

		if(!item.draw_call->is_indirect)
		{
			if(item.draw_call->is_indexed)
			{
				_render_device.bindIndexBuffer(item.mesh->index_buffer, item.mesh->index_format,
											   item.mesh->index_buffer_offset);

				if(item.num_instances <= 1)
				{
					_render_device.drawIndexed(item.draw_call->direct.count,
						item.draw_call->direct.start_index, item.draw_call->direct.start_vertex);
				}
				else
				{
					_render_device.drawIndexedInstanced(item.draw_call->direct.count, item.num_instances,
						item.draw_call->direct.start_index,
						item.draw_call->direct.start_vertex, 0);
				}
			}
			else
			{
				if(item.num_instances <= 1)
				{
					_render_device.draw(item.draw_call->direct.count, item.draw_call->direct.start_vertex);
				}
				else
				{
					_render_device.drawInstanced(item.draw_call->direct.count, item.num_instances,
						item.draw_call->direct.start_vertex, 0);
				}
			}
		}
		else
		{
			if(item.draw_call->is_indexed)
			{
				_render_device.bindIndexBuffer(item.mesh->index_buffer, item.mesh->index_format,
											   item.mesh->index_buffer_offset);

				_render_device.drawIndexedInstancedIndirect(item.draw_call->indirect.args_buffer,
															item.draw_call->indirect.args_offset);
			}
			else
				_render_device.drawInstancedIndirect(item.draw_call->indirect.args_buffer,
													 item.draw_call->indirect.args_offset);
		}
	}

	return true;
}

bool Renderer::render(const Camera& camera, u32 generator_name, const void* generator_args)
{
	//while(!_ready);
	//job_manager.wait(render_job);

	_current_frame_allocator->clear();

	//cache frame params
	_cached_frame_params = _render_device.cacheTemporaryParameterGroup(*_frame_params);

	//generate frustum

/*
	ResourceGenerator* rg = nullptr;

	//TODO: Search for resource_generator in Lua

	if(rg == nullptr)
	{
		//If not in Lua
		//Search for resource_generator in array of registered C/C++ resource generators
		for(u32 i = 0; i < _num_resource_generators; i++)
		{
			if(_resource_generators_names[i] == generator_name)
			{
				rg = _resource_generators[i];
				break;
			}
		}
	}

	if(rg == nullptr)
		return false; // Error: resource_geneator not found
*/
	static const u32 MAX_NUM_VIEWS = 16;

	RenderView         views[MAX_NUM_VIEWS];
	Frustum 		   frustums[MAX_NUM_VIEWS];
	ResourceGenerator* generators[MAX_NUM_VIEWS];

	int num_views     = 1;
	int num_new_views = 1;

	views[0].camera         = camera;
	views[0].generator_args = generator_args;
	views[0].generator_name = generator_name;

	while(num_new_views > 0)
	{
		u32 view_index = num_views - num_new_views;

		const RenderView& rv = views[view_index];

		frustums[view_index] = rv.camera.getPlanes();

		//Find view's resource generator

		ResourceGenerator* rg = nullptr;

		for(u32 i = 0; i < _num_resource_generators; i++)
		{
			if(_resource_generators_names[i] == rv.generator_name)
			{
				rg = _resource_generators[i];
				break;
			}
		}

		if(rg == nullptr)
			return false;

		generators[view_index] = rg;

		u32 num = rg->getSecondaryViews(rv.camera, &views[num_views]);

		num_views	  += num;
		num_new_views += num;

		num_new_views--;
	}

	//const u32 NUM_FRUSTUMS = 1;

	//Frustum frustum = camera.getPlanes();

	auto visibility_data = allocator::allocateArray<VisibilityData>(*_temp_allocator, _num_render_queue_generators * num_views);

	auto visibility_temp = allocator::allocateArray<VisibilityData*>(*_temp_allocator, num_views);

	for(u32 i = 0; i < _num_render_queue_generators; i++)
	{
		for(u32 j = 0; j < num_views; j++)
			visibility_temp[j] = &visibility_data[j*_num_render_queue_generators + i];

		//visibility_data[i] = _render_queue_generators[i]->cull(NUM_FRUSTUMS, &frustum);
		_render_queue_generators[i]->cull(num_views, frustums, visibility_temp);
	}

	for(u32 i = 0; i < _num_render_queue_generators; i++)
	{
		for(u32 j = 0; j < num_views; j++)
			visibility_temp[j] = &visibility_data[j*_num_render_queue_generators + i];

		//_render_queue_generators[i]->extract(*visibility_data[i]);
		_render_queue_generators[i]->extract(visibility_data[i]);
		//_render_queue_generators[i]->extract(visibility_temp);
	}
	
	//Run generators in different thread
	//generateResource(generator_name, generator_args, visibility_data);
	
	for(int i = num_views - 1; i >= 0; i--)
	{
		getRenderDevice()->unbindResources();

		bindFrameParameters();

		//Set view params
		Matrix4x4 mat = views[i].camera.getView();
		setViewParameter(getStringID("view"), &mat, sizeof(mat));

		mat = mat.Invert();
		setViewParameter(getStringID("inv_view"), &mat, sizeof(mat));

		mat = views[i].camera.getProj();
		setViewParameter(getStringID("proj"), &mat, sizeof(mat));

		mat = mat.Invert();
		setViewParameter(getStringID("inv_proj"), &mat, sizeof(mat));

		mat = views[i].camera.getView() * views[i].camera.getProj();
		setViewParameter(getStringID("view_proj"), &mat, sizeof(mat));

		mat = mat.Invert();
		setViewParameter(getStringID("inv_view_proj"), &mat, sizeof(mat));

		Vector3 camera_position = views[i].camera.getPosition();
		setViewParameter(getStringID("camera_position"), &camera_position, sizeof(camera_position));

		Vector2 clip_distances = views[i].camera.getClipDistances();
		setViewParameter(getStringID("clip_distances"), &clip_distances, sizeof(clip_distances));

		//cache view params
		_cached_view_params = _render_device.cacheTemporaryParameterGroup(*_view_params);

		bindViewParameters();

		//Run generators in different thread
		generateResource(views[i].generator_name, views[i].generator_args, &visibility_data[i*_num_render_queue_generators]);
	}

	//--------------------------------------------------------------------
	/*
	u32 passes_names[] = {getStringID("gbuffer")};

	RenderQueue rq;
	rq.size = 0;

	for(u32 i = 0; i < _num_render_queue_generators; i++)
	{
		_render_queue_generators[i]->getRenderItems(1, passes_names, visibility_data[i], &rq);
	}
	
	for(u32 i = 0; i < rq.size; i++)
	{
		render(*rq.sort_items[i].item);
	}
	*/

	return true;
}

bool Renderer::buildRenderQueues(u8 num_passes, const u32* passes_names, 
								 const VisibilityData* visibility_data, RenderQueue* out_queues)
{
	RenderQueue** queues = allocator::allocateArray<RenderQueue*>(*_temp_allocator, _num_render_queue_generators);

	u32 merge_job = 0;

	for(u32 i = 0; i < _num_render_queue_generators; i++)
	{
		queues[i] = allocator::allocateArray<RenderQueue>(*_temp_allocator, num_passes);

		for(u8 j = 0; j < num_passes; j++)
			queues[i][j].size = 0;

		if(visibility_data[i].num_visibles > 0)
			_render_queue_generators[i]->getRenderItems(num_passes, passes_names, visibility_data[i], queues[i]);
		
		/*
		if(queues[i].size == 0)
			continue;
		*/

		/*
		if(global_queue.size + x.size >= global_queue.capacity)
		{
			allocate new global_queue

			add copy and merge job
		} else
		{
			u32 new_merge_job = depending on merge_job
			merge_job = new_merge_job
		}

		*/

	}

	//WAIT ON JOBS

	static const u8 MAX_NUM_PASSES = 16;

	u32 total_num_sort_items[MAX_NUM_PASSES] = { 0 };

	for(u32 i = 0; i < _num_render_queue_generators; i++)
	{
		for(u8 j = 0; j < num_passes; j++)
		{
			total_num_sort_items[j] += queues[i][j].size;
		}
	}	

	/*
	if(total_num_sort_items == 0)
		return true;
	*/

	//SortItem* sort_items   = allocator::allocateArray<SortItem>(*_temp_allocator, total_num_sort_items);

	for(u8 i = 0; i < num_passes; i++)
	{
		u32 num = total_num_sort_items[i];

		out_queues[i].size = num;

		if(num == 0)
			continue;
		
		out_queues[i].sort_items = allocator::allocateArray<SortItem>(*_temp_allocator, num);
	}

	//Merge sort items
	for(u8 i = 0; i < num_passes; i++)
	{
		u32 offset = 0;

		for(u32 j = 0; j < _num_render_queue_generators; j++)
		{
			u32 queue_size = queues[j][i].size;
			//memcpy(sort_items + offset, queues[i].sort_items, sizeof(SortItem)*queues[i].size);
			memcpy(out_queues[i].sort_items + offset, queues[j][i].sort_items, sizeof(SortItem)*queue_size);
			offset += queue_size;
		}
	}

	//wait(merge_jobs)

	//Sort

	return true;
}

bool Renderer::compute(u32 count_x, u32 count_y, u32 count_z,
					   const ComputeKernelPermutation& kernel, const ParameterGroup& params)
{
	//_render_device.CSBindShader(kernel.shader);

	ComputePipelineState compute_pipeline;
	compute_pipeline.shader = kernel.shader;

	_render_device.setComputePipelineState(&compute_pipeline);

	_render_device.bindComputeParameterGroup(&params, kernel.params_bind_info);

	_render_device.dispatch(count_x, count_y, count_z);
	
	return true;
}

bool Renderer::compute(BufferH args_buffer, u32 args_offset,
					   const ComputeKernelPermutation& kernel, const ParameterGroup& params)
{
	ComputePipelineState compute_pipeline;
	compute_pipeline.shader = kernel.shader;

	_render_device.setComputePipelineState(&compute_pipeline);

	_render_device.bindComputeParameterGroup(&params, kernel.params_bind_info);

	_render_device.dispatchIndirect(args_buffer, args_offset);

	return true;
}

void Renderer::addResourceGenerator(u32 name, ResourceGenerator* generator)
{
	_resource_generators_names[_num_resource_generators] = name;
	_resource_generators[_num_resource_generators++]     = generator;
}

void Renderer::addRenderQueueGenerator(u32 name, RenderQueueGenerator* generator)
{
	_render_queue_generators_names[_num_render_queue_generators] = name;
	_render_queue_generators[_num_render_queue_generators++]     = generator;
}

bool Renderer::generateResource(u32 resource_generator_name, const void* args_blob, const VisibilityData* visibility)
{
	ResourceGenerator* rg = nullptr;

	//Search for resource_generator in Lua

	if(rg == nullptr)
	{
		//If not in Lua
		//Search for resource_generator in array of registered C/C++ resource generators
		for(u32 i = 0; i < _num_resource_generators; i++)
		{
			if(_resource_generators_names[i] == resource_generator_name)
			{
				rg = _resource_generators[i];
				break;
			}
		}
	}

	if(rg == nullptr)
		return false; // Error: resource_geneator not found

	rg->generate(args_blob, visibility);

	return true;
}

RenderDevice* Renderer::getRenderDevice()
{
	return &_render_device;
}

const ShaderManager* Renderer::getShaderManager() const
{
	return &_shader_manager;
}

RenderTexture Renderer::getBackBuffer() const
{
	return{ _render_device.getBackBufferRT(), _render_device.getBackBufferUAV(), 
			_back_buffer_width, _back_buffer_height };
}

Profiler* Renderer::getProfiler() const
{
	return _profiler;
}

void Renderer::present()
{
	_render_device.present();
	_render_device.clearFrameAllocator();
}

void Renderer::setFrameParameter(u32 name, const void* data, u32 size)
{
	u32 offset = _shader_manager.getFrameParametersDesc()->getConstantOffset(name);

	if(offset == UINT32_MAX)
		return;

	//memcpy(pointer_math::add(_frame_params->map_commands->data, offset), data, size);
	memcpy(pointer_math::add(_frame_params->getCBuffersData(), offset), data, size);
}

void Renderer::setViewParameter(u32 name, const void* data, u32 size)
{
	u32 offset = _shader_manager.getViewParametersDesc()->getConstantOffset(name);

	if(offset == UINT32_MAX)
		return;

	//memcpy(pointer_math::add(_view_params->map_commands->data, offset), data, size);
	memcpy(pointer_math::add(_view_params->getCBuffersData(), offset), data, size);
}

void Renderer::bindFrameParameters()
{
	_render_device.bindParameterGroup(*_cached_frame_params, _shader_manager.getFrameParametersBindInfo());
	_render_device.bindComputeParameterGroup(_cached_frame_params, _shader_manager.getFrameParametersBindInfo());
}

void Renderer::bindViewParameters()
{
	_render_device.bindParameterGroup(*_cached_view_params, _shader_manager.getViewParametersBindInfo());
	_render_device.bindComputeParameterGroup(_cached_view_params, _shader_manager.getViewParametersBindInfo());
}

void Renderer::setPassParameter(u8 pass_index, u32 name, const void* data, u32 size)
{
	u32 offset = _shader_manager.getPassParametersDesc(pass_index)->getConstantOffset(name);

	if(offset == UINT32_MAX)
		return;

	//memcpy(pointer_math::add(_passes_params[pass_index]->map_commands->data, offset), data, size);
	memcpy(pointer_math::add(_passes_params[pass_index]->getCBuffersData(), offset), data, size);
}

bool Renderer::initLua(lua_State* lua_state)
{
	//Create Renderer table
	lua_newtable(lua_state);
	lua_pushvalue(lua_state, -1); //duplicate the table
	lua_setglobal(lua_state, "Renderer");

	//Create Renderer.Format table
	lua_newtable(lua_state);
	lua_pushvalue(lua_state, -1); //duplicate the table
	lua_setfield(lua_state, -3, "Format");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::R8_UNORM);
	lua_setfield(lua_state, -2, "R8");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::RGBA8_UNORM);
	lua_setfield(lua_state, -2, "RGBA8");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::R16_FLOAT);
	lua_setfield(lua_state, -2, "R16");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::RGBA16_FLOAT);
	lua_setfield(lua_state, -2, "RGBA16");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::R32_FLOAT);
	lua_setfield(lua_state, -2, "R32");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::RG32_FLOAT);
	lua_setfield(lua_state, -2, "RG32");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::RGB32_FLOAT);
	lua_setfield(lua_state, -2, "RGB32");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::RGBA32_FLOAT);
	lua_setfield(lua_state, -2, "RGBA32");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::D16_UNORM);
	lua_setfield(lua_state, -2, "D16");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::D24_UNORM_S8_UINT);
	lua_setfield(lua_state, -2, "D24S8");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::D32_FLOAT);
	lua_setfield(lua_state, -2, "D32");

	lua_pushinteger(lua_state, (u8)RenderResourceFormat::R32_TYPELESS);
	lua_setfield(lua_state, -2, "R32_TYPELESS");

	lua_pop(lua_state, 2); //pop Renderer and Renderer.Format tables

	return true;
}

bool Renderer::setupSamplers()
{
	enum class SamplerID : u8
	{
		TRI_POINT_CLAMP,
		TRI_LINEAR_CLAMP,
		ANISO_CLAMP,

		TRI_POINT_MIRROR,
		TRI_LINEAR_MIRROR,
		ANISO_MIRROR,

		TRI_POINT_WRAP,
		TRI_LINEAR_WRAP,
		ANISO_WRAP,

		SHADOW,

		SAMPLER_ID_COUNT
	};

	SamplerStateH samplers[(u8)SamplerID::SAMPLER_ID_COUNT];

	SamplerStateDesc desc;
	desc.filter         = TextureFilter::MIN_MAG_MIP_POINT;
	desc.u_address_mode = TextureAddressMode::CLAMP;
	desc.v_address_mode = TextureAddressMode::CLAMP;
	desc.w_address_mode = TextureAddressMode::CLAMP;

	_render_device.createSamplerState(desc, samplers[(u8)SamplerID::TRI_POINT_CLAMP]);

	//////////////////////////////////////////////////

	desc.filter = TextureFilter::MIN_MAG_MIP_LINEAR;

	_render_device.createSamplerState(desc, samplers[(u8)SamplerID::TRI_LINEAR_CLAMP]);

	//////////////////////////////////////////////////

	desc.filter = TextureFilter::ANISOTROPIC;

	_render_device.createSamplerState(desc, samplers[(u8)SamplerID::ANISO_CLAMP]);

	//////////////////////////////////////////////////

	desc.filter = TextureFilter::MIN_MAG_MIP_POINT;
	desc.u_address_mode = TextureAddressMode::MIRROR;
	desc.v_address_mode = TextureAddressMode::MIRROR;
	desc.w_address_mode = TextureAddressMode::MIRROR;

	_render_device.createSamplerState(desc, samplers[(u8)SamplerID::TRI_POINT_MIRROR]);

	//////////////////////////////////////////////////

	desc.filter = TextureFilter::MIN_MAG_MIP_LINEAR;

	_render_device.createSamplerState(desc, samplers[(u8)SamplerID::TRI_LINEAR_MIRROR]);

	//////////////////////////////////////////////////

	desc.filter = TextureFilter::ANISOTROPIC;

	_render_device.createSamplerState(desc, samplers[(u8)SamplerID::ANISO_MIRROR]);

	//////////////////////////////////////////////////

	desc.filter = TextureFilter::MIN_MAG_MIP_POINT;
	desc.u_address_mode = TextureAddressMode::WRAP;
	desc.v_address_mode = TextureAddressMode::WRAP;
	desc.w_address_mode = TextureAddressMode::WRAP;

	_render_device.createSamplerState(desc, samplers[(u8)SamplerID::TRI_POINT_WRAP]);

	//////////////////////////////////////////////////

	desc.filter = TextureFilter::MIN_MAG_MIP_LINEAR;

	_render_device.createSamplerState(desc, samplers[(u8)SamplerID::TRI_LINEAR_WRAP]);

	//////////////////////////////////////////////////

	desc.filter = TextureFilter::ANISOTROPIC;

	_render_device.createSamplerState(desc, samplers[(u8)SamplerID::ANISO_WRAP]);

	//////////////////////////////////////////////////

	desc.filter           = TextureFilter::COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	desc.u_address_mode   = TextureAddressMode::CLAMP;
	desc.v_address_mode   = TextureAddressMode::CLAMP;
	desc.w_address_mode   = TextureAddressMode::CLAMP;
	//desc.MipLODBias     = 0.0f;
	//desc.MaxAnisotropy  = 1;
	desc.comparison_func  = ComparisonFunc::LESS_EQUAL;
	//desc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
	//desc.MinLOD         = 0;
	//desc.MaxLOD         = D3D11_FLOAT32_MAX;

	_render_device.createSamplerState(desc, samplers[(u8)SamplerID::SHADOW]);

	//////////////////////////////////////////////////

	SamplerGroup sampler_group;
	sampler_group.samplers     = samplers;
	sampler_group.num_samplers = (u8)SamplerID::SAMPLER_ID_COUNT;

	_render_device.bindSamplerGroup(&sampler_group, nullptr);

	_render_device.applySamplerChanges();

	for(u32 i = 0; i < (u8)SamplerID::SAMPLER_ID_COUNT; i++)
		RenderDevice::release(samplers[i]);

	/*
	RenderDevice::release(samplers[(u8)SamplerID::TRI_POINT_CLAMP]);
	RenderDevice::release(samplers[(u8)SamplerID::TRI_LINEAR_CLAMP]);
	RenderDevice::release(samplers[(u8)SamplerID::ANISO_CLAMP]);

	RenderDevice::release(samplers[(u8)SamplerID::TRI_POINT_MIRROR]);
	RenderDevice::release(samplers[(u8)SamplerID::TRI_LINEAR_MIRROR]);
	RenderDevice::release(samplers[(u8)SamplerID::ANISO_MIRROR]);

	RenderDevice::release(samplers[(u8)SamplerID::TRI_POINT_WRAP]);
	RenderDevice::release(samplers[(u8)SamplerID::TRI_LINEAR_WRAP]);
	RenderDevice::release(samplers[(u8)SamplerID::ANISO_WRAP]);

	RenderDevice::release(samplers[(u8)SamplerID::SHADOW]);
	*/
	return true;
}

bool Renderer::createCommonParameterGroups()
{
	///////////////////////////////////////////////////////////////////////////////
	//Create Frame params group
	///////////////////////////////////////////////////////////////////////////////

	const ParameterGroupDesc* params_desc = _shader_manager.getFrameParametersDesc();

	//_frame_params = createParameterGroup(*params_desc, true, *_main_allocator);
	_frame_params = _render_device.createParameterGroup(*_main_allocator, RenderDevice::ParameterGroupType::FRAME,
														*params_desc, UINT32_MAX, 0, nullptr);
	/*
	u8 num_cbuffers = params_desc->getNumCBuffers();

	if(num_cbuffers > 0)
	{
		const u32* cbuffers_sizes = params_desc->getCBuffersSizes();

		for(u8 i = 0; i < num_cbuffers; i++)
		{
			ConstantBufferDesc desc;
			desc.size = cbuffers_sizes[i];
			desc.update_mode = UpdateMode::CPU;

			//if(!_render_device.createConstantBuffer(desc, nullptr, _frame_params->cbuffers[i]))
			if(!_render_device.createConstantBuffer(desc, nullptr, _frame_params->getCBuffer(i)->cbuffer))
			{
#if _DEBUG
				Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Failed to create Frame cbuffers!");
#endif

				return false;
			}
		}
	}
	*/
	///////////////////////////////////////////////////////////////////////////////
	//Create View params group
	///////////////////////////////////////////////////////////////////////////////
	params_desc = _shader_manager.getViewParametersDesc();

	//_view_params = createParameterGroup(*params_desc, true, *_main_allocator);
	_view_params = _render_device.createParameterGroup(*_main_allocator, RenderDevice::ParameterGroupType::VIEW,
													   *params_desc, UINT32_MAX, 0, nullptr);
	/*
	num_cbuffers = params_desc->getNumCBuffers();

	if(num_cbuffers > 0)
	{
		const u32* cbuffers_sizes = params_desc->getCBuffersSizes();

		for(u8 i = 0; i < num_cbuffers; i++)
		{
			ConstantBufferDesc desc;
			desc.size = cbuffers_sizes[i];
			desc.update_mode = UpdateMode::CPU;

			//if(!_render_device.createConstantBuffer(desc, nullptr, _view_params->cbuffers[i]))
			if(!_render_device.createConstantBuffer(desc, nullptr, _view_params->getCBuffer(i)->cbuffer))
			{
#if _DEBUG
				Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Failed to create View cbuffers!");
#endif

				return false;
			}
		}
	}
	*/
	///////////////////////////////////////////////////////////////////////////////
	//Create Passes params groups
	///////////////////////////////////////////////////////////////////////////////

	_num_passes = _shader_manager.getNumPasses();

	if(_num_passes == 0)
	{
		_passes_params = nullptr;
	}
	else
	{
		_passes_params = allocator::allocateArrayNoConstruct<ParameterGroup*>(*_main_allocator, _num_passes);

		params_desc = _shader_manager.getPassesParametersDescs();

		for(u8 i = 0; i < _num_passes; i++)
		{
			//_passes_params[i] = createParameterGroup(params_desc[i], true, *_main_allocator);
			_passes_params[i] = _render_device.createParameterGroup(*_main_allocator, RenderDevice::ParameterGroupType::FRAME,
																	params_desc[i], UINT32_MAX, 0, nullptr);
			/*
			num_cbuffers = params_desc[i].getNumCBuffers();

			if(num_cbuffers > 0)
			{
				const u32* cbuffers_sizes = params_desc[i].getCBuffersSizes();

				for(u8 j = 0; j < num_cbuffers; j++)
				{
					ConstantBufferDesc desc;
					desc.size = cbuffers_sizes[j];
					desc.update_mode = UpdateMode::CPU;

					//if(!_render_device.createConstantBuffer(desc, nullptr, _passes_params[i]->cbuffers[j]))
					if(!_render_device.createConstantBuffer(desc, nullptr, _passes_params[i]->getCBuffer(j)->cbuffer))
					{
#if _DEBUG
						Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Failed to create Passes params cbuffers!");
#endif

						return false;
					}
				}
			}
			*/
		}
	}

	return true;
}

bool Renderer::destroyCommonParameterGroups()
{
	if(_num_passes != 0)
	{
		for(u8 i = 0; i < _num_passes; i++)
			_render_device.deleteParameterGroup(*_main_allocator, *_passes_params[i]);

		allocator::deallocateArray(*_main_allocator, _passes_params);
	}

	_render_device.deleteParameterGroup(*_main_allocator, *_view_params);
	_render_device.deleteParameterGroup(*_main_allocator, *_frame_params);

	return true;
}