#include "EndAllocator.h"
#include "..\..\Utilities\Debug.h"

using namespace aqua;

EndAllocator::EndAllocator(size_t size, void* start)
	: FixedLinearAllocator(size, start)
{
}

EndAllocator::~EndAllocator()
{}

void* EndAllocator::allocate(size_t size, u8 alignment)
{
	ASSERT(size != 0 && alignment != 0);

	void* address = pointer_math::subtract(_current_pos, size);

	u8 adjustment = pointer_math::alignBackwardAdjustment(address, alignment);

	if(_used_memory + adjustment + size > _size)
		return nullptr;

	void* aligned_address = pointer_math::subtract(address, adjustment);

	_current_pos = aligned_address;

	_used_memory = (uptr)_start - (uptr)_current_pos;

	return aligned_address;
}

void EndAllocator::rewind(void* p)
{
	ASSERT(_current_pos <= p && _start >= p);

	_current_pos = p;
	_used_memory = (uptr)_start - (uptr)_current_pos;
}