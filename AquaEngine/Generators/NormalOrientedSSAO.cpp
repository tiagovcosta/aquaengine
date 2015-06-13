#include "NormalOrientedSSAO.h"

#include "..\DevTools\Profiler.h"

#include "..\Renderer\Renderer.h"
#include "..\Renderer\RendererInterfaces.h"
#include "..\Renderer\RendererStructs.h"
#include "..\Renderer\Camera.h"

#include "..\Utilities\StringID.h"

#include "..\AquaTypes.h"

#include <cmath>

using namespace aqua;

void SSAOGenerator::init(aqua::Renderer& renderer, lua_State* lua_state, Allocator& allocator,
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
		texture_desc.mip_levels     = 1;
		texture_desc.array_size     = 1;
		texture_desc.format         = RenderResourceFormat::R8_UNORM;
		texture_desc.sample_count   = 1;
		texture_desc.sample_quality = 0;
		texture_desc.update_mode    = UpdateMode::GPU;
		texture_desc.generate_mips  = false;

		TextureViewDesc rt_view_desc;
		rt_view_desc.format            = RenderResourceFormat::R8_UNORM;
		rt_view_desc.most_detailed_mip = 0;
		rt_view_desc.mip_levels        = 1;

		TextureViewDesc sr_view_desc;
		sr_view_desc.format            = RenderResourceFormat::R8_UNORM;
		sr_view_desc.most_detailed_mip = 0;
		sr_view_desc.mip_levels        = 1;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc,
													  1, &rt_view_desc, 0, nullptr, 0, nullptr,
													  &_aux_buffer_sr, &_aux_buffer_rt, nullptr, nullptr);
	}

	//------------------------------------------------------------------------------------------------
	// SHADERS
	//------------------------------------------------------------------------------------------------

	// SSAO
	auto ssao_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/ssao.cshader"));
	_ssao_shader_permutation = ssao_shader->getPermutation(0);

	auto ssao_params_desc_set = ssao_shader->getInstanceParameterGroupDescSet();
	_ssao_params_desc         = getParameterGroupDesc(*ssao_params_desc_set, 0);

	_ssao_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																	  *_ssao_params_desc, UINT32_MAX, 0, nullptr);

	// Blur
	auto blur_shader = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/ssao_blur.cshader"));

	_ssao_blur_shader_permutation = blur_shader->getPermutation(0);

	auto blur_blur_params_desc_set = blur_shader->getInstanceParameterGroupDescSet();
	_ssao_blur_params_desc         = getParameterGroupDesc(*blur_blur_params_desc_set, 0);

	_ssao_blur_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																		   *_ssao_blur_params_desc, UINT32_MAX, 0, nullptr);
};

void SSAOGenerator::shutdown()
{
	RenderDevice& render_device = *_renderer->getRenderDevice();

	render_device.deleteParameterGroup(*_allocator, *_ssao_params);
	render_device.deleteParameterGroup(*_allocator, *_ssao_blur_params);

	RenderDevice::release(_aux_buffer_rt);
	RenderDevice::release(_aux_buffer_sr);
};

u32 SSAOGenerator::getSecondaryViews(const Camera& camera, RenderView* out_views)
{
	return 0;
}

void SSAOGenerator::generate(const void* args_, const VisibilityData* visibility)
{
	Profiler* profiler = _renderer->getProfiler();

	u32 scope_id;

	const Args& args = *(const Args*)args_;

	//-----------------------------------------------
	// SSAO
	//-----------------------------------------------

	{
		scope_id = profiler->beginScope("generate");

		_renderer->getRenderDevice()->unbindResources();

		_renderer->bindFrameParameters();
		_renderer->bindViewParameters();

		_ssao_params->setSRV(args.normal_buffer, 0);
		_ssao_params->setSRV(args.depth_buffer, 1);

		DrawCall dc = createDrawCall(false, 3, 0, 0);

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		RenderItem render_item;
		render_item.draw_call       = &dc;
		render_item.num_instances   = 1;
		render_item.shader          = _ssao_shader_permutation[0];
		render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_ssao_params);
		render_item.material_params = nullptr;
		render_item.mesh            = &mesh;

		_renderer->setViewport(*args.viewport, _width, _height);

		RenderTexture rt;
		rt.render_target = _aux_buffer_rt;
		rt.width         = _width;
		rt.height        = _height;

		_renderer->setRenderTarget(1, &rt, nullptr);

		_renderer->render(render_item);

		profiler->endScope(scope_id);
	}

	// Blur
	{
		scope_id = profiler->beginScope("blur");

		DrawCall dc = createDrawCall(false, 3, 0, 0);

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		RenderItem render_item;
		render_item.draw_call       = &dc;
		render_item.num_instances   = 1;
		render_item.shader          = _ssao_blur_shader_permutation[0];
		render_item.material_params = nullptr;
		render_item.mesh            = &mesh;

		u32 offset = _ssao_blur_params_desc->getConstantOffset(getStringID("one_over_texture_size"));

		if(offset != UINT32_MAX)
		{
			Vector2* one_over_texture_size = (Vector2*)pointer_math::add(_ssao_blur_params->getCBuffersData(), offset);

			*one_over_texture_size = Vector2(1.0f / _width, 1.0f / _height);
		}

		_ssao_blur_params->setSRV(_aux_buffer_sr, 0);

		render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_ssao_blur_params);

		_renderer->setViewport(*args.viewport, args.target_width, args.target_height);

		RenderTexture rt;
		rt.render_target = args.target;
		rt.width         = args.target_width;
		rt.height        = args.target_height;

		_renderer->setRenderTarget(1, &rt, nullptr);

		_renderer->render(render_item);

		profiler->endScope(scope_id);
	}
};

void SSAOGenerator::generate(lua_State* lua_state)
{
	//NOT IMPLEMENTED
};