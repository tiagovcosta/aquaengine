#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2014             
/////////////////////////////////////////////////////////////////////////////////////////////

#include "Allocator.h"

#include "..\..\AquaTypes.h"

//#define USE_ALLOCATION_IDENTIFIER

namespace aqua
{
	class FreeListAllocator : public Allocator
	{
	public:
		FreeListAllocator(size_t size, void* start);
		~FreeListAllocator();

		void* allocate(size_t size, u8 alignment) override;

		void deallocate(void* p) override;

#if AQUA_DEBUG || AQUA_DEVELOPMENT
		void checkFreeBlockList();
#endif

	private:

		struct AllocationHeader
		{
			#if AQUA_DEBUG || AQUA_DEVELOPMENT
			size_t identifier;
			u32	   magic_number;
			#endif
			size_t size;
			u8     adjustment;
		};

		struct FreeBlock
		{
			#if AQUA_DEBUG || AQUA_DEVELOPMENT
			u32 magic_number;
			#endif
			size_t     size;
			FreeBlock* next;
		};

		static_assert(sizeof(AllocationHeader) >= sizeof(FreeBlock), "sizeof(AllocationHeader) < sizeof(FreeBlock)");

		FreeListAllocator(const FreeListAllocator&);
		FreeListAllocator& operator=(const FreeListAllocator&);

		FreeBlock* _free_blocks;

		#if AQUA_DEBUG || AQUA_DEVELOPMENT
		size_t _next_identifier;
		#endif
	};
};