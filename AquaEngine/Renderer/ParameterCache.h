#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\AquaTypes.h"
//#include "..\..\Utilities\Debug.h"

namespace aqua
{
	class Allocator;

	struct ParameterCache
	{
		u32* names;
		u32* offsets;
		u8*  buffer;

		u32  buffer_size;
		u8   num_parameters;

	private:
		ParameterCache(){};

		friend class ParametersManager;
	};

	static_assert(sizeof(ParameterCache) <= 64, "ParameterCache larger than cache line. Is this ok?");

	class ParametersManager
	{
	public:
		ParametersManager(Allocator& allocator);

		ParameterCache createParameterCache();
		ParameterCache cloneParameterCache(const ParameterCache& cache);

		template<typename T>
		void setParameter(ParameterCache& cache, u32 name, const T& value)
		{
			for(u8 i = 0; i < cache.num_parameters; i++)
			{
				if(cache.names[i] == name)
				{
					T* cached_value = (T*)(cache.buffer + cache.offsets[i]);

					*cached_value = value;

					return;
				}
			}

			static_assert(__alignof(T) <= BUFFER_ALIGNMENT, "Check parameter cache buffer aligment");

			increaseCapacity(cache, sizeof(T));

			cache.names[cache.num_parameters - 1] = name;

			T* cached_value = (T*)(cache.buffer + cache.offsets[cache.num_parameters - 1]);

			*cached_value = value;
		}

		template<typename T>
		T* getParameter(const ParameterCache& cache, u32 name) const
		{
			for(u8 i = 0; i < cache.num_parameters; i++)
			{
				if(cache.names[i] == name)
				{
					return (T*)(cache.buffer + cache.offsets[i]);
				}
			}

			return nullptr;
		}

	private:

		static const u8 BUFFER_ALIGNMENT = 8;

		void increaseCapacity(ParameterCache& cache, u32 new_parameter_size);

		Allocator& _allocator;
	};
}