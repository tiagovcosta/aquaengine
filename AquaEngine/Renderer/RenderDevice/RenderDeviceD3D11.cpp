#include "RenderDevice.h"

#include "..\..\Core\Allocators\Allocator.h"
#include "..\..\Core\Allocators\LinearAllocator.h"
#include "..\..\Core\Allocators\DynamicLinearAllocator.h"
#include "..\..\Core\Allocators\ScopeStack.h"

#include "..\..\Utilities\Logger.h"
#include "..\..\Utilities\Debug.h"

using namespace aqua;

#if _WIN32

RenderDevice::RenderDevice()
{
	_device            = nullptr;
	_immediate_context = nullptr;
	_swap_chain        = nullptr;
	_debugger          = nullptr;
	_back_buffer_rt    = nullptr;
	_back_buffer_uav   = nullptr;

	_check_vertex_buffers = false;

	for(u8 i = 0; i < NUM_VERTEX_BUFFERS; i++)
	{
		_vertex_buffers[i]              = nullptr;
		_vertex_buffers_strides[i]      = 0;
		_vertex_buffers_offsets[i]      = 0;
		_last_vertex_buffers[i]         = nullptr;
		_last_vertex_buffers_strides[i] = 0;
		_last_vertex_buffers_offsets[i] = 0;
	}

	_index_buffer = nullptr;
	_index_format = IndexBufferFormat::UINT16;
	_index_offset = 0;

	for(u8 i = 0; i < NUM_RENDER_TARGET_SLOTS; i++)
		_render_targets[i] = nullptr;

	for(u8 i = 0; i < NUM_UAV_SLOTS; i++)
		_uavs[i] = nullptr;
}

RenderDevice::~RenderDevice()
{

}

int RenderDevice::init(u32 width, u32 height, WindowH wndH, bool windowed,
					   bool back_buffer_render_target, bool back_buffer_uav,
					   Allocator& allocator, LinearAllocator& temp_allocator)
{
	_main_allocator  = &allocator;
	_temp_allocator  = &temp_allocator;
	_frame_allocator = allocator::allocateNew<DynamicLinearAllocator>(*_main_allocator, *_main_allocator, 4096, 16);

	//Check supported feature level
	D3D_FEATURE_LEVEL feature_level;

	if(FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
		D3D11_SDK_VERSION, NULL, &feature_level, NULL)))
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Error getting D3D Feature Level.");
		return 0;
	}

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));

	sd.BufferCount                        = 1;
	sd.BufferDesc.Width                   = width;
	sd.BufferDesc.Height                  = height;
	sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.BufferDesc.RefreshRate.Numerator   = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage                        = 0;

	if(back_buffer_render_target)
		sd.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
	if(back_buffer_uav)
		sd.BufferUsage |= DXGI_USAGE_UNORDERED_ACCESS;

	sd.OutputWindow       = wndH,
	sd.SampleDesc.Count   = 1;
	sd.SampleDesc.Quality = 0;
	sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.Windowed           = windowed;

#if !_DEBUG
	// Create D3D11Device and SwapChain
	if(FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &feature_level, 1, D3D11_SDK_VERSION, &sd,
		&_swap_chain, &_device, NULL, &_immediate_context)))
	{
		if(FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, &feature_level, 1, D3D11_SDK_VERSION, &sd,
			&_swap_chain, &_device, NULL, &_immediate_context)))
		{
			Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Error creating D3D Device.");

			return 0;
		}
	}
#else

	// Create D3D11 Device, Context and SwapChain
	if(FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, &feature_level, 1, D3D11_SDK_VERSION, &sd,
		&_swap_chain, &_device, NULL, &_immediate_context)))
	{
		if(FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_DEBUG, &feature_level, 1, D3D11_SDK_VERSION, &sd,
			&_swap_chain, &_device, NULL, &_immediate_context)))
		{
			Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Error creating D3D Device.");

			return 0;
		}
	}
#endif

	Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::RENDERER, "D3D Device Created");

	//Access Debugger Device
#if _DEBUG
	if(FAILED(_device->QueryInterface(__uuidof(ID3D11Debug), (void**)&_debugger)))
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Error accessing Direct3D Debugger.");
		return 0;
	}
#endif

	//Access Backbuffer Texture
	ID3D11Texture2D*  swap_chain_buffer = 0;

	if(FAILED(_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&swap_chain_buffer)))
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Error accessing Direct3D Backbuffer texture.");
		return 0;
	}

	if(back_buffer_render_target)
	{
		if(FAILED(_device->CreateRenderTargetView(swap_chain_buffer, 0, &_back_buffer_rt)))
		{
			Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Error creating Direct3D Backbuffer Render Target.");
			return 0;
		}
	}

	if(back_buffer_uav)
	{
		if(FAILED(_device->CreateUnorderedAccessView(swap_chain_buffer, 0, &_back_buffer_uav)))
		{
			Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::RENDERER, "Error creating Direct3D Backbuffer UAV.");
			return 0;
		}
	}

	swap_chain_buffer->Release();

	//---------------------------------------------------
	//Create blend states
	//---------------------------------------------------

	_blend_states[0] = nullptr; // Default blend state

	BlendStateDesc blend_desc;
	blend_desc.blend_enable             = true;
	blend_desc.alpha_to_coverage_enable = false;
	blend_desc.src_blend                = BlendOption::SRC_ALPHA;
	blend_desc.dest_blend               = BlendOption::INV_SRC_ALPHA;
	blend_desc.blend_op                 = BlendOperation::BLEND_OP_ADD;
	blend_desc.src_blend_alpha          = BlendOption::ONE;
	blend_desc.dest_blend_alpha         = BlendOption::ZERO;
	blend_desc.blend_op_alpha           = BlendOperation::BLEND_OP_ADD;
	blend_desc.render_target_write_mask = 0x0f;

	createBlendState(blend_desc, _blend_states[1]); //alpha blend

	blend_desc.blend_enable             = true;
	blend_desc.alpha_to_coverage_enable = false;
	blend_desc.src_blend                = BlendOption::ONE;
	blend_desc.dest_blend               = BlendOption::ONE;
	blend_desc.blend_op                 = BlendOperation::BLEND_OP_ADD;
	blend_desc.src_blend_alpha          = BlendOption::ONE;
	blend_desc.dest_blend_alpha         = BlendOption::ONE;
	blend_desc.blend_op_alpha           = BlendOperation::BLEND_OP_ADD;
	blend_desc.render_target_write_mask = 0x0f;

	createBlendState(blend_desc, _blend_states[2]); //additive

	//---------------------------------------------------
	//Create rasterizer states
	//---------------------------------------------------

	_rasterizer_states[0] = nullptr; // Default rasterizer state

	RasterizerStateDesc rasterizer_desc;
	rasterizer_desc.fill_mode               = FillMode::WIREFRAME;
	rasterizer_desc.cull_mode               = CullMode::CULL_NONE;
	rasterizer_desc.front_counter_clockwise = false;
	//rasterizer_desc.DepthBias;
	//rasterizer_desc.DepthBiasClamp;
	//rasterizer_desc.SlopeScaledDepthBias;
	rasterizer_desc.depth_clip_enable       = true;
	//rasterizer_desc.ScissorEnable;
	rasterizer_desc.multisample_enable      = false;
	//rasterizer_desc.AntialiasedLineEnable;

	createRasterizerState(rasterizer_desc, _rasterizer_states[1]); //wireframe

	//--------------------------------------------------

	rasterizer_desc.fill_mode               = FillMode::SOLID;
	rasterizer_desc.cull_mode               = CullMode::CULL_NONE;
	rasterizer_desc.front_counter_clockwise = false;
	//rasterizer_desc.DepthBias;
	//rasterizer_desc.DepthBiasClamp;
	//rasterizer_desc.SlopeScaledDepthBias;
	rasterizer_desc.depth_clip_enable       = true;
	//rasterizer_desc.ScissorEnable;
	rasterizer_desc.multisample_enable      = false;
	//rasterizer_desc.AntialiasedLineEnable;

	createRasterizerState(rasterizer_desc, _rasterizer_states[2]); //no cull

	//---------------------------------------------------
	//Create depth stencil states
	//---------------------------------------------------

	_depth_stencil_states[0] = nullptr; // Default rasterizer state

	DepthStencilStateDesc depth_stencil_desc;
	depth_stencil_desc.depth_testing_enable = true;
	depth_stencil_desc.write_depth_enable   = false;
	depth_stencil_desc.comparison_func      = ComparisonFunc::LESS;
	//depth_stencil_desc.StencilEnable;
	//depth_stencil_desc.StencilReadMask;
	//depth_stencil_desc.StencilWriteMask;
	//depth_stencil_desc.FrontFace;
	//depth_stencil_desc.BackFace;

	createDepthStencilState(depth_stencil_desc, _depth_stencil_states[1]); //no depth write

	//--------------------------------------------------

	depth_stencil_desc.depth_testing_enable = true;
	depth_stencil_desc.write_depth_enable   = true;
	depth_stencil_desc.comparison_func      = ComparisonFunc::ALWAYS;

	createDepthStencilState(depth_stencil_desc, _depth_stencil_states[2]); //no depth test

	//--------------------------------------------------

	_temp_cbuffers = allocator::allocateNew<HashMap<u32, BufferH>>(*_main_allocator, *_main_allocator);

	return 1;
}

int RenderDevice::shutdown()
{
	auto it = _temp_cbuffers->begin();
	auto end = _temp_cbuffers->end();

	while(it.index < end.index)
	{
		RenderDevice::release(*it.value);

		it = _temp_cbuffers->next(it);
	}

	allocator::deallocateDelete(*_main_allocator, _temp_cbuffers);

	RenderDevice::release(_blend_states[1]);
	RenderDevice::release(_blend_states[2]);
	RenderDevice::release(_rasterizer_states[1]);
	RenderDevice::release(_rasterizer_states[2]);
	RenderDevice::release(_depth_stencil_states[1]);
	RenderDevice::release(_depth_stencil_states[2]);

	if(_back_buffer_rt != nullptr)
	{
		_back_buffer_rt->Release();
		_back_buffer_rt = nullptr;
	}

	if(_back_buffer_uav != nullptr)
	{
		_back_buffer_uav->Release();
		_back_buffer_uav = nullptr;
	}

	if(_swap_chain != nullptr)
	{
		_swap_chain->Release();
		_swap_chain = nullptr;
	}

	//_debugger->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

	if(_debugger != nullptr)
	{
		_debugger->Release();
		_debugger = nullptr;
	}

	if(_immediate_context != nullptr)
	{
		_immediate_context->Release();
		_immediate_context = nullptr;
	}

	if(_device != nullptr)
	{
		_device->Release();
		_device = nullptr;
	}

	allocator::deallocateDelete(*_main_allocator, _frame_allocator);

	return 1;
}

ID3D11Device* RenderDevice::getLowLevelDevice()
{
	return _device;
}

bool RenderDevice::createBuffer(const BufferDesc& desc, const void* initial_data,
								BufferH& out_buffer, ShaderResourceH* out_srv, UnorderedAccessH* out_uav)
{
	ASSERT(desc.num_elements > 0 && desc.stride > 0);

	ASSERT("Append buffer requires an UAV" && (desc.type != BufferType::APPEND || out_uav != nullptr));
	ASSERT("Counter buffer requires an UAV" && (desc.type != BufferType::COUNTER || out_uav != nullptr));

	D3D11_BUFFER_DESC d3d_desc;
	d3d_desc.ByteWidth           = desc.num_elements * desc.stride;
	d3d_desc.BindFlags           = 0;
	d3d_desc.CPUAccessFlags      = 0;
	d3d_desc.MiscFlags           = 0;
	d3d_desc.StructureByteStride = desc.stride;

	if(desc.update_mode == UpdateMode::NONE)
	{
		ASSERT("Cannot create Immutable buffer without initial data" && initial_data != nullptr);
		ASSERT("Cannot create UAV for Immutable buffer" && out_uav == nullptr);

		d3d_desc.Usage = D3D11_USAGE_IMMUTABLE;
	}
	else if(desc.update_mode == UpdateMode::GPU)
	{
		d3d_desc.Usage = D3D11_USAGE_DEFAULT;
	}
	else
	{
		ASSERT("Cannot create UAV for CPU-updated buffer" && out_uav == nullptr);

		d3d_desc.Usage = D3D11_USAGE_DYNAMIC;
		d3d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	if(out_uav != nullptr && (desc.bind_flags & (u8)BufferBindFlag::STREAM_OUTPUT) != 0)
		ASSERT("Cannot create UAV for a Stream Output enabled buffer" && out_uav == nullptr);

	if((desc.bind_flags & (u8)BufferBindFlag::VERTEX) != 0)
		d3d_desc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;

	if((desc.bind_flags & (u8)BufferBindFlag::INDEX) != 0)
		d3d_desc.BindFlags |= D3D11_BIND_INDEX_BUFFER;

	if((desc.bind_flags & (u8)BufferBindFlag::STREAM_OUTPUT) != 0)
		d3d_desc.BindFlags |= D3D11_BIND_STREAM_OUTPUT;

	if(out_srv != nullptr)
		d3d_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

	if(out_uav != nullptr)
		d3d_desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

	if(desc.type == BufferType::STRUCTURED || desc.type == BufferType::APPEND || desc.type == BufferType::COUNTER)
		d3d_desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	else if(desc.type == BufferType::RAW)
		d3d_desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

	if(desc.draw_indirect)
		d3d_desc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

	if(initial_data == nullptr)
	{
		if(FAILED(_device->CreateBuffer(&d3d_desc, nullptr, &out_buffer)))
			return false;
	}
	else
	{
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = initial_data;

		if(FAILED(_device->CreateBuffer(&d3d_desc, &data, &out_buffer)))
			return false;
	}

	if(out_srv != nullptr)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		srv_desc.Format = (DXGI_FORMAT)desc.format;

		if(desc.type == BufferType::RAW)
		{
			srv_desc.ViewDimension         = D3D11_SRV_DIMENSION_BUFFEREX;
			srv_desc.BufferEx.FirstElement = 0;
			srv_desc.BufferEx.NumElements  = desc.num_elements;
			srv_desc.BufferEx.Flags        = D3D11_BUFFEREX_SRV_FLAG_RAW;
		}
		else
		{
			srv_desc.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
			srv_desc.Buffer.FirstElement = 0;
			srv_desc.Buffer.NumElements  = desc.num_elements;
		}

		if(FAILED(_device->CreateShaderResourceView(out_buffer, &srv_desc, out_srv)))
		{
			release(out_buffer);
			return false;
		}
	}

	if(out_uav != nullptr)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		uav_desc.Format              = (DXGI_FORMAT)desc.format;
		uav_desc.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.NumElements  = desc.num_elements;

		if(desc.type == BufferType::DEFAULT)
			uav_desc.Buffer.Flags = 0;
		else if(desc.type == BufferType::APPEND)
			uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
		else if(desc.type == BufferType::COUNTER)
			uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
		else if(desc.type == BufferType::RAW)
			uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

		if(FAILED(_device->CreateUnorderedAccessView(out_buffer, &uav_desc, out_uav)))
		{
			release(out_buffer);

			if(out_srv != nullptr)
				release(*out_srv);

			return false;
		}
	}

	return true;
}

bool RenderDevice::createConstantBuffer(const ConstantBufferDesc& desc, const void* initial_data, BufferH& out_buffer)
{
	ASSERT(desc.size > 0 && desc.size % 16 == 0);

	D3D11_BUFFER_DESC d3d_desc;
	d3d_desc.ByteWidth           = desc.size;
	d3d_desc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
	d3d_desc.CPUAccessFlags      = 0;
	d3d_desc.MiscFlags           = 0;
	d3d_desc.StructureByteStride = 0;

	if(desc.update_mode == UpdateMode::NONE)
	{
		ASSERT("Cannot create Immutable buffer without initial data" && initial_data != nullptr);

		d3d_desc.Usage = D3D11_USAGE_IMMUTABLE;
	}
	else if(desc.update_mode == UpdateMode::GPU)
	{
		d3d_desc.Usage = D3D11_USAGE_DEFAULT;
	}
	else
	{
		d3d_desc.Usage = D3D11_USAGE_DYNAMIC;
		d3d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	if(initial_data == nullptr)
	{
		if(FAILED(_device->CreateBuffer(&d3d_desc, nullptr, &out_buffer)))
			return false;
	}
	else
	{
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = initial_data;

		if(FAILED(_device->CreateBuffer(&d3d_desc, &data, &out_buffer)))
			return false;
	}

	return true;
}

bool RenderDevice::createTexture1D(const Texture1DDesc& desc, const void** initial_data,
									u8 num_srvs, const TextureViewDesc* srvs_descs,
									u8 num_rtvs, const TextureViewDesc* rtvs_descs,
									u8 num_uavs, const TextureViewDesc* uavs_descs,
									ShaderResourceH* out_srs, RenderTargetH* out_rts, UnorderedAccessH* out_uavs)
{
	D3D11_TEXTURE1D_DESC tex_desc;
	tex_desc.Width          = desc.width;
	tex_desc.MipLevels      = desc.mip_levels;
	tex_desc.ArraySize      = desc.array_size;
	tex_desc.Format         = static_cast<DXGI_FORMAT>(desc.format);
	tex_desc.Usage          = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags      = 0;
	tex_desc.CPUAccessFlags = 0;
	tex_desc.MiscFlags      = 0;

	if(desc.update_mode == UpdateMode::CPU)
	{
		ASSERT("Cannot create RTs for CPU updated texture" && num_rtvs == 0);
		ASSERT("Cannot create UAVs for CPU updated texture" && num_uavs == 0);
		ASSERT("Cannot generate mips of CPU updated texture" && desc.generate_mips == false);

		tex_desc.Usage = D3D11_USAGE_DYNAMIC;
		tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	else if(desc.update_mode == UpdateMode::NONE)
	{
		tex_desc.Usage = D3D11_USAGE_IMMUTABLE;

		ASSERT("Cannot create immutable texture without initial data" && initial_data != nullptr);
		ASSERT("Cannot create RTs for immutable texture" && num_rtvs == 0);
		ASSERT("Cannot create UAVs for immutable texture" && num_uavs == 0);
		ASSERT("Cannot generate mips of immutable texture" && desc.generate_mips == false);
	}

	if(desc.generate_mips)
	{
		tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		tex_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		tex_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
	}

	if(num_srvs != 0)
	{
		tex_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}

	if(num_rtvs != 0)
	{
		tex_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
	}

	if(num_uavs != 0)
	{
		tex_desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	ID3D11Texture1D* texture;

	if(initial_data == nullptr)
	{
		if(FAILED(_device->CreateTexture1D(&tex_desc, nullptr, &texture)))
			return false;
	}
	else
	{
		u32 num_subresources = tex_desc.ArraySize * tex_desc.MipLevels;

		ScopeStack scope(*_temp_allocator);

		auto data = scope.newPODArray<D3D11_SUBRESOURCE_DATA>(num_subresources);

		for(u32 i = 0; i < num_subresources; i++)
		{
			data[i].pSysMem = initial_data[i];
		}

		if(FAILED(_device->CreateTexture1D(&tex_desc, data, &texture)))
			return false;
	}

	if(num_srvs != 0)
	{
		for(u8 i = 0; i < num_srvs; i++)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC view_desc;
			view_desc.Format = static_cast<DXGI_FORMAT>(srvs_descs[i].format);

			if(tex_desc.ArraySize == 1)
			{
				view_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE1D;
				view_desc.Texture1D.MostDetailedMip = srvs_descs[i].most_detailed_mip;
				view_desc.Texture1D.MipLevels       = srvs_descs[i].mip_levels;
			}
			else
			{
				view_desc.ViewDimension                  = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
				view_desc.Texture1DArray.MostDetailedMip = srvs_descs[i].most_detailed_mip;
				view_desc.Texture1DArray.MipLevels       = srvs_descs[i].mip_levels;
				view_desc.Texture1DArray.FirstArraySlice = srvs_descs[i].first_array_slice;
				view_desc.Texture1DArray.ArraySize       = srvs_descs[i].array_size;
			}

			if(FAILED(_device->CreateShaderResourceView(texture, &view_desc, &out_srs[i])))
			{
				release(texture);
				return false;
			}
		}
	}

	if(num_rtvs != 0)
	{
		for(u8 i = 0; i < num_rtvs; i++)
		{
			D3D11_RENDER_TARGET_VIEW_DESC view_desc;
			view_desc.Format = static_cast<DXGI_FORMAT>(rtvs_descs[i].format);

			if(tex_desc.ArraySize == 1)
			{
				view_desc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE1D;
				view_desc.Texture1D.MipSlice = rtvs_descs[i].most_detailed_mip;
			}
			else
			{
				view_desc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
				view_desc.Texture1DArray.MipSlice        = rtvs_descs[i].most_detailed_mip;
				view_desc.Texture1DArray.FirstArraySlice = rtvs_descs[i].first_array_slice;
				view_desc.Texture1DArray.ArraySize       = rtvs_descs[i].array_size;
			}

			if(FAILED(_device->CreateRenderTargetView(texture, &view_desc, &out_rts[i])))
			{
				release(texture);
				return false;
			}
		}
	}

	if(num_uavs != 0)
	{
		for(u8 i = 0; i < num_uavs; i++)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC view_desc;
			view_desc.Format = static_cast<DXGI_FORMAT>(uavs_descs[i].format);

			if(tex_desc.ArraySize == 1)
			{
				view_desc.ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE1D;
				view_desc.Texture1D.MipSlice = uavs_descs[i].most_detailed_mip;
			}
			else
			{
				view_desc.ViewDimension                  = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
				view_desc.Texture1DArray.MipSlice        = uavs_descs[i].most_detailed_mip;
				view_desc.Texture1DArray.FirstArraySlice = uavs_descs[i].first_array_slice;
				view_desc.Texture1DArray.ArraySize       = uavs_descs[i].array_size;
			}

			if(FAILED(_device->CreateUnorderedAccessView(texture, &view_desc, &out_uavs[i])))
			{
				release(texture);
				return false;
			}
		}
	}

	return true;
}

bool RenderDevice::createTexture2D(const Texture2DDesc& desc, const SubTextureData* initial_data,
	u8 num_srvs, const TextureViewDesc* srvs_descs,
	u8 num_rtvs, const TextureViewDesc* rtvs_descs,
	u8 num_dsvs, const TextureViewDesc* dsvs_descs,
	u8 num_uavs, const TextureViewDesc* uavs_descs,
	ShaderResourceH* out_srs, RenderTargetH* out_rts,
	DepthStencilTargetH* out_dsvs, UnorderedAccessH* out_uavs)
{
	D3D11_TEXTURE2D_DESC tex_desc;
	tex_desc.Width              = desc.width;
	tex_desc.Height             = desc.height;
	tex_desc.MipLevels          = desc.mip_levels;
	tex_desc.ArraySize          = desc.array_size;
	tex_desc.Format             = static_cast<DXGI_FORMAT>(desc.format);
	tex_desc.SampleDesc.Count   = desc.sample_count;
	tex_desc.SampleDesc.Quality = desc.sample_quality;
	tex_desc.Usage              = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags          = 0;
	tex_desc.CPUAccessFlags     = 0;
	tex_desc.MiscFlags          = 0;

	if(desc.update_mode == UpdateMode::CPU)
	{
		ASSERT("Cannot create RTs for CPU updated texture" && num_rtvs == 0);
		ASSERT("Cannot create UAVs for CPU updated texture" && num_uavs == 0);
		ASSERT("Cannot generate mips of CPU updated texture" && desc.generate_mips == false);

		tex_desc.Usage = D3D11_USAGE_DYNAMIC;
		tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	else if(desc.update_mode == UpdateMode::NONE)
	{
		tex_desc.Usage = D3D11_USAGE_IMMUTABLE;

		ASSERT("Cannot create immutable texture without initial data" && initial_data != nullptr);
		ASSERT("Cannot create RTs for immutable texture" && num_rtvs == 0);
		ASSERT("Cannot create UAVs for immutable texture" && num_uavs == 0);
		ASSERT("Cannot generate mips of immutable texture" && desc.generate_mips == false);
	}

	if(desc.generate_mips)
	{
		tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		tex_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		tex_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
	}

	if(num_srvs != 0)
	{
		tex_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}

	if(num_rtvs != 0)
	{
		tex_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
	}

	if(num_dsvs != 0)
	{
		tex_desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
	}

	if(num_uavs != 0)
	{
		tex_desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	ID3D11Texture2D* texture;

	if(initial_data == nullptr)
	{
		if(FAILED(_device->CreateTexture2D(&tex_desc, nullptr, &texture)))
			return false;
	}
	else
	{
		u32 num_subresources = tex_desc.ArraySize * tex_desc.MipLevels;

		ScopeStack scope(*_temp_allocator);

		auto data = scope.newPODArray<D3D11_SUBRESOURCE_DATA>(num_subresources);

		for(u32 i = 0; i < num_subresources; i++)
		{
			data[i].pSysMem     = initial_data[i].data;
			data[i].SysMemPitch = initial_data[i].line_offset;
		}

		if(FAILED(_device->CreateTexture2D(&tex_desc, data, &texture)))
			return false;
	}

	if(num_srvs != 0)
	{
		for(u8 i = 0; i < num_srvs; i++)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC view_desc;
			view_desc.Format = static_cast<DXGI_FORMAT>(srvs_descs[i].format);

			if(tex_desc.ArraySize == 1)
			{
				view_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
				view_desc.Texture2D.MostDetailedMip = srvs_descs[i].most_detailed_mip;
				view_desc.Texture2D.MipLevels       = srvs_descs[i].mip_levels;
			}
			else
			{
				view_desc.ViewDimension                  = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
				view_desc.Texture2DArray.MostDetailedMip = srvs_descs[i].most_detailed_mip;
				view_desc.Texture2DArray.MipLevels       = srvs_descs[i].mip_levels;
				view_desc.Texture2DArray.FirstArraySlice = srvs_descs[i].first_array_slice;
				view_desc.Texture2DArray.ArraySize       = srvs_descs[i].array_size;
			}

			if(FAILED(_device->CreateShaderResourceView(texture, &view_desc, &out_srs[i])))
			{
				release(texture);
				return false;
			}
		}
	}

	if(num_rtvs != 0)
	{
		for(u8 i = 0; i < num_rtvs; i++)
		{
			D3D11_RENDER_TARGET_VIEW_DESC view_desc;
			view_desc.Format = static_cast<DXGI_FORMAT>(rtvs_descs[i].format);

			if(tex_desc.ArraySize == 1)
			{
				view_desc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
				view_desc.Texture2D.MipSlice = rtvs_descs[i].most_detailed_mip;
			}
			else
			{
				view_desc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
				view_desc.Texture2DArray.MipSlice        = rtvs_descs[i].most_detailed_mip;
				view_desc.Texture2DArray.FirstArraySlice = rtvs_descs[i].first_array_slice;
				view_desc.Texture2DArray.ArraySize       = rtvs_descs[i].array_size;
			}

			if(FAILED(_device->CreateRenderTargetView(texture, &view_desc, &out_rts[i])))
			{
				release(texture);
				return false;
			}
		}
	}

	if(num_dsvs != 0)
	{
		for(u8 i = 0; i < num_dsvs; i++)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC view_desc;
			view_desc.Format = static_cast<DXGI_FORMAT>(dsvs_descs[i].format);
			view_desc.Flags = 0;

			if(tex_desc.ArraySize == 1)
			{
				view_desc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
				view_desc.Texture2D.MipSlice = dsvs_descs[i].most_detailed_mip;
			}
			else
			{
				view_desc.ViewDimension                  = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
				view_desc.Texture2DArray.MipSlice        = dsvs_descs[i].most_detailed_mip;
				view_desc.Texture2DArray.FirstArraySlice = dsvs_descs[i].first_array_slice;
				view_desc.Texture2DArray.ArraySize       = dsvs_descs[i].array_size;
			}

			if(FAILED(_device->CreateDepthStencilView(texture, &view_desc, &out_dsvs[i])))
			{
				release(texture);
				return false;
			}
		}
	}

	if(num_uavs != 0)
	{
		for(u8 i = 0; i < num_uavs; i++)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC view_desc;
			view_desc.Format = static_cast<DXGI_FORMAT>(uavs_descs[i].format);

			if(tex_desc.ArraySize == 1)
			{
				view_desc.ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE2D;
				view_desc.Texture2D.MipSlice = uavs_descs[i].most_detailed_mip;
			}
			else
			{
				view_desc.ViewDimension                  = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
				view_desc.Texture2DArray.MipSlice        = uavs_descs[i].most_detailed_mip;
				view_desc.Texture2DArray.FirstArraySlice = uavs_descs[i].first_array_slice;
				view_desc.Texture2DArray.ArraySize       = uavs_descs[i].array_size;
			}

			if(FAILED(_device->CreateUnorderedAccessView(texture, &view_desc, &out_uavs[i])))
			{
				release(texture);
				return false;
			}
		}
	}

	release(texture);

	return true;
}

int RenderDevice::createTexture(const TextureDesc& desc, u8 num_srvs, const int* srvs_descs,
	u8 num_rtvs, const int* rtvs_descs, u8 num_uavs, const int* uavs_descs,
	ShaderResourceH* out_srs, RenderTargetH* out_rts, UnorderedAccessH* out_uavs)
{
	return 1;
}

int RenderDevice::createRenderTarget(const RenderTargetDesc& desc,
	RenderTargetH& out_render_target, ShaderResourceH& out_shader_resource)
{
	D3D11_TEXTURE2D_DESC tex_desc;

	tex_desc.Width              = desc.width;
	tex_desc.Height             = desc.height;
	tex_desc.Format             = static_cast<DXGI_FORMAT>(desc.format);
	tex_desc.MipLevels          = 1;
	tex_desc.ArraySize          = 1;
	tex_desc.SampleDesc.Count   = 1;
	tex_desc.SampleDesc.Quality = 0;
	tex_desc.Usage              = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	tex_desc.CPUAccessFlags     = 0;
	tex_desc.MiscFlags          = 0;

	if(desc.generate_mips)
	{
		tex_desc.MipLevels = 0;
		tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	ID3D11Texture2D* texture;

	if(FAILED(_device->CreateTexture2D(&tex_desc, nullptr, &texture)))
		return 0;

	D3D11_RENDER_TARGET_VIEW_DESC rt_desc;
	rt_desc.ViewDimension      = D3D11_RTV_DIMENSION::D3D11_RTV_DIMENSION_TEXTURE2D;
	rt_desc.Format             = static_cast<DXGI_FORMAT>(desc.format);
	rt_desc.Texture2D.MipSlice = 0;

	if(FAILED(_device->CreateRenderTargetView(texture, &rt_desc, &out_render_target)))
	{
		texture->Release();

		return 0;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC sr_desc;
	sr_desc.ViewDimension             = D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2D;
	sr_desc.Format                    = static_cast<DXGI_FORMAT>(desc.format);
	sr_desc.Texture2D.MostDetailedMip = 0;
	sr_desc.Texture2D.MipLevels       = -1;

	if(FAILED(_device->CreateShaderResourceView(texture, &sr_desc, &out_shader_resource)))
	{
		texture->Release();
		out_render_target->Release();

		return 0;
	}

	texture->Release();

	return 1;
}

int RenderDevice::createDepthStencilTarget(const DepthStencilTargetDesc& desc,
	DepthStencilTargetH& out_depth_stencil_target, ShaderResourceH* out_shader_resource)
{
	D3D11_TEXTURE2D_DESC tex_desc;

	tex_desc.Width              = desc.width;
	tex_desc.Height             = desc.height;
	tex_desc.Format             = static_cast<DXGI_FORMAT>(desc.texture_format);
	tex_desc.MipLevels          = 1;
	tex_desc.ArraySize          = 1;
	tex_desc.SampleDesc.Count   = 1;
	tex_desc.SampleDesc.Quality = 0;
	tex_desc.Usage              = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
	tex_desc.CPUAccessFlags     = 0;
	tex_desc.MiscFlags          = 0;

	if(out_shader_resource != nullptr)
		tex_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

	if(desc.generate_mips)
	{
		tex_desc.MipLevels = 0;
		tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	ID3D11Texture2D* texture;

	if(FAILED(_device->CreateTexture2D(&tex_desc, nullptr, &texture)))
		return 0;

	D3D11_DEPTH_STENCIL_VIEW_DESC ds_desc;
	ds_desc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
	ds_desc.Format             = static_cast<DXGI_FORMAT>(desc.dst_format);
	ds_desc.Texture2D.MipSlice = 0;
	ds_desc.Flags              = 0;

	if(FAILED(_device->CreateDepthStencilView(texture, &ds_desc, &out_depth_stencil_target)))
	{
		texture->Release();

		return 0;
	}

	if(out_shader_resource != nullptr)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC sr_desc;
		sr_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
		sr_desc.Format                    = static_cast<DXGI_FORMAT>(desc.sr_format);
		sr_desc.Texture2D.MostDetailedMip = 0;
		sr_desc.Texture2D.MipLevels       = -1;

		if(FAILED(_device->CreateShaderResourceView(texture, &sr_desc, out_shader_resource)))
		{
			texture->Release();
			out_depth_stencil_target->Release();

			return 0;
		}
	}

	texture->Release();

	return 1;
}

int RenderDevice::createBlendState(const BlendStateDesc& desc, BlendStateH& out_blend_state)
{
	D3D11_BLEND_DESC d3d_desc;

	d3d_desc.AlphaToCoverageEnable                   = desc.alpha_to_coverage_enable;
	d3d_desc.IndependentBlendEnable                  = false;
	d3d_desc.RenderTarget[0].BlendEnable             = desc.blend_enable;
	d3d_desc.RenderTarget[0].BlendOp                 = (D3D11_BLEND_OP)desc.blend_op;
	d3d_desc.RenderTarget[0].SrcBlend                = (D3D11_BLEND)desc.src_blend;
	d3d_desc.RenderTarget[0].DestBlend               = (D3D11_BLEND)desc.dest_blend;
	d3d_desc.RenderTarget[0].BlendOpAlpha            = (D3D11_BLEND_OP)desc.blend_op_alpha;
	d3d_desc.RenderTarget[0].SrcBlendAlpha           = (D3D11_BLEND)desc.src_blend_alpha;
	d3d_desc.RenderTarget[0].DestBlendAlpha          = (D3D11_BLEND)desc.dest_blend_alpha;
	d3d_desc.RenderTarget[0].RenderTargetWriteMask   = desc.render_target_write_mask;
	//d3d_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	if(FAILED(_device->CreateBlendState(&d3d_desc, &out_blend_state)))
		return 0;

	return 1;
}

int RenderDevice::createRasterizerState(const RasterizerStateDesc& desc, RasterizerStateH& out_rasterizer_state)
{
	D3D11_RASTERIZER_DESC d3d_desc;

	d3d_desc.FillMode              = (D3D11_FILL_MODE)desc.fill_mode;
	d3d_desc.CullMode              = (D3D11_CULL_MODE)desc.cull_mode;
	d3d_desc.FrontCounterClockwise = desc.front_counter_clockwise;
	d3d_desc.DepthBias             = 0;
	d3d_desc.DepthBiasClamp        = 0.0f;
	d3d_desc.SlopeScaledDepthBias  = 0.0f;
	d3d_desc.DepthClipEnable       = desc.depth_clip_enable;
	d3d_desc.ScissorEnable         = false;
	d3d_desc.MultisampleEnable     = desc.multisample_enable;
	d3d_desc.AntialiasedLineEnable = false;;

	if(FAILED(_device->CreateRasterizerState(&d3d_desc, &out_rasterizer_state)))
		return 0;

	return 1;
}

int RenderDevice::createDepthStencilState(const DepthStencilStateDesc& desc, DepthStencilStateH& out_depth_stencil_state)
{
	D3D11_DEPTH_STENCIL_DESC d3d_desc;

	d3d_desc.DepthEnable = desc.depth_testing_enable;

	if(desc.write_depth_enable)
		d3d_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	else
		d3d_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

	d3d_desc.DepthFunc                    = (D3D11_COMPARISON_FUNC)desc.comparison_func;
	d3d_desc.StencilEnable                = false;
	d3d_desc.StencilReadMask              = D3D11_DEFAULT_STENCIL_READ_MASK;
	d3d_desc.StencilWriteMask             = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	d3d_desc.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
	d3d_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	d3d_desc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
	d3d_desc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
	d3d_desc.BackFace.StencilFunc         = D3D11_COMPARISON_ALWAYS;
	d3d_desc.BackFace.StencilDepthFailOp  = D3D11_STENCIL_OP_KEEP;
	d3d_desc.BackFace.StencilPassOp       = D3D11_STENCIL_OP_KEEP;
	d3d_desc.BackFace.StencilFailOp       = D3D11_STENCIL_OP_KEEP;


	if(FAILED(_device->CreateDepthStencilState(&d3d_desc, &out_depth_stencil_state)))
		return 0;

	return 1;
}

int RenderDevice::createSamplerState(const SamplerStateDesc& desc, SamplerStateH& out_sampler_state)
{
	D3D11_SAMPLER_DESC d3d_desc;

	d3d_desc.Filter         = (D3D11_FILTER)desc.filter;
	d3d_desc.AddressU       = (D3D11_TEXTURE_ADDRESS_MODE)desc.u_address_mode;
	d3d_desc.AddressV       = (D3D11_TEXTURE_ADDRESS_MODE)desc.v_address_mode;
	d3d_desc.AddressW       = (D3D11_TEXTURE_ADDRESS_MODE)desc.w_address_mode;
	d3d_desc.MinLOD         = -FLT_MAX;
	d3d_desc.MaxLOD         = FLT_MAX;
	d3d_desc.MipLODBias     = 0.0f;
	d3d_desc.MaxAnisotropy  = 16;
	d3d_desc.ComparisonFunc = (D3D11_COMPARISON_FUNC)desc.comparison_func;
	d3d_desc.BorderColor[0] = 0.0f;
	d3d_desc.BorderColor[1] = 0.0f;
	d3d_desc.BorderColor[2] = 0.0f;
	d3d_desc.BorderColor[3] = 0.0f;

	if(FAILED(_device->CreateSamplerState(&d3d_desc, &out_sampler_state)))
		return 0;

	return 1;
}

int RenderDevice::createInputLayout(const InputElementDesc* input_elements_descs, u32 num_elements,
	const void* shader_bytecode, u32 bytecode_length, InputLayoutH& out_input_layout)
{
	void* mark = _temp_allocator->getMark();

	D3D11_INPUT_ELEMENT_DESC* descs = allocator::allocateArray<D3D11_INPUT_ELEMENT_DESC>(*_temp_allocator, num_elements);

	for(u32 i = 0; i < num_elements; i++)
	{
		descs[i].SemanticName         = input_elements_descs[i].name;
		descs[i].SemanticIndex        = input_elements_descs[i].index;
		descs[i].Format               = (DXGI_FORMAT)input_elements_descs[i].format;
		descs[i].InputSlot            = input_elements_descs[i].input_slot;
		descs[i].AlignedByteOffset    = D3D11_APPEND_ALIGNED_ELEMENT;
		descs[i].InputSlotClass       = (D3D11_INPUT_CLASSIFICATION)input_elements_descs[i].input_class;
		descs[i].InstanceDataStepRate = input_elements_descs[i].instance_data_step_rate;
	}

	if(FAILED(_device->CreateInputLayout(descs, num_elements, shader_bytecode, (size_t)bytecode_length, &out_input_layout)))
	{
		_temp_allocator->rewind(mark);
		return 0;
	}

	_temp_allocator->rewind(mark);

	return 1;
}

int RenderDevice::createVertexShader(const void* shader_bytecode, size_t bytecode_length, VertexShaderH& out_vertex_shader)
{
	if(FAILED(_device->CreateVertexShader(shader_bytecode, bytecode_length, nullptr, &out_vertex_shader)))
		return 0;

	return 1;
}

int RenderDevice::createHullShader(const void* shader_bytecode, size_t bytecode_length, HullShaderH& out_hull_shader)
{
	if(FAILED(_device->CreateHullShader(shader_bytecode, bytecode_length, nullptr, &out_hull_shader)))
		return 0;

	return 1;
}

int RenderDevice::createDomainShader(const void* shader_bytecode, size_t bytecode_length, DomainShaderH& out_domain_shader)
{
	if(FAILED(_device->CreateDomainShader(shader_bytecode, bytecode_length, nullptr, &out_domain_shader)))
		return 0;

	return 1;
}

int RenderDevice::createGeometryShader(const void* shader_bytecode, size_t bytecode_length, GeometryShaderH& out_geometry_shader)
{
	if(FAILED(_device->CreateGeometryShader(shader_bytecode, bytecode_length, nullptr, &out_geometry_shader)))
		return 0;

	return 1;
}

int RenderDevice::createPixelShader(const void* shader_bytecode, size_t bytecode_length, PixelShaderH& out_pixel_shader)
{
	if(FAILED(_device->CreatePixelShader(shader_bytecode, bytecode_length, nullptr, &out_pixel_shader)))
		return 0;

	return 1;
}

int RenderDevice::createComputeShader(const void* shader_bytecode, size_t bytecode_length, ComputeShaderH& out_compute_shader)
{
	if(FAILED(_device->CreateComputeShader(shader_bytecode, bytecode_length, nullptr, &out_compute_shader)))
		return 0;

	return 1;
}

int RenderDevice::createQuery(QueryType type, QueryH& out_query)
{
	D3D11_QUERY_DESC desc;
	desc.Query     = (D3D11_QUERY)type;
	desc.MiscFlags = 0;

	if(FAILED(_device->CreateQuery(&desc, &out_query)))
		return 0;

	return 1;
}

void RenderDevice::clearFrameAllocator()
{
	_frame_allocator->clear();
}

ParameterGroup* RenderDevice::createParameterGroup(Allocator& allocator, ParameterGroupType type,
												   const ParameterGroupDesc& desc, u32 dynamic_cbuffers_mask, 
												   u32 dynamic_srvs_mask, const u32* srvs_sizes)
{
	ParameterGroup out;

	size_t size = sizeof(ParameterGroup);

	u8 diff = size % 8;

	if(diff != 0)
		size += 8 - diff;

	out.cbuffers_offset = (u16)size;

	size += desc.getNumCBuffers() * sizeof(CBuffer);

	out.srvs_offset = (u16)size;

	size += desc.getNumSRVs() * sizeof(SRV);

	ASSERT(size < UINT16_MAX);

	out.uavs_offset = (u16)size;

	size += desc.getNumUAVs() * sizeof(UnorderedAccessH);

	//----------

	u8 num_dynamic_cbuffers = 0;

	for(u8 i = 0; i < desc.getNumCBuffers(); i++)
	{
		if((dynamic_cbuffers_mask & (1 << i)) != 0)
			num_dynamic_cbuffers++;
	}

	u32 cbuffers_map_info_offset = (u32)size;

	size += num_dynamic_cbuffers * sizeof(CBufferMapInfo);

	u8 num_dynamic_srvs = 0;

	for(u8 i = 0; i < desc.getNumSRVs(); i++)
	{
		if((dynamic_srvs_mask & (1 << i)) != 0)
			num_dynamic_srvs++;
	}

	u32 srvs_map_info_offset = (u32)size;

	size += num_dynamic_srvs * sizeof(SRVMapInfo);

	//----------

	//Make sure that constants will be 16 byte aligned
	//Check https://developer.nvidia.com/content/constant-buffers-without-constant-pain-0

	diff = size % 16;

	if(diff != 0)
		size += 16 - diff;

	u32 constants_offset = (u32)size;

	const u32* cbuffers_sizes = desc.getCBuffersSizes();

	for(u8 i = 0; i < num_dynamic_cbuffers; i++)
		size += cbuffers_sizes[i];

	for(u8 i = 0; i < num_dynamic_srvs; i++)
		size += srvs_sizes[i];

	ASSERT(size <= UINT32_MAX);

	out.size = (u32)size;

	char* const data = (char*)allocator.allocate(size, 16);

	auto cbuffers = (CBuffer*)(data + out.cbuffers_offset);
	auto cbuffers_map_info = (CBufferMapInfo*)(data + cbuffers_map_info_offset);

	u8 current_dynamic_cbuffer = 0;

	for(u32 i = 0; i < desc.getNumCBuffers(); i++)
	{
		//cbuffers[i].cbuffer = nullptr;

		if((dynamic_cbuffers_mask & (1 << i)) != 0)
		{
			cbuffers[i].cbuffer = getTemporaryCBuffer(type, i, cbuffers_sizes[i]);
			cbuffers[i].map_info = cbuffers_map_info_offset + current_dynamic_cbuffer*sizeof(CBufferMapInfo);

			cbuffers_map_info[current_dynamic_cbuffer].offset = constants_offset;
			cbuffers_map_info[current_dynamic_cbuffer].size = cbuffers_sizes[i];

			constants_offset += cbuffers_sizes[i];

			current_dynamic_cbuffer++;
		}
		else
		{
			cbuffers[i].map_info = UINT32_MAX;
		}
	}

	auto srvs = (SRV*)(data + out.srvs_offset);
	auto srvs_map_info = (SRVMapInfo*)(data + srvs_map_info_offset);

	u8 current_dynamic_srv = 0;

	for(u32 i = 0; i < desc.getNumSRVs(); i++)
	{
		srvs[i].srv = nullptr;

		if((dynamic_srvs_mask & (1 << i)) != 0)
		{
			srvs[i].map_info = srvs_map_info_offset + current_dynamic_srv*sizeof(SRVMapInfo);

			srvs_map_info[current_dynamic_srv].resource = nullptr;
			srvs_map_info[current_dynamic_srv].offset   = constants_offset;
			srvs_map_info[current_dynamic_srv].size     = srvs_sizes[i];

			constants_offset += srvs_sizes[i];

			current_dynamic_srv++;
		}
		else
		{
			srvs[i].map_info = UINT32_MAX;
		}
	}

	*(ParameterGroup*)data = out;

	return (ParameterGroup*)data;
}

void RenderDevice::deleteParameterGroup(Allocator& allocator, ParameterGroup& param_group)
{
	allocator.deallocate(&param_group);
}

CachedParameterGroup* RenderDevice::cacheParameterGroup(const ParameterGroup& param_group)
{
	auto copy = (CachedParameterGroup*)_main_allocator->allocate(param_group.getSize(), 16);

	memcpy(copy, &param_group, param_group.getSize());

	return copy;
}

const CachedParameterGroup* RenderDevice::cacheTemporaryParameterGroup(const ParameterGroup& param_group)
{
	auto copy = (CachedParameterGroup*)_frame_allocator->allocate(param_group.getSize(), 16);

	memcpy(copy, &param_group, param_group.getSize());

	return copy;
}

void RenderDevice::deleteCachedParameterGroup(CachedParameterGroup& param_group)
{
	_main_allocator->deallocate(&param_group);
}

RenderTargetH RenderDevice::getBackBufferRT() const
{
	return _back_buffer_rt;
}

UnorderedAccessH RenderDevice::getBackBufferUAV() const
{
	return _back_buffer_uav;
}

#endif