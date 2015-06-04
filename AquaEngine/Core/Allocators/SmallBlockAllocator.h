#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2014             
/////////////////////////////////////////////////////////////////////////////////////////////

#include "Allocator.h"

#include "..\..\AquaTypes.h"

namespace aqua
{
	class SmallBlockAllocator : public Allocator
	{
	public:
		SmallBlockAllocator(Allocator& backing_allocator, u32 block_size, u32 block_alignment);
		~SmallBlockAllocator();

		void* allocate(size_t size, u8 alignment) override;

		void deallocate(void* p) override;

		static const u32 MIN_BLOCK_SIZE = 4 * 1024; //4kb

	private:

		struct AllocationHeader
		{
			size_t size;
			u8     adjustment;
		};

		struct FreeBlock
		{
			size_t     size;
			FreeBlock* next;
		};

		struct NewBlock
		{
			FreeBlock* block;
			FreeBlock* prev;
		};

		static_assert(sizeof(AllocationHeader) >= sizeof(FreeBlock),
					  "sizeof(AllocationHeader) must be >= sizeof(FreeBlock)");

		SmallBlockAllocator(const SmallBlockAllocator&);
		SmallBlockAllocator& operator=(const SmallBlockAllocator&);

		NewBlock allocateNewBlock();

		FreeBlock* _free_blocks;

		Allocator& _backing_allocator;
		u32        _block_size;
		u32        _block_alignment;
	};
};