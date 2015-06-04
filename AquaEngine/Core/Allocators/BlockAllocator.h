#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015             
/////////////////////////////////////////////////////////////////////////////////////////////

#include "Allocator.h"

#include "..\..\AquaTypes.h"

// POSSIBLE ISSUES:
// -	Since the size of each allocation is NUM_BLOCKS * BLOCK_SIZE + _header_size, if the user allocates
//		2 blocks the allocation size is 8208 (BLOCK_SIZE = 4096, _header_size = 16)
//		Later, if the user frees the 2 blocks and allocates a new one, BLOCK_SIZE bytes (4096 bytes), might be wasted
//		because the new hole is not large enough to store a block plus its header
//
//		Possible solutions:
//			1 - Use a different algorithm to find the best fit free block, 
//				that prevents (or at least minimizes) the issue
//			2 - Always allocate NUM_BLOCKS * (BLOCK_SIZE + _header_size) bytes so the issue doesnt.
//				(This solution causes a fixed ammount of memory to be reserved to header sizes)
//					Eg: BLOCK_SIZE = 4096, _header_size = 16, total_memory = 2GiB
//						~8MiB reserved for headers (~0.004% of total memory reserved)

namespace aqua
{
	class BlockAllocator : public Allocator
	{
	public:
		BlockAllocator(size_t size, void* start, size_t block_size, u8 alignment);
		~BlockAllocator() override;

		void* allocate(size_t size, u8 alignment) override;
		void deallocate(void* p) override;

		size_t getBlockSize() const;
		u8 getAlignment() const;

	protected:

		struct BlockHeader
		{
			size_t       size;
			BlockHeader* next_free_block;
		};

		BlockHeader* _free_blocks;

		size_t _block_size;
		size_t _header_size; //sizeof(BlockHeader) + adjustment to keep blocks properly aligned
		u8	   _alignment;
		u8	   _initial_adjustment;

	private:
		BlockAllocator(const BlockAllocator&);
		BlockAllocator& operator=(const BlockAllocator&);
	};
};