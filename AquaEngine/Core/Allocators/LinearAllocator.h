#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2014
/////////////////////////////////////////////////////////////////////////////////////////////

#include "Allocator.h"

#include "..\..\AquaTypes.h"

namespace aqua
{
	class LinearAllocator : public Allocator
	{
	public:

		LinearAllocator(size_t size, void* start);
		//virtual ~LinearAllocator();

		//NoOp - Use rewind() or clear()
		void  deallocate(void* p) override final;

		void* getStart() const;
		void* getMark() const;

		virtual void rewind(void* mark) = 0;
		virtual void clear() = 0;

	protected:

		void* _start;
		void* _current_pos;
	};
};