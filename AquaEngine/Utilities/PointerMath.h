#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2014             
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\AquaTypes.h"

namespace aqua
{
	//Declarations
	namespace pointer_math
	{
		bool        isAligned(const void* address, u8 alignment);

		template<class T>
		bool		isAligned(const T* address);

		void*       alignForward(void* address, u8 alignment);
		const void* alignForward(const void* address, u8 alignment);

		void*       alignBackward(void* address, u8 alignment);
		const void* alignBackward(const void* address, u8 alignment);

		u8          alignForwardAdjustment(const void* address, u8 alignment);
		//u8          alignForwardAdjustmentWithHeader(const void* address, u8 alignment, u8 header_size);
		template<typename T>
		u8			alignForwardAdjustmentWithHeader(const void* address, u8 alignment);

		u8          alignBackwardAdjustment(const void* address, u8 alignment);

		void*       add(void* p, size_t x);
		const void* add(const void* p, size_t x);

		void*       subtract(void* p, size_t x);
		const void* subtract(const void* p, size_t x);
	}

	//Inline Definitions
	namespace pointer_math
	{
		inline bool isAligned(const void* address, u8 alignment)
		{
			return alignForwardAdjustment(address, alignment) == 0;
		}

		template<class T>
		inline bool isAligned(const T* address)
		{
			return alignForwardAdjustment(address, __alignof(T)) == 0;
		}

		inline void* alignForward(void* address, u8 alignment)
		{
			return (void*)((reinterpret_cast<uptr>(address)+static_cast<uptr>(alignment - 1)) & static_cast<uptr>(~(alignment - 1)));
		}

		inline const void* alignForward(const void* address, u8 alignment)
		{
			return (void*)((reinterpret_cast<uptr>(address)+static_cast<uptr>(alignment - 1)) & static_cast<uptr>(~(alignment - 1)));
		}

		inline void* alignBackward(void* address, u8 alignment)
		{
			return (void*)(reinterpret_cast<uptr>(address)& static_cast<uptr>(~(alignment - 1)));
		}

		inline const void* alignBackward(const void* address, u8 alignment)
		{
			return (void*)(reinterpret_cast<uptr>(address)& static_cast<uptr>(~(alignment - 1)));
		}

		inline u8 alignForwardAdjustment(const void* address, u8 alignment)
		{
			u8 adjustment = alignment - (reinterpret_cast<uptr>(address)& static_cast<uptr>(alignment - 1));

			if(adjustment == alignment)
				return 0; //already aligned

			return adjustment;
		}
		/*
		inline u8 alignForwardAdjustmentWithHeader(const void* address, u8 alignment, u8 header_size)
		{
			u8 adjustment = alignForwardAdjustment(address, alignment);

			u8 needed_space = header_size;

			if(adjustment < needed_space)
			{
				needed_space -= adjustment;

				//Increase adjustment to fit header
				adjustment += alignment * (needed_space / alignment);

				if(needed_space % alignment > 0)
					adjustment += alignment;
			}

			return adjustment;
		}
		*/
		template<typename T>
		inline u8 alignForwardAdjustmentWithHeader(const void* address, u8 alignment)
		{
			if(__alignof(T) > alignment)
				alignment = __alignof(T);

			u8 adjustment = sizeof(T) + alignForwardAdjustment(pointer_math::add(address, sizeof(T)), alignment);

			return adjustment;
		}

		inline u8 alignBackwardAdjustment(const void* address, u8 alignment)
		{
			u8 adjustment = reinterpret_cast<uptr>(address)& static_cast<uptr>(alignment - 1);

			if(adjustment == alignment)
				return 0; //already aligned

			return adjustment;
		}

		inline void* add(void* p, size_t x)
		{
			return (void*)(reinterpret_cast<uptr>(p)+x);
		}

		inline const void* add(const void* p, size_t x)
		{
			return (const void*)(reinterpret_cast<uptr>(p)+x);
		}

		inline void* subtract(void* p, size_t x)
		{
			return (void*)(reinterpret_cast<uptr>(p)-x);
		}

		inline const void* subtract(const void* p, size_t x)
		{
			return (const void*)(reinterpret_cast<uptr>(p)-x);
		}
	}
}