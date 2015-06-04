// Simplified scope stack implementation

#include "LinearAllocator.h"
#include "..\..\AquaTypes.h"

namespace aqua
{
	struct Finalizer
	{
		void(*fn)(void *ptr);
		void*      obj;
		Finalizer* chain;
	};

	template <typename T>
	void destructorCall(void *ptr)
	{
		static_cast<T*>(ptr)->~T();
	}

	class ScopeStack
	{
	public:
		explicit ScopeStack(LinearAllocator& a)
			: _allocator(a), _rewind_point(a.getMark()), _finalizer_chain(nullptr)
		{}

		~ScopeStack()
		{
			for(Finalizer* f = _finalizer_chain; f; f = f->chain)
			{
				(*f->fn)(f->obj);
			}

			_allocator.rewind(_rewind_point);
		}

		template <typename T>
		T* newObject()
		{
			Finalizer* f = (Finalizer*)_allocator.allocate(sizeof(Finalizer), __alignof(Finalizer));

			T* obj       = new (_allocator.allocate(sizeof(T), __alignof(T))) T;

			// Link this finalizer onto the chain.
			f->fn            = &destructorCall<T>;
			f->obj           = obj;
			f->chain         = _finalizer_chain;
			_finalizer_chain = f;

			return obj;
		}

		template <typename T>
		T* newObjectArray(size_t count)
		{
			Finalizer* f = (Finalizer*)_allocator.allocate(sizeof(Finalizer)*count, __alignof(Finalizer));

			T* objs      = (T*)_allocator.allocate(sizeof(T)*count, __alignof(T));

			for(size_t i = 0; i < count; i++)
				new (&objs[i]) T;

			// Link this finalizers onto the chain.
			for(size_t i = 0; i < count; u++)
			{
				f[i].fn          = &destructorCall<T>;
				f[i].obj         = &objs[i];
				f[i].chain       = _finalizer_chain;
				_finalizer_chain = &f[i];
			}

			return objs;
		}

		template <typename T>
		T* newPOD()
		{
			return allocator::allocateNew<T>(_allocator);
		}

		template <typename T>
		T* newPODArray(size_t count)
		{
			return allocator::allocateArray<T>(_allocator, count);
		}

		// C++ suckage: need overloads ov newObject() for many arguments..
	private:
		LinearAllocator& _allocator;
		void*            _rewind_point;
		Finalizer*       _finalizer_chain;
	};
}