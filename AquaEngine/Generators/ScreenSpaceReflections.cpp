#include "ScreenSpaceReflections.h"

#include "..\DevTools\Profiler.h"

#include "..\Renderer\Renderer.h"
#include "..\Renderer\RendererInterfaces.h"
#include "..\Renderer\RendererStructs.h"
#include "..\Renderer\Camera.h"

#include "..\Utilities\StringID.h"

#include "..\AquaTypes.h"

#include <cmath>

using namespace aqua;

void ScreenSpaceReflections::init(aqua::Renderer& renderer, lua_State* lua_state, Allocator& allocator, 
								  LinearAllocator& scratchpad, u32 width, u32 height)
{
	_renderer             = &renderer;
	_allocator            = &allocator;
	_scratchpad_allocator = &scratchpad;

	_width  = width;
	_height = height;

	//------------------------------------------------------------------------------------------------
	// TEXTURES
	//------------------------------------------------------------------------------------------------
	{
		Texture2DDesc texture_desc;
		texture_desc.width          = _width;
		texture_desc.height         = _height;
		texture_desc.mip_levels     = NUM_GLOSSINESS_MIPS;
		texture_desc.array_size     = 1;
		texture_desc.format         = RenderResourceFormat::RGBA16_FLOAT;
		texture_desc.sample_count   = 1;
		texture_desc.sample_quality = 0;
		texture_desc.update_mode    = UpdateMode::GPU;
		texture_desc.generate_mips  = true;

		TextureViewDesc rt_view_descs[NUM_GLOSSINESS_MIPS];
		TextureViewDesc sr_view_descs[NUM_GLOSSINESS_MIPS + 1];

		for(u32 i = 0; i < NUM_GLOSSINESS_MIPS; i++)
		{
			rt_view_descs[i].format            = RenderResourceFormat::RGBA16_FLOAT;
			rt_view_descs[i].most_detailed_mip = i;
			rt_view_descs[i].mip_levels        = 1;

			sr_view_descs[i].format            = RenderResourceFormat::RGBA16_FLOAT;
			sr_view_descs[i].most_detailed_mip = i;
			sr_view_descs[i].mip_levels        = 1;
		}

		sr_view_descs[NUM_GLOSSINESS_MIPS].format = RenderResourceFormat::RGBA16_FLOAT;
		sr_view_descs[NUM_GLOSSINESS_MIPS].most_detailed_mip = 0;
		sr_view_descs[NUM_GLOSSINESS_MIPS].mip_levels = -1;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, NUM_GLOSSINESS_MIPS + 1, sr_view_descs, 
													  NUM_GLOSSINESS_MIPS, rt_view_descs, 0, nullptr, 0, nullptr, 
													  _reflection_target_sr, _reflection_target, nullptr, nullptr);

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, NUM_GLOSSINESS_MIPS, sr_view_descs, 
													  NUM_GLOSSINESS_MIPS, rt_view_descs, 0, nullptr, 0, nullptr, 
													  _glossiness_chain_target_sr, _glossiness_chain_target, nullptr, nullptr);

		//----------------------------------------------------------------------

		Texture2DDesc composite_texture_desc;
		composite_texture_desc.width          = _width;
		composite_texture_desc.height         = _height;
		composite_texture_desc.mip_levels     = 1;
		composite_texture_desc.array_size     = 1;
		composite_texture_desc.format         = RenderResourceFormat::RGBA16_FLOAT;
		composite_texture_desc.sample_count   = 1;
		composite_texture_desc.sample_quality = 0;
		composite_texture_desc.update_mode    = UpdateMode::GPU;
		composite_texture_desc.generate_mips  = false;
		
		TextureViewDesc rt_view_desc;
		rt_view_desc.format            = RenderResourceFormat::RGBA16_FLOAT;
		rt_view_desc.most_detailed_mip = 0;
		rt_view_desc.mip_levels        = 1;

		TextureViewDesc sr_view_desc;
		sr_view_desc.format            = RenderResourceFormat::RGBA16_FLOAT;
		sr_view_desc.most_detailed_mip = 0;
		sr_view_desc.mip_levels        = -1;

		_renderer->getRenderDevice()->createTexture2D(composite_texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
													  0, nullptr, &_composite_target_sr, &_composite_target, nullptr, nullptr);


	}

	//------------------------------------------------------------------------------------------------
	// SHADERS
	//------------------------------------------------------------------------------------------------

	// Raytrace
	auto ssr_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/screen_space_reflections.cshader"));
	_ssr_shader_permutation = ssr_shader->getPermutation(0);

	auto ssr_params_desc_set = ssr_shader->getInstanceParameterGroupDescSet();
	_ssr_params_desc         = getParameterGroupDesc(*ssr_params_desc_set, 0);

	_ssr_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																	 *_ssr_params_desc, UINT32_MAX, 0, nullptr);

	//Blur shaders
	auto blur_shader = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/screen_space_reflections_blur.cshader"));

	// Horizontal blur
	_horizontal_blur_shader_permutation = blur_shader->getPermutation(0);

	auto horizontal_blur_params_desc_set = blur_shader->getInstanceParameterGroupDescSet();
	_horizontal_blur_params_desc         = getParameterGroupDesc(*horizontal_blur_params_desc_set, 0);

	_horizontal_blur_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																				 *_horizontal_blur_params_desc, UINT32_MAX, 0, nullptr);

	// Vertical blur
	_vertical_blur_shader_permutation = blur_shader->getPermutation(1);

	auto vertical_blur_params_desc_set = blur_shader->getInstanceParameterGroupDescSet();
	_vertical_blur_params_desc         = getParameterGroupDesc(*vertical_blur_params_desc_set, 1);

	_vertical_blur_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																			   *_vertical_blur_params_desc, UINT32_MAX, 0, nullptr);

	// Composite
	auto composite_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/screen_space_reflections_composite.cshader"));
	_composite_shader_permutation = composite_shader->getPermutation(0);

	auto composite_params_desc_set = composite_shader->getInstanceParameterGroupDescSet();
	_composite_params_desc         = getParameterGroupDesc(*composite_params_desc_set, 0);

	_composite_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																		   *_composite_params_desc, UINT32_MAX, 0, nullptr);
};

void ScreenSpaceReflections::shutdown()
{
	RenderDevice& render_device = *_renderer->getRenderDevice();

	render_device.deleteParameterGroup(*_allocator, *_ssr_params);
	render_device.deleteParameterGroup(*_allocator, *_horizontal_blur_params);
	render_device.deleteParameterGroup(*_allocator, *_vertical_blur_params);
	render_device.deleteParameterGroup(*_allocator, *_composite_params);

	//RenderDevice::release(_reflection_target_sr);
	//RenderDevice::release(_reflection_target);

	for(u32 i = 0; i < NUM_GLOSSINESS_MIPS; i++)
	{
		RenderDevice::release(_reflection_target[i]);
		RenderDevice::release(_reflection_target_sr[i]);

		RenderDevice::release(_glossiness_chain_target[i]);
		RenderDevice::release(_glossiness_chain_target_sr[i]);
	}

	RenderDevice::release(_reflection_target_sr[NUM_GLOSSINESS_MIPS]);

	RenderDevice::release(_composite_target);
	RenderDevice::release(_composite_target_sr);
};

u32 ScreenSpaceReflections::getSecondaryViews(const Camera& camera, RenderView* out_views)
{
	return 0;
}

void ScreenSpaceReflections::generate(const void* args_, const VisibilityData* visibility)
{
	Profiler* profiler = _renderer->getProfiler();

	u32 scope_id;

	const Args& args = *(const Args*)args_;

	//-----------------------------------------------
	// RAYTRACE
	//-----------------------------------------------

	{
		scope_id = profiler->beginScope("raytrace");

		_renderer->getRenderDevice()->unbindResources();

		_renderer->bindFrameParameters();
		_renderer->bindViewParameters();

		_ssr_params->setSRV(args.color_texture, 0);
		_ssr_params->setSRV(args.normal_texture, 1);
		_ssr_params->setSRV(args.depth_texture, 2);

		Matrix4x4 project_to_pixel = args.camera->getProj();

		// Apply the scale/offset/bias matrix, which transforms from [-1,1]
		// post-projection space to [0,1] UV space
		Matrix4x4 tex_scale = Matrix4x4(Vector4(0.5f, 0.0f, 0.0f, 0.0f),
										Vector4(0.0f, -0.5f, 0.0f, 0.0f),
										Vector4(0.0f, 0.0f, 1.0f, 0.0f),
										Vector4(0.5f, 0.5f, 0.0f, 1.0f));

		Matrix4x4 size_scale = Matrix4x4(Vector4(_width, 0.0f, 0.0f, 0.0f),
										 Vector4(0.0f, _height, 0.0f, 0.0f),
										 Vector4(0.0f, 0.0f, 1.0f, 0.0f),
										 Vector4(0.0f, 0.0f, 0.0f, 1.0f));

		project_to_pixel = project_to_pixel * tex_scale * size_scale;

		u32 offset                     = _ssr_params_desc->getConstantOffset(getStringID("one_over_texture_size"));
		Vector2* one_over_texture_size = (Vector2*)pointer_math::add(_ssr_params->getCBuffersData(), offset);
		*one_over_texture_size         = Vector2(1.0f / _width, 1.0f / _height);
		
		offset         = _ssr_params_desc->getConstantOffset(getStringID("project_to_pixel"));
		Matrix4x4* p2p = (Matrix4x4*)pointer_math::add(_ssr_params->getCBuffersData(), offset);
		*p2p           = project_to_pixel;

		offset			   = _ssr_params_desc->getConstantOffset(getStringID("z_thickness"));
		float* z_thickness = (float*)pointer_math::add(_ssr_params->getCBuffersData(), offset);
		*z_thickness       = args.thickness;
		
		DrawCall dc = createDrawCall(false, 3, 0, 0);

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		RenderItem render_item;
		render_item.draw_call       = &dc;
		render_item.num_instances   = 1;
		render_item.shader          = _ssr_shader_permutation[0];
		render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_ssr_params);
		render_item.material_params = nullptr;
		render_item.mesh            = &mesh;

		_renderer->setViewport(*args.viewport, _width, _height);

		RenderTexture rt;
		rt.render_target = _reflection_target[0];
		rt.width         = _width;
		rt.height        = _height;

		_renderer->setRenderTarget(1, &rt, nullptr);
		//_renderer->setRenderTarget(1, args.target, nullptr);

		_renderer->render(render_item);

		profiler->endScope(scope_id);

		_renderer->getRenderDevice()->generateMips(_reflection_target_sr[NUM_GLOSSINESS_MIPS]);
	}

	// Horizontal
	{
		DrawCall dc = createDrawCall(false, 3, 0, 0);

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		RenderItem render_item;
		render_item.draw_call       = &dc;
		render_item.num_instances   = 1;
		render_item.shader          = _horizontal_blur_shader_permutation[0];
		render_item.material_params = nullptr;
		render_item.mesh            = &mesh;

		u32 offset = _horizontal_blur_params_desc->getConstantOffset(getStringID("one_over_texture_size"));

		Vector2* one_over_texture_size = (Vector2*)pointer_math::add(_horizontal_blur_params->getCBuffersData(), offset);

		for(u32 i = 0; i < NUM_GLOSSINESS_MIPS; i++)
		{
			_horizontal_blur_params->setSRV(_reflection_target_sr[i], 0);

			u32 width  = max(1, floor(1280 / pow(2, i)));
			u32 height = max(1, floor(720 / pow(2, i)));

			*one_over_texture_size = Vector2(1.0f / width, 1.0f / height);

			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_horizontal_blur_params);

			_renderer->setViewport(*args.viewport, width, height);

			RenderTexture rt;
			rt.render_target = _glossiness_chain_target[i];
			rt.width         = width;
			rt.height        = height;

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);
		}
	}

	// Vertical
	{
		DrawCall dc = createDrawCall(false, 3, 0, 0);

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		RenderItem render_item;
		render_item.draw_call       = &dc;
		render_item.num_instances   = 1;
		render_item.shader          = _vertical_blur_shader_permutation[0];
		render_item.material_params = nullptr;
		render_item.mesh            = &mesh;

		u32 offset = _vertical_blur_params_desc->getConstantOffset(getStringID("one_over_texture_size"));

		Vector2* one_over_texture_size = (Vector2*)pointer_math::add(_vertical_blur_params->getCBuffersData(), offset);

		for(u32 i = 0; i < NUM_GLOSSINESS_MIPS; i++)
		{
			_vertical_blur_params->setSRV(_glossiness_chain_target_sr[i], 0);

			u32 width  = max(1, floor(1280 / pow(2, i)));
			u32 height = max(1, floor(720 / pow(2, i)));

			*one_over_texture_size = Vector2(1.0f / width, 1.0f / height);

			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_vertical_blur_params);

			_renderer->setViewport(*args.viewport, width, height);

			RenderTexture rt;
			rt.render_target = _reflection_target[i];
			rt.width         = width;
			rt.height        = height;

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);
		}
	}

	//-----------------------------------------------
	// Composite
	//-----------------------------------------------

	{
		scope_id = profiler->beginScope("composite");

		_renderer->getRenderDevice()->unbindResources();

		_renderer->bindFrameParameters();
		_renderer->bindViewParameters();

		_composite_params->setSRV(args.color_texture, 0);
		_composite_params->setSRV(_reflection_target_sr[NUM_GLOSSINESS_MIPS], 1);
		_composite_params->setSRV(args.normal_texture, 2);
		//_composite_params->setSRV(args.depth_texture, 3);
		
		DrawCall dc = createDrawCall(false, 3, 0, 0);

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		RenderItem render_item;
		render_item.draw_call       = &dc;
		render_item.num_instances   = 1;
		render_item.shader          = _composite_shader_permutation[0];
		render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_composite_params);
		render_item.material_params = nullptr;
		render_item.mesh            = &mesh;

		_renderer->setViewport(*args.viewport, _width, _height);

		RenderTexture rt;
		rt.render_target = _composite_target;
		rt.width         = _width;
		rt.height        = _height;

		_renderer->setRenderTarget(1, &rt, nullptr);
		//_renderer->setRenderTarget(1, args.target, nullptr);

		_renderer->render(render_item);

		profiler->endScope(scope_id);
	}

	*args.output = _composite_target_sr;
};

void ScreenSpaceReflections::generate(lua_State* lua_state)
{
	//NOT IMPLEMENTED
};