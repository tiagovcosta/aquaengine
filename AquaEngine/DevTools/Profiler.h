#pragma once

#include "..\Renderer\RenderDevice\RenderDeviceTypes.h"

#include "..\AquaTypes.h"

namespace aqua
{
	class RenderDevice;
	//class DynamicLinearAllocator;

	class Profiler
	{
	public:

		Profiler(RenderDevice& render_device);
		~Profiler();

		bool beginFrame();
		bool endFrame();

		u32 beginScope(const char* name);
		bool endScope(u32 id);

		const char* getResults();

	private:

		static const u32 MAX_NUM_SCOPES = 256;
		static const u8  QUERY_LATENCY  = 5;
		static const u32 ROOT_SCOPE     = UINT32_MAX;

		//QueryH _disjoint_queries[QUERY_LATENCY];

		struct Scope
		{
			QueryH      timestamp_start_query;
			QueryH      timestamp_end_query;
			const char* name;
			u32         parent;
			u32         first_child;
			u32         last_child;
			u32         next_sibling;
		};

		struct Frame
		{
			//DynamicLinearAllocator* allocator;
			//Scope*                  scopes;

			QueryH                  disjoint_query;
			Scope                   scopes[MAX_NUM_SCOPES];
			u32                     num_scopes;
			bool					finished;
		};

		int getScopeResults(const Scope* scopes, u32 current_scope, u32 depth, u64 frequency,
							size_t max_chars, char* out);

		RenderDevice& _render_device;

		Frame _frames[QUERY_LATENCY];

		u32 _current_frame;
		u32 _current_scope;
	};
}