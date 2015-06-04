#include "Terrain.h"

#include "Renderer\Camera.h"

#include "Renderer\RendererUtilities.h"
#include "Renderer\Renderer.h"
#include "Renderer\RendererStructs.h"

#include "Core\Allocators\ScopeStack.h"
#include "Core\Allocators\LinearAllocator.h"
#include "Core\Allocators\Allocator.h"

#include "Utilities\StringID.h"

using namespace aqua;

static const u32 TERRAIN_PATCH_SIZE = 16;
static const u32 TERRAIN_PATCH_NUM_ROWS = TERRAIN_PATCH_SIZE + 1;

Terrain::Terrain(Allocator& allocator, LinearAllocator& temp_allocator, Renderer& renderer)
	: _allocator(allocator), _temp_allocator(&temp_allocator), _renderer(&renderer), _num_patches(0)
{
	//Get shaders

	_shader = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/terrain.cshader"));

	//_permutation = enableOption(*_shader->getInstanceParameterGroupDescSet(), getStringID("WIREFRAME"), _permutation);
	//_permutation = enableOption(*_shader->getInstanceParameterGroupDescSet(), getStringID("USE_LOD_COLOR"), _permutation);
	_permutation = enableOption(*_shader->getInstanceParameterGroupDescSet(), getStringID("HEIGHT_MAP"), _permutation);

	_shader_permutation = _shader->getPermutation(_permutation);

	auto instance_params_desc_set = _shader->getInstanceParameterGroupDescSet();
	_instance_params_desc = getParameterGroupDesc(*instance_params_desc_set, _permutation);

	//----------------------------

	ConstantBufferDesc desc;
	desc.size = _instance_params_desc->getCBuffersSizes()[0];
	desc.update_mode = UpdateMode::CPU;

	if(!_renderer->getRenderDevice()->createConstantBuffer(desc, nullptr, _cbuffer))
	{
		//NEED ERROR MESSAGE
		//return false;
	}

	_instances_params = allocator::allocateArray<ParameterGroup*>(_allocator, TER_WIDTH * TER_HEIGHT);

	for(u32 i = 0; i < TER_WIDTH * TER_HEIGHT; i++)
	{
		_instances_params[i] = _renderer->getRenderDevice()->createParameterGroup(_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																				  *_instance_params_desc, UINT32_MAX, 0, nullptr);

		//_instances_params[i]->setCBuffer(_cbuffer, 0);
	}

	//----------------------------

	//--------------------
	// Create quad mesh
	//--------------------

	ScopeStack scope(*_temp_allocator);

	const u32 num_vertices = (TERRAIN_PATCH_NUM_ROWS + 1)*(TERRAIN_PATCH_NUM_ROWS + 1);

	ASSERT(num_vertices <= UINT16_MAX);

	Vector3* vertices = scope.newPODArray<Vector3>(num_vertices);

	u32 index = 0;

	for(u32 i = 0; i < TERRAIN_PATCH_NUM_ROWS; i++)
	{
		for(u32 j = 0; j < TERRAIN_PATCH_NUM_ROWS; j++)
		{
			vertices[index].x = (float)i;
			vertices[index].y = 0.0f;
			vertices[index].z = (float)j;

			index++;
		}
	}

	const u32 num_indices = TERRAIN_PATCH_SIZE*TERRAIN_PATCH_SIZE * 2 * 3;

	u16* indices = scope.newPODArray<u16>(num_indices);

	index = 0;

	for(u16 i = 0; i < TERRAIN_PATCH_SIZE; i++)
	{
		for(u16 j = 0; j < TERRAIN_PATCH_SIZE; j++)
		{
			indices[index++] = (u16)(i * TERRAIN_PATCH_NUM_ROWS + j);
			indices[index++] = (u16)((i + 1) * TERRAIN_PATCH_NUM_ROWS + j + 1);;
			indices[index++] = (u16)((i + 1) * TERRAIN_PATCH_NUM_ROWS + j);

			indices[index++] = (u16)((i + 1) * TERRAIN_PATCH_NUM_ROWS + j + 1);
			indices[index++] = (u16)(i * TERRAIN_PATCH_NUM_ROWS + j);
			indices[index++] = (u16)(i * TERRAIN_PATCH_NUM_ROWS + j + 1);
		}
	}

	_mesh = createMesh(1, _allocator);
	_mesh->topology = PrimitiveTopology::TRIANGLE_LIST;
	_mesh->index_buffer_offset = 0;
	_mesh->index_format = IndexBufferFormat::UINT16;

	BufferDesc buffer_desc;
	buffer_desc.num_elements = num_vertices;
	buffer_desc.stride = sizeof(*vertices);
	buffer_desc.bind_flags = (u8)BufferBindFlag::VERTEX;
	buffer_desc.update_mode = UpdateMode::NONE;

	VertexBuffer& vb = _mesh->getVertexBuffer(0);
	vb.offset = 0;
	vb.stride = buffer_desc.stride;

	_renderer->getRenderDevice()->createBuffer(buffer_desc, vertices, vb.buffer, nullptr, nullptr);

	buffer_desc.num_elements = num_indices;
	buffer_desc.stride = sizeof(*indices);
	buffer_desc.bind_flags = (u8)BufferBindFlag::INDEX;

	_renderer->getRenderDevice()->createBuffer(buffer_desc, indices, _mesh->index_buffer, nullptr, nullptr);

	_draw_call = createDrawCall(true, num_indices, 0, 0);

	_num_levels = 0;

	/*
	//Init levels info
	_num_levels = 11;

	_levels_patch_size   = allocator::allocateArray<float>(_allocator, _num_levels);
	_levels_patch_radius = allocator::allocateArray<float>(_allocator, _num_levels);
	_levels_ranges       = allocator::allocateArray<float>(_allocator, _num_levels);

	_levels_patch_size[0]   = 16.0f;
	_levels_patch_radius[0] = 22.63f / 2;
	_levels_ranges[0]       = 50.0f;

	for(u32 i = 1; i < _num_levels; i++)
	{
	_levels_patch_size[i]   = _levels_patch_size[i - 1] * 2;
	_levels_patch_radius[i] = _levels_patch_radius[i - 1] * 2;
	_levels_ranges[i]       = _levels_ranges[i - 1] * 2;
	}
	*/

	//----------------------------
	// Setup material params

	auto material_params_desc_set = _shader->getMaterialParameterGroupDescSet();
	_material_params_desc = getParameterGroupDesc(*material_params_desc_set, _permutation);

	//----------------------------

	desc.size = _material_params_desc->getCBuffersSizes()[0];
	desc.update_mode = UpdateMode::CPU;

	if(!_renderer->getRenderDevice()->createConstantBuffer(desc, nullptr, _material_cbuffer))
	{
		//NEED ERROR MESSAGE
		//return false;
	}

	//_material_params = createParameterGroup(*_material_params_desc, true, _allocator);
	//_material_params->cbuffers[0] = _material_cbuffer;

	_material_params = _renderer->getRenderDevice()->createParameterGroup(_allocator, RenderDevice::ParameterGroupType::MATERIAL,
																		  *_material_params_desc, UINT32_MAX, 0, nullptr);

	/*
	u32 offset = _material_params_desc->getConstantOffset(getStringID("levels_morph_consts"));

	Vector4* consts = (Vector4*)pointer_math::add(_material_params->map_commands->data, offset);

	for(u32 i = 0; i < _num_levels; i++)
	{
	float start = _levels_ranges[i] * 0.7f;
	float k     = 1.0f / (_levels_ranges[i] - start);
	k = 1.0f;

	consts[i].x = _levels_ranges[i] * k;
	consts[i].y = k;
	}
	*/
}

Terrain::~Terrain()
{
	if(_num_levels > 0)
	{
		allocator::deallocateArray(_allocator, _levels_ranges);
		allocator::deallocateArray(_allocator, _levels_patch_radius);
		allocator::deallocateArray(_allocator, _levels_patch_size);
	}

	_allocator.deallocate(_material_params);

	for(u32 i = 0; i < TER_WIDTH * TER_HEIGHT; i++)
	{
		_allocator.deallocate(_instances_params[i]);
	}

	allocator::deallocateArray(_allocator, _instances_params);

	//Release buffers
	RenderDevice* render_device = _renderer->getRenderDevice();

	render_device->release(_material_cbuffer);
	render_device->release(_cbuffer);

	render_device->release(_mesh->getVertexBuffer(0).buffer);
	render_device->release(_mesh->index_buffer);

	_allocator.deallocate(_mesh);
}

bool Terrain::cull(u32 num_frustums, const Frustum* frustums, VisibilityData** out)
{
	if(_num_patches == 0)
		return true;

	for(u32 i = 0; i < num_frustums; i++)
	{
		u32 num_visibles = 0;

		out[i]->visibles_indices = allocator::allocateArray<u32>(*_temp_allocator, _num_patches);

		for(u32 j = 0; j < _num_patches; j++)
		{
			out[i]->visibles_indices[num_visibles] = j;

			num_visibles++;
		}

		out[i]->num_visibles = num_visibles;
	}

	return true;
}
bool Terrain::extract(const VisibilityData& visibility_data)
{
	return true;
}

bool Terrain::prepare()
{
	return true;
}

bool Terrain::getRenderItems(u8 num_passes, const u32* passes_names, const VisibilityData& visibility, RenderQueue* out_queues)
{
	if(_num_patches == 0)
		return true;

	u8 num_shader_passes           = _shader->getNumPasses();
	const u32* shader_passes_names = _shader->getPassesNames();

	struct Match
	{
		u8 pass;
		u8 shader_pass;
	};

	u8 num_matches = 0;

	Match* matches = allocator::allocateArrayNoConstruct<Match>(*_temp_allocator, num_passes);

	for(u8 i = 0; i < num_shader_passes; i++)
	{
		for(u8 j = 0; j < num_passes; j++)
		{
			if(shader_passes_names[i] == passes_names[j])
			{
				matches[num_matches].pass = j;
				matches[num_matches].shader_pass = i;
				num_matches++;

				break;
			}
		}
	}

	if(num_matches == 0)
		return true; //no passes match

	RenderItem** render_queues = allocator::allocateArrayNoConstruct<RenderItem*>(*_temp_allocator, num_matches);

	for(u8 i = 0; i < num_matches; i++)
	{
		u8 queue_index = matches[i].pass;

		render_queues[i]                   = allocator::allocateArrayNoConstruct<RenderItem>(*_temp_allocator, visibility.num_visibles);
		out_queues[queue_index].sort_items = allocator::allocateArrayNoConstruct<SortItem>(*_temp_allocator, visibility.num_visibles);

		//out_queues[queue_index].render_items = allocator::allocateArrayNoConstruct<RenderItem>(*_temp_allocator, _num_patches);
		//out_queues[queue_index].sort_items   = allocator::allocateArrayNoConstruct<SortItem>(*_temp_allocator, _num_patches);
	}

	RenderItem render_item;
	render_item.num_instances   = 1;
	render_item.draw_call       = &_draw_call;
	render_item.mesh            = _mesh;
	//render_item.material_params = _material_params;
	render_item.material_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_material_params);

	for(u32 i = 0; i < _num_patches; i++)
	{
		//render_item.instance_params = _instances_params[i];
		render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_instances_params[i]);

		for(u8 j = 0; j < num_matches; j++)
		{
			Match& match = matches[j];

			if(_shader_permutation[match.shader_pass] != nullptr)
			{
				render_item.shader = _shader_permutation[match.shader_pass];

				RenderQueue& queue = out_queues[match.pass];

				//RenderItem& ri = render_queues[match.pass][i];
				RenderItem& ri = render_queues[j][i];
				ri             = render_item;

				SortItem& sort_item = queue.sort_items[i];
				sort_item.item      = &ri;
				sort_item.sort_key  = 0;

				queue.size++;
			}
		}
	}

	return true;
}

void Terrain::selectLOD(const Vector3& camera_position, const Vector3& patch_position, u32 level)
{
	//Frustum cull
	/*
	if(not visible)
	return;
	*/

	float patch_size = _levels_patch_size[level];
	float patch_size_half = patch_size / 2;

	Vector3 patch_center = patch_position + Vector3(patch_size_half, 0, patch_size_half);

	float distance = (camera_position - patch_center).Length() - _levels_patch_radius[level];

	if(level == 0 || distance > _levels_ranges[level - 1])
	{
		//Add patch
		u32 offset = _instance_params_desc->getConstantOffset(getStringID("position_scale"));

		Vector4 position_scale(patch_position.x, patch_position.z, patch_size / 16, (float)level);

		memcpy(pointer_math::add(_instances_params[_num_patches]->getCBuffersData(), offset), &position_scale, sizeof(position_scale));

		_num_patches++;
	}
	else
	{
		selectLOD(camera_position, patch_position + Vector3(0, 0, 0), level - 1);
		selectLOD(camera_position, patch_position + Vector3(0, 0, patch_size_half), level - 1);
		selectLOD(camera_position, patch_position + Vector3(patch_size_half, 0, 0), level - 1);
		selectLOD(camera_position, patch_position + Vector3(patch_size_half, 0, patch_size_half), level - 1);
	}
}

void Terrain::updateLODs(const Camera& camera)
{
	_num_patches = 0;

	u32 offset = _material_params_desc->getConstantOffset(getStringID("eye_pos"));

	Vector3 eye_pos = camera.getPosition();
	eye_pos.y       = 0;

	memcpy(pointer_math::add(_material_params->getCBuffersData(), offset), &eye_pos, sizeof(eye_pos));

	float last_level_patch_size = _levels_patch_size[_num_levels - 1];

	u32 num_patches = (u32)ceil(_size / last_level_patch_size);

	for(u32 i = 0; i < num_patches; i++)
	{
		for(u32 j = 0; j < num_patches; j++)
		{
			selectLOD(eye_pos, Vector3(i*last_level_patch_size, 0, j*last_level_patch_size), _num_levels - 1);
		}
	}

	//selectLOD(cp, Vector3(0, 0, 0), _num_levels - 1);
}

void Terrain::setHeightMap(ShaderResourceH height_map, ShaderResourceH color_map,
	u32 size, u32 view_distance, u32 lod_size, u32 num_lod_levels,
	float inter_vertex_space, float height_scale, float height_offset)
{
	_size = size;

	u8 index = _material_params_desc->getSRVIndex(getStringID("height_map"));

	if(index != UINT8_MAX)
		_material_params->setSRV(height_map, index);

	index = _material_params_desc->getSRVIndex(getStringID("color_map"));

	if(index != UINT8_MAX)
		_material_params->setSRV(color_map, index);

	u32 offset = _material_params_desc->getConstantOffset(getStringID("height_scale"));

	float* x = (float*)pointer_math::add(_material_params->getCBuffersData(), offset);

	*x = height_scale;

	offset = _material_params_desc->getConstantOffset(getStringID("height_offset"));

	x = (float*)pointer_math::add(_material_params->getCBuffersData(), offset);

	*x = height_offset;

	offset = _material_params_desc->getConstantOffset(getStringID("height_map_size"));

	x = (float*)pointer_math::add(_material_params->getCBuffersData(), offset);

	*x = (float)(size + 1);

	//Setup lods info

	ASSERT(num_lod_levels > 0);

	if(_num_levels != num_lod_levels)
	{
		if(_num_levels > 0)
		{
			allocator::deallocateArray(_allocator, _levels_ranges);
			allocator::deallocateArray(_allocator, _levels_patch_radius);
			allocator::deallocateArray(_allocator, _levels_patch_size);
		}

		_num_levels = num_lod_levels;

		_levels_patch_size   = allocator::allocateArray<float>(_allocator, _num_levels);
		_levels_patch_radius = allocator::allocateArray<float>(_allocator, _num_levels);
		_levels_ranges       = allocator::allocateArray<float>(_allocator, _num_levels);
	}

	_levels_patch_size[0]   = inter_vertex_space * TERRAIN_PATCH_SIZE;
	_levels_patch_radius[0] = sqrt(_levels_patch_size[0] * _levels_patch_size[0] * 2) / 2;
	_levels_ranges[0]       = lod_size * inter_vertex_space / 2;

	for(u32 i = 1; i < _num_levels; i++)
	{
		_levels_patch_size[i]   = _levels_patch_size[i - 1] * 2;
		_levels_patch_radius[i] = _levels_patch_radius[i - 1] * 2;
		_levels_ranges[i]       = _levels_ranges[i - 1] * 2;
	}

	offset = _material_params_desc->getConstantOffset(getStringID("levels_morph_consts"));

	Vector4* consts = (Vector4*)pointer_math::add(_material_params->getCBuffersData(), offset);

	float prev_lod_end = 0;

	for(u32 i = 0; i < _num_levels; i++)
	{
		float start = prev_lod_end + (_levels_ranges[i] - prev_lod_end) * 0.7f;
		float k = 1.0f / (_levels_ranges[i] - start);
		//k           = 1.0f;

		consts[i].x = _levels_ranges[i] * k;
		consts[i].y = k;

		//consts[i].x = _levels_ranges[i] * k;
		//consts[i].y = 0.0f;

		prev_lod_end = _levels_ranges[i];
	}
}
/*
void Terrain::toggleWireframe()
{
if(_wireframe)
{
_permutation = enableOption(*_shader->getInstanceParameterGroupSetDesc(), getStringID("WIREFRAME"), _permutation);
_wireframe   = false;
}
else
{
_permutation = disableOption(*_shader->getInstanceParameterGroupSetDesc(), getStringID("WIREFRAME"), _permutation);
_wireframe   = true;
}

_shader_permutation = _shader->getPermutation(_permutation);
}*/

void Terrain::setWireframe(bool enabled)
{
	if(enabled)
	{
		_permutation = enableOption(*_shader->getInstanceParameterGroupDescSet(), getStringID("WIREFRAME"), _permutation);
	}
	else
	{
		_permutation = disableOption(*_shader->getInstanceParameterGroupDescSet(), getStringID("WIREFRAME"), _permutation);
	}

	_shader_permutation = _shader->getPermutation(_permutation);
}