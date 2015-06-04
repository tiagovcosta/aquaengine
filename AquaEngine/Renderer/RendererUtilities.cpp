#include "RendererUtilities.h"

#include "RendererStructs.h"
#include "RenderDevice\RenderDeviceTypes.h"

#include "..\Core\Allocators\Allocator.h"

#include <algorithm>

using namespace aqua;
using namespace aqua::renderer;

AABB aqua::transformAABB(const AABB& aabb, const Matrix4x4& transform)
{
	// Efficient algorithm for transforming an AABB
	// Taken from Horde3D (Graphics Gems)

	float minA[3] = { aabb.min.x, aabb.min.y, aabb.min.z };
	float maxA[3] = { aabb.max.x, aabb.max.y, aabb.max.z };

	float minB[3];
	float maxB[3];

	for(u32 i = 0; i < 3; i++)
	{
		minB[i] = transform.m[3][i];
		maxB[i] = transform.m[3][i];

		for(u32 j = 0; j < 3; j++)
		{
			float x = minA[j] * transform.m[j][i];
			float y = maxA[j] * transform.m[j][i];
			minB[i] += minf(x, y);
			maxB[i] += maxf(x, y);
		}
	}

	return AABB{ Vector3{ minB[0], minB[1], minB[2] }, Vector3{ maxB[0], maxB[1], maxB[2] } };
}

u32 renderer::cull(const Plane* planes, u32 num_actors, const BoundingSphere* bounding_spheres, u32* visibles)
{
	u32 num_visibles = 0;

	for(u32 i = 0; i < num_actors; i++)
	{
		visibles[num_visibles++] = i;
	}

	return num_visibles;
}
/*
RenderQueue* renderer::generateRenderQueues(u8 num_passes, u32 passes_mask, const RenderWorldView& view,
	                                        void* instancing_buffer, u32 instancing_buffer_size,
											Allocator& temp_allocator)
{
	RenderQueue* render_queues = allocator::allocateArrayNoConstruct<RenderQueue>(temp_allocator, num_passes);

	for(u8 i = 0; i < num_passes; i++)
	{
		render_queues[i].size = 0;

		if(passes_mask & (1 << i))
		{
			render_queues[i].render_items = allocator::allocateArray<RenderItem>(temp_allocator, MAX_NUM_RENDER_ITEMS);
			render_queues[i].sort_items   = allocator::allocateArray<SortItem>(temp_allocator, MAX_NUM_RENDER_ITEMS);
		}
		else
		{
			render_queues[i].render_items = nullptr;
			render_queues[i].sort_items   = nullptr;
		}
	}

	//Sort visible instances by model id (model + permutation)
	std::sort(view.visibles, view.visibles + view.num_visibles, [&view](u32 x, u32 y)
	{
		return view.models_ids[x] < view.models_ids[y];
	});

	//Fill queues with visible objects and build instancing buffer
	u32 start = 0;
	u32 start_index = view.visibles[start];
	u32 count = 1;

	for(u32 i = 0; i < view.num_visibles - 1; i++)
	{
		u32 next_index = view.visibles[i + 1];

		if(view.models_ids[start_index] == view.models_ids[next_index])
		{
			count++;
		}
		else
		{
			//Add to queues
			const ModelInstance& instance = view.models_instances[start_index];

			static_assert(sizeof(u32) == sizeof(float), "float and u32 must have the same size!");

			//Convert float to u16
			union { float f; u32 val; } f2u = { instance.depth };
			//Shit 15 bits and ignore most significant bit which should always be 0
			u16 depth16 = f2u.val >> 15;

			for(u8 j = 0; j < instance.model->num_passes; j++)
			{
				u8 pass_id = instance.model->passes_ids[j];
				bool instancing = false;

				//Check if context contains pass
				if(render_queues[pass_id].render_items == nullptr)
					continue;

				ModelPass& pass = instance.model->passes[j];

				for(u8 k = 0; k < pass.num_subsets; k++)
				{
					Subset& subset = pass.subsets[k];
					ShaderPair* shader_pair = &instance.shaders[j][k];

					RenderItem& render_item = render_queues[pass_id].render_items[render_queues[pass_id].size];
					render_item.mesh_cmds = pass.mesh_commands;
					render_item.shader = &shader_pair->shader;
					render_item.material_cmds = subset.material;

					if(count > 1 && shader_pair->instancing_permutation != nullptr)
					{
						render_item.shader = shader_pair->instancing_permutation;
					}
					else
					{

					}

					render_item.draw_call = &subset.draw_call;

					SortItem& sort_item = render_queues[pass_id].sort_items[render_queues[pass_id].size];
					sort_item.item = &render_item;

					sort_item.sort_key = subset.sort_key | depth16;

					render_queues[pass_id].size++;
				}
			}

			start = i + 1;
			start_index = view.visibles[start];
			count = 1;
		}
	}

	//Sort queues
	for(u8 i = 0; i < num_passes; i++)
	{
		if(render_queues[i].render_items == nullptr)
			continue;

		std::sort(render_queues[i].sort_items, render_queues[i].sort_items + render_queues[i].size,
			      [](const SortItem& x, const SortItem& y)
		{
			return x.sort_key < y.sort_key;
		});
	}

	return render_queues;
}

*/