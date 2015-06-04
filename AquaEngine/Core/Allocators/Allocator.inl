#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015              
/////////////////////////////////////////////////////////////////////////////////////////////

namespace allocator
{
	template <class T, class... Args>
	T* allocateNew(Allocator& allocator, Args&&... args)
	{
		return new (allocator.allocate(sizeof(T), __alignof(T))) T(std::forward<Args>(args)...);
	}

	template <class T>
	void deallocateDelete(Allocator& allocator, T* object)
	{
		object->~T();
		allocator.deallocate(object);
	}

	template <class T>
	T* allocateArray(Allocator& allocator, size_t length)
	{
		ASSERT(length != 0);

		u8 header_size = sizeof(size_t) / sizeof(T);

		if(sizeof(size_t) % sizeof(T) > 0)
			header_size += 1;

		//Allocate extra space to store array length in the bytes before the array
		T* p = ((T*)allocator.allocate(sizeof(T)*(length + header_size), __alignof(T))) + header_size;

		*(((size_t*)p) - 1) = length;

		for(size_t i = 0; i < length; i++)
			new (&p[i]) T;

		return p;
	}

	template <class T>
	T* allocateArrayNoConstruct(Allocator& allocator, size_t length)
	{
		ASSERT(length != 0);

		u8 header_size = sizeof(size_t) / sizeof(T);

		if(sizeof(size_t) % sizeof(T) > 0)
			header_size += 1;

		//Allocate extra space to store array length in the bytes before the array
		T* p = ((T*)allocator.allocate(sizeof(T)*(length + header_size), __alignof(T))) + header_size;

		*(((size_t*)p) - 1) = length;

		return p;
	}

	template <class T>
	void deallocateArray(Allocator& allocator, T* array)
	{
		ASSERT(array != nullptr);

		size_t length = *(((size_t*)array) - 1);

		//should be reverse order
		for(size_t i = 0; i < length; i++)
			array[i].~T();

		//Calculate how much extra memory was allocated to store the length before the array
		u8 header_size = sizeof(size_t) / sizeof(T);

		if(sizeof(size_t) % sizeof(T) > 0)
			header_size += 1;

		allocator.deallocate(array - header_size);
	}

	template <class T>
	void deallocateArrayNoDestruct(Allocator& allocator, T* array)
	{
		ASSERT(array != nullptr);

		//Calculate how much extra memory was allocated to store the length before the array
		u8 header_size = sizeof(size_t) / sizeof(T);

		if(sizeof(size_t) % sizeof(T) > 0)
			header_size += 1;

		allocator.deallocate(array - header_size);
	}
};