#pragma once

#include "..\Renderer\RendererInterfaces.h"
#include "..\Renderer\RenderDevice\RenderDeviceTypes.h"

#include "..\AquaMath.h"

namespace aqua
{
	class Renderer;
	class Camera;
	class Allocator;
	class LinearAllocator;
	struct Viewport;

	class ShadowMapGenerator : public ResourceGenerator
	{
	public:

		struct Args
		{
			DepthStencilTargetH dsv;
			const Viewport*     viewport;
			u32					shadow_map_width;
			u32					shadow_map_height;
		};

		void init(aqua::Renderer& renderer, lua_State* lua_state, Allocator& allocator, LinearAllocator& scratchpad);
		void shutdown();

		// ResourceGenerator interface
		u32 getSecondaryViews(const Camera& camera, RenderView* out_views) override final;
		void generate(const void* args_, const VisibilityData* visibility) override final;
		void generate(lua_State* lua_state) override final;

	private:

		Renderer*           _renderer;
		Allocator*          _allocator;
		LinearAllocator*    _scratchpad_allocator;
	};
};