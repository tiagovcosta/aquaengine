#include "MotionBlur.h"

#include "..\..\DevTools\Profiler.h"

#include "..\..\Renderer\Renderer.h"
#include "..\..\Renderer\RendererInterfaces.h"
#include "..\..\Renderer\RendererStructs.h"
#include "..\..\Renderer\Camera.h"

#include "..\..\Utilities\StringID.h"

#include "..\..\AquaTypes.h"

#include <cmath>

using namespace aqua;

void MotionBlur::init(Renderer& renderer, lua_State* lua_state, Allocator& allocator, LinearAllocator& temp_allocator,
					  u32 width, u32 height, u32 max_blur_radius)
{
	_renderer       = &renderer;
	_allocator      = &allocator;
	_temp_allocator = &temp_allocator;

	_width  = width;
	_height = height;

	_max_blur_radius = max_blur_radius;

	_tile_texture_width  = (_width + _max_blur_radius - 1) / _max_blur_radius;
	_tile_texture_height = (_height + _max_blur_radius - 1) / _max_blur_radius;

	//------------------------------------------------------------------------------------------------
	// TEXTURES
	//------------------------------------------------------------------------------------------------

	Texture2DDesc texture_desc;
	texture_desc.mip_levels     = 1;
	texture_desc.array_size     = 1;
	texture_desc.format         = RenderResourceFormat::RG8_UNORM;
	texture_desc.sample_count   = 1;
	texture_desc.sample_quality = 0;
	texture_desc.update_mode    = UpdateMode::GPU;
	texture_desc.generate_mips  = false;

	TextureViewDesc rt_view_desc;
	rt_view_desc.format            = RenderResourceFormat::RG8_UNORM;
	rt_view_desc.most_detailed_mip = 0;
	rt_view_desc.mip_levels        = 1;

	TextureViewDesc sr_view_desc;
	sr_view_desc.format            = RenderResourceFormat::RG8_UNORM;
	sr_view_desc.most_detailed_mip = 0;
	sr_view_desc.mip_levels        = -1;

	texture_desc.width  = _tile_texture_width;
	texture_desc.height = _height;

	_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
												  0, nullptr, &_tile_max_aux_target_sr, &_tile_max_aux_target, nullptr, nullptr);

	texture_desc.height = _tile_texture_height;

	_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
												  0, nullptr, &_tile_max_target_sr, &_tile_max_target, nullptr, nullptr);

	_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
												  0, nullptr, &_neighbor_max_target_sr, &_neighbor_max_target, nullptr, nullptr);

	texture_desc.format = RenderResourceFormat::RGBA16_FLOAT;
	texture_desc.width  = _width;
	texture_desc.height = _height;

	rt_view_desc.format = RenderResourceFormat::RGBA16_FLOAT;
	sr_view_desc.format = RenderResourceFormat::RGBA16_FLOAT;

	_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
												  0, nullptr, &_motion_blur_target_sr, &_motion_blur_target, nullptr, nullptr);

	//------------------------------------------------------------------------------------------------
	// SHADERS
	//------------------------------------------------------------------------------------------------

	// Tile max
	auto tile_max_shader          = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/tile_max.cshader"));
	auto tile_max_params_desc_set = tile_max_shader->getInstanceParameterGroupDescSet();

	_horizontal_tile_max_shader_permutation = tile_max_shader->getPermutation(0);

	_horizontal_tile_max_params_desc = getParameterGroupDesc(*tile_max_params_desc_set, 0);

	_horizontal_tile_max_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																					 *_horizontal_tile_max_params_desc, UINT32_MAX, 0, nullptr);
	
	//------------------

	_vertical_tile_max_shader_permutation = tile_max_shader->getPermutation(1);

	_vertical_tile_max_params_desc = getParameterGroupDesc(*tile_max_params_desc_set, 1);

	_vertical_tile_max_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																				   *_vertical_tile_max_params_desc, UINT32_MAX, 0, nullptr);

	_vertical_tile_max_params->setSRV(_tile_max_aux_target_sr, 0);

	// Neighbor max
	auto neighbor_max_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/neighbor_max.cshader"));
	_neighbor_max_shader_permutation = neighbor_max_shader->getPermutation(0);

	auto neighbor_max_params_desc_set = neighbor_max_shader->getInstanceParameterGroupDescSet();
	_neighbor_max_params_desc         = getParameterGroupDesc(*neighbor_max_params_desc_set, 0);

	_neighbor_max_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																			  *_neighbor_max_params_desc, UINT32_MAX, 0, nullptr);

	_neighbor_max_params->setSRV(_tile_max_target_sr, 0);

	// Motion blur
	auto motion_blur_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/motion_blur.cshader"));
	_motion_blur_shader_permutation = motion_blur_shader->getPermutation(0);

	auto motion_blur_params_desc_set = motion_blur_shader->getInstanceParameterGroupDescSet();
	_motion_blur_params_desc         = getParameterGroupDesc(*motion_blur_params_desc_set, 0);

	_motion_blur_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																			 *_motion_blur_params_desc, UINT32_MAX, 0, nullptr);

	_motion_blur_params->setSRV(_neighbor_max_target_sr, 0);
};

void MotionBlur::shutdown()
{
	RenderDevice& render_device = *_renderer->getRenderDevice();

	render_device.deleteParameterGroup(*_allocator, *_motion_blur_params);
	render_device.deleteParameterGroup(*_allocator, *_neighbor_max_params);
	render_device.deleteParameterGroup(*_allocator, *_vertical_tile_max_params);
	render_device.deleteParameterGroup(*_allocator, *_horizontal_tile_max_params);

	RenderDevice::release(_motion_blur_target_sr);
	RenderDevice::release(_motion_blur_target);

	RenderDevice::release(_neighbor_max_target_sr);
	RenderDevice::release(_neighbor_max_target);

	RenderDevice::release(_tile_max_target_sr);
	RenderDevice::release(_tile_max_target);

	RenderDevice::release(_tile_max_aux_target_sr);
	RenderDevice::release(_tile_max_aux_target);
};

u32 MotionBlur::getSecondaryViews(const Camera& camera, RenderView* out_views)
{
	return 0;
}

void MotionBlur::generate(const void* args_, const VisibilityData* visibility)
{
	Profiler* profiler = _renderer->getProfiler();

	u32 scope_id;

	const Args& args = *(const Args*)args_;

	//-----------------------------------------------
	// TILE MAX
	//-----------------------------------------------

	scope_id = profiler->beginScope("tile_max_velocity");

	{
		//Horizontal
		_renderer->getRenderDevice()->unbindResources();

		_renderer->bindFrameParameters();
		_renderer->bindViewParameters();

		DrawCall dc = createDrawCall(false, 3, 0, 0);

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		_horizontal_tile_max_params->setSRV(args.velocity_texture, 0);

		u32 offset           = _horizontal_tile_max_params_desc->getConstantOffset(getStringID("max_blur_radius"));
		u32* max_blur_radius = (u32*)pointer_math::add(_horizontal_tile_max_params->getCBuffersData(), offset);
		*max_blur_radius     = _max_blur_radius;

		RenderItem render_item;
		render_item.draw_call       = &dc;
		render_item.num_instances   = 1;
		render_item.shader          = _horizontal_tile_max_shader_permutation[0];
		render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_horizontal_tile_max_params);
		render_item.material_params = nullptr;
		render_item.mesh            = &mesh;

		_renderer->setViewport(*args.viewport, _tile_texture_width, _height);

		RenderTexture rt;
		rt.render_target = _tile_max_aux_target;
		rt.width         = _tile_texture_width;
		rt.height        = _height;

		_renderer->setRenderTarget(1, &rt, nullptr);

		_renderer->render(render_item);
	}

	{
		//vertical
		_renderer->getRenderDevice()->unbindResources();

		_renderer->bindFrameParameters();
		_renderer->bindViewParameters();

		DrawCall dc = createDrawCall(false, 3, 0, 0);

		u32 offset           = _vertical_tile_max_params_desc->getConstantOffset(getStringID("max_blur_radius"));
		u32* max_blur_radius = (u32*)pointer_math::add(_vertical_tile_max_params->getCBuffersData(), offset);
		*max_blur_radius     = _max_blur_radius;

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		RenderItem render_item;
		render_item.draw_call       = &dc;
		render_item.num_instances   = 1;
		render_item.shader          = _vertical_tile_max_shader_permutation[0];
		render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_vertical_tile_max_params);
		render_item.material_params = nullptr;
		render_item.mesh            = &mesh;

		_renderer->setViewport(*args.viewport, _tile_texture_width, _tile_texture_height);

		RenderTexture rt;
		rt.render_target = _tile_max_target;
		rt.width         = _tile_texture_width;
		rt.height        = _tile_texture_height;

		_renderer->setRenderTarget(1, &rt, nullptr);

		_renderer->render(render_item);
	}

	profiler->endScope(scope_id);

	//-----------------------------------------------
	// NEIGHBOR MAX
	//-----------------------------------------------

	{
		scope_id = profiler->beginScope("neighbor_max");

		_renderer->getRenderDevice()->unbindResources();

		_renderer->bindFrameParameters();
		_renderer->bindViewParameters();

		DrawCall dc = createDrawCall(false, 3, 0, 0);

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		RenderItem render_item;
		render_item.draw_call       = &dc;
		render_item.num_instances   = 1;
		render_item.shader          = _neighbor_max_shader_permutation[0];
		render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_neighbor_max_params);
		render_item.material_params = nullptr;
		render_item.mesh            = &mesh;

		_renderer->setViewport(*args.viewport, _tile_texture_width, _tile_texture_height);

		RenderTexture rt;
		rt.render_target = _neighbor_max_target;
		rt.width         = _tile_texture_width;
		rt.height        = _tile_texture_height;

		_renderer->setRenderTarget(1, &rt, nullptr);

		_renderer->render(render_item);

		profiler->endScope(scope_id);
	}

	//-----------------------------------------------
	// GATHER
	//-----------------------------------------------

	{
		scope_id = profiler->beginScope("gather");

		_renderer->getRenderDevice()->unbindResources();

		_renderer->bindFrameParameters();
		_renderer->bindViewParameters();

		DrawCall dc = createDrawCall(false, 3, 0, 0);

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		_motion_blur_params->setSRV(args.velocity_texture, 1);
		_motion_blur_params->setSRV(args.color_texture, 2);
		_motion_blur_params->setSRV(args.depth_texture, 3);

		u32 offset           = _motion_blur_params_desc->getConstantOffset(getStringID("max_blur_radius"));
		u32* max_blur_radius = (u32*)pointer_math::add(_motion_blur_params->getCBuffersData(), offset);
		*max_blur_radius     = _max_blur_radius;

		offset           = _motion_blur_params_desc->getConstantOffset(getStringID("num_samples"));
		u32* num_samples = (u32*)pointer_math::add(_motion_blur_params->getCBuffersData(), offset);
		*num_samples     = args.num_samples;

		offset             = _motion_blur_params_desc->getConstantOffset(getStringID("screen_size"));
		u32* screen_size   = (u32*)pointer_math::add(_motion_blur_params->getCBuffersData(), offset);
		*screen_size       = _width;
		*(screen_size + 1) = _height;

		offset                       = _motion_blur_params_desc->getConstantOffset(getStringID("screen_size_times_two"));
		u32* screen_size_times_two   = (u32*)pointer_math::add(_motion_blur_params->getCBuffersData(), offset);
		*screen_size_times_two       = _width * 2;
		*(screen_size_times_two + 1) = _height * 2;

		offset                     = _motion_blur_params_desc->getConstantOffset(getStringID("screen_size_minus_one"));
		u32* screen_size_minus_one = (u32*)pointer_math::add(_motion_blur_params->getCBuffersData(), offset);
		*screen_size_minus_one     = _width - 1;
		*(screen_size_minus_one+1) = _height - 1;

		RenderItem render_item;
		render_item.draw_call       = &dc;
		render_item.num_instances   = 1;
		render_item.shader          = _motion_blur_shader_permutation[0];
		render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_motion_blur_params);
		render_item.material_params = nullptr;
		render_item.mesh            = &mesh;

		_renderer->setViewport(*args.viewport, _width, _height);

		RenderTexture rt;
		rt.render_target = _motion_blur_target;
		rt.width         = _width;
		rt.height        = _height;

		_renderer->setRenderTarget(1, &rt, nullptr);

		_renderer->render(render_item);

		profiler->endScope(scope_id);
	}

	*args.output = _motion_blur_target_sr;
};

void MotionBlur::generate(lua_State* lua_state)
{
	//NOT IMPLEMENTED
};