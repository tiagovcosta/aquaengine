#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#if _WIN32
#include <d3d11.h>
#endif

#include "..\..\AquaTypes.h"

namespace aqua
{
#if _WIN32

	using DeviceChildH        = ID3D11DeviceChild*;

	using ResourceH           = ID3D11Resource*;

	using BufferH             = ID3D11Buffer*;
	using ShaderResourceH     = ID3D11ShaderResourceView*;
	using UnorderedAccessH    = ID3D11UnorderedAccessView*;
	using RenderTargetH       = ID3D11RenderTargetView*;
	using DepthStencilTargetH = ID3D11DepthStencilView*;

	using BlendStateH         = ID3D11BlendState*;
	using RasterizerStateH    = ID3D11RasterizerState*;
	using DepthStencilStateH  = ID3D11DepthStencilState*;
	using SamplerStateH       = ID3D11SamplerState*;

	using VertexShaderH       = ID3D11VertexShader*;
	using HullShaderH         = ID3D11HullShader*;
	using DomainShaderH       = ID3D11DomainShader*;
	using GeometryShaderH     = ID3D11GeometryShader*;
	using PixelShaderH        = ID3D11PixelShader*;
	using ComputeShaderH      = ID3D11ComputeShader*;

	using InputLayoutH        = ID3D11InputLayout*;

	using ViewportH           = D3D11_VIEWPORT;

	using QueryH              = ID3D11Query*;

	struct PipelineState
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

	struct ComputePipelineState
	{
		ComputeShaderH shader;
	};

	//----------------------------------------------------------
	/*
	struct MapCommand
	{
		void* data;
		u32   data_size;
	};

	struct SRVMapCommand
	{
		ResourceH resource;
		void* data;
		u32   data_size;
	};

	struct CBuffer
	{
		BufferH     buffer;
		MapCommand* map_command;
	};

	struct ShaderResource
	{
		ShaderResourceH srvs;
		SRVMapCommand*  map_command;
	};

	//Create using 'createParameterGroup' function
	struct ParameterGroup
	{
		BufferH*          cbuffers;
		ShaderResourceH*  srvs;
		UnorderedAccessH* uavs;
		MapCommand*       map_commands;

	private:
		ParameterGroup() {};
	};
	*/
	//----------------------------------------------------------

	struct SamplerGroup
	{
		SamplerStateH* samplers;
		u8			   num_samplers;
	};

	/*
	struct StaticTexture
	{
		ResourceH resource;

		ShaderResourceH srv;

		u32 width;
		u32 height;
		u32 depth;
	};

	struct DynamicTexture
	{
		ResourceH resource;

		ShaderResourceH*     srvs;
		RenderTargetH*       rtvs;
		DepthStencilTargetH* dsvs;
		UnorderedAccessH*    uavs;

		u8 num_srvs;
		u8 num_rtvs;
		u8 num_dsvs;
		u8 num_uavs;

		u32 width;
		u32 height;
		u32 depth;
	};
	*/

#endif
};