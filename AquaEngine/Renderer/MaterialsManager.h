#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "RenderDevice\ParameterGroup.h"

#include "..\AquaTypes.h"
//#include "..\..\Utilities\Debug.h"

namespace aqua
{
	class Allocator;

	class MaterialsManager
	{
	public:
		MaterialsManager(Allocator& allocator, const ParameterGroupDesc& desc);

		//ParameterCache createParameterCache();
		//ParameterCache cloneParameterCache(const ParameterCache& cache);

		template<typename T>
		bool setParameter(ParameterGroup& group, u32 name, const T& value)
		{
			u32 offset = _desc.getConstantOffset(name);

			if(offset != UINT32_MAX)
				return false; // Invalid parameter name

			char* cbuffer_data = (char*)group.getCBuffersData();

			*(T*)(cbuffer_data + offset) = value;

			return true;
		}

		template<typename T>
		T* getParameter(const ParameterGroup& cache, u32 name) const
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

		//void increaseCapacity(ParameterCache& cache, u32 new_parameter_size);

		Allocator& _allocator;
		ParameterGroupDesc& _desc;
	};
}