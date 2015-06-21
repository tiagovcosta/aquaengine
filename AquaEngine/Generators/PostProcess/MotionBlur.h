#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015              
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\..\Renderer\RendererInterfaces.h"
#include "..\..\Renderer\ShaderManager.h"
#include "..\..\Renderer\RenderDevice\RenderDeviceTypes.h"

#include "..\..\Utilities\StringID.h"

#include "..\..\AquaTypes.h"

#include <cmath>

namespace aqua
{
	class Allocator;
	class LinearAllocator;
	class Renderer;
	class Camera;
	class ParameterGroup;
	struct Viewport;
	struct RenderTexture;

	class MotionBlur : public ResourceGenerator
	{
	public:
		struct Args
		{
			ShaderResourceH velocity_texture;
			ShaderResourceH color_texture;
			ShaderResourceH depth_texture;
			const Viewport* viewport;

			u32				num_samples;

			ShaderResourceH* output;
		};

		void init(Renderer& renderer, lua_State* lua_state, Allocator& allocator, LinearAllocator& temp_allocator,
				  u32 width, u32 height, u32 max_blur_radius);

		void shutdown();

		u32 getSecondaryViews(const Camera& camera, RenderView* out_views) override final;

		void generate(const void* args_, const VisibilityData* visibility) override final;
		void generate(lua_State* lua_state) override final;

	private:

		Renderer*           _renderer;
		Allocator*          _allocator;
		LinearAllocator*    _temp_allocator;

		u32 _width;
		u32 _height;

		u32 _max_blur_radius;

		u32 _tile_texture_width;
		u32 _tile_texture_height;

		RenderTargetH       _tile_max_aux_target; //horizontal tile max output
		ShaderResourceH		_tile_max_aux_target_sr;

		RenderTargetH       _tile_max_target;
		ShaderResourceH		_tile_max_target_sr;

		RenderTargetH       _neighbor_max_target;
		ShaderResourceH		_neighbor_max_target_sr;

		RenderTargetH       _motion_blur_target;
		ShaderResourceH		_motion_blur_target_sr;

		ShaderPermutation         _horizontal_tile_max_shader_permutation;
		const ParameterGroupDesc* _horizontal_tile_max_params_desc;
		ParameterGroup*           _horizontal_tile_max_params;

		ShaderPermutation         _vertical_tile_max_shader_permutation;
		const ParameterGroupDesc* _vertical_tile_max_params_desc;
		ParameterGroup*           _vertical_tile_max_params;

		ShaderPermutation         _neighbor_max_shader_permutation;
		const ParameterGroupDesc* _neighbor_max_params_desc;
		ParameterGroup*           _neighbor_max_params;

		ShaderPermutation         _motion_blur_shader_permutation;
		const ParameterGroupDesc* _motion_blur_params_desc;
		ParameterGroup*           _motion_blur_params;
	};
}