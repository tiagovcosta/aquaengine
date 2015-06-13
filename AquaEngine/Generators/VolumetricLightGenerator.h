#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015              
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\Renderer\ShaderManager.h"
#include "..\Renderer\RendererInterfaces.h"
#include "..\Renderer\RenderDevice\RenderDeviceTypes.h"
//#include "..\Renderer\RendererStructs.h"

#include "..\AquaMath.h"

namespace aqua
{
	class Allocator;
	class LinearAllocator;
	class Renderer;
	class ParameterGroup;
	class Camera;
	struct Viewport;
	struct RenderTexture;

	class VolumetricLightGenerator : public ResourceGenerator
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

			ShaderResourceH*     output;
		};

		void init(aqua::Renderer& renderer, lua_State* lua_state, Allocator& allocator, LinearAllocator& temp_allocator,
				  u32 width, u32 height);

		void shutdown();

		ShaderResourceH get() const;

		u32 getSecondaryViews(const Camera& camera, RenderView* out_views) override final;

		void generate(const void* args_, const VisibilityData* visibility) override final;
		void generate(lua_State* lua_state) override final;

	private:

		Renderer*           _renderer;
		Allocator*          _allocator;
		LinearAllocator*    _temp_allocator;

		u32                 _width;
		u32                 _height;

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
	};
};