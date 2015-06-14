#pragma once

#include "..\Renderer\ShaderManager.h"

#include "..\Renderer\RendererInterfaces.h"
#include "..\Renderer\RenderDevice\RenderDeviceTypes.h"

#include "..\AquaMath.h"

namespace aqua
{
	class Renderer;
	class ParameterGroup;
	class Camera;
	class Allocator;
	class LinearAllocator;
	struct Viewport;

	class ScreenSpaceReflections : public ResourceGenerator
	{
	public:

		struct Args
		{
			ShaderResourceH color_texture;
			ShaderResourceH normal_texture;
			ShaderResourceH depth_texture;
			ShaderResourceH material_texture;
			ShaderResourceH rayleigh_texture;
			const Camera*   camera;
			const Viewport* viewport;

			float thickness;

			ShaderResourceH* output;
		};

		void init(aqua::Renderer& renderer, lua_State* lua_state, Allocator& allocator, LinearAllocator& scratchpad,
				  u32 width, u32 height);
		void shutdown();

		// ResourceGenerator interface
		u32 getSecondaryViews(const Camera& camera, RenderView* out_views) override final;
		void generate(const void* args_, const VisibilityData* visibility) override final;
		void generate(lua_State* lua_state) override final;

	private:

		Renderer*           _renderer;
		Allocator*          _allocator;
		LinearAllocator*    _scratchpad_allocator;

		static const u32 NUM_GLOSSINESS_MIPS = 5;

		RenderTargetH       _reflection_target[NUM_GLOSSINESS_MIPS];
		ShaderResourceH		_reflection_target_sr[NUM_GLOSSINESS_MIPS + 1];

		RenderTargetH       _glossiness_chain_target[NUM_GLOSSINESS_MIPS];
		ShaderResourceH		_glossiness_chain_target_sr[NUM_GLOSSINESS_MIPS];

		RenderTargetH       _composite_target;
		ShaderResourceH		_composite_target_sr;

		ShaderPermutation         _ssr_shader_permutation;
		const ParameterGroupDesc* _ssr_params_desc;
		ParameterGroup*           _ssr_params;

		ShaderPermutation         _horizontal_blur_shader_permutation;
		const ParameterGroupDesc* _horizontal_blur_params_desc;
		ParameterGroup*           _horizontal_blur_params;

		ShaderPermutation         _vertical_blur_shader_permutation;
		const ParameterGroupDesc* _vertical_blur_params_desc;
		ParameterGroup*           _vertical_blur_params;

		ShaderPermutation         _composite_shader_permutation;
		const ParameterGroupDesc* _composite_params_desc;
		ParameterGroup*           _composite_params;

		u32 _width;
		u32 _height;
	};
};