#include "PrimitiveMeshManager.h"

#include "Renderer\RendererStructs.h"
#include "Renderer\RenderDevice\RenderDevice.h"

#include "Core\Containers\Array.h"
#include "Core\Containers\HashMap.h"
#include "Core\Allocators\LinearAllocator.h"

using namespace aqua;

struct VertexExtra
{
	Vector3 normal;
	Vector2 tex_coords;
};

static int getNewVertex(Array<Vector3>& vertices, HashMap<u64, u32>& new_vertices, int i1, int i2)
{
	u64 key;
	if(i1 < i2)
		key = ((u64)i2) << 32 | i1;
	else
		key = ((u64)i1) << 32 | i2;

	if(new_vertices.has(key))
	{
		return new_vertices.lookup(key, UINT32_MAX);
	}

	u32 new_index = (u32)vertices.size();

	new_vertices.insert(key, new_index);

	vertices.push( (vertices[i1] + vertices[i2]) * 0.5f );

	return new_index;
}

static void subdivide(Array<Vector3>& positions, Array<u16>& indices, Allocator& scratchpad)
{
	HashMap<u64, u32> new_vertices(scratchpad);
	
	u32 num_triangles = (u32)indices.size() / 3;

	Array<u16> new_indices(scratchpad, num_triangles * 4 * 3);

	for(u32 i = 0; i < num_triangles; i++)
	{
		//       i2
		//       *
		//      / \
		//     /   \
		//   a*-----*b
		//   / \   / \
		//  /   \ /   \
		// *-----*-----*
		// i1    c      i3

		u16 i1 = indices[i * 3];
		u16 i2 = indices[i * 3 + 1];
		u16 i3 = indices[i * 3 + 2];
		u16 a = getNewVertex(positions, new_vertices, i1, i2);
		u16 b = getNewVertex(positions, new_vertices, i2, i3);
		u16 c = getNewVertex(positions, new_vertices, i3, i1);

		new_indices.push(i1); new_indices.push(a); new_indices.push(c);
		new_indices.push(i2); new_indices.push(b); new_indices.push(a);
		new_indices.push(i3); new_indices.push(c); new_indices.push(b);
		new_indices.push(a); new_indices.push(b); new_indices.push(c);
	}

	indices = new_indices;
}

PrimitiveMeshManager::PrimitiveMeshManager(Allocator& allocator, LinearAllocator& temp_allocator, RenderDevice& render_device)
	: _allocator(allocator), _temp_allocator(temp_allocator), _render_device(render_device)
{
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------

	void* temp_allocator_mark = _temp_allocator.getMark();

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

	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------

	Array<Vector3> sphere_positions(_temp_allocator, 12);
	sphere_positions.push(Vector3(-0.525731f, 0, 0.850651f));
	sphere_positions.push(Vector3(0.525731f, 0, 0.850651f));
	sphere_positions.push(Vector3(-0.525731f, 0, -0.850651f)); 
	sphere_positions.push(Vector3(0.525731f, 0, -0.850651f));
	sphere_positions.push(Vector3(0, 0.850651f, 0.525731f)); 
	sphere_positions.push(Vector3(0, 0.850651f, -0.525731f));
	sphere_positions.push(Vector3(0, -0.850651f, 0.525731f)); 
	sphere_positions.push(Vector3(0, -0.850651f, -0.525731f));
	sphere_positions.push(Vector3(0.850651f, 0.525731f, 0)); 
	sphere_positions.push(Vector3(-0.850651f, 0.525731f, 0));
	sphere_positions.push(Vector3(0.850651f, -0.525731f, 0)); 
	sphere_positions.push(Vector3(-0.850651f, -0.525731f, 0));

	Array<u16> sphere_indices(_temp_allocator, 60);

	{
		u16 sphere_indices_aux[60] =
		{
			1, 4, 0, 4, 9, 0, 4, 5, 9, 8, 5, 4, 1, 8, 4,
			1, 10, 8, 10, 3, 8, 8, 3, 5, 3, 2, 5, 3, 7, 2,
			3, 10, 7, 10, 6, 7, 6, 11, 7, 6, 0, 11, 6, 1, 0,
			10, 1, 6, 11, 0, 9, 2, 11, 9, 5, 2, 9, 11, 2, 7
		};

		sphere_indices.push(60, sphere_indices_aux);
	}
	
	//Subdivide
	static const u8 NUM_SUBDIVISIONS = 3;

	for(u8 i = 0; i < NUM_SUBDIVISIONS; i++)
		subdivide(sphere_positions, sphere_indices, _temp_allocator);

	Array<VertexExtra> sphere_extra(_temp_allocator, sphere_positions.size());

	for(size_t i = 0; i < sphere_positions.size(); i++)
	{
		VertexExtra extra;

		//project onto unit sphere
		sphere_positions[i].Normalize();

		extra.normal = sphere_positions[i];

		float theta = atan2(sphere_positions[i].z, sphere_positions[i].x);
		float phi = acos(sphere_positions[i].y);

		extra.tex_coords = Vector2(theta / DirectX::XM_2PI, phi / DirectX::XM_PI);

		sphere_extra.push(extra);

		sphere_positions[i] /= 2; //radius = 0.5
		sphere_positions[i].y += 0.5f;
	}

	_sphere.mesh                      = createMesh(2, _allocator);
	_sphere.mesh->index_buffer_offset = 0;
	_sphere.mesh->index_format        = IndexBufferFormat::UINT16;
	_sphere.mesh->topology            = PrimitiveTopology::TRIANGLE_LIST;

	//Position buffer
	desc.bind_flags    = (u8)BufferBindFlag::VERTEX;
	desc.draw_indirect = false;
	desc.num_elements  = sphere_positions.size();
	desc.stride        = sizeof(Vector3);
	desc.type          = BufferType::DEFAULT;
	desc.update_mode   = UpdateMode::NONE;

	auto& sphere_positions_buffer  = _sphere.mesh->getVertexBuffer(0);
	sphere_positions_buffer.offset = 0;
	sphere_positions_buffer.stride = sizeof(Vector3);

	_render_device.createBuffer(desc, &sphere_positions[0], sphere_positions_buffer.buffer, nullptr, nullptr);

	//Extra buffer
	desc.stride = sizeof(VertexExtra);

	auto& sphere_extra_buffer  = _sphere.mesh->getVertexBuffer(1);
	sphere_extra_buffer.offset = 0;
	sphere_extra_buffer.stride = sizeof(VertexExtra);

	_render_device.createBuffer(desc, &sphere_extra[0], sphere_extra_buffer.buffer, nullptr, nullptr);

	//Index buffer
	desc.bind_flags   = (u8)BufferBindFlag::INDEX;
	desc.num_elements = sphere_indices.size();
	desc.stride       = sizeof(u16);

	_render_device.createBuffer(desc, &sphere_indices[0], _sphere.mesh->index_buffer, nullptr, nullptr);

	_sphere.bounding_sphere.center = Vector3(0.0f, 0.5f, 0.0f);
	_sphere.bounding_sphere.radius = 0.5f;
	_sphere.permutation            = 4;
	_sphere.num_subsets            = 1;
	_sphere.draw_calls             = allocator::allocateArray<DrawCall>(_allocator, 1);
	_sphere.draw_calls[0]          = createDrawCall(true, sphere_indices.size(), 0, 0);

	_temp_allocator.rewind(temp_allocator_mark);
}

PrimitiveMeshManager::~PrimitiveMeshManager()
{
	_render_device.release(_sphere.mesh->getVertexBuffer(0).buffer);
	_render_device.release(_sphere.mesh->getVertexBuffer(1).buffer);
	_render_device.release(_sphere.mesh->index_buffer);

	allocator::deallocateArray(_allocator, _sphere.draw_calls);
	_allocator.deallocate(_sphere.mesh);

	//---------------------------------------------------------------------

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

const MeshData* PrimitiveMeshManager::getSphere()
{
	return &_sphere;
}