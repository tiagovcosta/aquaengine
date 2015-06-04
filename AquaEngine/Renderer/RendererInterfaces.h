#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015              
/////////////////////////////////////////////////////////////////////////////////////////////

//#include "RendererStructs.h"

#include "Camera.h"

#include "..\AquaTypes.h"

struct lua_State;

namespace aqua
{
	struct RenderQueue;

	struct VisibilityData
	{
		u32* visibles_indices;
		u32  num_visibles;
	};

	class ViewRenderer
	{
	public:

		virtual u32	getNumPasses() const = 0;
		virtual u32* getPassesNames() const = 0;
	};

	struct RenderView
	{
		Camera		camera;
		const void* generator_args;
		u32			generator_name;
	};

	class ResourceGenerator
	{
	public:
		//virtual void init(Renderer& renderer, lua_State* lua_state, Allocator& allocator) = 0;

		virtual u32 getSecondaryViews(const Camera& camera, RenderView* out_views) = 0;

		virtual void generate(const void* args, const VisibilityData* visibility) = 0; //Run generator from C++
		virtual void generate(lua_State* lua_state) = 0; //Run generator from Lua

		//virtual void shutdown() = 0;
	protected:
		~ResourceGenerator(){};
	};

	class RenderViewGenerator
	{
		virtual u32 getRenderViews(const Camera& main_camera, const RenderView* out_views) = 0;
	};

	using Frustum = const Plane*;
	//using Frustum = const Plane[6];

	class RenderQueueGenerator
	{
	public:

		//virtual const VisibilityData* cull(u32 num_frustums, const Frustum* frustums) = 0;
		virtual bool cull(u32 num_frustums, const Frustum* frustums, VisibilityData** out) = 0;
		virtual bool extract(const VisibilityData& visibility_data) = 0;
		virtual bool prepare() = 0;
		virtual bool getRenderItems(u8 num_passes, const u32* passes_names, 
									const VisibilityData& visibility_data, RenderQueue* out_queues) = 0;
	};
}