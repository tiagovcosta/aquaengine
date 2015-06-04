#include "ModelManager.h"

#include "..\Components\TransformManager.h"
#include "..\Renderer\Camera.h"

#include "..\Renderer\RendererUtilities.h"
#include "..\Renderer\Renderer.h"

#include "..\Core\Allocators\ScopeStack.h"
#include "..\Core\Allocators\SmallBlockAllocator.h"
//#include "..\Utilities\Allocators\LinearAllocator.h"
//#include "..\Utilities\Allocators\Allocator.h"

#include "..\Utilities\Blob.h"
#include "..\Utilities\StringID.h"

using namespace aqua;

const ModelManager::Instance ModelManager::INVALID_INSTANCE = { ModelManager::INVALID_INDEX };

ModelManager::ModelManager(Allocator& allocator, LinearAllocator& temp_allocator,
						   Renderer& renderer, TransformManager& transform, u32 inital_capacity)
	: _allocator(allocator), _temp_allocator(&temp_allocator),
	_renderer(&renderer), _transform_manager(&transform), _map(allocator), _params_manager(allocator)
{
	_params_groups_allocator = allocator::allocateNew<SmallBlockAllocator>(_allocator, _allocator, 4 * 1024, 16);

	_data.size     = 0;
	_data.capacity = 0;

	if(inital_capacity > 0)
		setCapacity(inital_capacity);

	auto shader_manager = _renderer->getShaderManager();

	// Get shaders
	_render_shader = shader_manager->getRenderShader(getStringID("data/shaders/model.cshader"));
}

ModelManager::~ModelManager()
{
	//Release buffers and views
	RenderDevice* render_device = _renderer->getRenderDevice();

	for(u32 i = 0; i < _data.size; i++)
	{
		render_device->deleteParameterGroup(*_params_groups_allocator, *_data.instance_params[i]);
	}

	/*
	render_device->release(_line.mesh->getVertexBuffer(0).buffer);
	render_device->release(_line.mesh->index_buffer);

	_allocator.deallocate(_line.mesh);
	*/

	if(_data.capacity > 0)
		_allocator.deallocate(_data.buffer);

	allocator::deallocateDelete(_allocator, _params_groups_allocator);
}

void ModelManager::update()
{
	RenderDevice& render_device = *_renderer->getRenderDevice();

	for(u32 i = 0; i < _data.size; i++)
	{
		auto params_desc_set = _render_shader->getInstanceParameterGroupDescSet();
		auto params_desc     = getParameterGroupDesc(*params_desc_set, _data.permutation[i]);

		//Update world matrix
		u32 offset = params_desc->getConstantOffset(getStringID("world"));

		Matrix4x4* world = (Matrix4x4*)pointer_math::add(_data.instance_params[i]->getCBuffersData(), offset);

		*world = _transform_manager->getWorld(_transform_manager->lookup(_data.entity[i]));

		//update bounding sphere
		Vector3 edge = _data.bounding_sphere2[i].center + VECTOR3_RIGHT * _data.bounding_sphere2[i].radius;

		Vector3 world_center = Vector3::Transform(_data.bounding_sphere2[i].center, *world);
		Vector3 world_edge   = Vector3::Transform(edge, *world);

		_data.bounding_sphere[i].center = world_center;
		_data.bounding_sphere[i].radius = (world_edge - world_center).Length();

		//Cache instance parameter group
		_data.cached_instance_params[i] = render_device.cacheTemporaryParameterGroup(*_data.instance_params[i]);
	}
}

bool ModelManager::cull(u32 num_frustums, const Frustum* frustums, VisibilityData** out)
{
	//VisibilityData* output = allocator::allocateArray<VisibilityData>(*_temp_allocator, num_frustums);

	//TODO: Implement in SIMD

	/*
	//Transform frustums data to SIMD friendly layout
	__m128* frustums_data;

	for(u32 j = 0; j < num_frustums; j++)
	{

	}
	*/

	for(u32 i = 0; i < num_frustums; i++)
	{
		u32 num_visibles = 0;

		out[i]->visibles_indices = allocator::allocateArray<u32>(*_temp_allocator, _data.size);
	
		for(u32 j = 0; j < _data.size; j++)
		{
			bool in = true;

			for(u8 k = 0; k < 6; k++)
			{
				float d = frustums[i][k].DotCoordinate(_data.bounding_sphere[j].center);

				if(d < -_data.bounding_sphere[j].radius)
				{
					in = false;
					break;
				}
			}

			if(in)
			{
				out[i]->visibles_indices[num_visibles] = j;

				num_visibles++;
			}
		}

		out[i]->num_visibles = num_visibles;
	}

	return true;
}

bool ModelManager::extract(const VisibilityData& visibility_data)
{
	return true;
}

bool ModelManager::prepare()
{
	return true;
}

bool ModelManager::getRenderItems(u8 num_passes, const u32* passes_names, 
								  const VisibilityData& visibility_data, RenderQueue* out_queues)
{
	if(_data.size == 0)
		return true;

	u8 num_shader_passes           = _render_shader->getNumPasses();
	const u32* shader_passes_names = _render_shader->getPassesNames();

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
				matches[num_matches].pass        = j;
				matches[num_matches].shader_pass = i;
				num_matches++;

				break;
			}
		}
	}

	if(num_matches == 0)
		return true; //no passes match

	//out_queues[0].size = 0;

	RenderItem** render_queues = allocator::allocateArrayNoConstruct<RenderItem*>(*_temp_allocator, num_matches);

	for(u8 i = 0; i < num_matches; i++)
	{
		u8 queue_index = matches[i].pass;

		render_queues[i]                   = allocator::allocateArrayNoConstruct<RenderItem>(*_temp_allocator, visibility_data.num_visibles);
		out_queues[queue_index].sort_items = allocator::allocateArrayNoConstruct<SortItem>(*_temp_allocator, visibility_data.num_visibles);
	}

	for(u32 i = 0; i < visibility_data.num_visibles; i++)
	{
		u32 actor_index = visibility_data.visibles_indices[i];
		//actor_index = i;

		RenderItem render_item;
		render_item.draw_call       = _data.subset[actor_index].draw_call;
		render_item.mesh            = _data.mesh[actor_index];
		render_item.material_params = _data.subset[actor_index].material_params;
		render_item.instance_params = _data.cached_instance_params[actor_index];
		render_item.num_instances   = 1;

		ShaderPermutation permutation = _data.subset[actor_index].shader;

		for(u8 i = 0; i < num_matches; i++)
		{
			Match& match = matches[i];

			if(permutation[match.shader_pass] != nullptr)
			{
				render_item.shader = permutation[match.shader_pass];

				RenderQueue& queue = out_queues[match.pass];

				//RenderItem& ri = render_queues[match.pass][queue.size];
				RenderItem& ri = render_queues[i][queue.size];
				ri             = render_item;

				SortItem& sort_item = queue.sort_items[queue.size];
				sort_item.item      = &ri;
				sort_item.sort_key  = 0;

				queue.size++;
			}
		}
	}

	return true;
}

ModelManager::Instance ModelManager::create(Entity e, const MeshData* mesh, Permutation render_permutation)
{
	if(_data.size > 0)
	{
		if(lookup(e).valid())
			return INVALID_INSTANCE; //Max one model per entity
	}

	ASSERT(_data.size <= _data.capacity);

	if(_data.size == _data.capacity)
		setCapacity(_data.capacity * 2 + 8);

	u32 index = _data.size++;

	_map.insert(e, index);

	_data.entity[index]                 = e;
	_data.mesh[index]                   = mesh->mesh;
	_data.bounding_sphere[index]        = mesh->bounding_sphere;
	_data.permutation[index]            = mesh->permutation.value | render_permutation.value;
	_data.subset[index].shader          = nullptr;
	_data.subset[index].material_params = nullptr;
	_data.subset[index].draw_call       = &mesh->draw_calls[0];
	_data.bounding_sphere2[index]       = mesh->bounding_sphere;

	//Instance params
	auto params_desc_set = _render_shader->getInstanceParameterGroupDescSet();
	auto params_desc = getParameterGroupDesc(*params_desc_set, render_permutation);
	//_data.instance_params[index] = createParameterGroup(*params_desc, UINT32_MAX, *_params_groups_allocator);
	//_data.instance_params[index] = createParameterGroup(*_params_groups_allocator, *params_desc, UINT32_MAX, 0, nullptr);
	_data.instance_params[index] = _renderer->getRenderDevice()->createParameterGroup(*_params_groups_allocator, 
		RenderDevice::ParameterGroupType::INSTANCE, *params_desc, UINT32_MAX, 0, nullptr);

	//_data.params[index]->cbuffers[0] = _cbuffer;

	return{ index };
}

ModelManager::Instance ModelManager::lookup(Entity e)
{
	u32 i = _map.lookup(e, UINT32_MAX);

	return{ i };
}

void ModelManager::destroy(Instance i)
{
	u32 last      = _data.size - 1;
	Entity e      = _data.entity[i.i];
	Entity last_e = _data.entity[last];

#define COPY_SINLE_INSTANCE_DATA(data) _data.data[i.i] = _data.data[last];

	COPY_SINLE_INSTANCE_DATA(entity);
	COPY_SINLE_INSTANCE_DATA(mesh);
	COPY_SINLE_INSTANCE_DATA(bounding_sphere);
	COPY_SINLE_INSTANCE_DATA(permutation);
	COPY_SINLE_INSTANCE_DATA(instance_params);
	COPY_SINLE_INSTANCE_DATA(cached_instance_params);
	COPY_SINLE_INSTANCE_DATA(subset);
	COPY_SINLE_INSTANCE_DATA(bounding_sphere2);

	_map.insert(last_e, i.i);
	_map.remove(e);

	_data.size--;
}

void ModelManager::setMesh(Instance i, const MeshData* mesh)
{
	_data.mesh[i.i] = mesh->mesh;
}

void ModelManager::addSubset(Instance i, u8 index, const Material* material)
{
	//clear material bits
	//_data.subset[i.i].permutation.value &= ~_render_shader->getMaterialParameterGroupDescSet()->options_mask.value;
	//_data.subset[i.i].permutation.value |= material->permutation.value;
	_data.subset[i.i].permutation.value = _data.permutation[i.i].value | material->permutation.value;

	_data.subset[i.i].shader          = _render_shader->getPermutation(_data.subset[i.i].permutation);
	_data.subset[i.i].material_params = material->params;
}

void ModelManager::setCapacity(u32 new_capacity)
{
	InstanceData new_data;
	new_data.size     = _data.size;
	new_data.capacity = new_capacity;
	new_data.buffer   = allocateBlob(_allocator, new_capacity, new_data.entity, new_data.mesh, 
									 new_data.bounding_sphere,  new_data.permutation, new_data.instance_params,
									 new_data.cached_instance_params, new_data.subset, new_data.bounding_sphere2);

	//CHECK IF ALL ARRAYS ARE PROPERLY ALIGNED
	ASSERT(pointer_math::isAligned(new_data.entity));
	ASSERT(pointer_math::isAligned(new_data.mesh));
	ASSERT(pointer_math::isAligned(new_data.bounding_sphere));
	ASSERT(pointer_math::isAligned(new_data.permutation));
	ASSERT(pointer_math::isAligned(new_data.instance_params));
	ASSERT(pointer_math::isAligned(new_data.cached_instance_params));
	ASSERT(pointer_math::isAligned(new_data.subset));
	ASSERT(pointer_math::isAligned(new_data.bounding_sphere2));

#define COPY_DATA(data) memcpy(new_data.data, _data.data, _data.size * sizeof(decltype(*new_data.data)));

	if(_data.size > 0)
	{
		COPY_DATA(entity);
		COPY_DATA(mesh);
		COPY_DATA(bounding_sphere);
		COPY_DATA(permutation);
		COPY_DATA(instance_params);
		COPY_DATA(cached_instance_params);
		COPY_DATA(subset);
		COPY_DATA(bounding_sphere2);
	}

	if(_data.capacity > 0)
		_allocator.deallocate(_data.buffer);

	_data = new_data;
}