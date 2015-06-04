#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\..\Utilities\PointerMath.h"

#include "..\..\Utilities\Debug.h"

#include <new>
#include <functional>

namespace aqua
{
	static const u8 DEFAULT_ALIGNMENT = 8;

	class Allocator
	{
	public:
		Allocator(size_t size);
		virtual ~Allocator();

		virtual void* allocate(size_t size, u8 alignment = DEFAULT_ALIGNMENT) = 0;

		virtual void deallocate(void* p) = 0;

		//virtual void check(void* p) = 0;

		size_t getSize() const;
		size_t getUsedMemory() const;
		size_t getNumAllocations() const;

	protected:
		Allocator(const Allocator&);
		Allocator& operator=(Allocator&);

		size_t        _size;

		size_t        _used_memory;
		size_t        _num_allocations;
	};

	namespace allocator
	{
		template <class T, class... Args>
		T* allocateNew(Allocator& allocator, Args&&... args);

		template<class T>
		void deallocateDelete(Allocator& allocator, T* object);

		template<class T>
		T* allocateArray(Allocator& allocator, size_t length);

		template<class T>
		T* allocateArrayNoConstruct(Allocator& allocator, size_t length);

		template<class T>
		void deallocateArray(Allocator& allocator, T* array);

		template<class T>
		void deallocateArrayNoDestruct(Allocator& allocator, T* array);
	};

#include "Allocator.inl"
};