#include "LightManager.h"

#include "TransformManager.h"

#include "..\Renderer\Camera.h"
#include "..\Renderer\Renderer.h"
#include "..\Renderer\ShaderManager.h"
#include "..\Renderer\RendererStructs.h"

#include "..\Renderer\RenderDevice\RenderDeviceDescs.h"

#include "..\Utilities\Blob.h"
#include "..\Utilities\StringID.h"
#include "..\Utilities\half.h"

#include <cmath>

using namespace aqua;

#define MAX_NUM_DIRECTONAL_LIGHTS 16
#define MAX_NUM_POINT_LIGHTS 2048
#define MAX_NUM_SPOT_LIGHTS 2048

const LightManager::Instance LightManager::INVALID_INSTANCE = { LightType::DIRECTIONAL, INVALID_INDEX };

LightManager::LightManager(Allocator& allocator, TransformManager& transform, Renderer& renderer, u32 inital_capacity)
	: _allocator(allocator), _map(allocator), _transform_manager(transform), _renderer(&renderer)
{
	_directional_lights_data.count    = 0;
	_directional_lights_data.capacity = 0;

	_point_lights_data.count    = 0;
	_point_lights_data.capacity = 0;

	_spot_lights_data.count    = 0;
	_spot_lights_data.capacity = 0;

	//-------------------------------------
	// Directional lights buffers
	//-------------------------------------

	BufferDesc desc;
	desc.num_elements  = MAX_NUM_DIRECTONAL_LIGHTS;
	desc.stride        = sizeof(Vector3);
	desc.bind_flags    = 0;
	desc.update_mode   = UpdateMode::CPU;
	desc.type          = BufferType::DEFAULT;
	desc.format        = RenderResourceFormat::RGB32_FLOAT;
	desc.draw_indirect = false;

	_renderer->getRenderDevice()->createBuffer(desc, nullptr, _directional_light_direction_buffer,
											   &_directional_light_direction_buffer_srv, nullptr);

	desc.num_elements  = MAX_NUM_DIRECTONAL_LIGHTS;
	desc.stride        = sizeof(u32);
	desc.bind_flags    = 0;
	desc.update_mode   = UpdateMode::CPU;
	desc.type          = BufferType::DEFAULT;
	desc.format        = RenderResourceFormat::RGBA8_UNORM;
	desc.draw_indirect = false;

	_renderer->getRenderDevice()->createBuffer(desc, nullptr, _directional_light_color_buffer,
											   &_directional_light_color_buffer_srv, nullptr);

	//-------------------------------------
	// Point lights buffers
	//-------------------------------------

	desc.num_elements  = MAX_NUM_POINT_LIGHTS;
	desc.stride        = sizeof(Vector4);
	desc.bind_flags    = 0;
	desc.update_mode   = UpdateMode::CPU;
	desc.type          = BufferType::DEFAULT;
	desc.format        = RenderResourceFormat::RGBA32_FLOAT;
	desc.draw_indirect = false;

	_renderer->getRenderDevice()->createBuffer(desc, nullptr, _point_light_position_radius_buffer,
											   &_point_light_position_radius_buffer_srv, nullptr);

	desc.num_elements  = MAX_NUM_POINT_LIGHTS;
	desc.stride        = sizeof(u32);
	desc.bind_flags    = 0;
	desc.update_mode   = UpdateMode::CPU;
	desc.type          = BufferType::DEFAULT;
	desc.format        = RenderResourceFormat::RGBA8_UNORM;
	desc.draw_indirect = false;

	_renderer->getRenderDevice()->createBuffer(desc, nullptr, _point_light_color_buffer,
											   &_point_light_color_buffer_srv, nullptr);

	//-------------------------------------
	// Spot lights buffers
	//-------------------------------------

	desc.num_elements  = MAX_NUM_SPOT_LIGHTS;
	desc.stride        = sizeof(Vector4);
	desc.bind_flags    = 0;
	desc.update_mode   = UpdateMode::CPU;
	desc.type          = BufferType::DEFAULT;
	desc.format        = RenderResourceFormat::RGBA32_FLOAT;
	desc.draw_indirect = false;

	_renderer->getRenderDevice()->createBuffer(desc, nullptr, _spot_light_position_radius_buffer,
											   &_spot_light_position_radius_buffer_srv, nullptr);

	desc.num_elements  = MAX_NUM_SPOT_LIGHTS;
	desc.stride        = sizeof(u32);
	desc.bind_flags    = 0;
	desc.update_mode   = UpdateMode::CPU;
	desc.type          = BufferType::DEFAULT;
	desc.format        = RenderResourceFormat::RGBA8_UNORM;
	desc.draw_indirect = false;

	_renderer->getRenderDevice()->createBuffer(desc, nullptr, _spot_light_color_buffer,
											   &_spot_light_color_buffer_srv, nullptr);

	desc.num_elements  = MAX_NUM_SPOT_LIGHTS;
	desc.stride        = sizeof(SpotParams);
	desc.bind_flags    = 0;
	desc.update_mode   = UpdateMode::CPU;
	desc.type          = BufferType::DEFAULT;
#if SPOT_PARAMS_HALF
	desc.format        = RenderResourceFormat::RGBA16_FLOAT;
#else
	desc.format        = RenderResourceFormat::RGBA32_FLOAT;
#endif
	desc.draw_indirect = false;

	_renderer->getRenderDevice()->createBuffer(desc, nullptr, _spot_light_params_buffer,
											   &_spot_light_params_buffer_srv, nullptr);

	//-------------------------------------

	if(inital_capacity > 0)
		setCapacity(inital_capacity);

	//-------------------------------------

	_tiled_deferred_shader = _renderer->getShaderManager()->getComputeShader(getStringID("data/shaders/tiled_deferred.cshader"));
	_tiled_deferred_kernel = _tiled_deferred_shader->getKernel(getStringID("tiled_deferred"));

	Permutation permutation;

	_tiled_deferred_kernel_permutation = _tiled_deferred_shader->getPermutation(_tiled_deferred_kernel, permutation);

	auto params_set = _tiled_deferred_shader->getKernelParameterGroupDescSet(_tiled_deferred_kernel);

	_tiled_deferred_params_desc = getParameterGroupDesc(*params_set, permutation);

	//_tiled_deferred_params = createParameterGroup(*_tiled_deferred_params_desc, true, _allocator);
	_tiled_deferred_params = _renderer->getRenderDevice()->createParameterGroup(_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																				*_tiled_deferred_params_desc, UINT32_MAX, 0, nullptr);

	_output_texture_index = _tiled_deferred_params_desc->getUAVIndex(getStringID("output_lighting_buffer"));
	/*
	ConstantBufferDesc cbuffer_desc;
	cbuffer_desc.size = _tiled_deferred_params_desc->getCBuffersSizes()[0];
	cbuffer_desc.update_mode = UpdateMode::CPU;

	if(!_renderer->getRenderDevice()->createConstantBuffer(cbuffer_desc, nullptr, _tiled_deferred_cbuffer))
	{
		//NEED ERROR MESSAGE
		//return false;
		return;
	}

	_tiled_deferred_params->cbuffers[0] = _tiled_deferred_cbuffer;
	*/
	u8 index = _tiled_deferred_params_desc->getSRVIndex(getStringID("directional_light_buffer_direction"));

	_tiled_deferred_params->setSRV(_directional_light_direction_buffer_srv, index);

	index = _tiled_deferred_params_desc->getSRVIndex(getStringID("directional_light_buffer_color"));

	_tiled_deferred_params->setSRV(_directional_light_color_buffer_srv, index);

	//-------------------

	index = _tiled_deferred_params_desc->getSRVIndex(getStringID("point_light_buffer_center_radius"));

	_tiled_deferred_params->setSRV(_point_light_position_radius_buffer_srv, index);

	index = _tiled_deferred_params_desc->getSRVIndex(getStringID("point_light_buffer_color"));

	_tiled_deferred_params->setSRV(_point_light_color_buffer_srv, index);

	//-------------------

	index = _tiled_deferred_params_desc->getSRVIndex(getStringID("spot_light_buffer_center_radius"));

	_tiled_deferred_params->setSRV(_spot_light_position_radius_buffer_srv, index);

	index = _tiled_deferred_params_desc->getSRVIndex(getStringID("spot_light_buffer_color"));

	_tiled_deferred_params->setSRV(_spot_light_color_buffer_srv, index);

	index = _tiled_deferred_params_desc->getSRVIndex(getStringID("spot_light_buffer_params"));

	_tiled_deferred_params->setSRV(_spot_light_params_buffer_srv, index);

	//-------------------

	setDirectionalLightsCapacity(MAX_NUM_DIRECTONAL_LIGHTS);
	setPointLightsCapacity(MAX_NUM_POINT_LIGHTS);
	setSpotLightsCapacity(MAX_NUM_SPOT_LIGHTS);
}

LightManager::~LightManager()
{
	_renderer->getRenderDevice()->deleteParameterGroup(_allocator, *_tiled_deferred_params);

	//RenderDevice::release(_tiled_deferred_cbuffer);

	//--------------------

	RenderDevice::release(_directional_light_direction_buffer);
	RenderDevice::release(_directional_light_direction_buffer_srv);

	RenderDevice::release(_directional_light_color_buffer);
	RenderDevice::release(_directional_light_color_buffer_srv);

	//--------------------

	RenderDevice::release(_point_light_position_radius_buffer);
	RenderDevice::release(_point_light_position_radius_buffer_srv);

	RenderDevice::release(_point_light_color_buffer);
	RenderDevice::release(_point_light_color_buffer_srv);

	//--------------------

	RenderDevice::release(_spot_light_position_radius_buffer);
	RenderDevice::release(_spot_light_position_radius_buffer_srv);

	RenderDevice::release(_spot_light_color_buffer);
	RenderDevice::release(_spot_light_color_buffer_srv);

	RenderDevice::release(_spot_light_params_buffer);
	RenderDevice::release(_spot_light_params_buffer_srv);

	//--------------------

	if(_directional_lights_data.capacity > 0)
		_allocator.deallocate(_directional_lights_data.buffer);

	if(_point_lights_data.capacity > 0)
		_allocator.deallocate(_point_lights_data.buffer);

	if(_spot_lights_data.capacity > 0)
		_allocator.deallocate(_spot_lights_data.buffer);
}

#define COLOR( r, g, b, a ) (DWORD)(((a) << 24) | ((b) << 16) | ((g) << 8) | (r))

Vector4 calculateSpotLightBoundingSphere(const Vector4& position_radius, const Vector3& direction, float angle)
{
	//float c = std::cos(angle);

	float bounding_sphere_radius = position_radius.w / 2 / std::cos(angle);

	Vector3 center = position_radius + direction * bounding_sphere_radius;

	return Vector4(center.x, center.y, center.z, bounding_sphere_radius);
}

void LightManager::setShadowsParams(ShaderResourceH shadow_map, Matrix4x4* cascades_matrices, float* cascades_ends)
{
	u8 index = _tiled_deferred_params_desc->getSRVIndex(getStringID("shadow_map"));

	_tiled_deferred_params->setSRV(shadow_map, index);

	u32 offset = _tiled_deferred_params_desc->getConstantOffset(getStringID("shadow_matrices"));

	void* cbuffers_data = _tiled_deferred_params->getCBuffersData();

	Matrix4x4* shadow_matrices = (Matrix4x4*)pointer_math::add(cbuffers_data, offset);

	memcpy(shadow_matrices, cascades_matrices, sizeof(Matrix4x4) * 4);

	offset = _tiled_deferred_params_desc->getConstantOffset(getStringID("cascade_splits"));

	float* ce = (float*)pointer_math::add(cbuffers_data, offset);

	memcpy(ce, cascades_ends, sizeof(float) * 4);
}

void LightManager::generate(const void* args_, const VisibilityData* visibility)
{
	if(_directional_lights_data.count == 0 && _point_lights_data.count == 0 && _spot_lights_data.count == 0)
		return;

	//Update light data buffers

	RenderDevice* render_device = _renderer->getRenderDevice();

	if(_directional_lights_data.count > 0)
	{
		auto mapped_subresource = render_device->map(_directional_light_direction_buffer, 0, MapType::DISCARD);

		memcpy(mapped_subresource.data, _directional_lights_data.direction, sizeof(Vector3)*_directional_lights_data.count);

		render_device->unmap(_directional_light_direction_buffer, 0);

		mapped_subresource = render_device->map(_directional_light_color_buffer, 0, MapType::DISCARD);

		memcpy(mapped_subresource.data, _directional_lights_data.color, sizeof(u32)*_directional_lights_data.count);

		render_device->unmap(_directional_light_color_buffer, 0);
	}

	//------------------

	if(_point_lights_data.count > 0)
	{
		auto mapped_subresource = render_device->map(_point_light_position_radius_buffer, 0, MapType::DISCARD);

		memcpy(mapped_subresource.data, _point_lights_data.position_radius, sizeof(Vector4)*_point_lights_data.count);

		render_device->unmap(_point_light_position_radius_buffer, 0);

		mapped_subresource = render_device->map(_point_light_color_buffer, 0, MapType::DISCARD);

		memcpy(mapped_subresource.data, _point_lights_data.color, sizeof(u32)*_point_lights_data.count);

		render_device->unmap(_point_light_color_buffer, 0);
	}

	//------------------

	if(_spot_lights_data.count > 0)
	{
		auto mapped_subresource = render_device->map(_spot_light_position_radius_buffer, 0, MapType::DISCARD);

		memcpy(mapped_subresource.data, _spot_lights_data.position_radius, sizeof(Vector4)*_spot_lights_data.count);

		render_device->unmap(_spot_light_position_radius_buffer, 0);

		mapped_subresource = render_device->map(_spot_light_color_buffer, 0, MapType::DISCARD);

		memcpy(mapped_subresource.data, _spot_lights_data.color, sizeof(u32)*_spot_lights_data.count);

		render_device->unmap(_spot_light_color_buffer, 0);

		mapped_subresource = render_device->map(_spot_light_params_buffer, 0, MapType::DISCARD);

		memcpy(mapped_subresource.data, _spot_lights_data.params, sizeof(SpotParams)*_spot_lights_data.count);

		render_device->unmap(_spot_light_params_buffer, 0);
	}
	//------------------

	const Args& args = *(const Args*)args_;

	_tiled_deferred_params->setUAV(args.target->uav, _output_texture_index);

	u8 index = _tiled_deferred_params_desc->getSRVIndex(getStringID("color_buffer"));

	_tiled_deferred_params->setSRV(args.color_buffer, index);

	index = _tiled_deferred_params_desc->getSRVIndex(getStringID("normal_buffer"));

	_tiled_deferred_params->setSRV(args.normal_buffer, index);

	index = _tiled_deferred_params_desc->getSRVIndex(getStringID("depth_buffer"));

	_tiled_deferred_params->setSRV(args.depth_buffer, index);

	index = _tiled_deferred_params_desc->getSRVIndex(getStringID("ssao_buffer"));

	_tiled_deferred_params->setSRV(args.ssao_buffer, index);

	index = _tiled_deferred_params_desc->getSRVIndex(getStringID("scattering"));

	_tiled_deferred_params->setSRV(args.scattering_buffer, index);

	/*
	index = _tiled_deferred_params_desc->getSRVIndex(getStringID("shadow_map"));

	_tiled_deferred_params->srvs[index] = args.shadow_map;
	*/

	u32 offset = _tiled_deferred_params_desc->getConstantOffset(getStringID("viewport_inv_view_proj"));

	void* cbuffers_data = _tiled_deferred_params->getCBuffersData();

	Matrix4x4* viewport_inv_view_proj = (Matrix4x4*)pointer_math::add(cbuffers_data, offset);

	Matrix4x4 inv_view_proj = args.camera->getViewProj().Invert();

	Matrix4x4 viewport_matrix(2.0f / (float)args.target->width, 0.0f, 0.0f, 0.0f,
							  0.0f, -2.0f / (float)args.target->height, 0.0f, 0.0f,
							  0.0f, 0.0f, 1.0f, 0.0f,
							  -1.0f, 1.0f, 0.0f, 1.0f);

	*viewport_inv_view_proj = viewport_matrix * inv_view_proj;

	//--

	offset = _tiled_deferred_params_desc->getConstantOffset(getStringID("num_directional_lights"));

	//Lights params
	u32* num_dir_lights = (u32*)pointer_math::add(cbuffers_data, offset);

	*num_dir_lights = _directional_lights_data.count;

	offset = _tiled_deferred_params_desc->getConstantOffset(getStringID("num_point_lights"));

	u32* num_point_lights = (u32*)pointer_math::add(cbuffers_data, offset);

	*num_point_lights = _point_lights_data.count;

	offset = _tiled_deferred_params_desc->getConstantOffset(getStringID("num_spot_lights"));

	u32* num_spot_lights = (u32*)pointer_math::add(cbuffers_data, offset);

	*num_spot_lights = _spot_lights_data.count;

	//CSM params
	/*
	offset = _tiled_deferred_params_desc->getConstantOffset(getStringID("shadow_matrices"));

	void* shadow_matrices = pointer_math::add(_tiled_deferred_params->map_commands->data, offset);

	memcpy(shadow_matrices, args.cascades_matrices, sizeof(Matrix4x4) * 4);

	offset = _tiled_deferred_params_desc->getConstantOffset(getStringID("cascade_splits"));

	void* cascades_ends = pointer_math::add(_tiled_deferred_params->map_commands->data, offset);

	memcpy(cascades_ends, args.cascades_ends, sizeof(float) * 4);
	*/
	/*
	offset = _tiled_deferred_params_desc->getConstantOffset(getStringID("shadow_matrices"));

	Matrix4x4* shadow_matrices = (Matrix4x4*)pointer_math::add(cbuffers_data, offset);

	offset = _tiled_deferred_params_desc->getConstantOffset(getStringID("cascade_splits"));

	float* ce = (float*)pointer_math::add(cbuffers_data, offset);
	*/
	//-----------------------

	static const u32 TILE_RES = 16;

	offset = _tiled_deferred_params_desc->getConstantOffset(getStringID("num_tiles_x"));

	u32* num_tiles_x = (u32*)pointer_math::add(cbuffers_data, offset);

	*num_tiles_x       = (args.target->width + TILE_RES - 1) / TILE_RES;
	*(num_tiles_x + 1) = (args.target->height + TILE_RES - 1) / TILE_RES;

	//round up number of thread groups
	_renderer->compute((args.target->width + TILE_RES - 1) / TILE_RES, (args.target->height + TILE_RES - 1) / TILE_RES, 1,
		*_tiled_deferred_kernel_permutation, *_tiled_deferred_params);

	_renderer->getRenderDevice()->unbindComputeResources();
}

void LightManager::generate(lua_State* lua_state)
{

}

LightManager::Instance LightManager::create(Entity e, LightType type)
{
	if(lookup(e).valid())
		return INVALID_INSTANCE; //Max one light per entity

	//ASSERT(_data.size <= _data.capacity);

	u32 index;
	Instance i = INVALID_INSTANCE;

	switch(type)
	{
	case LightType::DIRECTIONAL:

		if(_directional_lights_data.count == _directional_lights_data.capacity)
			setDirectionalLightsCapacity(_directional_lights_data.capacity * 2 + 8);

		index = _directional_lights_data.count & Instance::INDEX_MASK;

		i = { LightType::DIRECTIONAL, index };

		_map.insert(e, i);

		_directional_lights_data.count++;

		_directional_lights_data.entity[index]    = e;
		_directional_lights_data.direction[index] = Vector3(0.0f, 0.0f, 1.0f);
		_directional_lights_data.color[index]     = COLOR(255, 255, 255, 255);

		break;
	case LightType::POINT:

		if(_point_lights_data.count == _point_lights_data.capacity)
			setPointLightsCapacity(_point_lights_data.capacity * 2 + 8);

		index = _point_lights_data.count & Instance::INDEX_MASK;

		i = { LightType::POINT, index };

		_map.insert(e, i);

		_point_lights_data.count++;

		_point_lights_data.entity[index]          = e;
		_point_lights_data.position_radius[index] = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
		_point_lights_data.color[index]           = COLOR(255, 255, 255, 255);

		break;
	case LightType::SPOT:

		if(_spot_lights_data.count == _spot_lights_data.capacity)
			setSpotLightsCapacity(_spot_lights_data.capacity * 2 + 8);

		index = _spot_lights_data.count & Instance::INDEX_MASK;

		i = { LightType::SPOT, index };

		_map.insert(e, i);

		_spot_lights_data.count++;

		_spot_lights_data.entity[index]          = e;
		_spot_lights_data.position_radius[index] = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
		_spot_lights_data.color[index]           = COLOR(255, 255, 255, 255);
		_spot_lights_data.params[index]          = {};

		break;
	default:
		ASSERT("Unsupported light type" && false);
	}

	return i;
}

LightManager::Instance LightManager::lookup(Entity e)
{
	/*
	Instance i;
	i.i = _map.lookup(e, INVALID_INDEX);

	return i;
	*/

	return _map.lookup(e, INVALID_INSTANCE);
}

void LightManager::destroy(Instance i)
{
	LightType type = i.getType();
	u32 index = i.getIndex();

	Entity e;
	Entity last_e;
	u32 last;

	switch(type)
	{
	case LightType::DIRECTIONAL:

		last = _directional_lights_data.count - 1;

		e      = _directional_lights_data.entity[index];
		last_e = _directional_lights_data.entity[last];

		_directional_lights_data.entity[index]    = _directional_lights_data.entity[last];
		_directional_lights_data.direction[index] = _directional_lights_data.direction[last];
		_directional_lights_data.color[index]     = _directional_lights_data.color[last];

		_directional_lights_data.count--;

		break;
	case LightType::POINT:

		last = _point_lights_data.count - 1;

		e      = _point_lights_data.entity[index];
		last_e = _point_lights_data.entity[last];

		_point_lights_data.entity[index]          = _point_lights_data.entity[last];
		_point_lights_data.position_radius[index] = _point_lights_data.position_radius[last];
		_point_lights_data.color[index]           = _point_lights_data.color[last];

		_point_lights_data.count--;

		break;
	case LightType::SPOT:

		last = _spot_lights_data.count - 1;

		e      = _spot_lights_data.entity[index];
		last_e = _spot_lights_data.entity[last];

		_spot_lights_data.entity[index]          = _spot_lights_data.entity[last];
		_spot_lights_data.position_radius[index] = _spot_lights_data.position_radius[last];
		_spot_lights_data.color[index]           = _spot_lights_data.color[last];
		_spot_lights_data.params[index]          = _spot_lights_data.params[last];

		_spot_lights_data.count--;

		break;
	default:
		ASSERT("Invalid light type" && false);
	}

	_map.insert(last_e, i);
	_map.remove(e);
}

void LightManager::update()
{
	const Vector3 default_dir(0.0f, 0.0f, 1.0f);

	for(u32 i = 0; i < _directional_lights_data.count; i++)
	{
		TransformManager::Instance transform = _transform_manager.lookup(_directional_lights_data.entity[i]);

		ASSERT(transform.valid());

		_directional_lights_data.direction[i] = Vector3::Transform(default_dir, _transform_manager.getWorld(transform));
		_directional_lights_data.direction[i].Normalize();
	}

	for(u32 i = 0; i < _point_lights_data.count; i++)
	{
		TransformManager::Instance transform = _transform_manager.lookup(_point_lights_data.entity[i]);

		ASSERT(transform.valid());

		Vector3 position = Vector3::Transform(Vector3(0.0f, 0.0f, 0.0f), _transform_manager.getWorld(transform));

		_point_lights_data.position_radius[i].x = position.x;
		_point_lights_data.position_radius[i].y = position.y;
		_point_lights_data.position_radius[i].z = position.z;
	}

	for(u32 i = 0; i < _spot_lights_data.count; i++)
	{
		TransformManager::Instance transform = _transform_manager.lookup(_spot_lights_data.entity[i]);

		ASSERT(transform.valid());

		const Matrix4x4& world = _transform_manager.getWorld(transform);

		Vector3 position  = Vector3::Transform(Vector3(0.0f, 0.0f, 0.0f), world);
		Vector3 direction = world.Backward();
		direction.Normalize();

#if SPOT_PARAMS_HALF

	#if SPHEREMAP_ENCODE

			//encode direction
			float p = sqrt(direction.z*8+8);
			Vector2 enc(direction.x/p + 0.5f,direction.y/p + 0.5f);

			_spot_lights_data.params[i].light_dir_x = half_from_float(enc.x);
			_spot_lights_data.params[i].light_dir_y = half_from_float(enc.y);

			//decode direction and use decoded directon to prevent errors
			enc.x = half_to_float(_spot_lights_data.params[i].light_dir_x);
			enc.y = half_to_float(_spot_lights_data.params[i].light_dir_y);

			Vector2 fenc = enc * 4 - Vector2(2.0f, 2.0f);
			float f      = fenc.Dot(fenc);
			float g      = sqrt(1 - f / 4);

			Vector3 unpacked_dir(fenc.x*g, fenc.y*g, 1 - f / 2);

	#else

			_spot_lights_data.params[i].light_dir_x = half_from_float(direction.x);
			_spot_lights_data.params[i].light_dir_y = half_from_float(direction.y);

			Vector3 unpacked_dir;
			unpacked_dir.x = half_to_float(_spot_lights_data.params[i].light_dir_x);
			unpacked_dir.y = half_to_float(_spot_lights_data.params[i].light_dir_y);

			float temp = 1.0f - unpacked_dir.x*unpacked_dir.x - unpacked_dir.y*unpacked_dir.y;

			unpacked_dir.z = sqrt(temp > 0 ? temp : 0.0f);

			if(direction.z < 0.0f)
				unpacked_dir.z = -unpacked_dir.z;

	#endif

#else
		_spot_lights_data.params[i].light_dir_x = direction.x;
		_spot_lights_data.params[i].light_dir_y = direction.y;

		Vector3 unpacked_dir = direction;
#endif

		//-----------

		//Vector4 pos_radius(position.x, position.y, position.z,
		//				   half_to_float(_spot_lights_data.params[i].falloff_radius));

		Vector4 pos_radius(position.x, position.y, position.z, _spot_lights_data.radius[i]);

		_spot_lights_data.position_radius[i] = calculateSpotLightBoundingSphere(pos_radius, unpacked_dir,
																				_spot_lights_data.angle[i]);

#if !SPHEREMAP_ENCODE
		// put the sign bit for light dir z in the sign bit for the cone angle
		// (we can do this because we know the cone angle is always positive)
		if(unpacked_dir.z < 0.0f)
		{
#if SPOT_PARAMS_HALF
			_spot_lights_data.params[i].cosine_of_cone_angle_light_dir_zsign |= 0x8000;
#else
			if(_spot_lights_data.params[i].cosine_of_cone_angle_light_dir_zsign > 0)
				_spot_lights_data.params[i].cosine_of_cone_angle_light_dir_zsign = -_spot_lights_data.params[i].cosine_of_cone_angle_light_dir_zsign;
#endif
		}
		else
		{
#if SPOT_PARAMS_HALF
			_spot_lights_data.params[i].cosine_of_cone_angle_light_dir_zsign &= 0x7FFF;
#else
			if(_spot_lights_data.params[i].cosine_of_cone_angle_light_dir_zsign < 0)
				_spot_lights_data.params[i].cosine_of_cone_angle_light_dir_zsign = -_spot_lights_data.params[i].cosine_of_cone_angle_light_dir_zsign;
#endif
		}
#endif
	}
}

void LightManager::setColor(Instance i, u8 red, u8 green, u8 blue, u8 intensity)
{
	LightType type = i.getType();
	u32 index = i.getIndex();

	switch(type)
	{
	case LightType::DIRECTIONAL:

		_directional_lights_data.color[index] = COLOR(red, green, blue, intensity);

		break;
	case LightType::POINT:

		_point_lights_data.color[index] = COLOR(red, green, blue, intensity);

		break;
	case LightType::SPOT:

		_spot_lights_data.color[index] = COLOR(red, green, blue, intensity);

		break;
	default:
		ASSERT("Invalid light type" && false);
	}
}

bool LightManager::setRadius(Instance i, float radius)
{
	LightType type = i.getType();
	u32 index      = i.getIndex();

	switch(type)
	{
	case LightType::DIRECTIONAL:

		return false;

	case LightType::POINT:

		_point_lights_data.position_radius[index].w = radius;

		break;
	case LightType::SPOT:

#if SPOT_PARAMS_HALF
		_spot_lights_data.params[index].falloff_radius = half_from_float(radius);
#else
		_spot_lights_data.params[index].falloff_radius = radius;
#endif

		_spot_lights_data.radius[index] = radius;

		break;
	default:
		ASSERT("Invalid light type" && false);
	}

	return true;
}

bool LightManager::setAngle(Instance i, float angle)
{
	angle /= 2;

	LightType type = i.getType();
	u32 index      = i.getIndex();

	switch(type)
	{
	case LightType::DIRECTIONAL:
	case LightType::POINT:

		return false;

	case LightType::SPOT:

#if SPOT_PARAMS_HALF
		_spot_lights_data.params[index].cosine_of_cone_angle_light_dir_zsign = half_from_float(cos(angle));
#else
		_spot_lights_data.params[index].cosine_of_cone_angle_light_dir_zsign = cos(angle);
#endif
		_spot_lights_data.angle[index] = angle;

		break;
	default:
		ASSERT("Invalid light type" && false);
	}

	return true;
}

void LightManager::setCapacity(u32 new_capacity)
{
	DirectionalLightsData new_data;
	new_data.count = _directional_lights_data.count;
	new_data.capacity = new_capacity;

	new_data.buffer = allocateBlob(_allocator, new_capacity, new_data.entity, new_data.direction, new_data.color);

	//CHECK IF ALL ARRAYS ARE PROPERLY ALIGNED
	ASSERT(pointer_math::isAligned(new_data.entity));
	ASSERT(pointer_math::isAligned(new_data.direction));
	ASSERT(pointer_math::isAligned(new_data.color));

	if(_directional_lights_data.count > 0)
	{
		memcpy(new_data.entity, _directional_lights_data.entity, _directional_lights_data.count * sizeof(*new_data.entity));
		memcpy(new_data.direction, _directional_lights_data.direction, _directional_lights_data.count * sizeof(*new_data.direction));
		memcpy(new_data.color, _directional_lights_data.color, _directional_lights_data.count * sizeof(*new_data.color));
	}

	if(_directional_lights_data.capacity > 0)
		_allocator.deallocate(_directional_lights_data.buffer);

	_directional_lights_data = new_data;
}

void LightManager::setDirectionalLightsCapacity(u32 new_capacity)
{
	DirectionalLightsData new_data;
	new_data.count = _directional_lights_data.count;
	new_data.capacity = new_capacity;

	new_data.buffer = allocateBlob(_allocator, new_capacity, new_data.entity, new_data.direction, new_data.color);

	//CHECK IF ALL ARRAYS ARE PROPERLY ALIGNED
	ASSERT(pointer_math::isAligned(new_data.entity));
	ASSERT(pointer_math::isAligned(new_data.direction));
	ASSERT(pointer_math::isAligned(new_data.color));

	if(_directional_lights_data.count > 0)
	{
		memcpy(new_data.entity, _directional_lights_data.entity, _directional_lights_data.count * sizeof(*new_data.entity));
		memcpy(new_data.direction, _directional_lights_data.direction, _directional_lights_data.count * sizeof(*new_data.direction));
		memcpy(new_data.color, _directional_lights_data.color, _directional_lights_data.count * sizeof(*new_data.color));
	}

	if(_directional_lights_data.capacity > 0)
		_allocator.deallocate(_directional_lights_data.buffer);

	_directional_lights_data = new_data;
}

void LightManager::setPointLightsCapacity(u32 new_capacity)
{
	PointLightsData new_data;
	new_data.count = _point_lights_data.count;
	new_data.capacity = new_capacity;

	new_data.buffer = allocateBlob(_allocator, new_capacity, new_data.entity, new_data.position_radius, new_data.color);

	//CHECK IF ALL ARRAYS ARE PROPERLY ALIGNED
	ASSERT(pointer_math::isAligned(new_data.entity));
	ASSERT(pointer_math::isAligned(new_data.position_radius));
	ASSERT(pointer_math::isAligned(new_data.color));

	if(_point_lights_data.count > 0)
	{
		memcpy(new_data.entity, _point_lights_data.entity, _point_lights_data.count * sizeof(*new_data.entity));
		memcpy(new_data.position_radius, _point_lights_data.position_radius, _point_lights_data.count * sizeof(*new_data.position_radius));
		memcpy(new_data.color, _point_lights_data.color, _point_lights_data.count * sizeof(*new_data.color));
	}

	if(_point_lights_data.capacity > 0)
		_allocator.deallocate(_point_lights_data.buffer);

	_point_lights_data = new_data;
}

void LightManager::setSpotLightsCapacity(u32 new_capacity)
{
	SpotLightsData new_data;
	new_data.count = _spot_lights_data.count;
	new_data.capacity = new_capacity;

	new_data.buffer = allocateBlob(_allocator, new_capacity, new_data.entity,
								   new_data.position_radius, new_data.color, new_data.params, 
								   new_data.angle, new_data.radius);

	//CHECK IF ALL ARRAYS ARE PROPERLY ALIGNED
	ASSERT(pointer_math::isAligned(new_data.entity));
	ASSERT(pointer_math::isAligned(new_data.position_radius));
	ASSERT(pointer_math::isAligned(new_data.color));
	ASSERT(pointer_math::isAligned(new_data.params));
	ASSERT(pointer_math::isAligned(new_data.angle));
	ASSERT(pointer_math::isAligned(new_data.radius));

	if(_spot_lights_data.count > 0)
	{
		memcpy(new_data.entity, _spot_lights_data.entity, _spot_lights_data.count * sizeof(*new_data.entity));
		memcpy(new_data.position_radius, _spot_lights_data.position_radius, _spot_lights_data.count * sizeof(*new_data.position_radius));
		memcpy(new_data.color, _spot_lights_data.color, _spot_lights_data.count * sizeof(*new_data.color));
		memcpy(new_data.params, _spot_lights_data.params, _spot_lights_data.count * sizeof(*new_data.params));
		memcpy(new_data.angle, _spot_lights_data.angle, _spot_lights_data.count * sizeof(*new_data.angle));
		memcpy(new_data.radius, _spot_lights_data.radius, _spot_lights_data.count * sizeof(*new_data.radius));
	}

	if(_spot_lights_data.capacity > 0)
		_allocator.deallocate(_spot_lights_data.buffer);

	_spot_lights_data = new_data;
}