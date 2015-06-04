#include "ParameterCache.h"

#include "..\Core\Allocators\Allocator.h"
#include "..\Utilities\PointerMath.h"

using namespace aqua;

ParametersManager::ParametersManager(Allocator& allocator) : _allocator(allocator)
{

}

ParameterCache ParametersManager::createParameterCache()
{
	ParameterCache cache;
	cache.names          = nullptr;
	cache.offsets        = nullptr;
	cache.buffer         = nullptr;
	cache.buffer_size    = 0;
	cache.num_parameters = 0;

	return cache;
}

ParameterCache ParametersManager::cloneParameterCache(const ParameterCache& cache)
{
	ASSERT(cache.num_parameters > 0, "Can't clone empy parameter cache");

	void* new_buffer = _allocator.allocate(cache.buffer_size, BUFFER_ALIGNMENT);

	ParameterCache new_cache;
	new_cache.names          = (u32*)new_buffer;
	new_cache.offsets        = new_cache.names + cache.num_parameters;
	new_cache.buffer         = (u8*)pointer_math::add(new_buffer, sizeof(u32) * 2 * cache.num_parameters);
	new_cache.buffer_size    = cache.buffer_size;
	new_cache.num_parameters = cache.num_parameters;

	memcpy(new_cache.names, cache.names, sizeof(u32) * cache.num_parameters);
	memcpy(new_cache.offsets, cache.offsets, sizeof(u32) * cache.num_parameters);
	memcpy(new_cache.buffer, cache.buffer, cache.buffer_size);

	return cache;
}

void ParametersManager::increaseCapacity(ParameterCache& cache, u32 new_parameter_size)
{
	ParameterCache new_cache;
	new_cache.num_parameters = cache.num_parameters + 1;

	u32 new_header_size = sizeof(u32) * 2 * new_cache.num_parameters;

	u8 diff = new_header_size % BUFFER_ALIGNMENT;

	if(diff > 0)
		new_header_size += BUFFER_ALIGNMENT - diff; //keep buffer alignement

	u32 new_buffer_size = cache.buffer_size + new_parameter_size;

	diff = new_parameter_size % BUFFER_ALIGNMENT;

	if(diff > 0)
		new_buffer_size += BUFFER_ALIGNMENT - diff; //keep buffer alignement

	void* new_blob = _allocator.allocate(new_header_size + new_buffer_size, BUFFER_ALIGNMENT);

	new_cache.buffer_size    = new_buffer_size;
	new_cache.names          = (u32*)new_blob;
	new_cache.offsets        = new_cache.names + new_cache.num_parameters;
	new_cache.buffer         = (u8*)pointer_math::add(new_blob, new_header_size);

	new_cache.offsets[new_cache.num_parameters - 1] = cache.buffer_size;

	if(cache.num_parameters > 0)
	{
		memcpy(new_cache.names, cache.names, sizeof(u32) * cache.num_parameters);
		memcpy(new_cache.offsets, cache.offsets, sizeof(u32) * cache.num_parameters);
		memcpy(new_cache.buffer, cache.buffer, cache.buffer_size);

		_allocator.deallocate(cache.names);
	}

	cache = new_cache;
}