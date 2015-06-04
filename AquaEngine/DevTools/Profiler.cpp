#include "Profiler.h"

#include "..\Renderer\RenderDevice\RenderDevice.h"

//#include "..\Core\Allocators\DynamicLinearAllocator.h"
#include "..\Utilities\Debug.h"

using namespace aqua;

Profiler::Profiler(RenderDevice& render_device) : _render_device(render_device), _current_frame(0), _current_scope(UINT32_MAX)
{
	for(u8 i = 0; i < QUERY_LATENCY; i++)
	{
		Frame& frame = _frames[i];

		if(!_render_device.createQuery(QueryType::TIMESTAMP_DISJOINT, frame.disjoint_query))
			;// return false;

		for(u32 j = 0; j < MAX_NUM_SCOPES; j++)
		{
			Scope& scope = frame.scopes[j];

			if(!_render_device.createQuery(QueryType::TIMESTAMP, scope.timestamp_start_query))
				;// return false;

			if(!_render_device.createQuery(QueryType::TIMESTAMP, scope.timestamp_end_query))
				;// return false;
		}

		frame.num_scopes = 0;
		frame.finished   = false;
	}
}

Profiler::~Profiler()
{
	for(u8 i = 0; i < QUERY_LATENCY; i++)
	{
		Frame& frame = _frames[i];

		RenderDevice::release(frame.disjoint_query);

		for(u32 j = 0; j < MAX_NUM_SCOPES; j++)
		{
			Scope& scope = frame.scopes[j];

			RenderDevice::release(scope.timestamp_start_query);
			RenderDevice::release(scope.timestamp_end_query);
		}
	}
}

bool Profiler::beginFrame()
{
	ASSERT(_current_scope == UINT32_MAX);

	Frame& current_frame = _frames[_current_frame];

	_render_device.begin(current_frame.disjoint_query);

	u32 frame_scope = beginScope("frame");

	ASSERT(frame_scope == 0);

	return true;
}

bool Profiler::endFrame()
{
	endScope(0);

	Frame& current_frame = _frames[_current_frame];

	_render_device.end(current_frame.disjoint_query);

	//current_frame.num_scopes = _current_scope;
	current_frame.finished = true;

	//current_frame.allocator->clear();

	_current_frame = (_current_frame + 1) % QUERY_LATENCY;

	return true;
}

u32 Profiler::beginScope(const char* name)
{
	Frame& frame = _frames[_current_frame];

	u32 scope_index = frame.num_scopes++;

	Scope& scope = frame.scopes[scope_index];

	scope.name         = name;
	scope.parent       = _current_scope;
	scope.next_sibling = UINT32_MAX;
	scope.first_child  = UINT32_MAX;
	scope.last_child   = UINT32_MAX;

	if(_current_scope != UINT32_MAX)
	{
		if(frame.scopes[_current_scope].last_child == UINT32_MAX)
		{
			frame.scopes[_current_scope].first_child = scope_index;
		}
		else
		{
			frame.scopes[frame.scopes[_current_scope].last_child].next_sibling = scope_index;
		}

		frame.scopes[_current_scope].last_child = scope_index;
	}

	_current_scope = scope_index;

	_render_device.end(scope.timestamp_start_query);

	return scope_index;
}

bool Profiler::endScope(u32 id)
{
	ASSERT(id == _current_scope);

	Scope& scope = _frames[_current_frame].scopes[id];

	_render_device.end(scope.timestamp_end_query);

	_current_scope = scope.parent;

	return true;
}

int Profiler::getScopeResults(const Scope* scopes, u32 current_scope, u32 depth, u64 frequency, 
							  size_t max_chars, char* out)
{
	int chars_written = 0;

	while(current_scope != UINT32_MAX)
	{
		for(u32 i = 0; i < depth; i++)
		{
			*(out + chars_written + i) = '-';
		}

		chars_written += depth;

		const Scope& scope = scopes[current_scope];

		// Get scope timestamps
		UINT64 begin, end;

		_render_device.getData(scope.timestamp_start_query, &begin, sizeof(UINT64));
		_render_device.getData(scope.timestamp_end_query, &end, sizeof(UINT64));

		float duration = float(end - begin) / float(frequency) * 1000.0f;

		chars_written += sprintf_s(out + chars_written, max_chars - chars_written, "%s: %.3f\n", scope.name, duration);

		if(scope.first_child != UINT32_MAX)
		{
			chars_written += getScopeResults(scopes, scope.first_child, depth + 1, frequency,
											 max_chars - chars_written, out + chars_written);
		}

		current_scope = scopes[current_scope].next_sibling;
	}

	return chars_written;
}

const char* Profiler::getResults()
{
	Frame& current_frame = _frames[(_current_frame + 1) % QUERY_LATENCY];

	if(!current_frame.finished)
		return nullptr;

	while(!_render_device.checkStatus(current_frame.disjoint_query))
	{
		Sleep(1); // Wait a bit, but give other threads a chance to run
	}

	// Check whether timestamps were disjoint during the last frame
	D3D10_QUERY_DATA_TIMESTAMP_DISJOINT ts_disjoint;

	_render_device.getData(current_frame.disjoint_query, &ts_disjoint, sizeof(ts_disjoint));

	const unsigned int MAX_CHARS = 1023;
	static char buffer[MAX_CHARS + 1];

	int chars_written = 0;

	chars_written += sprintf_s(buffer + chars_written, MAX_CHARS - chars_written, "Performance Overview\n");

	getScopeResults(current_frame.scopes, 0, 0, ts_disjoint.Frequency, 
					MAX_CHARS - chars_written, buffer + chars_written);

	current_frame.num_scopes = 0;
	current_frame.finished   = false;

	if(ts_disjoint.Disjoint)
		return nullptr;

	return buffer;
}