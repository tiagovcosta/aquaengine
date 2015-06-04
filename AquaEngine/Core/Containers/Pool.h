#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2014              
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\Allocators\Allocator.h"

namespace aqua
{
	template <class T>
	class Pool
	{
	public:
		Pool(size_t size, Allocator& allocator);
		~Pool();

		T* get();

		void free(T* p);

	private:
		Pool(const Pool&);
		Pool& operator=(const Pool&);

		Allocator& _allocator;

		T*     _start;
		T**    _free_list;
		size_t _size;
	};

	//Definitions

	template<class T>
	Pool<T>::Pool(size_t size, Allocator& allocator) : _allocator(allocator), _size(size)
	{
		static_assert(sizeof(T) >= sizeof(void*));

		_free_list = allocator::allocateArray<T>(_allocator, size);
		_start = _free_list;

		T** p = _free_list;

		//Initialize free list
		for(size_t i = 0; i < size - 1; i++)
		{
			*p = ((T*)p) + 1;
			p = (T**)*p;
		}

		*p = nullptr;
	}

	template <class T>
	Pool<T>::~Pool()
	{
		_free_list = nullptr;
	}

	template <class T>
	T* Pool<T>::get()
	{
		if(_free_list == nullptr)
			return nullptr;

		T* p = _free_list;

		_free_list = (T**)(*_free_list);

		return p;
	}

	template <class T>
	void Pool<T>::free(T* p)
	{
		ASSERT(p >= _start && p < _start + _size);
		*((T**)p) = _free_list;

		_free_list = (T**)p;
	}
};