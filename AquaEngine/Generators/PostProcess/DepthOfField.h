#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015              
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\..\DevTools\Profiler.h"

#include "..\..\Renderer\Renderer.h"
#include "..\..\Renderer\RendererInterfaces.h"
#include "..\..\Renderer\RendererStructs.h"
#include "..\..\Renderer\Camera.h"

#include "..\..\Utilities\StringID.h"

#include "..\..\AquaTypes.h"

#include <cmath>

using namespace aqua;

class DepthOfField : public ResourceGenerator
{
public:
	struct Args
	{
		const Camera*		 camera;
		const Viewport*		 viewport;
		ShaderResourceH		 color_texture;
		ShaderResourceH		 depth_texture;
		const RenderTexture* target;
		float				 focus_plane_z;
		float				 dof_size;
		float				 near_blur_transition_size;
		float				 far_blur_transition_size;
		float				 near_blur_radius_fraction;
		float				 far_blur_radius_fraction;
	};

	void init(aqua::Renderer& renderer, lua_State* lua_state, Allocator& allocator, LinearAllocator& temp_allocator,
			  u32 width, u32 height)
	{
		_renderer       = &renderer;
		_allocator      = &allocator;
		_temp_allocator = &temp_allocator;

		_width  = width;
		_height = height;

		//------------------------------------------------------------------------------------------------
		// TEXTURES
		//------------------------------------------------------------------------------------------------

		Texture2DDesc texture_desc;
		texture_desc.width          = _width;
		texture_desc.height         = _height;
		texture_desc.mip_levels     = 1;
		texture_desc.array_size     = 1;
		texture_desc.format         = RenderResourceFormat::RGBA16_FLOAT;
		texture_desc.sample_count   = 1;
		texture_desc.sample_quality = 0;
		texture_desc.update_mode    = UpdateMode::GPU;
		texture_desc.generate_mips  = false;

		TextureViewDesc rt_view_desc;
		rt_view_desc.format            = RenderResourceFormat::RGBA16_FLOAT;
		rt_view_desc.most_detailed_mip = 0;
		rt_view_desc.mip_levels        = 1;

		TextureViewDesc sr_view_desc;
		sr_view_desc.format            = RenderResourceFormat::RGBA16_FLOAT;
		sr_view_desc.most_detailed_mip = 0;
		sr_view_desc.mip_levels        = -1;

		_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
			0, nullptr, &_color_coc_target_sr, &_color_coc_target, nullptr, nullptr);

		{
			Texture2DDesc texture_desc;
			texture_desc.width          = _width / 2;
			texture_desc.height         = _height;
			texture_desc.mip_levels     = 1;
			texture_desc.array_size     = 1;
			texture_desc.format         = RenderResourceFormat::RGBA16_FLOAT;
			texture_desc.sample_count   = 1;
			texture_desc.sample_quality = 0;
			texture_desc.update_mode    = UpdateMode::GPU;
			texture_desc.generate_mips  = false;

			TextureViewDesc rt_view_desc;
			rt_view_desc.format            = RenderResourceFormat::RGBA16_FLOAT;
			rt_view_desc.most_detailed_mip = 0;
			rt_view_desc.mip_levels        = 1;

			TextureViewDesc sr_view_desc;
			sr_view_desc.format            = RenderResourceFormat::RGBA16_FLOAT;
			sr_view_desc.most_detailed_mip = 0;
			sr_view_desc.mip_levels        = -1;

			_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
				0, nullptr, &_temp_near_target_sr, &_temp_near_target, nullptr, nullptr);

			_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
				0, nullptr, &_temp_blur_target_sr, &_temp_blur_target, nullptr, nullptr);
		}

		{
			Texture2DDesc texture_desc;
			texture_desc.width          = _width / 2;
			texture_desc.height         = _height / 2;
			texture_desc.mip_levels     = 1;
			texture_desc.array_size     = 1;
			texture_desc.format         = RenderResourceFormat::RGBA16_FLOAT;
			texture_desc.sample_count   = 1;
			texture_desc.sample_quality = 0;
			texture_desc.update_mode    = UpdateMode::GPU;
			texture_desc.generate_mips  = false;

			TextureViewDesc rt_view_desc;
			rt_view_desc.format            = RenderResourceFormat::RGBA16_FLOAT;
			rt_view_desc.most_detailed_mip = 0;
			rt_view_desc.mip_levels        = 1;

			TextureViewDesc sr_view_desc;
			sr_view_desc.format            = RenderResourceFormat::RGBA16_FLOAT;
			sr_view_desc.most_detailed_mip = 0;
			sr_view_desc.mip_levels        = -1;

			_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
														  0, nullptr, &_near_target_sr, &_near_target, nullptr, nullptr);

			_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &sr_view_desc, 1, &rt_view_desc, 0, nullptr,
														  0, nullptr, &_blur_target_sr, &_blur_target, nullptr, nullptr);
		}
		//------------------------------------------------------------------------------------------------
		// SHADERS
		//------------------------------------------------------------------------------------------------

		// Color + CoC
		auto color_coc_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/color_coc.cshader"));
		_color_coc_shader_permutation = color_coc_shader->getPermutation(0);

		auto color_coc_params_desc_set = color_coc_shader->getInstanceParameterGroupDescSet();
		_color_coc_params_desc         = getParameterGroupDesc(*color_coc_params_desc_set, 0);

		_color_coc_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																			   *_color_coc_params_desc, UINT32_MAX, 0, nullptr);

		// Blur
		auto blur_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/dof_blur.cshader"));
		_blur_shader_permutation = blur_shader->getPermutation(0);

		auto blur_params_desc_set = blur_shader->getInstanceParameterGroupDescSet();
		_blur_params_desc         = getParameterGroupDesc(*blur_params_desc_set, 0);

		_blur_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																		  *_blur_params_desc, UINT32_MAX, 0, nullptr);

		_blur_params->setSRV(_color_coc_target_sr, 0);
		//_blur_params->setSRV(_color_coc_target_sr, 1);

		_vertical_blur_shader_permutation = blur_shader->getPermutation(1);

		_vertical_blur_params_desc = getParameterGroupDesc(*blur_params_desc_set, 1);

		_vertical_blur_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																				   *_vertical_blur_params_desc, UINT32_MAX, 0, nullptr);

		_vertical_blur_params->setSRV(_temp_blur_target_sr, 0);
		_vertical_blur_params->setSRV(_temp_near_target_sr, 1);

		// Composite
		auto composite_shader         = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/dof_composite.cshader"));
		_composite_shader_permutation = composite_shader->getPermutation(0);

		auto composite_params_desc_set = composite_shader->getInstanceParameterGroupDescSet();
		_composite_params_desc         = getParameterGroupDesc(*composite_params_desc_set, 0);

		_composite_params = _renderer->getRenderDevice()->createParameterGroup(*_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																			   *_composite_params_desc, UINT32_MAX, 0, nullptr);

		_composite_params->setSRV(_color_coc_target_sr, 0);
		_composite_params->setSRV(_blur_target_sr, 1);
		_composite_params->setSRV(_near_target_sr, 2);
	};

	void shutdown()
	{
		RenderDevice& render_device = *_renderer->getRenderDevice();

		render_device.deleteParameterGroup(*_allocator, *_composite_params);
		render_device.deleteParameterGroup(*_allocator, *_vertical_blur_params);
		render_device.deleteParameterGroup(*_allocator, *_blur_params);
		render_device.deleteParameterGroup(*_allocator, *_color_coc_params);

		RenderDevice::release(_blur_target_sr);
		RenderDevice::release(_blur_target);
		RenderDevice::release(_near_target_sr);
		RenderDevice::release(_near_target);

		RenderDevice::release(_temp_blur_target_sr);
		RenderDevice::release(_temp_blur_target);
		RenderDevice::release(_temp_near_target_sr);
		RenderDevice::release(_temp_near_target);

		RenderDevice::release(_color_coc_target_sr);
		RenderDevice::release(_color_coc_target);
	};

	u32 getSecondaryViews(const Camera& camera, RenderView* out_views) override final
	{
		return 0;
	}

	void generate(const void* args_, const VisibilityData* visibility) override final
	{
		Profiler* profiler = _renderer->getProfiler();

		u32 scope_id;

		const Args& args = *(const Args*)args_;

		
		const float near_sharp_plane_z        = args.focus_plane_z - args.dof_size;
		const float far_sharp_plane_z         = args.focus_plane_z + args.dof_size;
		const float near_blurry_plane_z       = near_sharp_plane_z - args.near_blur_transition_size;
		const float far_blurry_plane_z        = far_sharp_plane_z + args.far_blur_transition_size;
		//const float near_blur_radius_fraction = 1.5f * 0.01f;
		//const float far_blur_radius_fraction  = 1.0f * 0.01f;
	
		const float max_radius_fraction = max(max(args.near_blur_radius_fraction, args.far_blur_radius_fraction), 0.001f);

		// This is a positive number
		const float near_normalize = (1.0f / (near_sharp_plane_z - near_blurry_plane_z)) * (args.near_blur_radius_fraction / max_radius_fraction);
		ASSERT(near_normalize >= 0.0f, "Near normalization must be a non-negative factor");

		// This is a positive number
		const float far_normalize = (1.0f / (far_blurry_plane_z - far_sharp_plane_z)) * (args.far_blur_radius_fraction / max_radius_fraction);
		ASSERT(far_normalize >= 0.0f, "Far normalization must be a non-negative factor");		

		//-----------------------------------------------
		// COLOR + COC
		//-----------------------------------------------

		{
			scope_id = profiler->beginScope("color_coc");

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			_color_coc_params->setSRV(args.color_texture, 0);
			_color_coc_params->setSRV(args.depth_texture, 1);

			u32 offset              = _color_coc_params_desc->getConstantOffset(getStringID("nearBlurryPlaneZ"));
			float* nearBlurryPlaneZ = (float*)pointer_math::add(_color_coc_params->getCBuffersData(), offset);
			*nearBlurryPlaneZ       = near_blurry_plane_z;

			offset                 = _color_coc_params_desc->getConstantOffset(getStringID("nearSharpPlaneZ"));
			float* nearSharpPlaneZ = (float*)pointer_math::add(_color_coc_params->getCBuffersData(), offset);
			*nearSharpPlaneZ       = near_sharp_plane_z;

			offset                = _color_coc_params_desc->getConstantOffset(getStringID("farSharpPlaneZ"));
			float* farSharpPlaneZ = (float*)pointer_math::add(_color_coc_params->getCBuffersData(), offset);
			*farSharpPlaneZ       = far_sharp_plane_z;

			offset                 = _color_coc_params_desc->getConstantOffset(getStringID("farBlurryPlaneZ"));
			float* farBlurryPlaneZ = (float*)pointer_math::add(_color_coc_params->getCBuffersData(), offset);
			*farBlurryPlaneZ       = far_blurry_plane_z;

			offset               = _color_coc_params_desc->getConstantOffset(getStringID("nearNormalize"));
			float* nearNormalize = (float*)pointer_math::add(_color_coc_params->getCBuffersData(), offset);
			*nearNormalize       = near_normalize;

			offset              = _color_coc_params_desc->getConstantOffset(getStringID("farNormalize"));
			float* farNormalize = (float*)pointer_math::add(_color_coc_params->getCBuffersData(), offset);
			*farNormalize       = far_normalize;

			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _color_coc_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_color_coc_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			_renderer->setViewport(*args.viewport, _width, _height);

			RenderTexture rt;
			rt.render_target = _color_coc_target;
			rt.width         = _width;
			rt.height        = _height;

			_renderer->setRenderTarget(1, &rt, nullptr);
			//_renderer->setRenderTarget(1, args.target, nullptr);

			_renderer->render(render_item);

			profiler->endScope(scope_id);
		}

		//-----------------------------------------------
		// BLUR
		//-----------------------------------------------

		{
			scope_id = profiler->beginScope("dof_blur");

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			u32 offset                 = _blur_params_desc->getConstantOffset(getStringID("max_coc_radius_pixels"));
			int* max_coc_radius_pixels = (int*)pointer_math::add(_blur_params->getCBuffersData(), offset);
			*max_coc_radius_pixels     = (int)std::ceil(_height*max_radius_fraction);

			offset                       = _blur_params_desc->getConstantOffset(getStringID("near_blur_radius_pixels"));
			int* near_blur_radius_pixels = (int*)pointer_math::add(_blur_params->getCBuffersData(), offset);
			*near_blur_radius_pixels     = (int)std::ceil(_height * args.near_blur_radius_fraction);

			offset                             = _blur_params_desc->getConstantOffset(getStringID("inv_near_blur_radius_pixels"));
			float* inv_near_blur_radius_pixels = (float*)pointer_math::add(_blur_params->getCBuffersData(), offset);
			*inv_near_blur_radius_pixels       = 1.0f / std::ceil(_height * args.near_blur_radius_fraction);

			offset                  = _blur_params_desc->getConstantOffset(getStringID("blur_source_size"));
			int* blur_source_size   = (int*)pointer_math::add(_blur_params->getCBuffersData(), offset);
			*blur_source_size       = _width;
			*(blur_source_size + 1) = _height;

			offset                  = _blur_params_desc->getConstantOffset(getStringID("near_source_size"));
			int* near_source_size   = (int*)pointer_math::add(_blur_params->getCBuffersData(), offset);
			*near_source_size       = _width;
			*(near_source_size + 1) = _height;

			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _blur_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_blur_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			_renderer->setViewport(*args.viewport, _width/2, _height);

			RenderTexture rts[2];
			rts[0].render_target = _temp_near_target;
			rts[0].width         = _width / 2;
			rts[0].height        = _height;

			rts[1].render_target = _temp_blur_target;
			rts[1].width         = _width / 2;
			rts[1].height        = _height;

			_renderer->setRenderTarget(2, rts, nullptr);
			//_renderer->setRenderTarget(1, args.target, nullptr);

			_renderer->render(render_item);

			profiler->endScope(scope_id);
		}

		{
			scope_id = profiler->beginScope("dof_blur");

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

			DrawCall dc = createDrawCall(false, 3, 0, 0);

			float dimension = _height;

			u32 offset                 = _vertical_blur_params_desc->getConstantOffset(getStringID("max_coc_radius_pixels"));
			int* max_coc_radius_pixels = (int*)pointer_math::add(_vertical_blur_params->getCBuffersData(), offset);
			*max_coc_radius_pixels     = (int)std::ceil(dimension * max_radius_fraction);

			offset                       = _vertical_blur_params_desc->getConstantOffset(getStringID("near_blur_radius_pixels"));
			int* near_blur_radius_pixels = (int*)pointer_math::add(_vertical_blur_params->getCBuffersData(), offset);
			*near_blur_radius_pixels     = (int)std::ceil(dimension * args.near_blur_radius_fraction);

			offset                             = _vertical_blur_params_desc->getConstantOffset(getStringID("inv_near_blur_radius_pixels"));
			float* inv_near_blur_radius_pixels = (float*)pointer_math::add(_vertical_blur_params->getCBuffersData(), offset);
			*inv_near_blur_radius_pixels       = 1.0f / std::ceil(dimension * args.near_blur_radius_fraction);

			offset                  = _vertical_blur_params_desc->getConstantOffset(getStringID("blur_source_size"));
			int* blur_source_size   = (int*)pointer_math::add(_vertical_blur_params->getCBuffersData(), offset);
			*blur_source_size       = _width / 2;
			*(blur_source_size + 1) = _height;

			offset                  = _vertical_blur_params_desc->getConstantOffset(getStringID("near_source_size"));
			int* near_source_size   = (int*)pointer_math::add(_vertical_blur_params->getCBuffersData(), offset);
			*near_source_size       = _width / 2;
			*(near_source_size + 1) = _height;

			Mesh mesh;
			mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

			RenderItem render_item;
			render_item.draw_call       = &dc;
			render_item.num_instances   = 1;
			render_item.shader          = _vertical_blur_shader_permutation[0];
			render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_vertical_blur_params);
			render_item.material_params = nullptr;
			render_item.mesh            = &mesh;

			_renderer->setViewport(*args.viewport, _width / 2, _height / 2);

			RenderTexture rts[2];
			rts[0].render_target = _near_target;
			rts[0].width         = _width / 2;
			rts[0].height        = _height / 2;

			rts[1].render_target = _blur_target;
			rts[1].width         = _width / 2;
			rts[1].height        = _height / 2;

			_renderer->setRenderTarget(2, rts, nullptr);
			//_renderer->setRenderTarget(1, args.target, nullptr);

			_renderer->render(render_item);

			profiler->endScope(scope_id);
		}

		//-----------------------------------------------
		// COMPOSITE
		//-----------------------------------------------

		{
			scope_id = profiler->beginScope("dof_composite");

			_renderer->getRenderDevice()->unbindResources();

			_renderer->bindFrameParameters();
			_renderer->bindViewParameters();

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

			_renderer->setViewport(*args.viewport, args.target->width, args.target->height);

			_renderer->setRenderTarget(1, args.target, nullptr);

			_renderer->render(render_item);

			profiler->endScope(scope_id);
		}
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

	RenderTargetH       _color_coc_target;
	ShaderResourceH		_color_coc_target_sr;

	RenderTargetH       _temp_near_target;
	ShaderResourceH		_temp_near_target_sr;

	RenderTargetH       _temp_blur_target;
	ShaderResourceH		_temp_blur_target_sr;

	RenderTargetH       _near_target;
	ShaderResourceH		_near_target_sr;

	RenderTargetH       _blur_target;
	ShaderResourceH		_blur_target_sr;

	ShaderPermutation         _color_coc_shader_permutation;
	const ParameterGroupDesc* _color_coc_params_desc;
	ParameterGroup*           _color_coc_params;

	ShaderPermutation         _blur_shader_permutation;
	const ParameterGroupDesc* _blur_params_desc;
	ParameterGroup*           _blur_params;

	ShaderPermutation         _vertical_blur_shader_permutation;
	const ParameterGroupDesc* _vertical_blur_params_desc;
	ParameterGroup*           _vertical_blur_params;

	ShaderPermutation         _composite_shader_permutation;
	const ParameterGroupDesc* _composite_params_desc;
	ParameterGroup*           _composite_params;
};