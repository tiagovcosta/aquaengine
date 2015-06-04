#pragma once

#include "..\Renderer\RendererInterfaces.h"
#include "..\Renderer\RenderDevice\RenderDeviceTypes.h"

#include "..\AquaMath.h"

namespace aqua
{
	class Renderer;
	class Camera;
	class Allocator;
	class LinearAllocator;
	struct Viewport;

	namespace CSMUtilities
	{
		struct CascadedShadowMap
		{
			DepthStencilTargetH dsv;
			ShaderResourceH     srv;

			Camera*    cascades_cameras;
			Viewport*  cascades_viewports;
			Matrix4x4* cascades_view_proj;
			float*     cascades_end;

			u32 cascade_size;
			u8 num_cascades;
		};

		CascadedShadowMap createShadowMap(Allocator& allocator, Renderer& renderer, u32 cascade_size, u8 num_cascades);
		void destroyShadowMap(Allocator& allocator, CascadedShadowMap& csm);

		void updateCascadedShadowMap(CascadedShadowMap& csm, const Camera& camera, const Vector3& light_dir);
	};
};