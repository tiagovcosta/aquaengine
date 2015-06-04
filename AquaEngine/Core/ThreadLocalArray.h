#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\Core\Allocators\Allocator.h"
#include "..\AquaTypes.h"

namespace aqua
{
	extern __declspec(thread) aqua::u8 THREAD_ID;

	template<typename T>
	class ThreadLocalArray
	{
	public:
		ThreadLocalArray(Allocator& allocator, u8 num_threads) : _allocator(allocator)
		{
			static const u8 CACHE_LINE_SIZE = 64;

			u8 diff = sizeof(T) % CACHE_LINE_SIZE;

			_stride = sizeof(T);
			
			if(diff != 0)
				_stride += CACHE_LINE_SIZE - diff;

			_array = (u8*)_allocator.allocate(_stride*num_threads, CACHE_LINE_SIZE);
		}

		~ThreadLocalArray()
		{
			_allocator.deallocate(_array);
		}

		T& operator[](u32 thread_id)
		{
			return *(T*)(_array + _stride*thread_id);
		}

		const T& operator[](u32 thread_id) const
		{
			return *(T*)(_array + _stride*thread_id);
		}

		T& get()
		{
			return *(T*)(_array + _stride*THREAD_ID);
		}

		const T& get() const
		{
			return *(T*)(_array + _stride*THREAD_ID);
		}


	private:

		Allocator& _allocator;
		u8*		   _array;
		u32		   _stride;

	};
};