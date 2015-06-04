#include "BinaryReader.h"

#include "Debug.h"

using namespace aqua;

BinaryReader::BinaryReader(const void* data, size_t size)
	: _data((const char*)data), _current_pos((const char*)data), _size(size)
{

}

BinaryReader::~BinaryReader()
{
	_data = nullptr;
	_current_pos = nullptr;
	_size = 0;
}

const void* BinaryReader::getData() const
{
	return _data;
}

size_t BinaryReader::getSize() const
{
	return _size;
}

int BinaryReader::setPosition(size_t pos)
{
	if(pos < 0 || pos > _size)
	{
		ASSERT("Invalid position");
		return 0;
	}

	_current_pos = (const char*)((uptr)_data + pos);

	return 1;
}

size_t BinaryReader::getPosition() const
{
	return (uptr)_current_pos - (uptr)_data;
}

const char* BinaryReader::nextString()
{
	const char* x = _current_pos;

	_current_pos += strlen(x) + 1;

	return x;
}

const void* BinaryReader::nextRawData(size_t size)
{
	ASSERT(_current_pos + size - _data <= _size);

	const void* x = _current_pos;

	_current_pos += size;

	return x;
}