#pragma once

#include "RenderDevice\RenderDeviceTypes.h"

#include "..\AquaTypes.h"

namespace aqua
{
	class ComputeShader
	{
	public:
		ComputeShader(const ComputeShaderH& compute_shader);
		~ComputeShader();

		void setFloat(u32 name, float value);
		void setInt(u32 name, int value);

		void setShaderResource(u32 name, const ShaderResourceH& sr);
		void setUAV(u32 name, const UnorderedAccessH& uav);

	private:
	};
}