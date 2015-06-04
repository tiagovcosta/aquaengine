#include "DynamicLinearAllocator.h"
#include "..\..\Utilities\Debug.h"

using namespace aqua;

struct DynamicLinearAllocator::BlockDesc
{
	void*      start;
	BlockDesc* prev_block;
	size_t     size;
	size_t     used_memory;
};

DynamicLinearAllocator::DynamicLinearAllocator(Allocator& backing_allocator, size_t block_size, u8 block_alignment)
	: LinearAllocator(1, nullptr), _backing_allocator(backing_allocator), _current_block(nullptr),
	_block_size(block_size), _block_alignment(block_alignment)
{
	_size = 0; //little hack because "cant" create Allocator with size 0
}

DynamicLinearAllocator::~DynamicLinearAllocator()
{
}

void* DynamicLinearAllocator::allocate(size_t size, u8 alignment)
{
	ASSERT(size != 0 && alignment != 0);

	if(_current_block == nullptr)
		allocateNewBlock(size, alignment);

	u8 adjustment = pointer_math::alignForwardAdjustment(_current_pos, alignment);

	if(_current_block->used_memory + adjustment + size > _current_block->size)
	{
		allocateNewBlock(size, alignment);

		adjustment = pointer_math::alignForwardAdjustment(_current_pos, alignment);
	}

	void* aligned_address = pointer_math::add(_current_pos, adjustment);

	size_t total_size = adjustment + size;

	_current_pos = pointer_math::add(_current_pos, total_size);

	_current_block->used_memory += total_size;
	_used_memory				+= total_size;

	return aligned_address;
}

void DynamicLinearAllocator::rewind(void* p)
{
	if(p == _current_pos)
		return;

	ASSERT(_current_block != nullptr);

	while(p <= _current_block + 1 || p > pointer_math::add(_current_block->start, _current_block->size))
	{
		_size		 -= _current_block->size;
		_used_memory -= _current_block->used_memory;

		BlockDesc* temp = _current_block->prev_block;

		_backing_allocator.deallocate(_current_block->start);

		_current_block = temp;

		if(_current_block != nullptr)
		{
			_used_memory -= _current_block->size - _current_block->used_memory;
		}
		else
		{
			ASSERT(p == nullptr);

			_current_pos = nullptr;
			return;
		}
	}

	ASSERT(p != nullptr);

	ASSERT(p > _current_block + 1 && p <= pointer_math::add(_current_block->start, _current_block->used_memory));

	size_t k = _current_block->used_memory - ((uptr)p - (uptr)_current_block->start);

	_current_block->used_memory -= k;
	_used_memory				-= k;

	_current_pos = p;
}

/*
void DynamicLinearAllocator::rewind(void* p)
{
if(p == _current_pos)
return;

ASSERT(_current_block != nullptr);

while(p <= _current_block->start || p > pointer_math::add(_current_block->start, _current_block->size))
{
_size		 -= _current_block->size;
_used_memory -= _current_block->used_memory;

BlockDesc* temp = _current_block->prev_block;

_backing_allocator.deallocate(_current_block->start);

_current_block = temp;

if(_current_block != nullptr)
{
_used_memory -= _current_block->size - _current_block->used_memory;
}
else
{
ASSERT(p == nullptr);

_current_pos = nullptr;
return;
}
}

ASSERT(p != nullptr);

if(p == _current_block + 1)
{
_size		 -= _current_block->size;
_used_memory -= _current_block->used_memory;

BlockDesc* temp = _current_block->prev_block;

_backing_allocator.deallocate(_current_block->start);

_current_block = temp;

if(_current_block != nullptr)
_current_pos = pointer_math::add(_current_block->start, _current_block->used_memory);
else
_current_pos = nullptr;
}
else
{
ASSERT(p > _current_block + 1 && p <= pointer_math::add(_current_block->start, _current_block->used_memory));

_used_memory -= _current_block->used_memory - ((uptr)p - (uptr)_current_block->start);

_current_pos = p;
}
}
*/

void DynamicLinearAllocator::clear()
{
	rewind(nullptr);

	ASSERT(_used_memory == 0		&& 
		   _size == 0				&& 
		   _num_allocations == 0	&& 
		   _start == nullptr		&&
		   _current_pos == nullptr	&&
		   _current_block == nullptr);
}

bool DynamicLinearAllocator::allocateNewBlock(size_t size, u8 alignment)
{
	//Some space might be wasted but KISS
	size_t total_size = size + alignment + sizeof(BlockDesc) + __alignof(BlockDesc);

	size_t num_blocks = total_size / _block_size;

	if(total_size % _block_size != 0)
		num_blocks++;

	size_t new_block_size = num_blocks * _block_size;

	void* new_block = _backing_allocator.allocate(new_block_size, _block_alignment);

	if(new_block == nullptr)
		return false;

	u8 new_block_desc_adjustment = pointer_math::alignForwardAdjustment(new_block, __alignof(BlockDesc));

	BlockDesc* new_block_desc = (BlockDesc*)pointer_math::add(new_block, new_block_desc_adjustment);

	new_block_desc->start       = new_block;
	new_block_desc->prev_block  = _current_block;
	new_block_desc->size        = new_block_size;
	new_block_desc->used_memory = sizeof(BlockDesc) + new_block_desc_adjustment;

	if(_current_block != nullptr)
		_used_memory += _current_block->size - _current_block->used_memory;

	_used_memory += new_block_desc->used_memory;

	_current_block = new_block_desc;
	_current_pos   = _current_block + 1;

	_size += new_block_size;

	return true;
}