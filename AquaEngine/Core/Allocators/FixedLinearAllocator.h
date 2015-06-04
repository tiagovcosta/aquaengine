#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "LinearAllocator.h"

#include "..\..\AquaTypes.h"

namespace aqua
{
	class FixedLinearAllocator : public LinearAllocator
	{
	public:
		FixedLinearAllocator(size_t size, void* start);
		virtual ~FixedLinearAllocator();

		virtual void* allocate(size_t size, u8 alignment = DEFAULT_ALIGNMENT) override;

		virtual void  rewind(void* mark) override;

		void		  clear() override final;

	private:
		FixedLinearAllocator(const FixedLinearAllocator&);
		FixedLinearAllocator& operator=(const FixedLinearAllocator&);
	};

	namespace allocator
	{
		inline FixedLinearAllocator* newFixedLinearAllocator(size_t size, Allocator& allocator)
		{
			void* p = allocator.allocate(size + sizeof(FixedLinearAllocator), __alignof(FixedLinearAllocator));
			return new (p)FixedLinearAllocator(size, pointer_math::add(p, sizeof(FixedLinearAllocator)));
		}

		inline void deleteFixedLinearAllocator(Allocator& allocator, FixedLinearAllocator& linear_allocator)
		{
			linear_allocator.~FixedLinearAllocator();

			allocator.deallocate(&linear_allocator);
		}
	};
};