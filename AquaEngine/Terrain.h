#pragma once

#include "Renderer\Renderer.h"
#include "Renderer\RendererInterfaces.h"
#include "Renderer\RendererStructs.h"
#include "Renderer\ShaderManager.h"

#include "AquaMath.h"
#include "AquaTypes.h"

namespace aqua
{
	class Renderer;
	class RenderShader;
	class ParameterGroupDesc;
	struct BoundingSphere;

	class LinearAllocator;

	class Terrain : public RenderQueueGenerator
	{
	public:

		Terrain(Allocator& allocator, LinearAllocator& temp_allocator, Renderer& renderer);
		~Terrain();

		// RenderQueueGenerator interface
		bool cull(u32 num_frustums, const Frustum* frustums, VisibilityData** out) override final;
		bool extract(const VisibilityData& visibility_data) override final;
		bool prepare() override final;
		bool getRenderItems(u8 num_passes, const u32* passes_names, 
							const VisibilityData& visibility_data, RenderQueue* out_queues) override final;

		void updateLODs(const Camera& camera);

		void setHeightMap(ShaderResourceH height_map, ShaderResourceH color_map,
						  u32 size, u32 view_distance, u32 lod_size, u32 num_lod_levels,
						  float inter_vertex_space, float height_scale, float height_offset);

		void setWireframe(bool enabled);

	private:

		void selectLOD(const Vector3& camera_position, const Vector3& patch_position, u32 level);

		Allocator&       _allocator;
		LinearAllocator* _temp_allocator;

		Renderer* _renderer;

		Permutation              _permutation;

		const RenderShader* _shader;
		ShaderPermutation	_shader_permutation;

		const ParameterGroupDesc* _material_params_desc;
		const ParameterGroupDesc* _instance_params_desc;

		Mesh*    _mesh;
		DrawCall _draw_call;

		BufferH _material_cbuffer;
		BufferH _cbuffer;

		static const u32 TER_WIDTH = 128;
		static const u32 TER_HEIGHT = 128;

		ParameterGroup*  _material_params;
		ParameterGroup** _instances_params;

		u32 _num_patches;

		u32    _size;
		u32    _num_levels;
		float* _levels_patch_size;
		float* _levels_patch_radius;
		float* _levels_ranges;

		bool _wireframe;
	};
};