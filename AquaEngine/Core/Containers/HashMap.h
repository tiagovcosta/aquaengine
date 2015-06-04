#pragma once

#include "Array.h"

#include "..\..\AquaTypes.h"

namespace aqua
{
	template<class K, class V>
	class HashMap
	{
	public:
		HashMap(Allocator& allocator);

		bool has(K key) const;

		// Returns the value stored for the specified key, or default if the key
		// does not exist in the hash.
		const V& lookup(K key, const V& default) const;

		void insert(K key, const V& value);

		bool remove(K key);

		void reserve(size_t size);

		void clear();

		//using Iterator = size_t;

		struct Iterator
		{
			size_t index;
			K      key;
			V*     value;
		};

		Iterator begin();
		Iterator next(const Iterator& it);
		Iterator end();

	private:

		void rehash(size_t size);

		enum State : u8
		{
			EMPTY,
			FILLED,
			REMOVED
		};

		struct Bucket
		{
			u8 hash : 6;
			u8 state : 2;
		};

		static_assert(sizeof(Bucket) == sizeof(u8), "Check Bucket struct size");

		Allocator*    _allocator;
		Array<Bucket> _buckets;
		Array<K>      _keys;
		Array<V>      _values;

		size_t        _size; //Number of used entries in the table
	};

	template<class K, class V>
	HashMap<K, V>::HashMap(Allocator& allocator)
		: _allocator(&allocator), _buckets(allocator), _keys(allocator), _values(allocator), _size(0)
	{

	}

	template<class K, class V>
	bool HashMap<K, V>::has(K key) const
	{
		size_t size  = _buckets.size();

		if(size == 0)
			return false;

		size_t start = key % size;

		u8 hash6 = key & 0x3F;

		// Search the buckets until we hit an empty one
		for(size_t i = start; i < size; ++i)
		{
			const Bucket* bucket = &_buckets[i];

			switch(bucket->state)
			{
			case State::EMPTY:
				return false;

			case State::FILLED:
				if(hash6 == bucket->hash && _keys[i] == key)
					return true;
				break;
			default:
				break;
			}
		}
		for(size_t i = 0; i < start; ++i)
		{
			const Bucket* bucket = &_buckets[i];

			switch(bucket->state)
			{
			case State::EMPTY:
				return false;

			case State::FILLED:
				if(hash6 == bucket->hash && _keys[i] == key)
					return true;
				break;
			default:
				break;
			}
		}

		return false;
	}

	template<class K, class V>
	const V& HashMap<K, V>::lookup(K key, const V& default) const
	{
		size_t size = _buckets.size();

		if(size == 0)
			return default;

		size_t start = key % size;

		u8 hash6 = key & 0x3F;

		// Search the buckets until we hit an empty one
		for(size_t i = start; i < size; ++i)
		{
			const Bucket* bucket = &_buckets[i];

			switch(bucket->state)
			{
			case State::EMPTY:
				return default;

			case State::FILLED:
				if(hash6 == bucket->hash && _keys[i] == key)
					return _values[i];
				break;
			default:
				break;
			}
		}

		for(size_t i = 0; i < start; ++i)
		{
			const Bucket* bucket = &_buckets[i];

			switch(bucket->state)
			{
			case State::EMPTY:
				return default;

			case State::FILLED:
				if(hash6 == bucket->hash && _keys[i] == key)
					return _values[i];
				break;
			default:
				break;
			}
		}

		return default;
	}

	template<class K, class V>
	void HashMap<K, V>::insert(K key, const V& value)
	{
		size_t buckets_size = _buckets.size();

		// Resize larger if the load factor goes over 2/3
		if(_size * 3 >= buckets_size * 2)
		{
			rehash(buckets_size * 2 + 8);

			buckets_size = _buckets.size();
		}
		
		size_t start = key % buckets_size;

		u8 hash6 = key & 0x3F;

		// Search for an unused bucket
		Bucket* bucket      = nullptr;
		size_t bucket_index = 0;

		for(size_t i = start; i < buckets_size; ++i)
		{
			Bucket* b = &_buckets[i];

			if(b->state != State::FILLED)
			{
				bucket       = b;
				bucket_index = i;
				break;
			}
		}

		if(bucket == nullptr)
		{
			for(size_t i = 0; i < start; ++i)
			{
				Bucket* b = &_buckets[i];

				if(b->state != State::FILLED)
				{
					bucket       = b;
					bucket_index = i;
					break;
				}
			}
		}

		assert(bucket != nullptr);

		// Store the hash, key, and value in the bucket
		bucket->hash          = hash6;
		bucket->state         = State::FILLED;
		_keys[bucket_index]   = key;
		_values[bucket_index] = value;

		++_size;
	}

	template<class K, class V>
	bool HashMap<K, V>::remove(K key)
	{
		size_t size = _buckets.size();

		if(size == 0)
			return false;

		size_t start = key % size;

		u8 hash6 = key & 0x3F;

		// Search the buckets until we hit an empty one
		for(size_t i = start; i < size; ++i)
		{
			Bucket* b = &_buckets[i];

			switch(b->state)
			{
			case State::EMPTY:
				return false;
			case State::FILLED:
				if(b->hash == hash6 && _keys[i] == key)
				{
					b->hash = 0;
					b->state = State::REMOVED;
					--_size;
					return true;
				}
				break;
			default:
				break;
			}
		}

		for(size_t i = 0; i < start; ++i)
		{
			Bucket* b = &_buckets[i];

			switch(b->state)
			{
			case State::EMPTY:
				return false;
			case State::FILLED:
				if(b->hash == hash6 && _keys[i] == key)
				{
					b->hash = 0;
					b->state = State::REMOVED;
					--_size;
					return true;
				}
				break;
			default:
				break;
			}
		}

		return false;
	}

	template<class K, class V>
	void HashMap<K, V>::reserve(size_t size)
	{
		rehash(size);
	}

	template<class K, class V>
	void HashMap<K, V>::clear()
	{
		_buckets.clear();
		_keys.clear();
		_values.clear();
	}

	template<class K, class V>
	void HashMap<K,V>::rehash(size_t size)
	{
		// Can't rehash down to smaller than current size or initial size
		//bucketCountNew = std::max(std::max(bucketCountNew, size),
		//	size_t(s_hashTableInitialSize));

		size = size > _size ? size : _size;

		// Build a new set of buckets, keys, and values
		Array<Bucket> buckets_new(*_allocator, size);
		Array<K> keys_new(*_allocator, size);
		Array<V> values_new(*_allocator, size);

		Bucket temp;
		temp.state = State::EMPTY;

		buckets_new.resize(size, temp);
		keys_new.resize(size);
		values_new.resize(size);

		// Walk through all the current elements and insert them into the new buckets
		for(size_t i = 0, end = _buckets.size(); i < end; ++i)
		{
			Bucket* b = &_buckets[i];

			if(b->state != State::FILLED)
				continue;

			K key = _keys[i];

			// Hash the key and find the starting bucket
			size_t start = key % size;

			u8 hash6 = key & 0x3F;

			// Search for an unused bucket
			Bucket* bucket      = nullptr;
			size_t bucket_index = 0;

			for(size_t j = start; j < size; ++j)
			{
				Bucket* b = &buckets_new[j];

				if(b->state != State::FILLED)
				{
					bucket       = b;
					bucket_index = j;
					break;
				}
			}

			if(bucket == nullptr)
			{
				for(size_t j = 0; j < start; ++j)
				{
					Bucket* b = &buckets_new[j];

					if(b->state != State::FILLED)
					{
						bucket       = b;
						bucket_index = j;
						break;
					}
				}
			}

			assert(bucket != nullptr);

			// Store the hash, key, and value in the bucket
			bucket->hash            = hash6;
			bucket->state           = State::FILLED;
			keys_new[bucket_index]   = key;
			values_new[bucket_index] = std::move(_values[i]);
		}

		// Swap the new buckets, keys, and values into place
		using std::swap;

		swap(_buckets, buckets_new);
		swap(_keys, keys_new);
		swap(_values, values_new);
	}

	template<class K, class V>
	typename HashMap<K, V>::Iterator HashMap<K, V>::begin()
	{
		size_t i         = 0;
		const size_t end = _buckets.size();

		for(; i < end; ++i)
		{
			if(_buckets[i].state == State::FILLED)
				break;
		}

		if(i == end)
			return{ end, 0, nullptr };

		return{ i, _keys[i], &_values[i] };
	}

	template<class K, class V>
	typename HashMap<K, V>::Iterator HashMap<K, V>::next(const Iterator& it)
	{
		size_t i         = it.index + 1;
		const size_t end = _buckets.size();

		for(; i < end; ++i)
		{
			if(_buckets[i].state == State::FILLED)
				break;
		}

		if(i == end)
			return{ end, 0, nullptr };

		return{ i, _keys[i], &_values[i] };
	}

	template<class K, class V>
	typename HashMap<K, V>::Iterator HashMap<K, V>::end()
	{
		return{ _buckets.size(), 0, nullptr };
	}
}