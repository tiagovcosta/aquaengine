
/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////
// D3D11 RenderDevice inline functions definitions //
/////////////////////////////////////////////////////

template<typename T, typename U>
void setShader(U func, T new_shader, T& old_shader, ID3D11DeviceContext* immediate_context)
{
	if(new_shader != old_shader)
	{
		old_shader = new_shader;
		(immediate_context->*func)(new_shader, NULL, 0);
	}
}

RENDER_INLINE void RenderDevice::setPipelineState(const PipelineState* pipeline_state)
{

	bindInputLayout(pipeline_state->input_layout);

	aqua::setShader(&ID3D11DeviceContext::VSSetShader, pipeline_state->vertex_shader, _vertex_shader, _immediate_context);
	aqua::setShader(&ID3D11DeviceContext::HSSetShader, pipeline_state->hull_shader, _hull_shader, _immediate_context);
	aqua::setShader(&ID3D11DeviceContext::DSSetShader, pipeline_state->domain_shader, _domain_shader, _immediate_context);
	aqua::setShader(&ID3D11DeviceContext::GSSetShader, pipeline_state->geometry_shader, _geometry_shader, _immediate_context);
	aqua::setShader(&ID3D11DeviceContext::PSSetShader, pipeline_state->pixel_shader, _pixel_shader, _immediate_context);

	bindBlendState(_blend_states[pipeline_state->blend_state]);
	bindDepthStencilState(_depth_stencil_states[pipeline_state->depth_stencil_state]);
	bindRasterizerState(_rasterizer_states[pipeline_state->rasterizer_state]);
}
/*
RENDER_INLINE void RenderDevice::bindParameterGroup(const void* parameter_group, const void* binding_info)
{
	const u8* params_bind_info = (const u8*)binding_info;

	const ParameterGroup& params = *(const ParameterGroup*)parameter_group;

	u8 index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		if(stages & (u8)ShaderType::VERTEX_SHADER)
			VSBindConstantBuffer(params.cbuffers[index], slot);
		if(stages & (u8)ShaderType::HULL_SHADER)
			HSBindConstantBuffer(params.cbuffers[index], slot);
		if(stages & (u8)ShaderType::DOMAIN_SHADER)
			DSBindConstantBuffer(params.cbuffers[index], slot);
		if(stages & (u8)ShaderType::GEOMETRY_SHADER)
			GSBindConstantBuffer(params.cbuffers[index], slot);
		if(stages & (u8)ShaderType::PIXEL_SHADER)
			PSBindConstantBuffer(params.cbuffers[index], slot);

		if(params.map_commands != nullptr)
		{
			const auto& map_command = params.map_commands[index];

			if(map_command.data != nullptr)
			{
				auto mapped_subresource = map(params.cbuffers[index], 0, MapType::DISCARD);

				memcpy(mapped_subresource.data, map_command.data, map_command.data_size);

				unmap(params.cbuffers[index], 0);
			}
		}

		index = *(params_bind_info++);
	}

	index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		if(stages & (u8)ShaderType::VERTEX_SHADER)
			VSBindShaderResource(params.srvs[index], slot);
		if(stages & (u8)ShaderType::HULL_SHADER)
			HSBindShaderResource(params.srvs[index], slot);
		if(stages & (u8)ShaderType::DOMAIN_SHADER)
			DSBindShaderResource(params.srvs[index], slot);
		if(stages & (u8)ShaderType::GEOMETRY_SHADER)
			GSBindShaderResource(params.srvs[index], slot);
		if(stages & (u8)ShaderType::PIXEL_SHADER)
			PSBindShaderResource(params.srvs[index], slot);

		index = *(params_bind_info++);
	}

	index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		if(stages & (u8)ShaderType::PIXEL_SHADER)
			bindUnorderedAccessTarget(params.uavs[index], slot);

		index = *(params_bind_info++);
	}
}
*/

RENDER_INLINE void RenderDevice::bindParameterGroup(const CachedParameterGroup& parameter_group, const void* binding_info)
{
	const u8* params_bind_info = (const u8*)binding_info;

	//const ParameterGroup& params = *(const ParameterGroup*)parameter_group;

	u8 index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot   = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		//auto cbuffer = params.getCBuffer(index);
		auto cbuffer = parameter_group.getCBuffer(index);

		if(stages & (u8)ShaderType::VERTEX_SHADER)
			VSBindConstantBuffer(cbuffer->cbuffer, slot);
		if(stages & (u8)ShaderType::HULL_SHADER)
			HSBindConstantBuffer(cbuffer->cbuffer, slot);
		if(stages & (u8)ShaderType::DOMAIN_SHADER)
			DSBindConstantBuffer(cbuffer->cbuffer, slot);
		if(stages & (u8)ShaderType::GEOMETRY_SHADER)
			GSBindConstantBuffer(cbuffer->cbuffer, slot);
		if(stages & (u8)ShaderType::PIXEL_SHADER)
			PSBindConstantBuffer(cbuffer->cbuffer, slot);

		if(cbuffer->map_info != UINT32_MAX)
		{
			//auto map_info = params.getCBufferMapInfo(cbuffer->map_info);
			auto map_info = parameter_group.getCBufferMapInfo(cbuffer->map_info);

			auto mapped_subresource = map(cbuffer->cbuffer, 0, MapType::DISCARD);

			memcpy(mapped_subresource.data, map_info.data, map_info.size);

			unmap(cbuffer->cbuffer, 0);
		}

		index = *(params_bind_info++);
	}

	index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		//auto srv = params.getSRV(index);
		auto srv = parameter_group.getSRV(index);

		if(stages & (u8)ShaderType::VERTEX_SHADER)
			VSBindShaderResource(srv, slot);
		if(stages & (u8)ShaderType::HULL_SHADER)
			HSBindShaderResource(srv, slot);
		if(stages & (u8)ShaderType::DOMAIN_SHADER)
			DSBindShaderResource(srv, slot);
		if(stages & (u8)ShaderType::GEOMETRY_SHADER)
			GSBindShaderResource(srv, slot);
		if(stages & (u8)ShaderType::PIXEL_SHADER)
			PSBindShaderResource(srv, slot);

		index = *(params_bind_info++);
	}

	index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		//auto uav = params.getUAV(index);
		auto uav = parameter_group.getUAV(index);

		if(stages & (u8)ShaderType::PIXEL_SHADER)
			bindUnorderedAccessTarget(uav, slot);

		index = *(params_bind_info++);
	}
}

RENDER_INLINE void RenderDevice::setComputePipelineState(const ComputePipelineState* pipeline_state)
{
	aqua::setShader(&ID3D11DeviceContext::CSSetShader, pipeline_state->shader, _compute_shader, _immediate_context);
}
/*
RENDER_INLINE void RenderDevice::bindComputeParameterGroup(const void* parameter_group, const void* binding_info)
{
	const u8* params_bind_info = (const u8*)binding_info;

	const ParameterGroup& params = *(const ParameterGroup*)parameter_group;

	u8 index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		CSBindConstantBuffer(params.cbuffers[index], slot);

		if(params.map_commands != nullptr)
		{
			const auto& map_command = params.map_commands[index];

			if(map_command.data != nullptr)
			{
				auto mapped_subresource = map(params.cbuffers[index], 0, MapType::DISCARD);

				memcpy(mapped_subresource.data, map_command.data, map_command.data_size);

				unmap(params.cbuffers[index], 0);
			}
		}

		index = *(params_bind_info++);
	}

	index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		CSBindShaderResource(params.srvs[index], slot);

		index = *(params_bind_info++);
	}

	index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		CSBindUAV(params.uavs[index], slot);

		index = *(params_bind_info++);
	}
}
*/

RENDER_INLINE void RenderDevice::bindComputeParameterGroup(const void* parameter_group, const void* binding_info)
{
	const u8* params_bind_info = (const u8*)binding_info;

	const ParameterGroup& params = *(const ParameterGroup*)parameter_group;

	u8 index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot   = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		auto cbuffer = params.getCBuffer(index);

		CSBindConstantBuffer(cbuffer->cbuffer, slot);

		if(cbuffer->map_info != UINT32_MAX)
		{
			auto map_info = params.getCBufferMapInfo(cbuffer->map_info);

			auto mapped_subresource = map(cbuffer->cbuffer, 0, MapType::DISCARD);

			memcpy(mapped_subresource.data, map_info.data, map_info.size);

			unmap(cbuffer->cbuffer, 0);
		}

		index = *(params_bind_info++);
	}

	index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot   = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		auto srv = params.getSRV(index);

		CSBindShaderResource(srv, slot);

		index = *(params_bind_info++);
	}

	index = *(params_bind_info++);

	while(index != UINT8_MAX)
	{
		u8 slot   = *(params_bind_info++);
		u8 stages = *(params_bind_info++);

		auto uav = params.getUAV(index);

		CSBindUAV(uav, slot);

		index = *(params_bind_info++);
	}
}

RENDER_INLINE void RenderDevice::bindSamplerGroup(const void* sampler_group, const void* binding_info)
{
	const SamplerGroup& samplers = *(const SamplerGroup*)sampler_group;

	for(u8 i = 0; i < samplers.num_samplers; i++)
	{
		VSBindSampler(samplers.samplers[i], i);
		HSBindSampler(samplers.samplers[i], i);
		DSBindSampler(samplers.samplers[i], i);
		GSBindSampler(samplers.samplers[i], i);
		PSBindSampler(samplers.samplers[i], i);
		CSBindSampler(samplers.samplers[i], i);
	}
}

RENDER_INLINE void RenderDevice::bindVertexBuffer(BufferH vertex_buffer, u32 stride, u32 offset, u8 slot)
{
	_vertex_buffers[slot] = vertex_buffer;
	_vertex_buffers_strides[slot] = stride;
	_vertex_buffers_offsets[slot] = offset;

	_check_vertex_buffers = true;
}

RENDER_INLINE void RenderDevice::bindIndexBuffer(BufferH buffer, IndexBufferFormat format, u32 offset)
{
	if(buffer != _index_buffer || format != _index_format || offset != _index_offset)
	{
		_index_buffer = buffer;
		_index_format = format;
		_index_offset = offset;

		_immediate_context->IASetIndexBuffer(buffer, (DXGI_FORMAT)format, offset);
	}
}

RENDER_INLINE void RenderDevice::bindInputLayout(InputLayoutH input_layout)
{
	if(input_layout != _input_layout)
	{
		_input_layout = input_layout;
		_immediate_context->IASetInputLayout(input_layout);
	}
}

RENDER_INLINE void RenderDevice::bindPrimitiveTopology(PrimitiveTopology primitive_topology)
{
	if(primitive_topology != _primitive_topology)
	{
		_primitive_topology = primitive_topology;
		_immediate_context->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)primitive_topology);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

RENDER_INLINE void RenderDevice::bindConstantBuffer(ShaderStage& stage, BufferH cbuffer, u8 slot)
{
	stage.cbuffers[slot] = cbuffer;
	stage.check_cbuffers = true;
}

RENDER_INLINE void RenderDevice::bindShaderResource(ShaderStage& stage, ShaderResourceH resource, u8 slot)
{
	stage.textures[slot] = resource;
	stage.check_textures = true;
}

RENDER_INLINE void RenderDevice::bindSamplerState(ShaderStage& stage, SamplerStateH sampler, u8 slot)
{
	stage.samplers[slot] = sampler;
	stage.check_samplers = true;
}

RENDER_INLINE void RenderDevice::bindUAV(ShaderStage& stage, UnorderedAccessH uav, u8 slot)
{
	ASSERT("Unsupported" && false);
}

RENDER_INLINE void RenderDevice::VSBindShader(VertexShaderH shader)
{
	if(shader != _vertex_shader)
	{
		_vertex_shader = shader;
		_immediate_context->VSSetShader(shader, NULL, 0);
	}
}

RENDER_INLINE void RenderDevice::VSBindConstantBuffer(BufferH constant_buffer, u8 slot)
{
	_vertex_shader_stage.cbuffers[slot] = constant_buffer;
	_vertex_shader_stage.check_cbuffers = true;
}

RENDER_INLINE void RenderDevice::VSBindShaderResource(ShaderResourceH shader_resource, u8 slot)
{
	_vertex_shader_stage.textures[slot] = shader_resource;
	_vertex_shader_stage.check_textures = true;
}

RENDER_INLINE void RenderDevice::VSBindSampler(SamplerStateH sampler, u8 slot)
{
	_vertex_shader_stage.samplers[slot] = sampler;
	_vertex_shader_stage.check_samplers = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

RENDER_INLINE void RenderDevice::HSBindShader(HullShaderH shader)
{
	if(shader != _hull_shader)
	{
		_hull_shader = shader;
		_immediate_context->HSSetShader(shader, NULL, 0);
	}
}

RENDER_INLINE void RenderDevice::HSBindConstantBuffer(BufferH constant_buffer, u8 slot)
{
	_hull_shader_stage.cbuffers[slot] = constant_buffer;
	_hull_shader_stage.check_cbuffers = true;
}

RENDER_INLINE void RenderDevice::HSBindShaderResource(ShaderResourceH shader_resource, u8 slot)
{
	_hull_shader_stage.textures[slot] = shader_resource;
	_hull_shader_stage.check_textures = true;
}

RENDER_INLINE void RenderDevice::HSBindSampler(SamplerStateH sampler, u8 slot)
{
	_hull_shader_stage.samplers[slot] = sampler;
	_hull_shader_stage.check_samplers = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

RENDER_INLINE void RenderDevice::DSBindShader(DomainShaderH shader)
{
	if(shader != _domain_shader)
	{
		_domain_shader = shader;
		_immediate_context->DSSetShader(shader, NULL, 0);
	}
}

RENDER_INLINE void RenderDevice::DSBindConstantBuffer(BufferH constant_buffer, u8 slot)
{
	_domain_shader_stage.cbuffers[slot] = constant_buffer;
	_domain_shader_stage.check_cbuffers = true;
}

RENDER_INLINE void RenderDevice::DSBindShaderResource(ShaderResourceH shader_resource, u8 slot)
{
	_domain_shader_stage.textures[slot] = shader_resource;
	_domain_shader_stage.check_textures = true;
}

RENDER_INLINE void RenderDevice::DSBindSampler(SamplerStateH sampler, u8 slot)
{
	_domain_shader_stage.samplers[slot] = sampler;
	_domain_shader_stage.check_samplers = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

RENDER_INLINE void RenderDevice::GSBindShader(GeometryShaderH shader)
{
	if(shader != _geometry_shader)
	{
		_geometry_shader = shader;
		_immediate_context->GSSetShader(shader, NULL, 0);
	}
}

RENDER_INLINE void RenderDevice::GSBindConstantBuffer(BufferH constant_buffer, u8 slot)
{
	_geometry_shader_stage.cbuffers[slot] = constant_buffer;
	_geometry_shader_stage.check_cbuffers = true;
}

RENDER_INLINE void RenderDevice::GSBindShaderResource(ShaderResourceH shader_resource, u8 slot)
{
	_geometry_shader_stage.textures[slot] = shader_resource;
	_geometry_shader_stage.check_textures = true;
}

RENDER_INLINE void RenderDevice::GSBindSampler(SamplerStateH sampler, u8 slot)
{
	_geometry_shader_stage.samplers[slot] = sampler;
	_geometry_shader_stage.check_samplers = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

RENDER_INLINE void RenderDevice::PSBindShader(PixelShaderH shader)
{
	if(shader != _pixel_shader)
	{
		_pixel_shader = shader;
		_immediate_context->PSSetShader(shader, NULL, 0);
	}
}

RENDER_INLINE void RenderDevice::PSBindConstantBuffer(BufferH constant_buffer, u8 slot)
{
	_pixel_shader_stage.cbuffers[slot] = constant_buffer;
	_pixel_shader_stage.check_cbuffers = true;
}

RENDER_INLINE void RenderDevice::PSBindShaderResource(ShaderResourceH shader_resource, u8 slot)
{
	_pixel_shader_stage.textures[slot] = shader_resource;
	_pixel_shader_stage.check_textures = true;
}

RENDER_INLINE void RenderDevice::PSBindSampler(SamplerStateH sampler, u8 slot)
{
	_pixel_shader_stage.samplers[slot] = sampler;
	_pixel_shader_stage.check_samplers = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

RENDER_INLINE void RenderDevice::CSBindShader(ComputeShaderH shader)
{
	if(shader != _compute_shader)
	{
		_compute_shader = shader;
		_immediate_context->CSSetShader(shader, NULL, 0);
	}
}

RENDER_INLINE void RenderDevice::CSBindConstantBuffer(BufferH constant_buffer, u8 slot)
{
	_compute_shader_stage.cbuffers[slot] = constant_buffer;
	_compute_shader_stage.check_cbuffers = true;
}

RENDER_INLINE void RenderDevice::CSBindShaderResource(ShaderResourceH shader_resource, u8 slot)
{
	_compute_shader_stage.textures[slot] = shader_resource;
	_compute_shader_stage.check_textures = true;
}

RENDER_INLINE void RenderDevice::CSBindSampler(SamplerStateH sampler, u8 slot)
{
	_compute_shader_stage.samplers[slot] = sampler;
	_compute_shader_stage.check_samplers = true;
}

RENDER_INLINE void RenderDevice::CSBindUAV(UnorderedAccessH sampler, u8 slot)
{
	_compute_uavs[slot] = sampler;
	_compute_check_uavs = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

RENDER_INLINE void RenderDevice::bindRenderTarget(RenderTargetH render_target, u8 slot)
{
	if(_render_targets[slot] != render_target)
	{
		_render_targets[slot] = render_target;
		_update_output_merger = true;
	}
}

RENDER_INLINE void RenderDevice::bindUnorderedAccessTarget(UnorderedAccessH uav, u8 slot)
{
	if(_uavs[slot] != uav)
	{
		_uavs[slot] = uav;
		_update_output_merger = true;
	}
}

RENDER_INLINE void RenderDevice::bindDepthStencilTarget(DepthStencilTargetH depth_stencil_target)
{
	if(_depth_stencil_target != depth_stencil_target)
	{
		_depth_stencil_target = depth_stencil_target;
		_update_output_merger = true;
	}
}

RENDER_INLINE void RenderDevice::bindBlendState(BlendStateH state)
{
	if(_blend_state != state)
	{
		_blend_state = state;
		_immediate_context->OMSetBlendState(state, nullptr, 0xffffffff);
	}
}

RENDER_INLINE void RenderDevice::bindRasterizerState(RasterizerStateH state)
{
	if(_rasterizer_state != state)
	{
		_rasterizer_state = state;
		_immediate_context->RSSetState(state);
	}
}

RENDER_INLINE void RenderDevice::bindDepthStencilState(DepthStencilStateH state)
{
	if(_depth_stencil_state != state)
	{
		_depth_stencil_state = state;
		_immediate_context->OMSetDepthStencilState(state, 0);
	}
}

RENDER_INLINE void RenderDevice::bindViewport(const ViewportH& viewport)
{
	if(_viewport != viewport)
	{
		_viewport = viewport;
		_immediate_context->RSSetViewports(1, &viewport);
	}
}

//--------------------------------------------------------------------------

RENDER_INLINE void RenderDevice::draw(u32 vertex_count, u32 start_vertex)
{
	applyStateChanges();
	_immediate_context->Draw(vertex_count, start_vertex);
}

RENDER_INLINE void RenderDevice::drawIndexed(u32 index_count, u32 start_index, u32 base_vertex)
{
	applyStateChanges();
	_immediate_context->DrawIndexed(index_count, start_index, base_vertex);
}

RENDER_INLINE void RenderDevice::drawInstanced(u32 vertex_count, u32 num_instances, u32 start_vertex, u32 start_instance)
{
	applyStateChanges();
	_immediate_context->DrawInstanced(vertex_count, num_instances, start_vertex, start_instance);
}

RENDER_INLINE void RenderDevice::drawIndexedInstanced(u32 index_count, u32 num_instances, u32 start_index, u32 base_vertex, u32 start_instance)
{
	applyStateChanges();
	_immediate_context->DrawIndexedInstanced(index_count, num_instances, start_index, base_vertex, start_instance);
}

RENDER_INLINE void RenderDevice::dispatch(u32 count_x, u32 count_y, u32 count_z)
{
	applyComputeChanges();
	_immediate_context->Dispatch(count_x, count_y, count_z);
}

//--------------------------------------------------------------------------

RENDER_INLINE void RenderDevice::drawInstancedIndirect(BufferH args, u32 offset)
{
	applyStateChanges();
	_immediate_context->DrawInstancedIndirect(args, offset);
}

RENDER_INLINE void RenderDevice::drawIndexedInstancedIndirect(BufferH args, u32 offset)
{
	applyStateChanges();
	_immediate_context->DrawIndexedInstancedIndirect(args, offset);
}

RENDER_INLINE void RenderDevice::dispatchIndirect(BufferH args, u32 offset)
{
	applyComputeChanges();
	_immediate_context->DispatchIndirect(args, offset);
}

//--------------------------------------------------------------------------

RENDER_INLINE void RenderDevice::copyStructureCount(BufferH dest_buffer, u32 offset, UnorderedAccessH src_uav)
{
	_immediate_context->CopyStructureCount(dest_buffer, offset, src_uav);
}

RENDER_INLINE void RenderDevice::generateMips(ShaderResourceH srv)
{
	_immediate_context->GenerateMips(srv);
}

RENDER_INLINE MappedSubresource RenderDevice::map(ResourceH resource, u32 subresource, MapType map_type)
{
	D3D11_MAPPED_SUBRESOURCE mapped_resource;

	_immediate_context->Map(resource, subresource, (D3D11_MAP)map_type, 0, &mapped_resource);

	return{ mapped_resource.pData, mapped_resource.RowPitch, mapped_resource.DepthPitch };
}

RENDER_INLINE void RenderDevice::unmap(ResourceH resource, u32 subresource)
{
	_immediate_context->Unmap(resource, subresource);
}

RENDER_INLINE void RenderDevice::clearRenderTarget(RenderTargetH render_target, const float clear_color[4])
{
	_immediate_context->ClearRenderTargetView(render_target, clear_color);
}

RENDER_INLINE void RenderDevice::clearDepthStencilTarget(DepthStencilTargetH depth_stencil_target, float depth)
{
	_immediate_context->ClearDepthStencilView(depth_stencil_target, D3D11_CLEAR_DEPTH, depth, 0);
}

RENDER_INLINE void RenderDevice::present()
{
	_swap_chain->Present(0, 0);
}

RENDER_INLINE void RenderDevice::unbindResources()
{
	for(u32 i = 0; i < NUM_VERTEX_BUFFERS; i++)
		bindVertexBuffer(nullptr, 0, 0, i);

	_check_vertex_buffers = true;

	bindIndexBuffer(nullptr, IndexBufferFormat::UINT16, 0);

	for(u32 i = 0; i < NUM_RENDER_TARGET_SLOTS; i++)
		bindRenderTarget(nullptr, i);

	bindDepthStencilTarget(nullptr);

	for(u32 i = 0; i < NUM_TEXTURE_SLOTS; i++)
	{
		VSBindShaderResource(nullptr, i);
		HSBindShaderResource(nullptr, i);
		DSBindShaderResource(nullptr, i);
		GSBindShaderResource(nullptr, i);
		PSBindShaderResource(nullptr, i);
		CSBindShaderResource(nullptr, i);
	}

	for(u32 i = 0; i < NUM_CBUFFER_SLOTS; i++)
	{
		VSBindConstantBuffer(nullptr, i);
		HSBindConstantBuffer(nullptr, i);
		DSBindConstantBuffer(nullptr, i);
		GSBindConstantBuffer(nullptr, i);
		PSBindConstantBuffer(nullptr, i);
		CSBindConstantBuffer(nullptr, i);
	}

	for(u32 i = 0; i < NUM_UAV_SLOTS; i++)
	{
		CSBindUAV(nullptr, i);
	}

	applyStateChanges();
	applyComputeChanges();
}

RENDER_INLINE void RenderDevice::unbindComputeResources()
{
	for(u32 i = 0; i < NUM_TEXTURE_SLOTS; i++)
		CSBindShaderResource(nullptr, i);

	for(u32 i = 0; i < NUM_CBUFFER_SLOTS; i++)
		CSBindConstantBuffer(nullptr, i);

	for(u32 i = 0; i < NUM_UAV_SLOTS; i++)
		CSBindUAV(nullptr, i);

	applyComputeChanges();
}

RENDER_INLINE void RenderDevice::begin(const QueryH& query)
{
	_immediate_context->Begin(query);
}

RENDER_INLINE void RenderDevice::end(const QueryH& query)
{
	_immediate_context->End(query);
}

RENDER_INLINE bool RenderDevice::checkStatus(const QueryH& query)
{
	return _immediate_context->GetData(query, nullptr, 0, 0) == S_OK;
}

RENDER_INLINE void RenderDevice::getData(const QueryH& query, void* data, u32 data_size)
{
	_immediate_context->GetData(query, data, data_size, 0);
}

template<typename T, typename U>
void checkAndApplyResources(U func, T* resources, T* prev_resources, u8 num_resources, bool& check,
	ID3D11DeviceContext* immediate_context)
{
	if(check)
	{
		u32 start = 0;
		u32 count = 0;

		for(u32 i = 0; i < num_resources; i++)
		{
			if(resources[i] != prev_resources[i])
			{
				count++;
				prev_resources[i] = resources[i];
			}
			else
			{
				if(count)
					(immediate_context->*func)(start, count, &resources[start]);

				start = i + 1;
				count = 0;
			}
		}

		if(count)
			(immediate_context->*func)(start, count, &resources[start]);

		check = false;
	}
}

//Macro madness to bind resources (checks which slots must be updated and makes the API calls)
#define CHECK_AND_APPLY(resource_type, max_num, stage_name, function_name)                                              \
if(_##stage_name##_shader_stage.check_##resource_type)																	\
{																														\
	start = 0;																											\
	count = 0;																											\
																														\
	for(u32 i = 0; i < max_num; i++)																					\
		{																													\
		if(_##stage_name##_shader_stage.##resource_type##[i] != _last_##stage_name##_shader_stage.##resource_type##[i]) \
				{																												\
			count++;																									\
			_last_##stage_name##_shader_stage.##resource_type##[i] = _##stage_name##_shader_stage.##resource_type##[i]; \
				}																												\
				else																											\
		{																												\
			if(count)																									\
				_immediate_context->##function_name(start, count, &_##stage_name##_shader_stage.##resource_type[start]);\
																														\
			start = i + 1;																								\
			count = 0;																									\
		}																												\
		}																													\
																														\
	if(count)																											\
		_immediate_context->##function_name(start, count, &_##stage_name##_shader_stage.##resource_type[start]);		\
																														\
	_##stage_name##_shader_stage.check_##resource_type = false;															\
}
// #end define CHECK_AND_APPLY

RENDER_INLINE void RenderDevice::applyStateChanges()
{
	u32 start;
	u32 count;

	//VERTEX BUFFERS

	if(_check_vertex_buffers)
	{
		start = 0;
		count = 0;

		for(u32 i = 0; i < NUM_VERTEX_BUFFERS; i++)
		{
			if(_vertex_buffers[i] != _last_vertex_buffers[i] ||
				_vertex_buffers_strides[i] != _last_vertex_buffers_strides[i] ||
				_vertex_buffers_offsets[i] != _last_vertex_buffers_offsets[i])
			{
				count++;
				_last_vertex_buffers[i] = _vertex_buffers[i];
				_last_vertex_buffers_strides[i] = _vertex_buffers_strides[i];
				_last_vertex_buffers_offsets[i] = _vertex_buffers_offsets[i];
			}
			else
			{
				if(count)
					_immediate_context->IASetVertexBuffers(start, count,
					&_vertex_buffers[start],
					&_vertex_buffers_strides[start],
					&_vertex_buffers_offsets[start]);

				start = i + 1;
				count = 0;
			}
		}

		//Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::GENERAL, "%d %d", start, count);

		if(count)
			_immediate_context->IASetVertexBuffers(start, count,
			&_vertex_buffers[start],
			&_vertex_buffers_strides[start],
			&_vertex_buffers_offsets[start]);

		_check_vertex_buffers = false;
	}

	//RENDER TARGETS

	if(_update_output_merger)
	{
		s8 count = -1;

		for(u32 i = 0; i < NUM_RENDER_TARGET_SLOTS; i++)
		{
			if(_render_targets[i] != nullptr)
				count = i;
		}

		_immediate_context->OMSetRenderTargets(count + 1, _render_targets, _depth_stencil_target);
		
		_update_output_merger = false;
	}

	//SHADER STAGES TEXTURES AND CBUFFERS

	aqua::checkAndApplyResources(&ID3D11DeviceContext::VSSetShaderResources, _vertex_shader_stage.textures,
		_last_vertex_shader_stage.textures, NUM_TEXTURE_SLOTS,
		_vertex_shader_stage.check_textures, _immediate_context);

	aqua::checkAndApplyResources(&ID3D11DeviceContext::HSSetShaderResources, _hull_shader_stage.textures,
		_last_hull_shader_stage.textures, NUM_TEXTURE_SLOTS,
		_hull_shader_stage.check_textures, _immediate_context);

	aqua::checkAndApplyResources(&ID3D11DeviceContext::DSSetShaderResources, _domain_shader_stage.textures,
		_last_domain_shader_stage.textures, NUM_TEXTURE_SLOTS,
		_domain_shader_stage.check_textures, _immediate_context);

	aqua::checkAndApplyResources(&ID3D11DeviceContext::GSSetShaderResources, _geometry_shader_stage.textures,
		_last_geometry_shader_stage.textures, NUM_TEXTURE_SLOTS,
		_geometry_shader_stage.check_textures, _immediate_context);

	aqua::checkAndApplyResources(&ID3D11DeviceContext::PSSetShaderResources, _pixel_shader_stage.textures,
		_last_pixel_shader_stage.textures, NUM_TEXTURE_SLOTS,
		_pixel_shader_stage.check_textures, _immediate_context);

	//-------------------

	aqua::checkAndApplyResources(&ID3D11DeviceContext::VSSetConstantBuffers, _vertex_shader_stage.cbuffers,
		_last_vertex_shader_stage.cbuffers, NUM_CBUFFER_SLOTS,
		_vertex_shader_stage.check_cbuffers, _immediate_context);

	aqua::checkAndApplyResources(&ID3D11DeviceContext::HSSetConstantBuffers, _hull_shader_stage.cbuffers,
		_last_hull_shader_stage.cbuffers, NUM_CBUFFER_SLOTS,
		_hull_shader_stage.check_cbuffers, _immediate_context);

	aqua::checkAndApplyResources(&ID3D11DeviceContext::DSSetConstantBuffers, _domain_shader_stage.cbuffers,
		_last_domain_shader_stage.cbuffers, NUM_CBUFFER_SLOTS,
		_domain_shader_stage.check_cbuffers, _immediate_context);

	aqua::checkAndApplyResources(&ID3D11DeviceContext::GSSetConstantBuffers, _geometry_shader_stage.cbuffers,
		_last_geometry_shader_stage.cbuffers, NUM_CBUFFER_SLOTS,
		_geometry_shader_stage.check_cbuffers, _immediate_context);

	aqua::checkAndApplyResources(&ID3D11DeviceContext::PSSetConstantBuffers, _pixel_shader_stage.cbuffers,
		_last_pixel_shader_stage.cbuffers, NUM_CBUFFER_SLOTS,
		_pixel_shader_stage.check_cbuffers, _immediate_context);

	/*

	CHECK_AND_APPLY(textures, NUM_TEXTURE_SLOTS, vertex, VSSetShaderResources);
	CHECK_AND_APPLY(textures, NUM_TEXTURE_SLOTS, hull, HSSetShaderResources);
	CHECK_AND_APPLY(textures, NUM_TEXTURE_SLOTS, domain, DSSetShaderResources);
	CHECK_AND_APPLY(textures, NUM_TEXTURE_SLOTS, geometry, GSSetShaderResources);
	CHECK_AND_APPLY(textures, NUM_TEXTURE_SLOTS, pixel, PSSetShaderResources);
	//CHECK_AND_APPLY(textures, NUM_TEXTURE_SLOTS, compute, CSSetShaderResources);

	CHECK_AND_APPLY(cbuffers, NUM_CBUFFER_SLOTS, vertex, VSSetConstantBuffers);
	CHECK_AND_APPLY(cbuffers, NUM_CBUFFER_SLOTS, hull, HSSetConstantBuffers);
	CHECK_AND_APPLY(cbuffers, NUM_CBUFFER_SLOTS, domain, DSSetConstantBuffers);
	CHECK_AND_APPLY(cbuffers, NUM_CBUFFER_SLOTS, geometry, GSSetConstantBuffers);
	CHECK_AND_APPLY(cbuffers, NUM_CBUFFER_SLOTS, pixel, PSSetConstantBuffers);
	//CHECK_AND_APPLY(cbuffers, NUM_CBUFFER_SLOTS, compute, CSSetConstantBuffers);

	*/
}

RENDER_INLINE void RenderDevice::applySamplerChanges()
{
	u32 start;
	u32 count;

	CHECK_AND_APPLY(samplers, NUM_SAMPLER_SLOTS, vertex, VSSetSamplers);
	CHECK_AND_APPLY(samplers, NUM_SAMPLER_SLOTS, hull, HSSetSamplers);
	CHECK_AND_APPLY(samplers, NUM_SAMPLER_SLOTS, domain, DSSetSamplers);
	CHECK_AND_APPLY(samplers, NUM_SAMPLER_SLOTS, geometry, GSSetSamplers);
	CHECK_AND_APPLY(samplers, NUM_SAMPLER_SLOTS, pixel, PSSetSamplers);
	//CHECK_AND_APPLY(samplers, NUM_SAMPLER_SLOTS, compute, CSSetSamplers);

	aqua::checkAndApplyResources(&ID3D11DeviceContext::CSSetSamplers, _compute_shader_stage.samplers,
		_last_compute_shader_stage.samplers, NUM_SAMPLER_SLOTS,
		_compute_shader_stage.check_samplers, _immediate_context);
}

RENDER_INLINE void RenderDevice::applyComputeChanges()
{
	u32 start;
	u32 count;

	aqua::checkAndApplyResources(&ID3D11DeviceContext::CSSetShaderResources, _compute_shader_stage.textures,
		_last_compute_shader_stage.textures, NUM_TEXTURE_SLOTS,
		_compute_shader_stage.check_textures, _immediate_context);

	aqua::checkAndApplyResources(&ID3D11DeviceContext::CSSetConstantBuffers, _compute_shader_stage.cbuffers,
		_last_compute_shader_stage.cbuffers, NUM_CBUFFER_SLOTS,
		_compute_shader_stage.check_cbuffers, _immediate_context);

	/*
	CHECK_AND_APPLY(textures, NUM_TEXTURE_SLOTS, compute, CSSetShaderResources);
	CHECK_AND_APPLY(cbuffers, NUM_CBUFFER_SLOTS, compute, CSSetConstantBuffers);
	*/

	if(_compute_check_uavs)
	{
		start = 0;
		count = 0;

		for(u32 i = 0; i < NUM_UAV_SLOTS; i++)
		{
			if(_compute_uavs[i] != _last_compute_uavs[i])
			{
				count++;
				_last_compute_uavs[i] = _compute_uavs[i];
			}
			else
			{
				if(count)
					_immediate_context->CSSetUnorderedAccessViews(start, count, &_compute_uavs[start], nullptr);

				start = i + 1;
				count = 0;
			}
		}

		if(count)
			_immediate_context->CSSetUnorderedAccessViews(start, count, &_compute_uavs[start], nullptr);

		_compute_check_uavs = false;
	}
}