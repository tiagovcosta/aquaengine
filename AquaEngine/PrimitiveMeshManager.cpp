#include "PrimitiveMeshManager.h"

#include "Renderer\RendererStructs.h"
#include "Renderer\RenderDevice\RenderDevice.h"

using namespace aqua;

struct VertexExtra
{
	Vector3 normal;
	Vector2 tex_coords;
};

PrimitiveMeshManager::PrimitiveMeshManager(Allocator& allocator, LinearAllocator& temp_allocator, RenderDevice& render_device)
	: _allocator(allocator), _temp_allocator(temp_allocator), _render_device(render_device)
{
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------

	static const u32 NUM_VERTICES = 4;

	Vector3 positions[NUM_VERTICES];
	VertexExtra extra[NUM_VERTICES];

	u32 index = 0;

	positions[index]        = Vector3(-0.5f, 0.0f, -0.5f);
	extra[index].normal     = VECTOR3_UP;
	extra[index].tex_coords = Vector2(0.0f, 0.0f);
	index++;
	positions[index]        = Vector3(-0.5f, 0.0f, 0.5f);
	extra[index].normal     = VECTOR3_UP;
	extra[index].tex_coords = Vector2(1.0f, 0.0f);
	index++;
	positions[index]        = Vector3(0.5f, 0.0f, 0.5f);
	extra[index].normal     = VECTOR3_UP;
	extra[index].tex_coords = Vector2(1.0f, 1.0f);
	index++;
	positions[index]        = Vector3(0.5f, 0.0f, -0.5f);
	extra[index].normal     = VECTOR3_UP;
	extra[index].tex_coords = Vector2(0.0f, 1.0f);
	index++;

	u16 indices[] =
	{
		0, 1, 2,
		0, 2, 3
	};

	_plane.mesh                      = createMesh(2, _allocator);
	_plane.mesh->index_buffer_offset = 0;
	_plane.mesh->index_format        = IndexBufferFormat::UINT16;
	_plane.mesh->topology            = PrimitiveTopology::TRIANGLE_LIST;

	//Position buffer
	BufferDesc desc;
	desc.bind_flags    = (u8)BufferBindFlag::VERTEX;
	desc.draw_indirect = false;
	desc.num_elements  = NUM_VERTICES;
	desc.stride        = sizeof(Vector3);
	desc.type          = BufferType::DEFAULT;
	desc.update_mode   = UpdateMode::NONE;

	auto& positions_buffer  = _plane.mesh->getVertexBuffer(0);
	positions_buffer.offset = 0;
	positions_buffer.stride = sizeof(Vector3);

	_render_device.createBuffer(desc, positions, positions_buffer.buffer, nullptr, nullptr);

	//Extra buffer
	desc.stride = sizeof(VertexExtra);

	auto& extra_buffer  = _plane.mesh->getVertexBuffer(1);
	extra_buffer.offset = 0;
	extra_buffer.stride = sizeof(VertexExtra);

	_render_device.createBuffer(desc, extra, extra_buffer.buffer, nullptr, nullptr);

	//Index buffer
	desc.bind_flags   = (u8)BufferBindFlag::INDEX;
	desc.num_elements = sizeof(indices) / sizeof(*indices);
	desc.stride       = sizeof(u16);

	_render_device.createBuffer(desc, indices, _plane.mesh->index_buffer, nullptr, nullptr);

	_plane.bounding_sphere.center = Vector3(0.0f, 0.0f, 0.0f);
	_plane.bounding_sphere.radius = sqrt(2 * 0.5f*0.5f);
	_plane.permutation            = 4;
	_plane.num_subsets            = 1;
	_plane.draw_calls             = allocator::allocateArray<DrawCall>(_allocator, 1);
	_plane.draw_calls[0]          = createDrawCall(true, 6, 0, 0);

	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------

	static const u32 NUM_BOX_VERTICES = 4 * 6;

	Vector3 box_positions[NUM_BOX_VERTICES];
	VertexExtra box_extra[NUM_BOX_VERTICES];

	static const Vector3 box_positions_helper[] =
	{
		Vector3(-0.5f, 0.0f, -0.5f),	//blb
		Vector3(-0.5f, 0.0f, 0.5f),		//blf
		Vector3(0.5f, 0.0f, 0.5f),		//brf
		Vector3(0.5f, 0.0f, -0.5f),		//brb

		Vector3(-0.5f, 1.0f, -0.5f),	//tlb
		Vector3(-0.5f, 1.0f, 0.5f),		//tlf
		Vector3(0.5f, 1.0f, 0.5f),		//trf
		Vector3(0.5f, 1.0f, -0.5f),		//trb
	};

	/*

	   5 .+------+ 6
	   .' |    .'|
	4 +---+--+' 7|
	  |   |  |   |
	  | 1,+--+---+ 2
	  |.'    | .'
	0 +------+' 3

	*/

	index = 0;

	//bottom
	box_positions[index]        = box_positions_helper[3];
	box_extra[index].normal     = VECTOR3_DOWN;
	box_extra[index].tex_coords = Vector2(0.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[2];
	box_extra[index].normal     = VECTOR3_DOWN;
	box_extra[index].tex_coords = Vector2(1.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[1];
	box_extra[index].normal     = VECTOR3_DOWN;
	box_extra[index].tex_coords = Vector2(1.0f, 1.0f);
	index++;
	box_positions[index]        = box_positions_helper[0];
	box_extra[index].normal     = VECTOR3_DOWN;
	box_extra[index].tex_coords = Vector2(0.0f, 1.0f);
	index++;

	//top
	box_positions[index]        = box_positions_helper[4];
	box_extra[index].normal     = VECTOR3_UP;
	box_extra[index].tex_coords = Vector2(0.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[5];
	box_extra[index].normal     = VECTOR3_UP;
	box_extra[index].tex_coords = Vector2(1.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[6];
	box_extra[index].normal     = VECTOR3_UP;
	box_extra[index].tex_coords = Vector2(1.0f, 1.0f);
	index++;
	box_positions[index]        = box_positions_helper[7];
	box_extra[index].normal     = VECTOR3_UP;
	box_extra[index].tex_coords = Vector2(0.0f, 1.0f);
	index++;

	//forward
	box_positions[index]        = box_positions_helper[2];
	box_extra[index].normal     = VECTOR3_FORWARD;
	box_extra[index].tex_coords = Vector2(0.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[6];
	box_extra[index].normal     = VECTOR3_FORWARD;
	box_extra[index].tex_coords = Vector2(1.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[5];
	box_extra[index].normal     = VECTOR3_FORWARD;
	box_extra[index].tex_coords = Vector2(1.0f, 1.0f);
	index++;
	box_positions[index]        = box_positions_helper[1];
	box_extra[index].normal     = VECTOR3_FORWARD;
	box_extra[index].tex_coords = Vector2(0.0f, 1.0f);
	index++;

	//backward
	box_positions[index]        = box_positions_helper[0];
	box_extra[index].normal     = VECTOR3_BACK;
	box_extra[index].tex_coords = Vector2(0.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[4];
	box_extra[index].normal     = VECTOR3_BACK;
	box_extra[index].tex_coords = Vector2(1.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[7];
	box_extra[index].normal     = VECTOR3_BACK;
	box_extra[index].tex_coords = Vector2(1.0f, 1.0f);
	index++;
	box_positions[index]        = box_positions_helper[3];
	box_extra[index].normal     = VECTOR3_BACK;
	box_extra[index].tex_coords = Vector2(0.0f, 1.0f);
	index++;

	//left
	box_positions[index]        = box_positions_helper[1];
	box_extra[index].normal     = VECTOR3_LEFT;
	box_extra[index].tex_coords = Vector2(0.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[5];
	box_extra[index].normal     = VECTOR3_LEFT;
	box_extra[index].tex_coords = Vector2(1.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[4];
	box_extra[index].normal     = VECTOR3_LEFT;
	box_extra[index].tex_coords = Vector2(1.0f, 1.0f);
	index++;
	box_positions[index]        = box_positions_helper[0];
	box_extra[index].normal     = VECTOR3_LEFT;
	box_extra[index].tex_coords = Vector2(0.0f, 1.0f);
	index++;

	//right
	box_positions[index]        = box_positions_helper[3];
	box_extra[index].normal     = VECTOR3_RIGHT;
	box_extra[index].tex_coords = Vector2(0.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[7];
	box_extra[index].normal     = VECTOR3_RIGHT;
	box_extra[index].tex_coords = Vector2(1.0f, 0.0f);
	index++;
	box_positions[index]        = box_positions_helper[6];
	box_extra[index].normal     = VECTOR3_RIGHT;
	box_extra[index].tex_coords = Vector2(1.0f, 1.0f);
	index++;
	box_positions[index]        = box_positions_helper[2];
	box_extra[index].normal     = VECTOR3_RIGHT;
	box_extra[index].tex_coords = Vector2(0.0f, 1.0f);
	index++;

	const u16 NUM_BOX_INDICES = 6 * 3 * 2;

	u16 box_indices[NUM_BOX_INDICES];

	for(u32 i = 0; i < 6; i++)
	{
		box_indices[i * 6 + 0] = i * 4 + 0;
		box_indices[i * 6 + 1] = i * 4 + 1;
		box_indices[i * 6 + 2] = i * 4 + 2;

		box_indices[i * 6 + 3] = i * 4 + 0;
		box_indices[i * 6 + 4] = i * 4 + 2;
		box_indices[i * 6 + 5] = i * 4 + 3;
	}

	_box.mesh                      = createMesh(2, _allocator);
	_box.mesh->index_buffer_offset = 0;
	_box.mesh->index_format        = IndexBufferFormat::UINT16;
	_box.mesh->topology            = PrimitiveTopology::TRIANGLE_LIST;

	//Position buffer
	desc.bind_flags    = (u8)BufferBindFlag::VERTEX;
	desc.draw_indirect = false;
	desc.num_elements  = NUM_BOX_VERTICES;
	desc.stride        = sizeof(Vector3);
	desc.type          = BufferType::DEFAULT;
	desc.update_mode   = UpdateMode::NONE;

	auto& box_positions_buffer  = _box.mesh->getVertexBuffer(0);
	box_positions_buffer.offset = 0;
	box_positions_buffer.stride = sizeof(Vector3);

	_render_device.createBuffer(desc, box_positions, box_positions_buffer.buffer, nullptr, nullptr);

	//Extra buffer
	desc.stride = sizeof(VertexExtra);

	auto& box_extra_buffer  = _box.mesh->getVertexBuffer(1);
	box_extra_buffer.offset = 0;
	box_extra_buffer.stride = sizeof(VertexExtra);

	_render_device.createBuffer(desc, box_extra, box_extra_buffer.buffer, nullptr, nullptr);

	//Index buffer
	desc.bind_flags   = (u8)BufferBindFlag::INDEX;
	desc.num_elements = NUM_BOX_INDICES;
	desc.stride       = sizeof(u16);

	_render_device.createBuffer(desc, box_indices, _box.mesh->index_buffer, nullptr, nullptr);

	_box.bounding_sphere.center = Vector3(0.0f, 0.5f, 0.0f);
	_box.bounding_sphere.radius = sqrt(2 * 0.5f*0.5f);
	_box.permutation            = 4;
	_box.num_subsets            = 1;
	_box.draw_calls             = allocator::allocateArray<DrawCall>(_allocator, 1);
	_box.draw_calls[0]          = createDrawCall(true, NUM_BOX_INDICES, 0, 0);
}

PrimitiveMeshManager::~PrimitiveMeshManager()
{
	_render_device.release(_box.mesh->getVertexBuffer(0).buffer);
	_render_device.release(_box.mesh->getVertexBuffer(1).buffer);
	_render_device.release(_box.mesh->index_buffer);

	allocator::deallocateArray(_allocator, _box.draw_calls);
	_allocator.deallocate(_box.mesh);

	//---------------------------------------------------------------------

	_render_device.release(_plane.mesh->getVertexBuffer(0).buffer);
	_render_device.release(_plane.mesh->getVertexBuffer(1).buffer);
	_render_device.release(_plane.mesh->index_buffer);

	allocator::deallocateArray(_allocator,_plane.draw_calls);
	_allocator.deallocate(_plane.mesh);
}

const MeshData* PrimitiveMeshManager::getPlane()
{
	return &_plane;
}

const MeshData* PrimitiveMeshManager::getBox()
{
	return &_box;
}