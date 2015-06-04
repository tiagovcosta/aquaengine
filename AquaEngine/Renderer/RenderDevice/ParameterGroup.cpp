#include "ParameterGroup.h"

using namespace aqua;

const u32* ParameterGroupDesc::getCBuffersSizes() const
{
	if(_num_cbuffers == 0)
		return nullptr;

	return (const u32*)pointer_math::add(this, _cbuffers_offset + _num_cbuffers*sizeof(u32));
}

u8 ParameterGroupDesc::getSRVIndex(u32 name) const
{
	auto srvs_names = (const u32*)pointer_math::add(this, _srvs_names_offset);

	for(u32 i = 0; i < _num_srvs; i++)
	{
		if(srvs_names[i] == name)
			return i;
	}

	return UINT8_MAX;
}

u8 ParameterGroupDesc::getUAVIndex(u32 name) const
{
	auto uavs_names = (const u32*)pointer_math::add(this, _uavs_names_offset);

	for(u32 i = 0; i < _num_uavs; i++)
	{
		if(uavs_names[i] == name)
			return i;
	}

	return UINT8_MAX;
}

u32 ParameterGroupDesc::getConstantOffset(u32 name) const
{
	auto constants_names = (const u32*)pointer_math::add(this, _constants_offset);
	auto constants_offsets = (const u32*)pointer_math::add(this, _constants_offset + _num_constants*sizeof(u32));

	for(u32 i = 0; i < _num_constants; i++)
	{
		if(constants_names[i] == name)
			return constants_offsets[i];
	}

	return UINT32_MAX;
}