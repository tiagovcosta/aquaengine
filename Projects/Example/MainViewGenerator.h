#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015              
/////////////////////////////////////////////////////////////////////////////////////////////

#include <DevTools\Profiler.h>

#include <Generators\CSMUtilties.h>
#include <Generators\ShadowMapGenerator.h>
#include <Generators\ScreenSpaceReflections.h>

#include <Generators\PostProcess\DepthOfField.h>
#include <Generators\PostProcess\ToneMapper.h>

#include <Components\VolumetricLightManager.h>
#include <Components\LightManager.h>

#include <Renderer\Renderer.h>
#include <Renderer\RendererInterfaces.h>
#include <Renderer\RendererStructs.h>
#include <Renderer\Camera.h>

#include <Utilities\StringID.h>

using namespace aqua;

class MainViewGenerator : public ResourceGenerator
{
public:
	struct Args
	{
		const Camera*        camera;
		const Viewport*      viewport;
		const RenderTexture* target;
		Vector3              sun_dir;
		Vector3              sun_color;
		ShaderResourceH		 scattering;

		//SSR args
		bool                 enable_ssr;
		float				 thickness;

		// DOF args
		float				 focus_plane_z;
		float				 dof_size;
		float				 near_blur_transition_size;
		float				 far_blur_transition_size;
		float				 near_blur_radius_fraction;
		float				 far_blur_radius_fraction;
	};

	void init(aqua::Renderer& renderer, lua_State* lua_state, Allocator& allocator, LinearAllocator& temp_allocator,
		LightManager& light_manager, u32 width, u32 height)
	{
		_renderer       = &renderer;
		_allocator      = &allocator;
		_temp_allocator = &temp_allocator;

		_light_manager = &light_manager;

		_width  = width;
		_height = height;

		Texture2DDesc texture_desc;

		//DEPTH TARGET

		texture_desc.width          = _width;
		texture_desc.height         = _height;
		texture_desc.mip_levels     = 1;
		texture_desc.array_size     = 1;
		texture_desc.format         = RenderResourceFormat::R24G8_TYPELESS;
		texture_desc.sample_count   = 1;
		texture_desc.sample_quality = 0;
		texture_desc.update_mode    = UpdateMode::GPU;
		texture_desc.generate_mips  = false;

		//_depth_target = nullptr;

		TextureViewDesc view_desc;
		view_desc.format            = RenderResourceFormat::D24_UNORM_S8_UINT;
		view_desc.most_detailed_mip = 0;

		TextureViewDesc view_desc2;
		view_desc2.format            = RenderResourceFormat::R24_UNORM_X8_TYPELESS;
		view_desc2.most_detailed_mip = 0;
		view_desc2.mip_levels        = -1;
		/*
		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 0, nullptr, 0, nullptr, 1, &view_desc,
		0, nullptr, nullptr, nullptr, _depth_target.dsvs, nullptr);
		*/
		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc2, 0, nullptr, 1, &view_desc,
													  0, nullptr, &_depth_target2_sr, nullptr, &_depth_target2, nullptr);

		//COLOR BUFFER

		texture_desc.width          = _width;
		texture_desc.height         = _height;
		texture_desc.mip_levels     = 1;
		texture_desc.array_size     = 1;
		texture_desc.format         = RenderResourceFormat::RGBA8_UNORM;
		texture_desc.sample_count   = 1;
		texture_desc.sample_quality = 0;
		texture_desc.update_mode    = UpdateMode::GPU;
		texture_desc.generate_mips  = false;

		view_desc.format            = RenderResourceFormat::RGBA8_UNORM;
		view_desc.most_detailed_mip = 0;
		view_desc.mip_levels        = -1;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc, 1, &view_desc, 0, nullptr,
													  0, nullptr, &_color_buffer_sr, &_color_buffer_rt, nullptr, nullptr);

		//NORMAL BUFFER

		texture_desc.format = RenderResourceFormat::RGBA16_UNORM;
		view_desc.format    = RenderResourceFormat::RGBA16_UNORM;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc, 1, &view_desc, 0, nullptr,
													  0, nullptr, &_normal_buffer_sr, &_normal_buffer_rt, nullptr, nullptr);

		// SSAO BUFFER
		texture_desc.format = RenderResourceFormat::R8_UNORM;
		view_desc.format    = RenderResourceFormat::R8_UNORM;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc, 1, &view_desc, 0, nullptr,
													  0, nullptr, &_ssao_buffer_sr, &_ssao_buffer_rt, nullptr, nullptr);

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc, 1, &view_desc, 0, nullptr,
													  0, nullptr, &_ssao_buffer2_sr, &_ssao_buffer2_rt, nullptr, nullptr);

		//LIGHTING BUFFER

		texture_desc.width          = _width;
		texture_desc.height         = _height;
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

		//DOF BUFFER

		texture_desc.width          = _width;
		texture_desc.height         = _height;
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
													  0, nullptr, &_dof_sr, &_dof_rt, nullptr, nullptr);

		//SSAO
		auto ssao_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/ssao.cshader"));
		_ssao_shader_permutation = ssao_shader->getPermutation(0);

		auto ssao_params_desc_set = ssao_shader->getInstanceParameterGroupDescSet();
		_ssao_params_desc         = getParameterGroupDesc(*ssao_params_desc_set, 0);

		_ssao_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																		  *_ssao_params_desc, UINT32_MAX, 0, nullptr);
		_ssao_params->setSRV(_normal_buffer_sr, 0);
		_ssao_params->setSRV(_depth_target2_sr, 1);

		//SSAO blur
		auto blur_shader              = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/blur.cshader"));
		_ssao_blur_shader_permutation = blur_shader->getPermutation(0);

		auto ssao_blur_params_desc_set = blur_shader->getInstanceParameterGroupDescSet();
		_ssao_blur_params_desc         = getParameterGroupDesc(*ssao_blur_params_desc_set, 0);

		_ssao_blur_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																			   *_ssao_blur_params_desc, UINT32_MAX, 0, nullptr);
		_ssao_blur_params->setSRV(_ssao_buffer_sr, 0);

		//Apply Gamma
		auto apply_gamma_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/apply_gamma.cshader"));
		_apply_gamma_shader_permutation = apply_gamma_shader->getPermutation(0);

		auto apply_gamma_params_desc_set = apply_gamma_shader->getInstanceParameterGroupDescSet();
		_apply_gamma_params_desc         = getParameterGroupDesc(*apply_gamma_params_desc_set, 0);

		_apply_gamma_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																				 *_apply_gamma_params_desc, UINT32_MAX, 0, nullptr);

		//_apply_gamma_params->setSRV(_color_buffer_sr, 0);
		//_apply_gamma_params->setSRV(_normal_buffer_sr, 0);
		_apply_gamma_params->setSRV(_lighting_buffer_sr, 0);

		//---------------------------------------------------------------------------
		//---------------------------------------------------------------------------

		_csm = CSMUtilities::createShadowMap(*_allocator, *_renderer, 2048, 4);
	};

	void shutdown()
	{
		CSMUtilities::destroyShadowMap(*_allocator, _csm);

		RenderDevice& render_device = *_renderer->getRenderDevice();

		render_device.deleteParameterGroup(*_allocator, *_apply_gamma_params);
		render_device.deleteParameterGroup(*_allocator, *_ssao_blur_params);
		render_device.deleteParameterGroup(*_allocator, *_ssao_params);

		RenderDevice::release(_dof_rt);
		RenderDevice::release(_dof_sr);

		RenderDevice::release(_lighting_buffer_uav);
		RenderDevice::release(_lighting_buffer_rt);
		RenderDevice::release(_lighting_buffer_sr);

		RenderDevice::release(_ssao_buffer2_rt);
		RenderDevice::release(_ssao_buffer2_sr);

		RenderDevice::release(_ssao_buffer_rt);
		RenderDevice::release(_ssao_buffer_sr);

		RenderDevice::release(_normal_buffer_rt);
		RenderDevice::release(_normal_buffer_sr);

		RenderDevice::release(_color_buffer_rt);
		RenderDevice::release(_color_buffer_sr);

		RenderDevice::release(_depth_target2_sr);
		RenderDevice::release(_depth_target2);
	};

	void setSunLightDir(const Vector3& dir)
	{
		_sun_light_dir = dir;
	}

	u32 getSecondaryViews(const Camera& camera, RenderView* out_views) override final
	{/*
		_csm.cascades_end[0] = 0.001f;
		_csm.cascades_end[1] = 0.005f;
		_csm.cascades_end[2] = 0.01f;
		//_csm.cascades_end[3] = 0.02f;
		_csm.cascades_end[3] = 1.00f;
		*/
		_csm.cascades_end[0] = 0.025f;
		_csm.cascades_end[1] = 0.05f;
		_csm.cascades_end[2] = 0.50f;
		_csm.cascades_end[3] = 1.00f;

		CSMUtilities::updateCascadedShadowMap(_csm, camera, _sun_light_dir);

		for(u32 i = 0; i < _csm.num_cascades; i++)
		{
			out_views[i].camera         = _csm.cascades_cameras[i];
			out_views[i].generator_name = getStringID("shadow_map");

			//ShadowMapGenerator::Args args;
			_csm_args[i].dsv               = _csm.dsv;
			_csm_args[i].shadow_map_width  = _csm.cascade_size * _csm.num_cascades;
			_csm_args[i].shadow_map_height = _csm.cascade_size;
			_csm_args[i].viewport          = &_csm.cascades_viewports[i];

			out_views[i].generator_args = &_csm_args[i];
		}

		_renderer->getRenderDevice()->clearDepthStencilTarget(_csm.dsv, 1.0f);

		_light_manager->setShadowsParams(_csm.srv, _csm.cascades_view_proj, _csm.cascades_end);

		return _csm.num_cascades;

		//return 0;
	}

	void generate(const void* args_, const VisibilityData* visibility) override final
	{
		Profiler* profiler = _renderer->getProfiler();

		u32 scope_id;

		const Args& args = *(const Args*)args_;

		float color_clear[]  = { 0.0f, 0.0f, 0.0f, 0.0f };
		float normal_clear[] = { 0.5f, 0.5f, 0.5f, 0.0f };

		_renderer->getRenderDevice()->clearDepthStencilTarget(_depth_target2, 1.0f);
		_renderer->getRenderDevice()->clearRenderTarget(_color_buffer_rt, color_clear);
		_renderer->getRenderDevice()->clearRenderTarget(_normal_buffer_rt, normal_clear);
		/*
		const Matrix4x4& view_proj = args.camera->getViewProj();
		const Matrix4x4& inv_view = args.camera->getView().Invert();
		const Vector3& camera_position = args.camera->getPosition();

		_renderer->setViewParameter(getStringID("inv_view"), &inv_view, sizeof(inv_view));
		_renderer->setViewParameter(getStringID("view_proj"), &view_proj, sizeof(view_proj));
		_renderer->setViewParameter(getStringID("camera_position"), &camera_position, sizeof(camera_position));

		_renderer->bindFrameParameters();
		_renderer->bindViewParameters();
		*/
		//BUILD QUEUES

		u32 passes_names[] = { getStringID("gbuffer"),
							   getStringID("gbuffer_alpha_masked"),
							   getStringID("skydome"),
							   getStringID("debug") };

		enum class PassNameIndex : u8
		{
			GBUFFER,
			GBUFFER_ALPHA_MASKED,
			SKYDOOME,
			DEBUG,
			NUM_PASSES
		};

		RenderQueue queues[(u8)PassNameIndex::NUM_PASSES];

		_renderer->buildRenderQueues((u8)PassNameIndex::NUM_PASSES, passes_names, visibility, queues);

		u8 pass_index;

		//-----------------------------------------------
		//GBuffer pass
		//-----------------------------------------------

		scope_id = profiler->beginScope("gbuffer");

		RenderTexture gbuffer[] = { { _color_buffer_rt, nullptr, _width, _height, 1 },
									{ _normal_buffer_rt, nullptr, _width, _height, 1 } };

		_renderer->setViewport(*args.viewport, _width, _height);
		_renderer->setRenderTarget(2, gbuffer, _depth_target2);

		pass_index = _renderer->getShaderManager()->getPassIndex(passes_names[(u8)PassNameIndex::GBUFFER]);

		_renderer->render(pass_index, queues[(u8)PassNameIndex::GBUFFER]);

		profiler->endScope(scope_id);

		//-----------------------------------------------
		//GBuffer Alpha Masked pass
		//-----------------------------------------------

		scope_id = profiler->beginScope("gbuffer_alpha_masked");

		pass_index = _renderer->getShaderManager()->getPassIndex(passes_names[(u8)PassNameIndex::GBUFFER_ALPHA_MASKED]);

		_renderer->render(pass_index, queues[(u8)PassNameIndex::GBUFFER_ALPHA_MASKED]);

		profiler->endScope(scope_id);

		//-----------------------------------------------
		//SSAO
		//-----------------------------------------------

		scope_id = profiler->beginScope("ssao");

		{
			auto scope_id = profiler->beginScope("generate");

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

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

			_renderer->setViewport(*args.viewport, args.target->width, args.target->height);

			RenderTexture rt;
			rt.render_target = _ssao_buffer_rt;
			rt.width         = _width;
			rt.height        = _height;

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);

			profiler->endScope(scope_id);
		}

		{
			auto scope_id = profiler->beginScope("blur");

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			u32 offset = _ssao_blur_params_desc->getConstantOffset(getStringID("one_over_texture_size"));

			Vector2* one_over_texture_size = (Vector2*)pointer_math::add(_ssao_blur_params->getCBuffersData(), offset);

			*one_over_texture_size = Vector2(1.0f / args.target->width, 1.0f / args.target->height);

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _ssao_blur_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_ssao_blur_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			_renderer->setViewport(*args.viewport, args.target->width, args.target->height);

			RenderTexture rt;
			rt.render_target = _ssao_buffer2_rt;
			rt.width         = _width;
			rt.height        = _height;

			_renderer->setRenderTarget(1, &rt, nullptr);

			_renderer->render(render_item);

			profiler->endScope(scope_id);
		}

		profiler->endScope(scope_id);

		//-----------------------------------------------
		// VOLUMETRIC LIGHTS
		//-----------------------------------------------

		{
			scope_id = profiler->beginScope("volumetric_lights");

			VolumetricLightManager::Args vl_args;
			vl_args.camera            = args.camera;
			vl_args.viewport          = args.viewport;
			vl_args.target            = nullptr;
			vl_args.input_depth       = _depth_target2_sr;
			vl_args.shadow_map        = _csm.srv;
			vl_args.light_dir         = _sun_light_dir;
			//vl_args.light_color     = Vector3(20.0f, 20.0f, 20.0f);
			vl_args.light_color       = args.sun_color;
			vl_args.cascades_matrices = _csm.cascades_view_proj;
			vl_args.cascades_splits   = _csm.cascades_end;

			_renderer->generateResource(getStringID("volumetric_lights"), &vl_args, nullptr);

			profiler->endScope(scope_id);
		}

		//-----------------------------------------------
		//Tiled lighting pass
		//-----------------------------------------------

		//_renderer->setRenderTarget(*args.viewport, 0, nullptr, nullptr);

		_renderer->getRenderDevice()->unbindResources();

		_renderer->bindFrameParameters();
		_renderer->bindViewParameters();

		RenderTexture li;
		li.uav = _lighting_buffer_uav;
		li.width = _width;
		li.height = _height;
		li.depth = 1;

		LightManager::Args lighting_args;
		lighting_args.camera              = args.camera;
		lighting_args.viewport            = args.viewport;
		lighting_args.target              = &li;
		lighting_args.color_buffer        = _color_buffer_sr;
		lighting_args.normal_buffer       = _normal_buffer_sr;
		lighting_args.depth_buffer        = _depth_target2_sr;
		lighting_args.ssao_buffer         = _ssao_buffer2_sr;
		lighting_args.scattering_buffer   = args.scattering;
		//lighting_args.scattering_buffer = nullptr;

		scope_id = profiler->beginScope("tiled_deferred");

		_renderer->generateResource(getStringID("light_generator"), &lighting_args, nullptr);

		profiler->endScope(scope_id);

		//-----------------------------------------------
		//Skydome pass
		//-----------------------------------------------

		scope_id = profiler->beginScope("skydome");

		RenderTexture kkk = { _lighting_buffer_rt, nullptr, _width, _height, 1 };

		//_renderer->setRenderTarget(*args.viewport, 1, args.target, _depth_target2);
		_renderer->setViewport(*args.viewport, _width, _height);
		_renderer->setRenderTarget(1, &kkk, _depth_target2);

		pass_index = _renderer->getShaderManager()->getPassIndex(passes_names[(u8)PassNameIndex::SKYDOOME]);

		_renderer->render(pass_index, queues[(u8)PassNameIndex::SKYDOOME]);

		profiler->endScope(scope_id);

		//-----------------------------------------------
		//Transparency
		//-----------------------------------------------

		//-----------------------------------------------
		//Post process
		//-----------------------------------------------

		//scope_id = profiler->beginScope("post_process");

		/*
		DrawCall dc = createDrawCall(false, 3, 0, 0);

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		RenderItem render_item;
		render_item.draw_call         = &dc;
		render_item.num_instances     = 1;
		//render_item.shader          = _ssao_shader_permutation[0];
		//render_item.instance_params = _ssao_params;
		render_item.shader            = _apply_gamma_shader_permutation[0];
		render_item.instance_params   = _apply_gamma_params;
		render_item.material_params   = nullptr;
		render_item.mesh              = &mesh;

		_renderer->setViewport(*args.viewport, args.target->width, args.target->height);
		_renderer->setRenderTarget(1, args.target, nullptr);

		_renderer->render(render_item);
		*/

		//-------------------------------------------------------------

		ShaderResourceH ssr_output;

		if(args.enable_ssr)
		{
			scope_id = profiler->beginScope("screen_space_reflections");

			ScreenSpaceReflections::Args ssr_args;
			ssr_args.camera            = args.camera;
			ssr_args.color_texture     = _lighting_buffer_sr;
			ssr_args.normal_texture    = _normal_buffer_sr;
			ssr_args.depth_texture     = _depth_target2_sr;
			ssr_args.material_texture  = _color_buffer_sr;
			ssr_args.viewport          = args.viewport;
			ssr_args.thickness         = args.thickness;
			ssr_args.output            = &ssr_output;

			_renderer->generateResource(getStringID("screen_space_reflections"), &ssr_args, nullptr);

			profiler->endScope(scope_id);
		}
		else
		{
			ssr_output = _lighting_buffer_sr;
		}
		//-------------------------------------------------------------

		scope_id = profiler->beginScope("depth_of_field");

		RenderTexture dof;
		dof.render_target = _dof_rt;
		dof.width         = _width;
		dof.height        = _height;
		dof.depth         = 1;

		DepthOfField::Args dof_args;
		dof_args.camera                      = args.camera;
		dof_args.color_texture               = ssr_output;
		dof_args.depth_texture               = _depth_target2_sr;
		dof_args.target                      = &dof;
		dof_args.viewport                    = args.viewport;
		//dof_args.near_blurry_plane_z       = 1.11f;
		//dof_args.near_sharp_plane_z        = 2.22f;
		//dof_args.far_sharp_plane_z         = 100005.0f;
		//dof_args.far_blurry_plane_z        = 1000010.0f;
		//dof_args.near_blur_radius_fraction = 1.5f * 0.01f;
		//dof_args.far_blur_radius_fraction  = 1.0f * 0.01f;
		dof_args.focus_plane_z               = args.focus_plane_z;
		dof_args.dof_size                    = args.dof_size;
		dof_args.near_blur_transition_size   = args.near_blur_transition_size;
		dof_args.far_blur_transition_size    = args.far_blur_transition_size;
		dof_args.near_blur_radius_fraction   = args.near_blur_radius_fraction;
		dof_args.far_blur_radius_fraction    = args.far_blur_radius_fraction;

		_renderer->generateResource(getStringID("depth_of_field"), &dof_args, nullptr);

		profiler->endScope(scope_id);

		//-------------------------------------------------------------

		scope_id = profiler->beginScope("tonemap");

		ToneMapper::Args tone_map_args;
		tone_map_args.source   = _dof_sr;
		tone_map_args.target   = args.target;
		tone_map_args.viewport = args.viewport;

		_renderer->generateResource(getStringID("tone_mapper"), &tone_map_args, nullptr);

		profiler->endScope(scope_id);

		//-----------------------------------------------
		//Debug pass
		//-----------------------------------------------
		/*
		pass_index = _renderer->getShaderManager()->getPassIndex(passes_names[(u8)PassNameIndex::DEBUG]);

		float aspect = (float)args.target->width / args.target->height;

		_renderer->setPassParameter(pass_index, getStringID("aspect"), &aspect, sizeof(aspect));

		//_renderer->getRenderDevice()->clearDepthStencilTarget(_depth_target2, 1.0f);

		_renderer->setViewport(*args.viewport, args.target->width, args.target->height);
		_renderer->setRenderTarget(1, args.target, _depth_target2);

		_renderer->render(pass_index, queues[(u8)PassNameIndex::DEBUG]);
		*/
	};

	void generate(lua_State* lua_state) override final
	{
		//NOT IMPLEMENTED
	};

private:

	Renderer*           _renderer;
	Allocator*          _allocator;
	LinearAllocator*    _temp_allocator;

	u32 _width;
	u32 _height;

	RenderTextureX      _depth_target;
	DepthStencilTargetH _depth_target2;
	ShaderResourceH		_depth_target2_sr;

	RenderTargetH       _color_buffer_rt;
	ShaderResourceH		_color_buffer_sr;

	RenderTargetH       _normal_buffer_rt;
	ShaderResourceH		_normal_buffer_sr;

	RenderTargetH       _ssao_buffer_rt;
	ShaderResourceH		_ssao_buffer_sr;

	RenderTargetH       _ssao_buffer2_rt;
	ShaderResourceH		_ssao_buffer2_sr;

	UnorderedAccessH    _lighting_buffer_uav;
	RenderTargetH		_lighting_buffer_rt;
	ShaderResourceH		_lighting_buffer_sr;

	RenderTargetH       _dof_rt;
	ShaderResourceH		_dof_sr;

	ShaderPermutation         _ssao_shader_permutation;
	const ParameterGroupDesc* _ssao_params_desc;
	ParameterGroup*           _ssao_params;

	ShaderPermutation         _ssao_blur_shader_permutation;
	const ParameterGroupDesc* _ssao_blur_params_desc;
	ParameterGroup*           _ssao_blur_params;

	ShaderPermutation         _apply_gamma_shader_permutation;
	const ParameterGroupDesc* _apply_gamma_params_desc;
	ParameterGroup*           _apply_gamma_params;

	CSMUtilities::CascadedShadowMap _csm;
	ShadowMapGenerator::Args _csm_args[4];

	LightManager* _light_manager;

	Vector3       _sun_light_dir;
};