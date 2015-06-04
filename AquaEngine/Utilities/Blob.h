#pragma once

#include "..\AquaTypes.h"

namespace aqua
{
	template<class T>
	size_t calcBlobSize(size_t num, u8 alignment)
	{
		return num * sizeof(T);
	}

	template<class T, class V, class... U>
	size_t calcBlobSize(size_t num, u8 alignment)
	{
		size_t size = num * sizeof(T);

		u8 diff = size % alignment;

		if(diff != 0)
		{
			size += alignment - diff;
		}

		return size + calcBlobSize<V, U...>(num, alignment);
	}

	template<class T>
	void setupBlob(void* buffer, size_t num, u8 alignment, T*& x)
	{
		x = (T*)buffer;
	}

	template<class T, class V, class... U>
	void setupBlob(void* buffer, size_t num, u8 alignment, T*& x, V*& y, U*&... rest)
	{
		x = (T*)buffer;

		buffer = pointer_math::add(buffer, num*sizeof(T));
		buffer = pointer_math::alignForward(buffer, alignment);

		setupBlob(buffer, num, alignment, y, rest...);
	}
	/*
	template<class... T>
	void* allocateBlob(Allocator& allocator, size_t num, T*&... args)
	{
		size_t size = calcBlobSize<T...>(num, 16);

		void* buffer = allocator.allocate(size, 16);

		setupBlob(buffer, num, 16, args...);

		return buffer;
	}
	*/
	template<class T, class... U>
	void* allocateBlob(Allocator& allocator, size_t num, T*& arg1, U*&... args)
	{
		size_t size = calcBlobSize<T, U...>(num, __alignof(T));

		void* buffer = allocator.allocate(size, __alignof(T));

		setupBlob(buffer, num, __alignof(T), arg1, args...);

		return buffer;
	}
};