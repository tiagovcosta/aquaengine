#include "DynamicSky.h"

#include "Renderer\Camera.h"

#include "Renderer\RendererUtilities.h"
#include "Renderer\Renderer.h"
#include "Renderer\RendererStructs.h"

#include "Core\Allocators\ScopeStack.h"
#include "Core\Allocators\LinearAllocator.h"
#include "Core\Allocators\Allocator.h"

#include "Utilities\StringID.h"

#include <cmath>

using namespace aqua;

#define SCATTERING_TEXTURES_WIDTH 128
#define SCATTERING_TEXTURES_HEIGHT 64

#define ALWAYS_UPDATE_SCATTERING 0

DynamicSky::DynamicSky(Allocator& allocator, LinearAllocator& temp_allocator, Renderer& renderer)
	: _allocator(allocator), _temp_allocator(&temp_allocator),
	_renderer(&renderer)
{
	//Get shaders

	_shader = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/dynamic_sky.cshader"));

	_shader_permutation = _shader->getPermutation(0);

	auto instance_params_desc_set = _shader->getInstanceParameterGroupDescSet();
	_instance_params_desc = getParameterGroupDesc(*instance_params_desc_set, 0);

	//----------------------------
	//rayleigh
	_update_rayleigh_shader = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/update_rayleigh.cshader"));

	_update_rayleigh_shader_permutation = _update_rayleigh_shader->getPermutation(0);

	instance_params_desc_set = _update_rayleigh_shader->getInstanceParameterGroupDescSet();
	_update_instance_params_desc = getParameterGroupDesc(*instance_params_desc_set, 0);

	//mie
	_update_mie_shader = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/update_mie.cshader"));

	_update_mie_shader_permutation = _update_mie_shader->getPermutation(0);

	//----------------------------
	/*
	ConstantBufferDesc desc;
	desc.size = _instance_params_desc->getCBuffersSizes()[0];
	desc.update_mode = UpdateMode::CPU;

	if(!_renderer->getRenderDevice()->createConstantBuffer(desc, nullptr, _dome_cbuffer))
	{
		//NEED ERROR MESSAGE
		//return false;
	}
	*/
	_dome_params = _renderer->getRenderDevice()->createParameterGroup(_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																	  *_instance_params_desc, UINT32_MAX, 0, nullptr);

	//_dome_params->setCBuffer(_dome_cbuffer, 0);

	//----------------------------
	/*
	desc.size = _update_instance_params_desc->getCBuffersSizes()[0];
	desc.update_mode = UpdateMode::CPU;

	if(!_renderer->getRenderDevice()->createConstantBuffer(desc, nullptr, _update_cbuffer))
	{
		//NEED ERROR MESSAGE
		//return false;
	}
	*/
	//--------------------

	Texture2DDesc texture_desc;
	texture_desc.width          = SCATTERING_TEXTURES_WIDTH;
	texture_desc.height         = SCATTERING_TEXTURES_HEIGHT;
	texture_desc.mip_levels     = 1;
	texture_desc.array_size     = 1;
	texture_desc.format         = RenderResourceFormat::RGBA16_FLOAT;
	texture_desc.sample_count   = 1;
	texture_desc.sample_quality = 0;
	texture_desc.update_mode    = UpdateMode::GPU;
	texture_desc.generate_mips  = false;

	//_depth_target = nullptr;

	TextureViewDesc view_desc;
	view_desc.format            = RenderResourceFormat::RGBA16_FLOAT;
	view_desc.most_detailed_mip = 0;
	view_desc.mip_levels        = 1;
	/*
	_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 0, nullptr, 0, nullptr, 1, &view_desc,
	0, nullptr, nullptr, nullptr, _depth_target.dsvs, nullptr);
	*/
	_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc, 1, &view_desc, 0, nullptr,
		0, nullptr, &_rayleigh2_sr, &_rayleigh2, nullptr, nullptr);

	_renderer->getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &view_desc, 1, &view_desc, 0, nullptr,
		0, nullptr, &_mie2_sr, &_mie2, nullptr, nullptr);

	//--------------------

	_wavelengths = Vector3(0.65f, 0.57f, 0.475f);
	//_wavelengths = Vector3(pow(0.65f, 2.2f), pow(0.57f, 2.2f), pow(0.475f, 2.2f));

	float theta = DirectX::XM_PI / 2;
	float phi = DirectX::XM_PI;

	_sun_dir.x = sin(theta)*cos(phi);
	_sun_dir.y = cos(theta);
	_sun_dir.z = sin(theta)*sin(phi);

	//--------------------

	_update_params = _renderer->getRenderDevice()->createParameterGroup(_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																		*_update_instance_params_desc, UINT32_MAX, 0, nullptr);

	//_update_params->cbuffers[0] = _update_cbuffer;

	void* update_cbuffers_data = _update_params->getCBuffersData();

	u32 offset = _update_instance_params_desc->getConstantOffset(getStringID("inv_wavelength"));

	Vector3* inv_wavelength = (Vector3*)pointer_math::add(update_cbuffers_data, offset);

	inv_wavelength->x = 1.0f / pow(_wavelengths.x, 4.0f);
	inv_wavelength->y = 1.0f / pow(_wavelengths.y, 4.0f);
	inv_wavelength->z = 1.0f / pow(_wavelengths.z, 4.0f);

	offset = _update_instance_params_desc->getConstantOffset(getStringID("wavelength_mie"));

	Vector3* wavelength_mie = (Vector3*)pointer_math::add(update_cbuffers_data, offset);

	wavelength_mie->x = 1.0f / pow(_wavelengths.x, 0.84f);
	wavelength_mie->y = 1.0f / pow(_wavelengths.y, 0.84f);
	wavelength_mie->z = 1.0f / pow(_wavelengths.z, 0.84f);

	//memcpy(pointer_math::add(_update_params->map_commands->data, offset), &world_view_proj, sizeof(world_view_proj));

	offset = _update_instance_params_desc->getConstantOffset(getStringID("sun_dir"));

	Vector3* sun_dir = (Vector3*)pointer_math::add(update_cbuffers_data, offset);

	*sun_dir = _sun_dir;

	//--------------------
	// Create dome mesh
	//--------------------

	int DomeN     = 32;
	int Latitude  = DomeN / 2;
	int Longitude = DomeN;
	int DVSize    = Longitude * Latitude;
	int DISize    = (Longitude - 1) * (Latitude - 1) * 2 * 3;
	DVSize *= 2;
	DISize *= 2;

	ScopeStack scope(*_temp_allocator);

	struct Vertex
	{
		Vector3 pos;
		Vector2 texc;
	};

	Vertex* vertices = scope.newPODArray<Vertex>(DVSize);

	int DomeIndex = 0;
	for(int i = 0; i < Longitude; i++)
	{
		//double MoveXZ = 100.0f * (i / ((float)Longitude - 1.0f)) * DirectX::XM_PI / 180.0;
		double MoveXZ = 100.0f * (i / ((float)Longitude - 1.0f)) * DirectX::XM_PI / 180.0;

		for(int j = 0; j < Latitude; j++)
		{
			double MoveY = DirectX::XM_PI * j / (Latitude - 1);

			vertices[DomeIndex].pos.x = (float)(sin(MoveXZ) * cos(MoveY));
			vertices[DomeIndex].pos.y = (float)cos(MoveXZ);
			vertices[DomeIndex].pos.z = (float)(sin(MoveXZ) * sin(MoveY));

			//vertices[DomeIndex].pos *= _radius;

			vertices[DomeIndex].texc.x = i / (float)Longitude;
			vertices[DomeIndex].texc.y = j / (float)Latitude;

			DomeIndex++;
		}
	}

	for(int i = 0; i < Longitude; i++)
	{
		double MoveXZ = 100.0f * (i / ((float)Longitude - 1.0f)) * DirectX::XM_PI / 180.0;

		for(int j = 0; j < Latitude; j++)
		{
			double MoveY = (DirectX::XM_2PI) - (DirectX::XM_PI * j / (Latitude - 1));

			vertices[DomeIndex].pos.x = (float)(sin(MoveXZ) * cos(MoveY));
			vertices[DomeIndex].pos.y = (float)cos(MoveXZ);
			vertices[DomeIndex].pos.z = (float)(sin(MoveXZ) * sin(MoveY));

			//domeVerts[DomeIndex].pos *= _radius;

			vertices[DomeIndex].texc.x = i / (float)Longitude;
			vertices[DomeIndex].texc.y = j / (float)Latitude;

			DomeIndex++;
		}
	}

	u16* indices = scope.newPODArray<u16>(DISize);

	int index = 0;
	for(u16 i = 0; i < Longitude - 1; i++)
	{
		for(u16 j = 0; j < Latitude - 1; j++)
		{
			indices[index++] = (u16)(i * Latitude + j);
			indices[index++] = (u16)((i + 1) * Latitude + j);
			indices[index++] = (u16)((i + 1) * Latitude + j + 1);

			indices[index++] = (u16)((i + 1) * Latitude + j + 1);
			indices[index++] = (u16)(i * Latitude + j + 1);
			indices[index++] = (u16)(i * Latitude + j);
		}
	}

	WORD Offset = (WORD)(Latitude * Longitude);
	for(WORD i = 0; i < Longitude - 1; i++)
	{
		for(WORD j = 0; j < Latitude - 1; j++)
		{
			indices[index++] = (WORD)(Offset + i * Latitude + j);
			indices[index++] = (WORD)(Offset + (i + 1) * Latitude + j + 1);
			indices[index++] = (WORD)(Offset + (i + 1) * Latitude + j);

			indices[index++] = (WORD)(Offset + i * Latitude + j + 1);
			indices[index++] = (WORD)(Offset + (i + 1) * Latitude + j + 1);
			indices[index++] = (WORD)(Offset + i * Latitude + j);
		}
	}

	//num_indices = index;

	_dome_mesh = createMesh(1, _allocator);
	_dome_mesh->topology = PrimitiveTopology::TRIANGLE_LIST;
	_dome_mesh->index_buffer_offset = 0;
	_dome_mesh->index_format = IndexBufferFormat::UINT16;

	BufferDesc dome_buffer_desc;
	dome_buffer_desc.num_elements = DVSize;
	dome_buffer_desc.stride = sizeof(Vertex);
	dome_buffer_desc.bind_flags = (u8)BufferBindFlag::VERTEX;
	dome_buffer_desc.update_mode = UpdateMode::NONE;

	VertexBuffer& vb = _dome_mesh->getVertexBuffer(0);
	vb.offset = 0;
	vb.stride = dome_buffer_desc.stride;

	_renderer->getRenderDevice()->createBuffer(dome_buffer_desc, vertices, vb.buffer, nullptr, nullptr);

	dome_buffer_desc.num_elements = DISize;
	dome_buffer_desc.stride = sizeof(u16);
	dome_buffer_desc.bind_flags = (u8)BufferBindFlag::INDEX;

	_renderer->getRenderDevice()->createBuffer(dome_buffer_desc, indices, _dome_mesh->index_buffer, nullptr, nullptr);

	_dome_draw_call = createDrawCall(true, DISize, 0, 0);

	_update_rayleigh = true;
	_update_mie = false;
}

DynamicSky::~DynamicSky()
{
	_allocator.deallocate(_dome_params);
	_allocator.deallocate(_update_params);

	//Release buffers
	RenderDevice* render_device = _renderer->getRenderDevice();

	render_device->release(_rayleigh2);
	render_device->release(_mie2);

	render_device->release(_rayleigh2_sr);
	render_device->release(_mie2_sr);

	//render_device->release(_dome_cbuffer);
	//render_device->release(_update_cbuffer);

	render_device->release(_dome_mesh->getVertexBuffer(0).buffer);
	render_device->release(_dome_mesh->index_buffer);

	_allocator.deallocate(_dome_mesh);
}

bool DynamicSky::cull(u32 num_frustums, const Frustum* frustums, VisibilityData** out)
{
	//u32 num_visibles = 0;

	for(u32 i = 0; i < num_frustums; i++)
	{
		out[i]->visibles_indices = nullptr;
		out[i]->num_visibles     = 1;

		/*
		out[i]->visibles_indices = allocator::allocateArray<u32>(*_temp_allocator, 1);

		for(u32 j = 0; j < _data.size; j++)
		{
			out[i]->visibles_indices[num_visibles] = j;

			num_visibles++;
		}

		out[i]->num_visibles = num_visibles;
		*/
	}

	return true;
}

bool DynamicSky::extract(const VisibilityData& visibility_data)
{
	return true;
}

bool DynamicSky::prepare()
{
	return true;
}

bool DynamicSky::getRenderItems(u8 num_passes, const u32* passes_names,
								const VisibilityData& visibility_data, RenderQueue* out_queues)
{
	u8 num_shader_passes           = _shader->getNumPasses();
	const u32* shader_passes_names = _shader->getPassesNames();

	struct Match
	{
		u8 pass;
		u8 shader_pass;
	};

	u8 num_matches = 0;

	Match* matches = allocator::allocateArrayNoConstruct<Match>(*_temp_allocator, num_passes);

	for(u8 i = 0; i < num_shader_passes; i++)
	{
		for(u8 j = 0; j < num_passes; j++)
		{
			if(shader_passes_names[i] == passes_names[j])
			{
				matches[num_matches].pass        = j;
				matches[num_matches].shader_pass = i;
				num_matches++;

				break;
			}
		}
	}

	if(num_matches == 0)
		return true; //no passes match

	RenderItem** render_queues = allocator::allocateArrayNoConstruct<RenderItem*>(*_temp_allocator, num_matches);

	for(u8 i = 0; i < num_matches; i++)
	{
		u8 queue_index = matches[i].pass;

		render_queues[i]                   = allocator::allocateArrayNoConstruct<RenderItem>(*_temp_allocator, visibility_data.num_visibles);
		out_queues[queue_index].sort_items = allocator::allocateArrayNoConstruct<SortItem>(*_temp_allocator, visibility_data.num_visibles);
	}
	
	out_queues[matches->pass].size = 1;
	/*
	out_queues[matches->pass].render_items = allocator::allocateArrayNoConstruct<RenderItem>(*_temp_allocator, 1);
	out_queues[matches->pass].sort_items   = allocator::allocateArrayNoConstruct<SortItem>(*_temp_allocator, 1);
	*/

	//Update params
	u8 index = _instance_params_desc->getSRVIndex(getStringID("rayleigh_scatter"));

	_dome_params->setSRV(_rayleigh2_sr, index);

	index = _instance_params_desc->getSRVIndex(getStringID("mie_scatter"));

	_dome_params->setSRV(_mie2_sr, index);

	void* dome_params_cbuffer_data = _dome_params->getCBuffersData();

	u32 offset = _instance_params_desc->getConstantOffset(getStringID("sun_dir"));

	Vector3* sun_dir = (Vector3*)pointer_math::add(dome_params_cbuffer_data, offset);

	*sun_dir = _sun_dir;

	offset = _instance_params_desc->getConstantOffset(getStringID("world"));

	Matrix4x4 world = _rotation;

	memcpy(pointer_math::add(dome_params_cbuffer_data, offset), &world, sizeof(world));

	RenderItem& render_item     = render_queues[0][0];
	render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_dome_params); //cache params
	render_item.num_instances   = 1;
	render_item.draw_call       = &_dome_draw_call;
	render_item.shader          = _shader_permutation[matches->shader_pass];
	render_item.mesh            = _dome_mesh;
	render_item.material_params = nullptr;

	SortItem& sort_item = out_queues[matches->pass].sort_items[0];
	sort_item.item      = &render_item;
	sort_item.sort_key  = 0;

	return true;
}

void DynamicSky::update()
{
#if ALWAYS_UPDATE_SCATTERING
	_update_rayleigh = true;
#endif
	Viewport vp;
	vp.width         = 1.0;
	vp.height        = 1.0f;
	vp.x             = 0;
	vp.y             = 0;

	RenderTexture rt;
	rt.width  = SCATTERING_TEXTURES_WIDTH;
	rt.height = SCATTERING_TEXTURES_HEIGHT;

	DrawCall dc = createDrawCall(false, 3, 0, 0);

	Mesh mesh;
	mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

	RenderItem render_item;
	render_item.draw_call       = &dc;
	render_item.num_instances   = 1;
	render_item.instance_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_update_params); //cache params
	render_item.material_params = nullptr;
	render_item.mesh            = &mesh;

	if(_update_mie)
	{
		rt.render_target = _mie2;

		_renderer->setViewport(vp, rt.width, rt.height);
		_renderer->setRenderTarget(1, &rt, nullptr);

		render_item.shader = _update_mie_shader_permutation[0];

		_renderer->render(render_item);

		_update_mie = false;
	}
#if ALWAYS_UPDATE_SCATTERING
	if(_update_rayleigh)
#else
	else if(_update_rayleigh)
#endif
	{
		rt.render_target = _rayleigh2;

		_renderer->setViewport(vp, rt.width, rt.height);
		_renderer->setRenderTarget(1, &rt, nullptr);

		render_item.shader = _update_rayleigh_shader_permutation[0];

		_renderer->render(render_item);

		_update_rayleigh = false;
		_update_mie      = true;
	}
}

void DynamicSky::setSunDirection(const Vector3& direction)
{
	/*
	float phi = std::acos(direction.y);
	float theta = std::atan2(direction.z, direction.x);

	_sun_dir.x = sin(phi)*cos(theta);
	_sun_dir.y = cos(phi);
	_sun_dir.z = sin(phi)*sin(theta);

	_rotation = Matrix4x4::CreateRotationY(-theta);

	Matrix4x4 rot = Matrix4x4::CreateRotationY(theta);

	_sun_dir = Vector3::Transform(_sun_dir, rot);
	*/

	float theta = std::atan2(direction.z, direction.x);

	_rotation = Matrix4x4::CreateRotationY(-theta);

	Matrix4x4 rot = Matrix4x4::CreateRotationY(theta);

	_sun_dir = Vector3::Transform(direction, rot);

	u32 offset = _update_instance_params_desc->getConstantOffset(getStringID("sun_dir"));

	Vector3* sun_dir = (Vector3*)pointer_math::add(_update_params->getCBuffersData(), offset);

	*sun_dir = _sun_dir;

	_update_rayleigh = true;
}

void DynamicSky::setScattering(ShaderResourceH scattering)
{
	u8 index = _instance_params_desc->getSRVIndex(getStringID("scattering"));

	_dome_params->setSRV(scattering, index);
}

static const float INNER_RADIUS          = 6356.7523142f;
static const float OUTER_RADIUS          = INNER_RADIUS * 1.0157313f;
static const float PI                    = 3.1415159f;
static const float NUM_SAMPLES           = 10;
static const float HEIGH_SCALE           = 1.0 / (OUTER_RADIUS - INNER_RADIUS);
static const float RAYLEIGH_SCALE_HEIGHT = 0.25f;
static const float MIE_SCALE_HEIGHT      = 0.1f;

static const float ESun   = 20.0f;
static const float Kr     = 0.0025f;
static const float Km     = 0.0010f;
static const float KrESun = Kr * ESun;
static const float KmESun = Km * ESun;
static const float Kr4PI  = Kr * 4.0f * PI;
static const float Km4PI  = Km * 4.0f * PI;

static const float g = -0.990;
static const float g2 = g * g;

static float HitOuterSphere(Vector3 o, Vector3 dir)
{
	Vector3 L = -o;

	float B = L.Dot(dir);
	float C = L.Dot(L);
	float D = C - B * B;
	float q = sqrt(OUTER_RADIUS * OUTER_RADIUS - D);
	float t = B;
	t += q;

	return t;
}

static Vector2 GetDensityRatio(float height)
{
	const float altitude = (height - INNER_RADIUS) * HEIGH_SCALE;
	return Vector2(exp(-altitude / RAYLEIGH_SCALE_HEIGHT), exp(-altitude / MIE_SCALE_HEIGHT));
}

Vector2 t(Vector3 p, Vector3 px)
{
	Vector2 optical_depth;

	Vector3 vec  = px - p;
	float length = vec.Length();
	Vector3 dir  = vec / length;

	float sample_length = length / NUM_SAMPLES;
	float scaled_length = sample_length * HEIGH_SCALE;
	Vector3 sample_ray  = dir * sample_length;
	p += sample_ray * 0.5f;

	for(int i = 0; i < NUM_SAMPLES; i++)
	{
		float height = p.Length();
		optical_depth += GetDensityRatio(height);
		p += sample_ray;
	}

	optical_depth *= scaled_length;

	return optical_depth;
}

float getRayleighPhase(float fCos2)
{
	return 0.75 * (1.0 + fCos2);
}

float getMiePhase(float fCos, float fCos2)
{
	Vector3 v3HG;
	v3HG.x = 1.5f * ((1.0f - g2) / (2.0f + g2));
	v3HG.y = 1.0f + g2;
	v3HG.z = 2.0f * g;
	return v3HG.x * (1.0 + fCos2) / pow(abs(v3HG.y - v3HG.z * fCos), 1.5);
}

Vector3 DynamicSky::getSunColor() const
{
	Vector3 inv_wavelength;
	inv_wavelength.x = 1.0f / pow(_wavelengths.x, 4.0f);
	inv_wavelength.y = 1.0f / pow(_wavelengths.y, 4.0f);
	inv_wavelength.z = 1.0f / pow(_wavelengths.z, 4.0f);

	const Vector3 v3PointPv = Vector3(0.0f, INNER_RADIUS + 0.001f, 0.0f);

	float fFarPvPa = HitOuterSphere(v3PointPv, _sun_dir);
	Vector3 ray  = _sun_dir;

	Vector3 point_p    = v3PointPv;
	float sample_length = fFarPvPa / NUM_SAMPLES;
	float scaled_length = sample_length * HEIGH_SCALE;

	Vector3 sample_ray = ray * sample_length;
	point_p += sample_ray * 0.5f;

	Vector3 rayleigh_sum;
	Vector3 mie_sum;

	for(int k = 0; k < NUM_SAMPLES; k++)
	{
		float point_p_height = point_p.Length();

		Vector2 DensityRatio = GetDensityRatio(point_p_height);
		DensityRatio *= scaled_length;

		Vector2 ViewerOpticalDepth = t(point_p, v3PointPv);

		float dFarPPc = HitOuterSphere(point_p, _sun_dir);
		Vector2 SunOpticalDepth = t(point_p, point_p + _sun_dir * dFarPPc);

		Vector2 OpticalDepthP = SunOpticalDepth + ViewerOpticalDepth;
		//Vector3 v3Attenuation = exp(-Kr4PI * inv_wavelength * OpticalDepthP.x - Km4PI * wavelength_mie * OpticalDepthP.y);
		//Vector3 v3Attenuation = exp(-Kr4PI * inv_wavelength * OpticalDepthP.x - Km4PI * OpticalDepthP.y);
		Vector3 attenuation;
		attenuation.x = exp(-Kr4PI * inv_wavelength.x * OpticalDepthP.x - Km4PI * OpticalDepthP.y);
		attenuation.y = exp(-Kr4PI * inv_wavelength.y * OpticalDepthP.x - Km4PI * OpticalDepthP.y);
		attenuation.z = exp(-Kr4PI * inv_wavelength.z * OpticalDepthP.x - Km4PI * OpticalDepthP.y);

		rayleigh_sum += DensityRatio.x * attenuation;
		mie_sum		 += DensityRatio.y * attenuation;

		point_p += sample_ray;
	}

	Vector3 rayleigh = rayleigh_sum * KrESun;
	rayleigh *= inv_wavelength;

	Vector3 mie = mie_sum * KmESun;

	float x = getRayleighPhase(1.0f);
	float y = getMiePhase(-1.0f, 1.0f);

	Vector3 color = getRayleighPhase(1.0f) * rayleigh + getMiePhase(-1.0f, 1.0f) * mie;

	return color;
}