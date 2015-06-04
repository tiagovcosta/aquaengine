#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "LinearAllocator.h"

#include "..\..\AquaTypes.h"

/*
	TODO: Remove getStart() from LinearAllocator
		because it doesn't make sense in DynamicLinearAllocators and there's no real need for it
			
*/

namespace aqua
{
	class Allocator;

	class DynamicLinearAllocator : public LinearAllocator
	{
	public:
		DynamicLinearAllocator(Allocator& backing_allocator, size_t block_size, u8 block_alignment);
		~DynamicLinearAllocator() override final;

		void* allocate(size_t size, u8 alignment = DEFAULT_ALIGNMENT) override final;

		void rewind(void* mark) override final;

		void clear() override final;

	private:
		DynamicLinearAllocator(const DynamicLinearAllocator&);
		DynamicLinearAllocator& operator=(const DynamicLinearAllocator&);

		bool allocateNewBlock(size_t size, u8 alignment);

		struct BlockDesc;

		Allocator& _backing_allocator;

		BlockDesc* _current_block;

		size_t _block_size;
		u8     _block_alignment;
	};
};