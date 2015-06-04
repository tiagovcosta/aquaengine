#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "RenderDeviceEnums.h"
#include "RenderDeviceTypes.h"

#include "..\..\AquaTypes.h"

namespace aqua
{
	struct PipelineStateDesc
	{
		InputLayoutH    input_layout;
		VertexShaderH   vertex_shader;
		HullShaderH     hull_shader;
		DomainShaderH   domain_shader;
		GeometryShaderH geometry_shader;
		PixelShaderH    pixel_shader;
		u8              rasterizer_state;
		u8              blend_state;
		u8              depth_stencil_state;
	};

	struct SubTextureData
	{
		const void* data;
		u32         line_offset;
		u32         depth_offset;
	};

	struct BufferDesc
	{
		BufferDesc() : num_elements(0), stride(0), format(RenderResourceFormat::UNKNOWN),
			bind_flags(0), type(BufferType::DEFAULT), update_mode(UpdateMode::NONE),
			draw_indirect(false) {}
		//u32                    size;
		u32                  num_elements; // size/sizeof(format)
		u32                  stride;
		RenderResourceFormat format; //Only used by buffers with SRV and/or UAV
		u8			         bind_flags;
		BufferType           type;
		UpdateMode           update_mode;
		bool                 draw_indirect;
	};

	/*struct ByteAddressBufferDesc
	{
	u32                    size;
	u8			           bind_flags;
	bool                   indirect_args;
	UpdateMode             update_mode;
	};

	struct StructuredBufferDesc
	{
	u32                    num_elements;
	u32                    struct_size;
	bool                   counter;
	UpdateMode             update_mode;
	};

	struct AppendConsumeBufferDesc
	{
	u32                    num_elements;
	u32                    struct_size;
	};*/

	struct ConstantBufferDesc
	{
		u32                    size;
		UpdateMode             update_mode;
	};

	struct TextureViewDesc
	{
		RenderResourceFormat format;
		u32                  most_detailed_mip;
		u32                  mip_levels;
		u32                  first_array_slice;
		u32                  array_size;
	};

	struct Texture1DDesc
	{
		u32                  width;
		u32                  mip_levels;
		u32                  array_size;
		RenderResourceFormat format;
		UpdateMode           update_mode;
		bool                 generate_mips;
	};

	struct Texture2DDesc
	{
		u32                  width;
		u32                  height;
		u32                  mip_levels;
		u32                  array_size;
		RenderResourceFormat format;
		u32					 sample_count;
		u32					 sample_quality;
		UpdateMode           update_mode;
		bool                 generate_mips;
	};

	struct TextureCubeDesc
	{
		u32                  width;
		u32                  mip_levels;
		u8                   array_size;
		RenderResourceFormat format;
		bool                 immutable;
		bool                 generate_mips;
	};

	struct TextureDesc
	{
		u32                  width;
		u32                  height;
		u32                  depth;
		u32                  mip_levels;
		RenderResourceFormat format;
		u32					 sample_count;
		u32					 sample_quality;
		bool                 immutable;
		bool                 generate_mips;
		bool				 texture3d;
	};

	struct RenderTargetDesc
	{
		u32                  width;
		u32                  height;
		RenderResourceFormat format;
		bool                 generate_mips;
	};

	struct DepthStencilTargetDesc
	{
		u32                  width;
		u32                  height;
		RenderResourceFormat texture_format;
		RenderResourceFormat dst_format;
		RenderResourceFormat sr_format;
		bool                 generate_mips;
	};

	struct BlendStateDesc
	{
		bool           alpha_to_coverage_enable;
		bool           blend_enable;
		BlendOption    src_blend;
		BlendOption    dest_blend;
		BlendOperation blend_op;
		BlendOption    src_blend_alpha;
		BlendOption    dest_blend_alpha;
		BlendOperation blend_op_alpha;
		u8             render_target_write_mask;
	};

	struct RasterizerStateDesc
	{
		FillMode fill_mode;
		CullMode cull_mode;
		bool     front_counter_clockwise;
		bool     depth_clip_enable;
		bool     multisample_enable;
	};

	struct SamplerStateDesc
	{
		TextureFilter      filter;
		TextureAddressMode u_address_mode;
		TextureAddressMode v_address_mode;
		TextureAddressMode w_address_mode;
		ComparisonFunc     comparison_func;
	};

	struct DepthStencilStateDesc
	{
		bool           depth_testing_enable;
		bool           write_depth_enable;
		ComparisonFunc comparison_func;
		//TODO: stenciling support
	};

	struct InputElementDesc
	{
		const char*          name;
		u32                  index;
		u32                  input_slot;
		u32                  instance_data_step_rate;
		RenderResourceFormat format;
		InputClassification  input_class;
	};
};