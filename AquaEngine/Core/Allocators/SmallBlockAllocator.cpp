#include "SmallBlockAllocator.h"
#include "..\..\Utilities\Debug.h"

using namespace aqua;

SmallBlockAllocator::SmallBlockAllocator(Allocator& backing_allocator, u32 block_size, u32 block_alignment)
	: Allocator(1), _free_blocks(nullptr), _backing_allocator(backing_allocator), 
	_block_size(block_size), _block_alignment(block_alignment)
{
	ASSERT(block_size >= MIN_BLOCK_SIZE);

	static const u8 MIN_ALIGNMENT = 8;

	static_assert(MIN_ALIGNMENT >= __alignof(AllocationHeader) && MIN_ALIGNMENT >= __alignof(FreeBlock), 
				  "Check SmallBlockAllocator minimum aligment");

	ASSERT(_block_alignment >= MIN_ALIGNMENT && "Block aligment must be larger than 8");

	_size = 0; //little hack because "cant" create Allocator with size 0
}

SmallBlockAllocator::~SmallBlockAllocator()
{
	//_free_blocks = nullptr;
}

void* SmallBlockAllocator::allocate(size_t size, u8 alignment)
{
	ASSERT(size != 0 && alignment != 0 && size <= _block_size - sizeof(AllocationHeader));

	FreeBlock* prev_free_block = nullptr;
	FreeBlock* free_block      = _free_blocks;

	FreeBlock* best_fit_prev       = nullptr;
	FreeBlock* best_fit            = nullptr;
	u8         best_fit_adjustment = 0;
	size_t     best_fit_total_size = 0;

	//Find best fit
	while(free_block != nullptr)
	{
		//Calculate adjustment needed to keep object correctly aligned
		u8 adjustment = pointer_math::alignForwardAdjustmentWithHeader<AllocationHeader>(free_block, alignment);

		size_t total_size = size + adjustment;

		//If its an exact match use this free block
		if(free_block->size == total_size)
		{
			best_fit_prev       = prev_free_block;
			best_fit            = free_block;
			best_fit_adjustment = adjustment;
			best_fit_total_size = total_size;

			break;
		}

		//If its a better fit switch
		if(free_block->size > total_size && (best_fit == nullptr || free_block->size < best_fit->size))
		{
			best_fit_prev       = prev_free_block;
			best_fit            = free_block;
			best_fit_adjustment = adjustment;
			best_fit_total_size = total_size;
		}

		prev_free_block = free_block;
		free_block      = free_block->next;
	}

	if(best_fit == nullptr)
	{
		NewBlock new_block = allocateNewBlock();

		if(new_block.block == nullptr)
			return nullptr;

		best_fit            = new_block.block;
		best_fit_prev       = new_block.prev;
		best_fit_adjustment = pointer_math::alignForwardAdjustmentWithHeader<AllocationHeader>(best_fit, alignment);
		best_fit_total_size = size + best_fit_adjustment;
	}

	//If allocations in the remaining memory will be impossible
	if(best_fit->size - best_fit_total_size <= sizeof(AllocationHeader))
	{
		//Increase allocation size instead of creating a new FreeBlock
		best_fit_total_size = best_fit->size;

		if(best_fit_prev != nullptr)
			best_fit_prev->next = best_fit->next;
		else
			_free_blocks = best_fit->next;
	}
	else
	{
		//Prevent new block from overwriting best fit block info
		ASSERT(best_fit_total_size > sizeof(FreeBlock));

		//Else create a new FreeBlock containing remaining memory
		FreeBlock* new_block = (FreeBlock*)(pointer_math::add(best_fit, best_fit_total_size));
		new_block->size      = best_fit->size - best_fit_total_size;
		new_block->next      = best_fit->next;

		if(best_fit_prev != nullptr)
			best_fit_prev->next = new_block;
		else
			_free_blocks = new_block;
	}

	uptr aligned_address = (uptr)best_fit + best_fit_adjustment;

	AllocationHeader* header = (AllocationHeader*)(aligned_address - sizeof(AllocationHeader));
	header->size             = best_fit_total_size;
	header->adjustment       = best_fit_adjustment;

	ASSERT(pointer_math::isAligned(header));

	_used_memory += best_fit_total_size;
	_num_allocations++;

	ASSERT(pointer_math::alignForwardAdjustment((void*)aligned_address, alignment) == 0);

	return (void*)aligned_address;
}

void SmallBlockAllocator::deallocate(void* p)
{
	ASSERT(p != nullptr);

	AllocationHeader* header = (AllocationHeader*)pointer_math::subtract(p, sizeof(AllocationHeader));

	uptr   block_start = reinterpret_cast<uptr>(p)-header->adjustment;
	size_t block_size  = header->size;
	uptr   block_end   = block_start + block_size;

	FreeBlock* prev_free_block = nullptr;

	FreeBlock* free_block = _free_blocks;

	while(free_block != nullptr)
	{
		if((uptr)free_block >= block_end)
			break;

		prev_free_block = free_block;
		free_block      = free_block->next;
	}

	if(prev_free_block == nullptr)
	{
		prev_free_block       = (FreeBlock*)block_start;
		prev_free_block->size = block_size;
		prev_free_block->next = _free_blocks;

		_free_blocks = prev_free_block;
	}
	else if((uptr)prev_free_block + prev_free_block->size == block_start)
	{
		prev_free_block->size += block_size;
	}
	else
	{
		FreeBlock* temp       = (FreeBlock*)block_start;
		temp->size            = block_size;
		temp->next            = prev_free_block->next;

		prev_free_block->next = temp;
		prev_free_block       = temp;
	}

	if(free_block != nullptr && (uptr)free_block == block_end)
	{
		prev_free_block->size += free_block->size;
		prev_free_block->next = free_block->next;
	}

	if(prev_free_block->size == _block_size)
	{
		_backing_allocator.deallocate(prev_free_block);
		_size -= _block_size;
	}

	_num_allocations--;
	_used_memory -= block_size;
}

SmallBlockAllocator::NewBlock SmallBlockAllocator::allocateNewBlock()
{
	NewBlock new_block;

	void* block = _backing_allocator.allocate(_block_size, _block_alignment);

	if(block == nullptr)
	{
		new_block.block = nullptr;
		new_block.prev = nullptr;
	}
	else
	{
		new_block.block       = (FreeBlock*)block;
		new_block.block->size = _block_size;

		_size += _block_size;

		FreeBlock* prev_free_block = nullptr;
		FreeBlock* free_block      = _free_blocks;

		while(free_block != nullptr)
		{
			if((uptr)free_block >= (uptr)new_block.block)
				break;

			prev_free_block = free_block;
			free_block      = free_block->next;
		}

		if(prev_free_block == nullptr)
		{
			new_block.block->next = _free_blocks;

			_free_blocks = new_block.block;
		}
		else
		{
			new_block.block->next = prev_free_block->next;
			prev_free_block->next = new_block.block;
		}

		new_block.prev = prev_free_block;
	}

	return new_block;
}