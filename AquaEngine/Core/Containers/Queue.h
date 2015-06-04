#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2014              
/////////////////////////////////////////////////////////////////////////////////////////////

#include "Array.h"

#include "..\..\Utilities\Logger.h"

#include "..\Allocators\Allocator.h"

namespace aqua
{
	template<class T>
	class Queue
	{
	public:
		Queue(Allocator& allocator);
		~Queue();

		void push(T value);

		T& front();
		void pop();

		bool reserve(size_t capacity);

		bool   empty() const;
		size_t size() const;
		size_t capacity() const;

	private:
		void increaseCapacity(size_t new_capacity);
		void grow(size_t min_capacity = 0);

		Allocator& _allocator;
		Array<T>   _data;
		size_t	   _start;
		size_t	   _size;
	};

	template<class T>
	Queue<T>::Queue(Allocator& allocator)
		: _allocator(allocator), _data(allocator), _start(0), _size(0)
	{

	}

	template<class T>
	Queue<T>::~Queue()
	{
		/*
		if(_data != nullptr)
			allocator::deallocateArray(_allocator, _data);
		*/
	}

	template<class T>
	void Queue<T>::push(T value)
	{
		if(_size == _data.size())
			grow();

		size_t end = (_start + _size) % _data.size();
		_data[end] = value;

		_size++;
	}

	template<class T>
	T& Queue<T>::front()
	{
		return _data[_start];
	}

	template<class T>
	void Queue<T>::pop()
	{
		_start = (_start + 1) % _data.size();
		_size--;
	}

	template<class T>
	bool Queue<T>::reserve(size_t capacity)
	{
		if(capacity > _data.size())
			increaseCapacity(capacity);

		return true;
	}

	template<class T>
	bool Queue<T>::empty() const
	{
		return _size == 0;
	}

	template<class T>
	size_t Queue<T>::size() const
	{
		return _size;
	}

	template<class T>
	size_t Queue<T>::capacity() const
	{
		return _data.size();
	}

	template<typename T> 
	void Queue<T>::increaseCapacity(size_t new_capacity)
	{
		size_t end = _data.size();
		_data.resize(new_capacity);

		// eg:
		// |3|4|-|1|2| --> |3|4|-|1|2|-|-|-| --> |3|4|-|-|-|-|1|2|

		if(_start + _size > end)
		{
			size_t end_items = end - _start;
			memmove(&_data[0] + new_capacity - end_items, &_data[0] + _start, end_items * sizeof(T));
			_start += new_capacity - end;
		}
	}

	template<typename T> 
	void Queue<T>::grow(size_t min_capacity = 0)
	{
		size_t new_capacity = _data.size() * 2 + 8;

		if(new_capacity < min_capacity)
			new_capacity = min_capacity;

		increaseCapacity(new_capacity);
	}
	
	//-----------------------------------------------------------------
	//-- FIXED SIZE QUEUE
	//-----------------------------------------------------------------
	
	template<class T, size_t size>
	class FixedSizeQueue
	{
	public:
		FixedSizeQueue();
		~FixedSizeQueue();

		bool empty() const;

		void push(T value);

		size_t count() const;

		T pop();

	private:
		size_t _start;
		size_t _count;
		T      _data[size];
	};

	template<class T, size_t size>
	FixedSizeQueue<T, size>::FixedSizeQueue() : _start(0), _count(0)
	{

	}

	template<class T, size_t size>
	FixedSizeQueue<T, size>::~FixedSizeQueue()
	{

	}

	template<class T, size_t size>
	bool FixedSizeQueue<T, size>::empty() const
	{
		return _count == 0;
	}

	template<class T, size_t size>
	void FixedSizeQueue<T, size>::push(T value)
	{
		size_t end = (_start + _count) % size;
		_data[end] = value;

		if(_count == size)
		{
			_start = (_start + 1) % size; /* full, overwrite */
			Logger::get().write(MESSAGE_LEVEL::WARNING_MESSAGE, CHANNEL::GENERAL, "Queue: Overwriting last element!\n");
		}
		else
			_count++;
	}

	template<class T, size_t size>
	T FixedSizeQueue<T, size>::pop()
	{
		T& value = _data[_start];
		_start   = (_start + 1) % size;
		_count--;

		return value;
	}

	template<class T, size_t size>
	size_t FixedSizeQueue<T, size>::count() const
	{
		return _count;
	}
};