#pragma once

#include "..\Renderer\ShaderManager.h"

#include "..\Renderer\RendererInterfaces.h"
#include "..\Renderer\RenderDevice\RenderDeviceTypes.h"

#include "..\AquaMath.h"
#include "..\AquaTypes.h"

namespace aqua
{
	class Renderer;
	class ParameterGroup;
	class Camera;
	class Allocator;
	class LinearAllocator;
	struct Viewport;

	//Implementation based on http://john-chapman-graphics.blogspot.co.uk/2013/01/ssao-tutorial.html
	class SSAOGenerator : public ResourceGenerator
	{
	public:

		struct Args
		{
			ShaderResourceH		normal_buffer;
			ShaderResourceH		depth_buffer;
			const Viewport*     viewport;
			RenderTargetH       target;
			u32					target_width;
			u32					target_height;
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

		u32                 _width;
		u32                 _height;

		RenderTargetH       _aux_buffer_rt;
		ShaderResourceH		_aux_buffer_sr;

		ShaderPermutation         _ssao_shader_permutation;
		const ParameterGroupDesc* _ssao_params_desc;
		ParameterGroup*           _ssao_params;

		ShaderPermutation         _ssao_blur_shader_permutation;
		const ParameterGroupDesc* _ssao_blur_params_desc;
		ParameterGroup*           _ssao_blur_params;
	};
};