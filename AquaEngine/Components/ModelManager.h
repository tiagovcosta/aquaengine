#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "EntityManager.h"

//#include "..\Renderer\Renderer.h"
#include "..\Renderer\RendererInterfaces.h"
#include "..\Renderer\RendererStructs.h"
#include "..\Renderer\ShaderManager.h"

#include "..\Renderer\ParameterCache.h"

#include "..\Core\Containers\HashMap.h"

#include "..\AquaMath.h"
#include "..\AquaTypes.h"

namespace aqua
{
	class TransformManager;

	class Renderer;
	class RenderShader;
	class ParameterGroupDesc;

	struct MeshData;
	struct Material;

	class LinearAllocator;
	class SmallBlockAllocator;

	class ModelManager : public RenderQueueGenerator
	{
	public:

		struct Instance
		{
			u32 i;

			bool valid() const
			{
				return i != INVALID_INDEX;
			}
		};

		static const u32 INVALID_INDEX = UINT32_MAX;
		static const Instance INVALID_INSTANCE;

		ModelManager(Allocator& allocator, LinearAllocator& temp_allocator,
					 Renderer& renderer, TransformManager& transform, u32 inital_capacity);
		~ModelManager();

		void update();

		// RenderQueueGenerator interface
		bool cull(u32 num_frustums, const Frustum* frustums, VisibilityData** out) override final;

		bool extract(const VisibilityData& visibility_data) override final;

		bool prepare() override final;

		bool getRenderItems(u8 num_passes, const u32* passes_names, 
							const VisibilityData& visibility_data, RenderQueue* out_queues) override final;

		//---------------------------------------

		Instance create(Entity e, const MeshData* mesh, Permutation render_permutation);
		Instance lookup(Entity e);
		void	 destroy(Instance i);

		void setMesh(Instance i, const MeshData* mesh);
		void addSubset(Instance i, DrawCall draw_call, const Material* material);

		void addSubset(Instance i, u8 index, const Material* material);

		void setCapacity(u32 new_capacity);

	private:

		struct Subset
		{
			Permutation					permutation;
			ShaderPermutation			shader;
			const CachedParameterGroup* material_params;
			const DrawCall*				draw_call;
		};

		struct InstanceData
		{
			u32                          size;
			u32                          capacity;

			void*                        buffer;

			Entity*						 entity;
			const Mesh**				 mesh;
			BoundingSphere*              bounding_sphere;
			Permutation*			     permutation;
			ParameterGroup**			 instance_params;
			const CachedParameterGroup** cached_instance_params;
			Subset*						 subset;
			//ParameterCache*            parameter_cache;
			BoundingSphere*              bounding_sphere2;
		};

		Allocator&       _allocator;
		LinearAllocator* _temp_allocator;

		SmallBlockAllocator* _params_groups_allocator;

		Renderer*		  _renderer;
		TransformManager* _transform_manager;

		HashMap<Entity, u32> _map;

		InstanceData _data;

		const RenderShader* _render_shader;

		ParametersManager _params_manager;
	};
};