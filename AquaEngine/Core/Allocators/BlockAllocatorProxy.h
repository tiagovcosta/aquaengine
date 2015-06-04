#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015             
/////////////////////////////////////////////////////////////////////////////////////////////

#include "BlockAllocator.h"
#include "ProxyAllocator.h"

#include "..\..\AquaTypes.h"

namespace aqua
{
#if _DEBUG
	class BlockAllocatorProxy : public ProxyAllocator
	{
	public:
		BlockAllocatorProxy(BlockAllocator& allocator);
		~BlockAllocatorProxy() override;

		size_t getBlockSize() const;
		u8 getAlignment() const;

	private:

		BlockAllocatorProxy(const BlockAllocatorProxy&);
		BlockAllocatorProxy& operator=(const BlockAllocatorProxy&);

		BlockAllocator& _allocator;
	};
#endif
};

#if AQUA_DEVELOPMENT || AQUA_DEBUG
	#define NEW_BLOCK_ALLOCATOR_PROXY(alloc, block_allocator) \
		allocator::allocateNew<BlockAllocatorProxy>(alloc, block_allocator)
#else
	#define NEW_BLOCK_ALLOCATOR_PROXY(alloc, block_allocator) &block_allocator
#endif