#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2014
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\Utilities\Debug.h"

#include "..\AquaTypes.h"

namespace aqua
{
	class BinaryReader
	{
	public:
		BinaryReader(const void* data, size_t size);
		~BinaryReader();

		const void* getData() const;
		size_t      getSize() const;

		int         setPosition(size_t pos);
		size_t      getPosition() const;

		const char* nextString();
		const void* nextRawData(size_t size);

		template<typename T>
		T next()
		{
			ASSERT((size_t)(_current_pos + sizeof(T) - _data) <= _size);

			T x = *(T*)_current_pos;
			_current_pos += sizeof(T);
			return x;
		}

		template<typename T>
		T* nextArray(size_t count)
		{
			ASSERT((size_t)(_current_pos + sizeof(T) * count - _data) <= _size);

			T* x = (T*)_current_pos;
			_current_pos += sizeof(T) * count;
			return x;
		}

	private:
		const char*  _data;
		const char*  _current_pos;
		size_t       _size;
	};
};