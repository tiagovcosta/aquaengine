#include "FixedLinearAllocator.h"
#include "..\..\Utilities\Debug.h"

using namespace aqua;

FixedLinearAllocator::FixedLinearAllocator(size_t size, void* start) : LinearAllocator(size, start)
{

}

FixedLinearAllocator::~FixedLinearAllocator()
{
}

void* FixedLinearAllocator::allocate(size_t size, u8 alignment)
{
	ASSERT(size != 0 && alignment != 0);

	u8 adjustment = pointer_math::alignForwardAdjustment(_current_pos, alignment);

	if(_used_memory + adjustment + size > _size)
		return nullptr;

	uptr aligned_address = (uptr)_current_pos + adjustment;

	_current_pos = (void*)(aligned_address + size);

	_used_memory = (uptr)_current_pos - (uptr)_start;

	return (void*)aligned_address;
}

void FixedLinearAllocator::rewind(void* p)
{
	ASSERT(_current_pos > p && _start < p);

	_current_pos = p;
	_used_memory = (uptr)_current_pos - (uptr)_start;
}

void FixedLinearAllocator::clear()
{
	_num_allocations = 0;
	_used_memory     = 0;

	_current_pos = _start;
}