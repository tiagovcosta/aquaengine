#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015              
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\Renderer\Renderer.h"
#include "..\Renderer\RendererInterfaces.h"
#include "..\Renderer\RendererStructs.h"
#include "..\Renderer\Camera.h"

#include "..\Utilities\StringID.h"

#include "..\AquaTypes.h"

#include <cmath>

using namespace aqua;

class ToneMapper : public ResourceGenerator
{
public:
	struct Args
	{
		const Viewport* viewport;
		ShaderResourceH source;
		const RenderTexture* target;

		float shoulder_strength;
		float linear_strength;
		float linear_angle;
		float toe_strength;
		float toe_rise;
		float toe_run;
		float linear_white;
		float middle_grey;
	};

	void init(aqua::Renderer& renderer, lua_State* lua_state, Allocator& allocator, LinearAllocator& temp_allocator,
		u32 width, u32 height)
	{
		_renderer       = &renderer;
		_allocator      = &allocator;
		_temp_allocator = &temp_allocator;

		//------------------------------------------------------------------------------------------------
		// TEXTURES
		//------------------------------------------------------------------------------------------------

		Texture2DDesc texture_desc;

		// LUMINANCE TARGET

		const u32 LUMINANCE_WIDTH  = width / 2;
		const u32 LUMINANCE_HEIGHT = height / 2;

		u32 num_mip_levels = 1 + (u32)std::floor(log2(max(LUMINANCE_WIDTH, LUMINANCE_HEIGHT)));

		texture_desc.width          = LUMINANCE_WIDTH;
		texture_desc.height         = LUMINANCE_HEIGHT;
		texture_desc.mip_levels     = num_mip_levels;
		texture_desc.array_size     = 1;
		texture_desc.format         = RenderResourceFormat::R16_FLOAT;
		texture_desc.sample_count   = 1;
		texture_desc.sample_quality = 0;
		texture_desc.update_mode    = UpdateMode::GPU;
		texture_desc.generate_mips  = true;

		TextureViewDesc rt_view_desc;
		rt_view_desc.format            = RenderResourceFormat::R16_FLOAT;
		rt_view_desc.most_detailed_mip = 0;
		rt_view_desc.mip_levels        = 1;

		TextureViewDesc sr_view_desc;
		sr_view_desc.format            = RenderResourceFormat::R16_FLOAT;
		sr_view_desc.most_detailed_mip = 0;
		sr_view_desc.mip_levels        = -1;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
			0, nullptr, &_luminance_target_sr, &_luminance_target, nullptr, nullptr);

		// AVG LUMINANCE TARGETS

		texture_desc.width         = 1;
		texture_desc.height        = 1;
		texture_desc.mip_levels    = 1;
		texture_desc.generate_mips = false;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
			0, nullptr, &_avg_luminance_target_sr, &_avg_luminance_target, nullptr, nullptr);

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
			0, nullptr, &_prev_avg_luminance_target_sr, &_prev_avg_luminance_target, nullptr, nullptr);

		//const float prev_luminance_clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		//_renderer->getRenderDevice()->clearRenderTarget(_prev_avg_luminance_target, prev_luminance_clear);
		{
			// BRIGHT PASS TARGET

			static const u32 NUM_BLOOM_MIPS = 5;

			texture_desc.width          = width / 2;
			texture_desc.height         = height / 2;
			texture_desc.mip_levels     = NUM_BLOOM_MIPS;
			texture_desc.array_size     = 1;
			//texture_desc.format         = RenderResourceFormat::RGBA16_FLOAT;
			texture_desc.format         = RenderResourceFormat::RG11B10_FLOAT;
			texture_desc.sample_count   = 1;
			texture_desc.sample_quality = 0;
			texture_desc.update_mode    = UpdateMode::GPU;
			texture_desc.generate_mips  = true;

			TextureViewDesc rt_view_descs[NUM_BLOOM_MIPS];
			TextureViewDesc sr_view_descs[NUM_BLOOM_MIPS + 1];

			for(u32 i = 0; i < NUM_BLOOM_MIPS; i++)
			{
				rt_view_descs[i].format            = RenderResourceFormat::RG11B10_FLOAT;
				rt_view_descs[i].most_detailed_mip = i;
				rt_view_descs[i].mip_levels        = 1;

				sr_view_descs[i].format            = RenderResourceFormat::RG11B10_FLOAT;
				sr_view_descs[i].most_detailed_mip = i;
				sr_view_descs[i].mip_levels        = 1;
			}

			sr_view_descs[5].format            = RenderResourceFormat::RG11B10_FLOAT;
			sr_view_descs[5].most_detailed_mip = 0;
			sr_view_descs[5].mip_levels        = -1;

			_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, NUM_BLOOM_MIPS + 1, sr_view_descs, NUM_BLOOM_MIPS, rt_view_descs, 0, nullptr,
														  0, nullptr, _bright_pass_target_sr, _bright_pass_target, nullptr, nullptr);

			_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, NUM_BLOOM_MIPS, sr_view_descs, NUM_BLOOM_MIPS, rt_view_descs, 0, nullptr,
														  0, nullptr, _bloom_sr, _bloom_target, nullptr, nullptr);
		}

		//------------------------------------------------------------------------------------------------
		// SHADERS
		//------------------------------------------------------------------------------------------------

		//Luminance
		auto luminance_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/luminance.cshader"));
		_luminance_shader_permutation = luminance_shader->getPermutation(0);

		auto luminance_params_desc_set = luminance_shader->getInstanceParameterGroupDescSet();
		_luminance_params_desc         = getParameterGroupDesc(*luminance_params_desc_set, 0);

		_luminance_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																			   *_luminance_params_desc, UINT32_MAX, 0, nullptr);
		//_luminance_params->setSRV(_normal_buffer_sr, 0);

		//Eye adapt
		auto eye_adapt_shader = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/eye_adapt.cshader"));
		_eye_adapt_shader_permutation = eye_adapt_shader->getPermutation(0);

		auto eye_adapt_params_desc_set = eye_adapt_shader->getInstanceParameterGroupDescSet();
		_eye_adapt_params_desc = getParameterGroupDesc(*eye_adapt_params_desc_set, 0);

		_eye_adapt_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																			   *_eye_adapt_params_desc, UINT32_MAX, 0, nullptr);
		
		u32 offset = _eye_adapt_params_desc->getConstantOffset(getStringID("last_mip_level"));

		u32* last_mip_level = (u32*)pointer_math::add(_eye_adapt_params->getCBuffersData(), offset);

		*last_mip_level = num_mip_levels - 1;

		// Bright pass
		auto bright_pass_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/bright_pass.cshader"));
		_bright_pass_shader_permutation = bright_pass_shader->getPermutation(0);

		auto bright_pass_params_desc_set = bright_pass_shader->getInstanceParameterGroupDescSet();
		_bright_pass_params_desc         = getParameterGroupDesc(*bright_pass_params_desc_set, 0);

		_bright_pass_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																				 *_bright_pass_params_desc, UINT32_MAX, 0, nullptr);

		// Bloom blur
		auto blur_shader                    = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/blur.cshader"));
		_horizontal_blur_shader_permutation = blur_shader->getPermutation(0);
		_vertical_blur_shader_permutation   = blur_shader->getPermutation(1);

		auto blur_params_desc_set = blur_shader->getInstanceParameterGroupDescSet();
		_horizontal_blur_params_desc         = getParameterGroupDesc(*blur_params_desc_set, 0);

		_horizontal_blur_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																					 *_horizontal_blur_params_desc, UINT32_MAX, 0, nullptr);

		_vertical_blur_params_desc = getParameterGroupDesc(*blur_params_desc_set, 1);

		_vertical_blur_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																				   *_vertical_blur_params_desc, UINT32_MAX, 0, nullptr);

		//Tonemap
		auto tonemap_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/tonemap.cshader"));
		_tonemap_shader_permutation = tonemap_shader->getPermutation(0);

		auto tonemap_params_desc_set = tonemap_shader->getInstanceParameterGroupDescSet();
		_tonemap_params_desc         = getParameterGroupDesc(*tonemap_params_desc_set, 0);

		_tonemap_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																			 *_tonemap_params_desc, UINT32_MAX, 0, nullptr);
		//_tonemap_params->setSRV(_lighting_buffer_sr, 0);
	};

	void shutdown()
	{
		RenderDevice& render_device = *_renderer->getRenderDevice();

		render_device.deleteParameterGroup(*_allocator, *_tonemap_params);
		render_device.deleteParameterGroup(*_allocator, *_vertical_blur_params);
		render_device.deleteParameterGroup(*_allocator, *_horizontal_blur_params);
		render_device.deleteParameterGroup(*_allocator, *_bright_pass_params);
		render_device.deleteParameterGroup(*_allocator, *_eye_adapt_params);
		render_device.deleteParameterGroup(*_allocator, *_luminance_params);

		for(u32 i = 0; i < 5; i++)
		{
			RenderDevice::release(_bright_pass_target[i]);
			RenderDevice::release(_bright_pass_target_sr[i]);

			RenderDevice::release(_bloom_target[i]);
			RenderDevice::release(_bloom_sr[i]);
		}

		RenderDevice::release(_bright_pass_target_sr[5]);

		RenderDevice::release(_avg_luminance_target);
		RenderDevice::release(_avg_luminance_target_sr);

		RenderDevice::release(_prev_avg_luminance_target);
		RenderDevice::release(_prev_avg_luminance_target_sr);

		RenderDevice::release(_luminance_target_sr);
		RenderDevice::release(_luminance_target);
	};

	u32 getSecondaryViews(const Camera& camera, RenderView* out_views) override final
	{
		return 0;
	}

	void generate(const void* args_, const VisibilityData* visibility) override final
	{
		//Profiler* profiler = _renderer->getProfiler();

		u32 scope_id;

		const Args& args = *(const Args*)args_;

		//-----------------------------------------------
		//LUMINANCE
		//-----------------------------------------------

		//scope_id = profiler->beginScope("luminance");

		{
			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			_luminance_params->setSRV(args.source, 0);

			u32 offset = _luminance_params_desc->getConstantOffset(getStringID("input_texel_size"));

			Vector2* input_texel_size = (Vector2*)pointer_math::add(_luminance_params->getCBuffersData(), offset);

			*input_texel_size = Vector2(1.0f / 1280, 1.0f / 720);

			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _luminance_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_luminance_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			_renderer->setViewport(*args.viewport, 1280/2, 720/2);

			RenderTexture rt;
			rt.render_target = _luminance_target;
			rt.width         = 1280;
			rt.height        = 720;

			_renderer->setRenderTarget(1, &rt, nullptr);
			//_renderer->setRenderTarget(1, args.target, nullptr);

			_renderer->render(render_item);
		}

		_renderer->getRenderDevice()->generateMips(_luminance_target_sr);

		//profiler->endScope(scope_id);

		//-----------------------------------------------
		// EYE ADAPT
		//-----------------------------------------------

		//scope_id = profiler->beginScope("eye_adapt");

		{
			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			_eye_adapt_params->setSRV(_luminance_target_sr, 0);
			_eye_adapt_params->setSRV(_prev_avg_luminance_target_sr, 1);

			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _eye_adapt_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_eye_adapt_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			_renderer->setViewport(*args.viewport, 1, 1);

			RenderTexture rt;
			rt.render_target = _avg_luminance_target;
			rt.width         = 1;
			rt.height        = 1;

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);
		}

		//profiler->endScope(scope_id);

		//-----------------------------------------------
		// BRIGHT PASS
		//-----------------------------------------------

		{
			//scope_id = profiler->beginScope("bright_pass");

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			_bright_pass_params->setSRV(args.source, 0);
			_bright_pass_params->setSRV(_avg_luminance_target_sr, 1);
			
			u32 offset = _bright_pass_params_desc->getConstantOffset(getStringID("threshold"));

			float* threshold = (float*)pointer_math::add(_bright_pass_params->getCBuffersData(), offset);

			*threshold = 1.0f;
			
			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _bright_pass_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_bright_pass_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			_renderer->setViewport(*args.viewport, 1280 / 2, 720 / 2);
			
			RenderTexture rt;
			rt.render_target = _bright_pass_target[0];
			rt.width         = 1280 / 2;
			rt.height        = 720 / 2;

			_renderer->setRenderTarget(1, &rt, nullptr);
			
			//_renderer->setRenderTarget(1, , nullptr);

			_renderer->render(render_item);

			_renderer->getRenderDevice()->generateMips(_bright_pass_target_sr[5]);

			//profiler->endScope(scope_id);
		}

		//-----------------------------------------------
		// BLUR
		//-----------------------------------------------

		{
			//scope_id = profiler->beginScope("blur");

			u32 width = 1280;
			u32 height = 720;

			u32 widths[]  = { 1280 / 2, 1280 / 4, 1280 / 8, 1280 / 16, 1280 / 32 };
			u32 heights[] = { 720 / 2, 720 / 4, 720 / 8, 720 / 16, 720 / 32 };

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			//Horizontal

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			_horizontal_blur_params->setSRV(_bright_pass_target_sr[4], 0);
			
			u32 offset = _horizontal_blur_params_desc->getConstantOffset(getStringID("one_over_texture_size"));

			Vector2* one_over_texture_size = (Vector2*)pointer_math::add(_horizontal_blur_params->getCBuffersData(), offset);

			*one_over_texture_size = Vector2(1.0f / widths[4], 1.0f / heights[4]);
			
			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _horizontal_blur_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_horizontal_blur_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			_renderer->setViewport(*args.viewport, widths[4], heights[4]);

			RenderTexture rt;
			rt.render_target = _bloom_target[4];
			rt.width         = widths[4];
			rt.height        = heights[4];

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);

			//profiler->endScope(scope_id);

			// 3

			_horizontal_blur_params->setSRV(_bright_pass_target_sr[3], 0);

			*one_over_texture_size = Vector2(1.0f / widths[3], 1.0f / heights[3]);

			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_horizontal_blur_params);

			_renderer->setViewport(*args.viewport, widths[3], heights[3]);

			rt.render_target = _bloom_target[3];
			rt.width         = widths[3];
			rt.height        = heights[3];

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);

			// 2

			_horizontal_blur_params->setSRV(_bright_pass_target_sr[2], 0);

			*one_over_texture_size = Vector2(1.0f / widths[2], 1.0f / heights[2]);

			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_horizontal_blur_params);

			_renderer->setViewport(*args.viewport, widths[2], heights[2]);

			rt.render_target = _bloom_target[2];
			rt.width         = widths[2];
			rt.height        = heights[2];

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);

			// 1

			_horizontal_blur_params->setSRV(_bright_pass_target_sr[1], 0);

			*one_over_texture_size = Vector2(1.0f / widths[1], 1.0f / heights[1]);

			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_horizontal_blur_params);

			_renderer->setViewport(*args.viewport, widths[1], heights[1]);

			rt.render_target = _bloom_target[1];
			rt.width         = widths[1];
			rt.height        = heights[1];

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);

			// 0

			_horizontal_blur_params->setSRV(_bright_pass_target_sr[0], 0);

			*one_over_texture_size = Vector2(1.0f / widths[0], 1.0f / heights[0]);

			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_horizontal_blur_params);

			_renderer->setViewport(*args.viewport, widths[0], heights[0]);

			rt.render_target = _bloom_target[0];
			rt.width         = widths[0];
			rt.height        = heights[0];

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);

			// Vertical

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			render_item.shader          = _vertical_blur_shader_permutation[0];

			one_over_texture_size = (Vector2*)pointer_math::add(_vertical_blur_params->getCBuffersData(), offset);

			// 4

			_vertical_blur_params->setSRV(_bloom_sr[4], 0);
			_vertical_blur_params->setSRV(nullptr, 1);

			*one_over_texture_size = Vector2(1.0f / widths[4], 1.0f / heights[4]);

			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_vertical_blur_params);

			_renderer->setViewport(*args.viewport, widths[4], heights[4]);

			rt.render_target = _bright_pass_target[4];
			rt.width         = widths[4];
			rt.height        = heights[4];

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);

			// 3

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			_vertical_blur_params->setSRV(_bloom_sr[3], 0);
			_vertical_blur_params->setSRV(_bright_pass_target_sr[4], 1);

			*one_over_texture_size = Vector2(1.0f / widths[3], 1.0f / heights[3]);

			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_vertical_blur_params);

			_renderer->setViewport(*args.viewport, widths[3], heights[3]);

			rt.render_target = _bright_pass_target[3];
			rt.width         = widths[3];
			rt.height        = heights[3];

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);

			// 2

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			_vertical_blur_params->setSRV(_bloom_sr[2], 0);
			_vertical_blur_params->setSRV(_bright_pass_target_sr[3], 1);

			*one_over_texture_size = Vector2(1.0f / widths[2], 1.0f / heights[2]);

			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_vertical_blur_params);

			_renderer->setViewport(*args.viewport, widths[2], heights[2]);

			rt.render_target = _bright_pass_target[2];
			rt.width         = widths[2];
			rt.height        = heights[2];

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);

			// 1

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			_vertical_blur_params->setSRV(_bloom_sr[1], 0);
			_vertical_blur_params->setSRV(_bright_pass_target_sr[2], 1);

			*one_over_texture_size = Vector2(1.0f / widths[1], 1.0f / heights[1]);

			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_vertical_blur_params);

			_renderer->setViewport(*args.viewport, widths[1], heights[1]);

			rt.render_target = _bright_pass_target[1];
			rt.width         = widths[1];
			rt.height        = heights[1];

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);

			// 0

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			_vertical_blur_params->setSRV(_bloom_sr[0], 0);
			_vertical_blur_params->setSRV(_bright_pass_target_sr[1], 1);

			*one_over_texture_size = Vector2(1.0f / widths[0], 1.0f / heights[0]);

			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_vertical_blur_params);

			_renderer->setViewport(*args.viewport, widths[0], heights[0]);

			rt.render_target = _bright_pass_target[0];
			rt.width         = widths[0];
			rt.height        = heights[0];

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);
		}

		//-----------------------------------------------
		// TONE MAP
		//-----------------------------------------------

		//scope_id = profiler->beginScope("tone_map");

		{
			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			_tonemap_params->setSRV(args.source, 0);
			_tonemap_params->setSRV(_avg_luminance_target_sr, 1);
			_tonemap_params->setSRV(_bright_pass_target_sr[0], 2);
			/*
			u32 offset = _tonemap_params_desc->getConstantOffset(getStringID("input_texel_size"));

			Vector2* input_texel_size = (Vector2*)pointer_math::add(_tonemap_params->getCBuffersData(), offset);

			*input_texel_size = Vector2(1.0f / 1280, 1.0f / 720);
			*/
			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _tonemap_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_tonemap_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			_renderer->setViewport(*args.viewport, args.target->width, args.target->height);
			/*
			RenderTexture rt;
			rt.render_target = _luminance_target;
			rt.width = 1280;
			rt.height = 720;

			_renderer->setRenderTarget(1, &rt, nullptr);
			*/
			_renderer->setRenderTarget(1, args.target, nullptr);

			_renderer->render(render_item);
		}

		//profiler->endScope(scope_id);

		//SWITCH AVERAGE LUMINANCE TARGETS
		auto temp1                 = _avg_luminance_target;
		_avg_luminance_target      = _prev_avg_luminance_target;
		_prev_avg_luminance_target = temp1;

		auto temp2                    = _avg_luminance_target_sr;
		_avg_luminance_target_sr      = _prev_avg_luminance_target_sr;
		_prev_avg_luminance_target_sr = temp2;
	};

	void generate(lua_State* lua_state) override final
	{
		//NOT IMPLEMENTED
	};

private:

	Renderer*           _renderer;
	Allocator*          _allocator;
	LinearAllocator*    _temp_allocator;

	RenderTargetH       _luminance_target;
	ShaderResourceH		_luminance_target_sr;

	RenderTargetH       _avg_luminance_target;
	ShaderResourceH		_avg_luminance_target_sr;

	RenderTargetH       _prev_avg_luminance_target;
	ShaderResourceH		_prev_avg_luminance_target_sr;

	RenderTargetH       _bright_pass_target[5];
	ShaderResourceH		_bright_pass_target_sr[6];

	RenderTargetH       _bloom_target[5];
	ShaderResourceH		_bloom_sr[5];

	ShaderPermutation         _luminance_shader_permutation;
	const ParameterGroupDesc* _luminance_params_desc;
	ParameterGroup*           _luminance_params;

	ShaderPermutation         _eye_adapt_shader_permutation;
	const ParameterGroupDesc* _eye_adapt_params_desc;
	ParameterGroup*           _eye_adapt_params;

	ShaderPermutation         _bright_pass_shader_permutation;
	const ParameterGroupDesc* _bright_pass_params_desc;
	ParameterGroup*           _bright_pass_params;

	ShaderPermutation         _horizontal_blur_shader_permutation;
	const ParameterGroupDesc* _horizontal_blur_params_desc;
	ParameterGroup*           _horizontal_blur_params;

	ShaderPermutation         _vertical_blur_shader_permutation;
	const ParameterGroupDesc* _vertical_blur_params_desc;
	ParameterGroup*           _vertical_blur_params;

	ShaderPermutation         _tonemap_shader_permutation;
	const ParameterGroupDesc* _tonemap_params_desc;
	ParameterGroup*           _tonemap_params;
};