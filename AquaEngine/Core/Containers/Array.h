#pragma once

#include "..\Allocators\Allocator.h"

#include <algorithm> // std::copy

namespace aqua
{
	template<class T>
	class Array
	{
	public:
		Array(Allocator& allocator, size_t capacity = 0);
		Array(const Array& other);
		Array(Array&& other);

		~Array();

		Array& operator=(Array other);

		template<class T>
		friend void swap(Array<T>& x, Array<T>& y);

		T& operator[](size_t i);
		const T& operator[](size_t i) const;

		void push(T value);
		void push(size_t count, const T* values);
		void pop();

		void clear();

		bool reserve(size_t new_capacity);
		bool resize(size_t new_size);
		bool resize(size_t new_size, const T& value);
		bool setCapacity(size_t new_capacity);

		bool grow(size_t min_capacity = 0);

		bool   empty() const;
		size_t size() const;
		size_t capacity() const;

	private:
		Allocator* _allocator; //must use pointer instead of reference for swap to work
		T*         _data;
		size_t	   _size;
		size_t	   _capacity;
	};

	template<class T>
	Array<T>::Array(Allocator& allocator, size_t capacity)
		: _allocator(&allocator),
		  _data(capacity > 0 ? (T*)allocator.allocate(sizeof(T)*capacity, __alignof(T)) : nullptr),
		  _size(0), _capacity(capacity)
	{

	}

	template<class T>
	Array<T>::Array(const Array<T>& other)
		: _allocator(other._allocator),
		  _data(other._capacity > 0 ? (T*)other._allocator->allocate(sizeof(T)*other._capacity, __alignof(T)) : nullptr),
		  _size(other._size), _capacity(other._capacity)
	{
		std::copy(other._data, other._data + _size, _data);
	}

	template<class T>
	Array<T>::Array(Array<T>&& other) : Array(*other._allocator, 0)
	{
		swap(*this, other);
	}

	template<class T>
	Array<T>::~Array()
	{
		/*if(_data != nullptr)
			allocator::deallocateArray(_allocator, _data);*/

		if(_data != nullptr)
			_allocator->deallocate(_data);

		//_data = nullptr;
	}

	template<class T>
	Array<T>& Array<T>::operator=(Array<T> other)
	{
		swap(*this, other);

		return *this;
	}

	template<class T>
	void swap(Array<T>& x, Array<T>& y)
	{
		using std::swap;

		swap(x._allocator, y._allocator);
		swap(x._data, y._data);
		swap(x._size, y._size);
		swap(x._capacity, y._capacity);
	}

	template<class T>
	T& Array<T>::operator[](size_t i)
	{
		return _data[i];
	}

	template<class T>
	const T& Array<T>::operator[](size_t i) const
	{
		return _data[i];
	}

	template<class T>
	void Array<T>::push(T value)
	{
		if(_size + 1 > _capacity)
			grow();

		_data[_size++] = std::move(value);
	}

	template<class T>
	void Array<T>::push(size_t count, const T* values)
	{
		if(_size + count > _capacity)
			grow();

		size_t start_size = _size;

		for(size_t i = 0; i < count; i++)
			_data[start_size + i] = values[i];

		_size += count;
	}

	template<class T>
	void Array<T>::pop()
	{
		_size--;
	}

	template<class T>
	void Array<T>::clear()
	{
		resize(0);
	}

	template<class T>
	bool Array<T>::reserve(size_t new_capacity)
	{
		if(new_capacity > _capacity)
			return setCapacity(new_capacity);

		return true;
	}

	template<class T>
	bool Array<T>::resize(size_t new_size)
	{
		if(new_size > _capacity)
		{
			if(!grow(new_size))
				return false;
		}

		_size = new_size;

		return true;
	}

	template<class T>
	bool Array<T>::resize(size_t new_size, const T& value)
	{
		size_t size = _size;

		resize(new_size);

		for(size_t i = size; i < _size; i++)
			_data[i] = value;

		return true;
	}

	template<class T>
	bool Array<T>::setCapacity(size_t new_capacity)
	{
		if(new_capacity == _capacity)
			return true;

		if(new_capacity < _size)
			resize(new_capacity);

		T* new_data = nullptr;

		if(new_capacity > 0)
		{
			new_data = (T *)_allocator->allocate(sizeof(T)*new_capacity, __alignof(T));
			memcpy(new_data, _data, sizeof(T)*_size);
		}

		if(_data != nullptr)
			_allocator->deallocate(_data);

		_data     = new_data;
		_capacity = new_capacity;

		return true;
	}

	template<class T>
	bool Array<T>::grow(size_t min_capacity)
	{
		size_t new_capacity = _capacity * 2 + 8;

		if(new_capacity < min_capacity)
			new_capacity = min_capacity;

		return setCapacity(new_capacity);
	}

	template<class T>
	bool Array<T>::empty() const
	{
		return _size == 0;
	}

	template<class T>
	size_t Array<T>::size() const
	{
		return _size;
	}

	template<class T>
	size_t Array<T>::capacity() const
	{
		return _capacity;
	}
}