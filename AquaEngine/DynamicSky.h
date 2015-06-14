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

	class DynamicSky : public RenderQueueGenerator
	{
	public:

		DynamicSky(Allocator& allocator, LinearAllocator& temp_allocator, Renderer& renderer);
		~DynamicSky();

		// RenderQueueGenerator interface
		bool cull(u32 num_frustums, const Frustum* frustums, VisibilityData** out) override final;
		bool extract(const VisibilityData& visibility_data) override final;
		bool prepare() override final;
		bool getRenderItems(u8 num_passes, const u32* passes_names, 
							const VisibilityData& visibility_data, RenderQueue* out_queues) override final;

		void update();

		Vector3 getSunColor() const;
		ShaderResourceH getRayleightScatteringTexture() const;
		ShaderResourceH getMieScatteringTexture() const;

		void setSunDirection(const Vector3& direction);
		void setScattering(ShaderResourceH scattering);

	private:

		Allocator&       _allocator;
		LinearAllocator* _temp_allocator;

		Renderer* _renderer;

		const RenderShader*      _shader;
		ShaderPermutation _shader_permutation;

		const ParameterGroupDesc* _instance_params_desc;

		const RenderShader*      _update_rayleigh_shader;
		ShaderPermutation		 _update_rayleigh_shader_permutation;

		const ParameterGroupDesc* _update_instance_params_desc;

		const RenderShader*      _update_mie_shader;
		ShaderPermutation		 _update_mie_shader_permutation;

		Mesh*    _dome_mesh;
		DrawCall _dome_draw_call;

		RenderTextureX _rayleigh;
		RenderTextureX _mie;

		RenderTargetH _rayleigh2;
		RenderTargetH _mie2;

		ShaderResourceH _rayleigh2_sr;
		ShaderResourceH _mie2_sr;

		BufferH _dome_cbuffer;

		ParameterGroup*  _dome_params;

		BufferH _update_cbuffer;

		ParameterGroup*  _update_params;

		Vector3 _wavelengths;
		Vector3 _sun_dir;

		Matrix4x4 _rotation;

		bool _update_rayleigh;
		bool _update_mie;
	};
};