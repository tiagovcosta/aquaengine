#include "LinearAllocator.h"
#include "..\..\Utilities\Debug.h"

using namespace aqua;

LinearAllocator::LinearAllocator(size_t size, void* start)
	: Allocator(size), _start(start), _current_pos(start)
{

}

void LinearAllocator::deallocate(void* p)
{
	//ASSERT("Cannot call deallocate on Linear Allocators" && false);
}

void* LinearAllocator::getStart() const
{
	return _start;
}

void* LinearAllocator::getMark() const
{
	return _current_pos;
}