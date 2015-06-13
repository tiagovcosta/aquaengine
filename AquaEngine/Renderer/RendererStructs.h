#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "RenderDevice\RenderDeviceTypes.h"
#include "RenderDevice\RenderDeviceEnums.h"

#include "..\Core\Allocators\Allocator.h"

#include "..\AquaTypes.h"

namespace aqua
{
	struct ShaderPermutationPass;
	class CachedParameterGroup;
	class ParameterGroup;

	struct DrawDirect
	{
		u32  count;
		u32  start_index;
		u32  start_vertex;
	};

	struct DrawIndirect
	{
		BufferH args_buffer;
		u32     args_offset;
	};

	struct DrawCall
	{
		union
		{
			DrawDirect direct;
			DrawIndirect indirect;
		};

		bool is_indexed;
		bool is_indirect;
	};

	inline DrawCall createDrawCall(bool indexed, u32 count, u32 start_vertex, u32 start_index)
	{
		DrawCall out;

		out.is_indexed          = indexed;
		out.is_indirect         = false;
		out.direct.count        = count;
		out.direct.start_vertex = start_vertex;
		out.direct.start_index  = start_index;

		return out;
	}

	inline DrawCall createDrawCallIndirect(bool indexed, BufferH args_buffer, u32 args_offset)
	{
		DrawCall out;

		out.is_indexed           = indexed;
		out.is_indirect          = true;
		out.indirect.args_buffer = args_buffer;
		out.indirect.args_offset = args_offset;

		return out;
	}

	struct VertexBuffer
	{
		BufferH buffer;
		u32     stride;
		u32     offset;
	};

	//Create using 'createMesh' function
	struct Mesh
	{
		BufferH           index_buffer;
		u32               index_buffer_offset;
		IndexBufferFormat index_format;
		PrimitiveTopology topology;
		//VertexBuffer*     vertex_buffers;

		inline VertexBuffer& getVertexBuffer(u8 index)
		{
			return ((VertexBuffer*)(this + 1))[index];
		}

		inline const VertexBuffer& getVertexBuffer(u8 index) const
		{
			return ((VertexBuffer*)(this + 1))[index];
		}

	private:
		//Mesh() {};
	};

	inline Mesh* createMesh(u8 num_vertex_buffers, Allocator& allocator)
	{
		return (Mesh*)allocator.allocate(sizeof(Mesh) + sizeof(VertexBuffer)*num_vertex_buffers, __alignof(Mesh));
	}

	struct RenderItem
	{
		const DrawCall*				 draw_call;
		const ShaderPermutationPass* shader;
		const Mesh*					 mesh;
		//const ParameterGroup*		 material_params;
		const CachedParameterGroup*  material_params;
		//const ParameterGroup*		 instance_params;
		const CachedParameterGroup*  instance_params;
		u32							 num_instances;
	};

	struct SortItem
	{
		u64				  sort_key;
		const RenderItem* item;
	};

	struct RenderQueue
	{
		//RenderItem* render_items;
		SortItem* sort_items;
		u32       size;
	};

	struct Viewport
	{
		float x;
		float y;
		float width;
		float height;
	};
};