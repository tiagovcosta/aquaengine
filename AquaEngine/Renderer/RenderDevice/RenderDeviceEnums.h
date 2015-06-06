#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2014
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\..\AquaTypes.h"

#if _WIN32
#include <d3d11.h>
#endif

namespace aqua
{
#if _WIN32
	enum class RenderResourceFormat : u8
	{
		UNKNOWN               = DXGI_FORMAT_UNKNOWN,
		R8_UNORM              = DXGI_FORMAT_R8_UNORM,
		RGBA8_UNORM           = DXGI_FORMAT_R8G8B8A8_UNORM,
		RG11B10_FLOAT         = DXGI_FORMAT_R11G11B10_FLOAT,
		R16_FLOAT             = DXGI_FORMAT_R16_FLOAT,
		R16_UNORM             = DXGI_FORMAT_R16_UNORM,
		RGBA16_FLOAT          = DXGI_FORMAT_R16G16B16A16_FLOAT,
		RGBA16_UNORM          = DXGI_FORMAT_R16G16B16A16_UNORM,
		R32_FLOAT             = DXGI_FORMAT_R32_FLOAT,
		RG32_FLOAT            = DXGI_FORMAT_R32G32_FLOAT,
		RGB32_FLOAT           = DXGI_FORMAT_R32G32B32_FLOAT,
		RGBA32_FLOAT          = DXGI_FORMAT_R32G32B32A32_FLOAT,
		R32_UINT              = DXGI_FORMAT_R32_UINT,
		RG32_UINT             = DXGI_FORMAT_R32G32_UINT,
		RGB32_UINT            = DXGI_FORMAT_R32G32B32_UINT,
		RGBA32_UINT           = DXGI_FORMAT_R32G32B32A32_UINT,
		D16_UNORM             = DXGI_FORMAT_D16_UNORM,
		R24G8_TYPELESS        = DXGI_FORMAT_R24G8_TYPELESS,
		D24_UNORM_S8_UINT     = DXGI_FORMAT_D24_UNORM_S8_UINT,
		R24_UNORM_X8_TYPELESS = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
		D32_FLOAT             = DXGI_FORMAT_D32_FLOAT,
		R32_TYPELESS          = DXGI_FORMAT_R32_TYPELESS,
		BC2_UNORM             = DXGI_FORMAT_BC2_UNORM
	};

	enum class IndexBufferFormat : u8
	{
		UINT16 = DXGI_FORMAT_R16_UINT,
		UINT32 = DXGI_FORMAT_R32_UINT
	};

	enum class PrimitiveTopology : u8
	{
		POINT_LIST    = D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
		TRIANGLE_LIST = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};

	enum class BlendOption : u8
	{
		ZERO             = D3D11_BLEND_ZERO,
		ONE              = D3D11_BLEND_ONE,
		SRC_COLOR        = D3D11_BLEND_SRC_COLOR,
		INV_SRC_COLOR    = D3D11_BLEND_INV_SRC_COLOR,
		SRC_ALPHA        = D3D11_BLEND_SRC_ALPHA,
		INV_SRC_ALPHA    = D3D11_BLEND_INV_SRC_ALPHA,
		DEST_ALPHA       = D3D11_BLEND_DEST_ALPHA,
		INV_DEST_ALPHA   = D3D11_BLEND_INV_DEST_ALPHA,
		DEST_COLOR       = D3D11_BLEND_DEST_COLOR,
		INV_DEST_COLOR   = D3D11_BLEND_INV_DEST_COLOR,
		SRC_ALPHA_SAT    = D3D11_BLEND_SRC_ALPHA_SAT,
		BLEND_FACTOR     = D3D11_BLEND_BLEND_FACTOR,
		INV_BLEND_FACTOR = D3D11_BLEND_INV_BLEND_FACTOR,
		SRC1_COLOR       = D3D11_BLEND_SRC1_COLOR,
		INV_SRC1_COLOR   = D3D11_BLEND_INV_SRC1_COLOR,
		SRC1_ALPHA       = D3D11_BLEND_SRC1_ALPHA,
		INV_SRC1_ALPHA   = D3D11_BLEND_INV_SRC1_ALPHA
	};

	enum class BlendOperation : u8
	{
		BLEND_OP_ADD          = D3D11_BLEND_OP_ADD,
		BLEND_OP_SUBTRACT     = D3D11_BLEND_OP_SUBTRACT,
		BLEND_OP_REV_SUBTRACT = D3D11_BLEND_OP_REV_SUBTRACT,
		BLEND_OP_MIN          = D3D11_BLEND_OP_MIN,
		BLEND_OP_MAX          = D3D11_BLEND_OP_MAX
	};

	enum class FillMode : u8
	{
		WIREFRAME = D3D11_FILL_WIREFRAME,
		SOLID     = D3D11_FILL_SOLID
	};

	enum class CullMode : u8
	{
		CULL_NONE  = D3D11_CULL_NONE,
		CULL_FRONT = D3D11_CULL_FRONT,
		CULL_BACK  = D3D11_CULL_BACK
	};

	enum class TextureFilter : u8
	{
		MIN_MAG_MIP_POINT               = D3D11_FILTER_MIN_MAG_MIP_POINT,
		MIN_MAG_POINT_MIP_LINEAR        = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR,
		MIN_POINT_MAG_LINEAR_MIP_POINT  = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
		MIN_POINT_MAG_MIP_LINEAR        = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR,
		MIN_LINEAR_MAG_MIP_POINT        = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT,
		MIN_LINEAR_MAG_POINT_MIP_LINEAR = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		MIN_MAG_LINEAR_MIP_POINT        = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		MIN_MAG_MIP_LINEAR              = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		ANISOTROPIC                     = D3D11_FILTER_ANISOTROPIC,

		COMPARISON_MIN_MAG_MIP_POINT               = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT,
		COMPARISON_MIN_MAG_POINT_MIP_LINEAR        = D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR,
		COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT  = D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT,
		COMPARISON_MIN_POINT_MAG_MIP_LINEAR        = D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR,
		COMPARISON_MIN_LINEAR_MAG_MIP_POINT        = D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT,
		COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR = D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		COMPARISON_MIN_MAG_LINEAR_MIP_POINT        = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		COMPARISON_MIN_MAG_MIP_LINEAR              = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
		COMPARISON_ANISOTROPIC                     = D3D11_FILTER_COMPARISON_ANISOTROPIC,
	};

	enum class TextureAddressMode : u8
	{
		WRAP          = D3D11_TEXTURE_ADDRESS_WRAP,
		MIRROR        = D3D11_TEXTURE_ADDRESS_MIRROR,
		CLAMP         = D3D11_TEXTURE_ADDRESS_CLAMP,
		//BORDER      = D3D11_TEXTURE_ADDRESS_BORDER, //Not used
		//MIRROR_ONCE = D3D11_TEXTURE_ADDRESS_MIRROR_ONCE //Not used
	};

	enum class ComparisonFunc : u8
	{
		NEVER         = D3D11_COMPARISON_NEVER,
		LESS          = D3D11_COMPARISON_LESS,
		EQUAL         = D3D11_COMPARISON_EQUAL,
		LESS_EQUAL    = D3D11_COMPARISON_LESS_EQUAL,
		GREATER       = D3D11_COMPARISON_GREATER,
		NOT_EQUAL     = D3D11_COMPARISON_NOT_EQUAL,
		GREATER_EQUAL = D3D11_COMPARISON_GREATER_EQUAL,
		ALWAYS        = D3D11_COMPARISON_ALWAYS
	};

	enum class InputClassification : u8
	{
		PER_VERTEX_DATA   = D3D11_INPUT_PER_VERTEX_DATA,
		PER_INSTANCE_DATA = D3D11_INPUT_PER_INSTANCE_DATA
	};

	enum class MapType : u8
	{
		READ         = D3D11_MAP_READ,
		WRITE        = D3D11_MAP_WRITE,
		READ_WRITE   = D3D11_MAP_READ_WRITE,
		DISCARD      = D3D11_MAP_WRITE_DISCARD,
		NO_OVERWRITE = D3D11_MAP_WRITE_NO_OVERWRITE
	};

	enum class QueryType : u8
	{
		EVENT              = D3D11_QUERY_EVENT,
		//D3D11_QUERY_OCCLUSION,
		TIMESTAMP          = D3D11_QUERY_TIMESTAMP,
		TIMESTAMP_DISJOINT = D3D11_QUERY_TIMESTAMP_DISJOINT,
		//D3D11_QUERY_PIPELINE_STATISTICS,
		//D3D11_QUERY_OCCLUSION_PREDICATE,
		//D3D11_QUERY_SO_STATISTICS,
		//D3D11_QUERY_SO_OVERFLOW_PREDICATE,
		//D3D11_QUERY_SO_STATISTICS_STREAM0,
		//D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0,
		//D3D11_QUERY_SO_STATISTICS_STREAM1,
		//D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1,
		//D3D11_QUERY_SO_STATISTICS_STREAM2,
		//D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2,
		//D3D11_QUERY_SO_STATISTICS_STREAM3,
		//D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3

	};

#endif
	/*
	enum class ShaderType : u8
	{
	UNKNOWN_SHADER  = 0x00,
	VERTEX_SHADER   = 0x01,
	HULL_SHADER     = 0x02,
	DOMAIN_SHADER   = 0x03,
	GEOMETRY_SHADER = 0x04,
	PIXEL_SHADER    = 0x05,
	COMPUTE_SHADER  = 0x06
	};
	*/
	enum class ShaderType : u8
	{
		VERTEX_SHADER   = 1 << 0,
		HULL_SHADER     = 1 << 1,
		DOMAIN_SHADER   = 1 << 2,
		GEOMETRY_SHADER = 1 << 3,
		PIXEL_SHADER    = 1 << 4,
		COMPUTE_SHADER  = 1 << 5
	};

	enum class BufferBindFlag : u8
	{
		VERTEX             = 1 << 0,
		INDEX              = 1 << 1,
		STREAM_OUTPUT      = 1 << 2
		//SHADER_RESOURCE  = 1 << 3,
		//UNORDORED_ACCESS = 1 << 4
	};

	enum class BufferType : u8
	{
		DEFAULT,
		STRUCTURED,
		RAW,
		APPEND,
		COUNTER
		//DRAW_INDIRECT
	};

	/*enum class TEXTURE_BIND_FLAG : u8
	{
	SHADER_RESOURCE      = 1 << 0,
	RENDER_TARGET        = 1 << 1,
	DEPTH_STENCIL_TARGET = 1 << 2,
	UNORDORED_ACCESS     = 1 << 3
	};*/

	enum class UpdateMode : u8
	{
		NONE = 0,
		CPU  = 1,
		GPU  = 2
	};
};