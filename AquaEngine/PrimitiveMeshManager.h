#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

//#include "Renderer\RendererStructs.h"
#include "Renderer\RendererUtilities.h"

#include "AquaMath.h"
#include "AquaTypes.h"

namespace aqua
{
	class RenderDevice;
	class LinearAllocator;

	class PrimitiveMeshManager
	{
	public:

		PrimitiveMeshManager(Allocator& allocator, LinearAllocator& temp_allocator, RenderDevice& render_device);
		~PrimitiveMeshManager();

		const MeshData* getPlane();
		const MeshData* getBox();
		const MeshData* getSphere();
		const MeshData* getHalfSphere();
		const MeshData* getCone();

	private:

		Allocator&       _allocator;
		LinearAllocator& _temp_allocator;

		RenderDevice&	 _render_device;

		MeshData _plane;
		MeshData _box;
		MeshData _sphere;
		MeshData _half_sphere;
		MeshData _cone;
	};
};