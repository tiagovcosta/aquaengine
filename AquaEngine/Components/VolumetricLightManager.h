#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015              
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\DevTools\Profiler.h"

#include "LightManager.h"

#include "..\Renderer\Renderer.h"
#include "..\Renderer\RendererInterfaces.h"
#include "..\Renderer\RendererStructs.h"
#include "..\Renderer\Camera.h"

#include "..\Utilities\StringID.h"

using namespace aqua;

class VolumetricLightManager : public ResourceGenerator
{
public:
	struct Args
	{
		const Camera*        camera;
		const Viewport*      viewport;
		const RenderTexture* target;
		ShaderResourceH      input_depth;
		ShaderResourceH      shadow_map;
		Vector3				 light_dir;
		Vector3				 light_color;
		const Matrix4x4*	 cascades_matrices;
		float*				 cascades_splits;
	};

	void init(aqua::Renderer& renderer, lua_State* lua_state, Allocator& allocator, LinearAllocator& temp_allocator,
		LightManager& light_manager)
	{
		_renderer       = &renderer;
		_allocator      = &allocator;
		_temp_allocator = &temp_allocator;

		_light_manager = &light_manager;

		Texture2DDesc   texture_desc;
		TextureViewDesc view_desc;

		//DOWNSCALED TEXTURE

		texture_desc.width          = 1280 / 2;
		texture_desc.height         = 720 / 2;
		texture_desc.mip_levels     = 1;
		texture_desc.array_size     = 1;
		texture_desc.format         = RenderResourceFormat::R16_UNORM;
		texture_desc.sample_count   = 1;
		texture_desc.sample_quality = 0;
		texture_desc.update_mode    = UpdateMode::GPU;
		texture_desc.generate_mips  = false;

		view_desc.format            = RenderResourceFormat::R16_UNORM;
		view_desc.most_detailed_mip = 0;
		view_desc.mip_levels        = -1;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc, 1, &view_desc, 0, nullptr,
			0, nullptr, &_downscaled_sr, &_downscaled_rt, nullptr, nullptr);

		// ACCUMULATION TEXTURE

		texture_desc.format = RenderResourceFormat::RGBA16_FLOAT;
		view_desc.format    = RenderResourceFormat::RGBA16_FLOAT;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc, 1, &view_desc, 0, nullptr,
			0, nullptr, &_accumulation_sr, &_accumulation_rt, nullptr, nullptr);

		// HORIZONTAL BLUR TEXTURE

		texture_desc.format = RenderResourceFormat::RGBA16_FLOAT;
		view_desc.format    = RenderResourceFormat::RGBA16_FLOAT;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc, 1, &view_desc, 0, nullptr,
			0, nullptr, &_horizontal_blur_sr, &_horizontal_blur_rt, nullptr, nullptr);

		// VERTICAL BLUR TEXTURE

		texture_desc.format = RenderResourceFormat::RGBA16_FLOAT;
		view_desc.format    = RenderResourceFormat::RGBA16_FLOAT;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc, 1, &view_desc, 0, nullptr,
			0, nullptr, &_vertical_blur_sr, &_vertical_blur_rt, nullptr, nullptr);

		/*
		//LIGHTING BUFFER

		texture_desc.width          = 1280;
		texture_desc.height         = 720;
		texture_desc.mip_levels     = 1;
		texture_desc.array_size     = 1;
		texture_desc.format         = RenderResourceFormat::RGBA16_FLOAT;
		texture_desc.sample_count   = 1;
		texture_desc.sample_quality = 0;
		texture_desc.update_mode    = UpdateMode::GPU;
		texture_desc.generate_mips  = false;

		view_desc.format            = RenderResourceFormat::RGBA16_FLOAT;
		view_desc.most_detailed_mip = 0;
		view_desc.mip_levels        = -1;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc, 1, &view_desc, 0, nullptr,
			1, &view_desc, &_lighting_buffer_sr, &_lighting_buffer_rt, nullptr, &_lighting_buffer_uav);
		*/
		//SHADERS

		//DOWNSCALE
		auto downscale_shader = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/downscale.cshader"));
		_downscale_shader_permutation = downscale_shader->getPermutation(0);

		auto downscale_params_desc_set = downscale_shader->getInstanceParameterGroupDescSet();
		_downscale_params_desc = getParameterGroupDesc(*downscale_params_desc_set, 0);

		_downscale_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																			   *_downscale_params_desc, UINT32_MAX, 0, nullptr);

		// ACCUMULATE
		auto accumulation_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/accumulation.cshader"));
		_accumulation_shader_permutation = accumulation_shader->getPermutation(0);

		auto accumulation_params_desc_set = accumulation_shader->getInstanceParameterGroupDescSet();
		_accumulation_params_desc         = getParameterGroupDesc(*accumulation_params_desc_set, 0);

		_accumulation_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																				  *_accumulation_params_desc, UINT32_MAX, 0, nullptr);
		_accumulation_params->setSRV(_downscaled_sr, 0);

		// HORIZONTAL BLUR
		auto horizontal_blur_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/bilateral_blur.cshader"));
		_horizontal_blur_shader_permutation = horizontal_blur_shader->getPermutation(0);

		auto horizontal_blur_params_desc_set = horizontal_blur_shader->getInstanceParameterGroupDescSet();
		_horizontal_blur_params_desc         = getParameterGroupDesc(*horizontal_blur_params_desc_set, 0);

		_horizontal_blur_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																					 *_horizontal_blur_params_desc, UINT32_MAX, 0, nullptr);
		_horizontal_blur_params->setSRV(_accumulation_sr, 0);
		_horizontal_blur_params->setSRV(_downscaled_sr, 1);
	};

	void shutdown()
	{
		RenderDevice& render_device = *_renderer->getRenderDevice();

		render_device.deleteParameterGroup(*_allocator, *_horizontal_blur_params);
		render_device.deleteParameterGroup(*_allocator, *_accumulation_params);
		render_device.deleteParameterGroup(*_allocator, *_downscale_params);

		RenderDevice::release(_vertical_blur_sr);
		RenderDevice::release(_vertical_blur_rt);

		RenderDevice::release(_horizontal_blur_sr);
		RenderDevice::release(_horizontal_blur_rt);

		RenderDevice::release(_accumulation_sr);
		RenderDevice::release(_accumulation_rt);

		RenderDevice::release(_downscaled_sr);
		RenderDevice::release(_downscaled_rt);
	};

	ShaderResourceH get() const
	{
		return _vertical_blur_sr;
	}

	void setSunLightDir(const Vector3& dir)
	{
		_sun_light_dir = dir;
	}

	u32 getSecondaryViews(const Camera& camera, RenderView* out_views) override final
	{
		return 0;
	}

	void generate(const void* args_, const VisibilityData* visibility) override final
	{
		Profiler* profiler = _renderer->getProfiler();

		u32 scope_id;

		const Args& args = *(const Args*)args_;
		/*
		float color_clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float normal_clear[] = { 0.5f, 0.5f, 0.5f, 0.0f };
		
		_renderer->getRenderDevice()->clearDepthStencilTarget(_depth_target2, 1.0f);
		_renderer->getRenderDevice()->clearRenderTarget(_color_buffer_rt, color_clear);
		_renderer->getRenderDevice()->clearRenderTarget(_normal_buffer_rt, normal_clear);
		*/

		//-----------------------------------------------
		// DOWNSCALE DEPTH
		//-----------------------------------------------

		scope_id = profiler->beginScope("downscale_depth");

		{
			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			_downscale_params->setSRV(args.input_depth, 0);

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _downscale_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_downscale_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			//_renderer->setViewport(*args.viewport, args.target->width, args.target->height);
			_renderer->setViewport(*args.viewport, 1280 / 2, 720 / 2);

			RenderTexture rt;
			rt.render_target = _downscaled_rt;
			rt.width         = 1280 / 2;
			rt.height        = 720 / 2;

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);
		}

		profiler->endScope(scope_id);

		//-----------------------------------------------
		// ACCUMULATION
		//-----------------------------------------------

		scope_id = profiler->beginScope("accumulation");

		{
			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			//Update params

			u8 index = _accumulation_params_desc->getSRVIndex(getStringID("shadow_map"));

			_accumulation_params->setSRV(args.shadow_map, index);

			void* cbuffers_data = _accumulation_params->getCBuffersData();

			u32 offset = _accumulation_params_desc->getConstantOffset(getStringID("light_dir"));

			*((Vector3*)pointer_math::add(cbuffers_data, offset)) = args.light_dir;

			offset = _accumulation_params_desc->getConstantOffset(getStringID("light_color"));

			*((Vector3*)pointer_math::add(cbuffers_data, offset)) = args.light_color;

			offset = _accumulation_params_desc->getConstantOffset(getStringID("shadow_matrices"));

			Matrix4x4* shadow_matrices = (Matrix4x4*)pointer_math::add(cbuffers_data, offset);

			memcpy(shadow_matrices, args.cascades_matrices, sizeof(Matrix4x4) * 4);

			offset = _accumulation_params_desc->getConstantOffset(getStringID("cascade_splits"));

			float* ce = (float*)pointer_math::add(cbuffers_data, offset);

			memcpy(ce, args.cascades_splits, sizeof(float) * 4);

			//---------------------------
			
			DrawCall dc = createDrawCall(false, 3, 0, 0);

			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _accumulation_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_accumulation_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			//_renderer->setViewport(*args.viewport, args.target->width, args.target->height);
			_renderer->setViewport(*args.viewport, 1280 / 2, 720 / 2);

			RenderTexture rt;
			rt.render_target = _accumulation_rt;
			rt.width         = 1280 / 2;
			rt.height        = 720 / 2;

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);
		}

		profiler->endScope(scope_id);

		//-----------------------------------------------
		// HORIZONTAL BLUR
		//-----------------------------------------------

		scope_id = profiler->beginScope("horizontal_blur");

		{
			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			//Update params

			_horizontal_blur_params->setSRV(_accumulation_sr, 0);

			void* cbuffers_data = _horizontal_blur_params->getCBuffersData();

			u32 offset = _horizontal_blur_params_desc->getConstantOffset(getStringID("one_over_texture_size"));

			*((Vector2*)pointer_math::add(cbuffers_data, offset)) = Vector2(1.0f / 1280 * 2, 1.0f / 720 * 2);

			//---------------------------

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _horizontal_blur_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_horizontal_blur_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			//_renderer->setViewport(*args.viewport, args.target->width, args.target->height);
			_renderer->setViewport(*args.viewport, 1280 / 2, 720 / 2);

			RenderTexture rt;
			rt.render_target = _horizontal_blur_rt;
			rt.width         = 1280 / 2;
			rt.height        = 720 / 2;

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);
		}

		profiler->endScope(scope_id);

		//-----------------------------------------------
		// VERTICAL BLUR
		//-----------------------------------------------

		scope_id = profiler->beginScope("vertical_blur");

		{
			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			//Update params

			_horizontal_blur_params->setSRV(_horizontal_blur_sr, 0);

			void* cbuffers_data = _horizontal_blur_params->getCBuffersData();

			u32 offset = _horizontal_blur_params_desc->getConstantOffset(getStringID("one_over_texture_size"));

			*((Vector2*)pointer_math::add(cbuffers_data, offset)) = Vector2(1.0f / 1280 * 2, 1.0f / 720 * 2);

			//---------------------------

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _horizontal_blur_shader_permutation[1];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_horizontal_blur_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			//_renderer->setViewport(*args.viewport, args.target->width, args.target->height);
			_renderer->setViewport(*args.viewport, 1280 / 2, 720 / 2);

			RenderTexture rt;
			rt.render_target = _vertical_blur_rt;
			rt.width         = 1280 / 2;
			rt.height        = 720 / 2;

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);
		}

		profiler->endScope(scope_id);
	};

	void generate(lua_State* lua_state) override final
	{
		//NOT IMPLEMENTED
	};

private:

	Renderer*           _renderer;
	Allocator*          _allocator;
	LinearAllocator*    _temp_allocator;

	RenderTargetH       _downscaled_rt;
	ShaderResourceH		_downscaled_sr;

	RenderTargetH       _accumulation_rt;
	ShaderResourceH		_accumulation_sr;

	RenderTargetH       _horizontal_blur_rt;
	ShaderResourceH		_horizontal_blur_sr;

	RenderTargetH       _vertical_blur_rt;
	ShaderResourceH		_vertical_blur_sr;

	ShaderPermutation         _downscale_shader_permutation;
	const ParameterGroupDesc* _downscale_params_desc;
	ParameterGroup*           _downscale_params;

	ShaderPermutation         _accumulation_shader_permutation;
	const ParameterGroupDesc* _accumulation_params_desc;
	ParameterGroup*           _accumulation_params;

	ShaderPermutation         _horizontal_blur_shader_permutation;
	const ParameterGroupDesc* _horizontal_blur_params_desc;
	ParameterGroup*           _horizontal_blur_params;

	LightManager* _light_manager;

	Vector3       _sun_light_dir;
};