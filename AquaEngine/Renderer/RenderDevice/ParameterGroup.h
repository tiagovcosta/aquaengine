#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "RenderDeviceTypes.h"

#include "..\..\Core\Allocators\Allocator.h"

namespace aqua
{
	class ParameterGroupDesc
	{
	public:

		const u32* getCBuffersSizes() const;

		u8 getSRVIndex(u32 name) const;
		u8 getUAVIndex(u32 name) const;
		u32 getConstantOffset(u32 name) const;

		u8 getNumCBuffers() const
		{
			return _num_cbuffers;
		}

		u8 getNumSRVs() const
		{
			return _num_srvs;
		}

		u8 getNumUAVs() const
		{
			return _num_uavs;
		}

	private:

		ParameterGroupDesc(const ParameterGroupDesc&) = delete;
		ParameterGroupDesc& operator=(ParameterGroupDesc) = delete;

		u32 _cbuffers_offset;	//u32 array (names) + u32 array (sizes)
		u32 _srvs_names_offset;	//u32 array
		u32 _uavs_names_offset;	//u32 array
		u32 _constants_offset;	//u32 array (names) + u32 array (offsets)

		u8  _num_cbuffers;
		u8  _num_srvs;
		u8  _num_uavs;
		u8  _num_constants;
	};

	//struct CBufferMapInfoInternal
	struct CBufferMapInfo
	{
		u32 offset;
		u32 size;
	};

	struct CBufferMapInfo2
	{
		void* data;
		u32 size;
	};

	struct SRVMapInfo
	{
		ResourceH resource;
		u32 offset;
		u32 size;
	};

	struct CBuffer
	{
		BufferH cbuffer;
		u32     map_info;
	};

	struct SRV
	{
		ShaderResourceH srv;
		u32             map_info;
	};

	class ParameterGroup
	{
	public:

		u32 getSize() const
		{
			return size;
		}

		void* getCBuffersData()
		{
			auto data = (u8*)this;

			auto map_info_offset = ((CBuffer*)(data + cbuffers_offset))[0].map_info;

			return data + ((CBufferMapInfo*)(data + map_info_offset))->offset;

			//return data + ((CBufferMapInfo*)(data + ((CBuffer*)data + cbuffers_offset)[0].map_info))->offset;
		}

		CBuffer* getCBuffer(u8 index)
		{
			auto data = (u8*)this;

			return &((CBuffer*)(data + cbuffers_offset))[index];
		}

		const CBuffer* getCBuffer(u8 index) const
		{
			auto data = (u8*)this;

			return &((const CBuffer*)(data + cbuffers_offset))[index];
		}

		CBufferMapInfo2 getCBufferMapInfo(u32 map_info_offset) const
		{
			auto data = (u8*)this;

			auto map_info = (CBufferMapInfo*)(data + map_info_offset);

			return{ data + map_info->offset, map_info->size };
		}

		ShaderResourceH getSRV(u8 index) const
		{
			auto data = (u8*)this;

			return ((SRV*)(data + srvs_offset))[index].srv;
		}

		UnorderedAccessH getUAV(u8 index) const
		{
			auto data = (u8*)this;

			return ((UnorderedAccessH*)(data + uavs_offset))[index];
		}

		void setCBuffer(BufferH cbuffer, u8 index)
		{
			auto data = (u8*)this;

			((CBuffer*)(data + cbuffers_offset))[index].cbuffer = cbuffer;
		}

		void setSRV(ShaderResourceH srv, u8 index)
		{
			auto data = (u8*)this;

			((SRV*)(data + srvs_offset))[index].srv = srv;
		}

		void setUAV(UnorderedAccessH uav, u8 index)
		{
			auto data = (u8*)this;

			((UnorderedAccessH*)(data + uavs_offset))[index] = uav;
		}

	private:
		u32 size;
		u16 cbuffers_offset;
		u16 srvs_offset;
		u16 uavs_offset;

		friend class RenderDevice;
	};

	class CachedParameterGroup
	{
	public:

		u32 getSize() const
		{
			return size;
		}
		/*
		void* getCBuffersData()
		{
			auto data = (u8*)this;

			auto map_info_offset = ((CBuffer*)(data + cbuffers_offset))[0].map_info;

			return data + ((CBufferMapInfo*)(data + map_info_offset))->offset;

			//return data + ((CBufferMapInfo*)(data + ((CBuffer*)data + cbuffers_offset)[0].map_info))->offset;
		}
		*/
		const CBuffer* getCBuffer(u8 index) const
		{
			auto data = (u8*)this;

			return &((const CBuffer*)(data + cbuffers_offset))[index];
		}

		CBufferMapInfo2 getCBufferMapInfo(u32 map_info_offset) const
		{
			auto data = (u8*)this;

			auto map_info = (CBufferMapInfo*)(data + map_info_offset);

			return{ data + map_info->offset, map_info->size };
		}

		ShaderResourceH getSRV(u8 index) const
		{
			auto data = (u8*)this;

			return ((SRV*)(data + srvs_offset))[index].srv;
		}

		UnorderedAccessH getUAV(u8 index) const
		{
			auto data = (u8*)this;

			return ((UnorderedAccessH*)(data + uavs_offset))[index];
		}

	private:
		u32 size;
		u16 cbuffers_offset;
		u16 srvs_offset;
		u16 uavs_offset;

		friend class RenderDevice;
	};

	class ParameterGroupInstance
	{
	public:

		bool setSRV(u32 name, ShaderResourceH srv)
		{
			u8 index = _desc->getSRVIndex(name);

			if(index == UINT8_MAX)
				return false;

			_instance->setSRV(srv, index);

			return true;
		}

		bool setUAV(u32 name, UnorderedAccessH uav)
		{
			u8 index = _desc->getUAVIndex(name);

			if(index == UINT8_MAX)
				return false;

			_instance->setUAV(uav, index);

			return true;
		}

		template<typename T>
		bool setConstant(u32 name, const T& constant)
		{
			u32 offset = _desc->getConstantOffset(name);

			if(offset == UINT32_MAX)
				return false;

			*(T*)((u8*)_instance->getCBuffersData() + offset) = constant;

			return true;
		}

	private:

		ParameterGroupInstance(const ParameterGroupInstance&) = delete;
		ParameterGroupInstance& operator=(ParameterGroupInstance) = delete;

		const ParameterGroupDesc* _desc;
		ParameterGroup*			  _instance;
	};
}