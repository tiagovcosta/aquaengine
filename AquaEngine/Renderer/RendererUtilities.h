#pragma once

#include "..\AquaMath.h"
#include "..\AquaTypes.h"

namespace aqua
{
	//struct RenderItem;
	//struct SortItem;
	struct RenderQueue;

	struct Mesh;
	struct DrawCall;
	struct Permutation;
	//struct ParameterGroup;
	//class ParameterGroup;
	class CachedParameterGroup;
	class ParameterGroupDesc;
	class Renderer;

	//struct Model;

	class Allocator;

	struct BoundingSphere
	{
		Vector3 center;
		float   radius;
	};

	struct Permutation
	{
		Permutation(u64 v = 0) : value(v) {}
		u64 value; //do not modify
	};

	struct MeshRenderData
	{

	};

	struct MeshData
	{
		Mesh*		   mesh;
		DrawCall*	   draw_calls;
		BoundingSphere bounding_sphere;
		Permutation    permutation;
		u8			   num_subsets;
	};
	/*
	namespace mesh_utilities
	{
		void createMesh(const MeshDesc& desc, u8 num_elements, const ElementDesc* elements_descs,
			u32 num_vertices, const void* const * vertex_data, void* const * vertex_buffers);

		void fillVertexBuffers(const MeshDesc& desc, u8 num_elements, const ElementDesc* elements_descs,
							   u32 num_vertices, const void* const * vertex_data, void* const * vertex_buffers);
	}
	*/
	struct Material
	{
		Permutation			  permutation;
		CachedParameterGroup* params;
		//const CachedParameterGroup* params;
	};

	struct AABB
	{
		Vector3 min;
		Vector3 max;
	};

	AABB transformAABB(const AABB& aabb, const Matrix4x4& transform);

	namespace renderer
	{
		/*
		void drawProcedural(Renderer& renderer, PrimitiveTopology topology, u32 vertex_count, u32 instance_count)
		{

		}
		*/
		u32 cull(const Plane* planes, u32 num_actors, const BoundingSphere* bounding_spheres, u32* visibles);

		RenderQueue* createRenderQueues(u8 num_passes, u32 passes_mask);
		/*
		RenderQueue* generateRenderQueues(u8 num_passes, u32 passes_mask, const RenderWorldView& view,
										  void* instancing_buffer, u32 instancing_buffer_size, Allocator& temp_allocator);
		*/
	}
	/*
	struct ComputeKernelInstance
	{
		const ComputeShader* shader;
		u8                   kernel;
		Permutation          permutation;
		ParameterGroup*      params;
	};*/
};