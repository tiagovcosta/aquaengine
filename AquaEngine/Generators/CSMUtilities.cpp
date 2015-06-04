#pragma once

#include "CSMUtilties.h"

#include "..\Renderer\Renderer.h"
#include "..\Renderer\RendererStructs.h"

#include "..\Renderer\Camera.h"

#include "..\Utilities\StringID.h"

using namespace aqua;

CSMUtilities::CascadedShadowMap CSMUtilities::createShadowMap(Allocator& allocator, Renderer& renderer, 
															  u32 cascade_size, u8 num_cascades)
{
	CascadedShadowMap csm;
	csm.cascade_size       = cascade_size;
	csm.num_cascades       = num_cascades;
	csm.cascades_cameras   = allocator::allocateArrayNoConstruct<Camera>(allocator, num_cascades);
	csm.cascades_viewports = allocator::allocateArrayNoConstruct<Viewport>(allocator, num_cascades);
	csm.cascades_view_proj = allocator::allocateArrayNoConstruct<Matrix4x4>(allocator, num_cascades);
	csm.cascades_end       = allocator::allocateArrayNoConstruct<float>(allocator, num_cascades);

	Texture2DDesc texture_desc;
	texture_desc.width          = cascade_size * num_cascades;
	texture_desc.height         = cascade_size;
	texture_desc.mip_levels     = 1;
	texture_desc.array_size     = 1;
	texture_desc.format         = RenderResourceFormat::R32_TYPELESS;
	texture_desc.sample_count   = 1;
	texture_desc.sample_quality = 0;
	texture_desc.update_mode    = UpdateMode::GPU;
	texture_desc.generate_mips  = false;

	TextureViewDesc dsv_desc;
	dsv_desc.format            = RenderResourceFormat::D32_FLOAT;
	dsv_desc.most_detailed_mip = 0;

	TextureViewDesc srv_desc;
	srv_desc.format            = RenderResourceFormat::R32_FLOAT;
	srv_desc.most_detailed_mip = 0;
	srv_desc.mip_levels        = -1;

	renderer.getRenderDevice()->createTexture2D(texture_desc, nullptr, 1, &srv_desc, 0, nullptr, 1, &dsv_desc,
												0, nullptr, &csm.srv, nullptr, &csm.dsv, nullptr);

	return csm;
}

void CSMUtilities::destroyShadowMap(Allocator& allocator, CascadedShadowMap& csm)
{
	RenderDevice::release(csm.srv);
	RenderDevice::release(csm.dsv);

	allocator::deallocateArrayNoDestruct(allocator, csm.cascades_cameras);
	allocator::deallocateArrayNoDestruct(allocator, csm.cascades_viewports);
	allocator::deallocateArrayNoDestruct(allocator, csm.cascades_view_proj);
	allocator::deallocateArrayNoDestruct(allocator, csm.cascades_end);
}

static float round2(float number)
{
	return number < 0.0f ? ceilf(number - 0.5f) : floorf(number + 0.5f);
}

static Camera generateCascadeCamera(const Vector3& light_dir, const Vector3* main_camera_corners,
									u32 shadow_map_size, float start, float end)
{
	Vector3 corners[8];

	// Scale by the shadow view distance
	for(u32 i = 0; i < 4; i++)
	{
		Vector3 corner_ray      = main_camera_corners[i + 4] - main_camera_corners[i];
		Vector3 near_corner_ray = corner_ray * start;
		Vector3 far_corner_ray  = corner_ray * end;

		corners[i]     = main_camera_corners[i] + near_corner_ray;
		corners[i + 4] = main_camera_corners[i] + far_corner_ray;
	}

	// Calculate the centroid of the view frustum
	Vector3 center;

	for(u32 i = 0; i < 8; i++)
		center += corners[i];

	center /= 8.0f;

	// Calculate the radius of a bounding sphere
	float radius = 0.0f;

	for(u32 i = 0; i < 8; i++)
	{
		float dist = (corners[i] - center).Length();

		radius = dist > radius ? dist : radius; //max
	}

	//float camera_offset = 10000.0f;
	float camera_offset = 100.0f;

	Vector3 shadow_cam_pos = center + light_dir * (camera_offset);

	Camera camera;
	camera.setPosition(shadow_cam_pos);
	//camera.setDirection(center - shadowCamPos);
	camera.setDirection(-light_dir);
	camera.setOrthograpic(-radius, radius, radius, -radius, 5.0f, camera_offset + radius);
	camera.update();

	//Move the light in texel-sized increments
	Matrix4x4 shadow_matrix = camera.getViewProj();

	Vector4 shadow_origin(0.0f, 0.0f, 0.0f, 1.0f);
	shadow_origin = Vector4::Transform(shadow_origin, shadow_matrix);
	shadow_origin *= (shadow_map_size / 2.0f);

	Vector2 rounded_origin = Vector2(round2(shadow_origin.x), round2(shadow_origin.y));

	Vector2 round_offset = rounded_origin - Vector2(shadow_origin.x, shadow_origin.y);
	round_offset /= (shadow_map_size / 2.0f);

	shadow_matrix = camera.getProj();

	shadow_matrix._41 += round_offset.x;
	shadow_matrix._42 += round_offset.y;

	camera.setProjection(shadow_matrix);

	camera.update();

	/*
	Matrix4x4 roundMatrix;
	D3DXMatrixTranslation(&roundMatrix, rounding.x, rounding.y, 0.0f);

	camera._ViewProj *= roundMatrix;
	*/

	return camera;
}

void CSMUtilities::updateCascadedShadowMap(CascadedShadowMap& csm, const Camera& camera, const Vector3& light_dir)
{
	for(u32 i = 0; i < csm.num_cascades; i++)
	{
		float start = 0.0f;

		if(i != 0)
			start = csm.cascades_end[i - 1];

		csm.cascades_cameras[i] = generateCascadeCamera(light_dir, camera.getFrustumCorners(),
														csm.cascade_size, start, csm.cascades_end[i]);
	}

	const float cascade_size = 1.0f / csm.num_cascades;

	for(u32 i = 0; i < csm.num_cascades; i++)
	{
		csm.cascades_viewports[i].x      = i*cascade_size;
		csm.cascades_viewports[i].y      = 0;
		csm.cascades_viewports[i].width  = cascade_size;
		csm.cascades_viewports[i].height = 1.0f;
	}

	//--------------------------------------------------------

	const auto clip_distances = camera.getClipDistances();
	const float clip_distance = clip_distances.y - clip_distances.x;

	for(u32 i = 0; i < csm.num_cascades; i++)
	{
		csm.cascades_end[i] = clip_distances.x + csm.cascades_end[i] * clip_distance;

		Matrix4x4 shadow_matrix = csm.cascades_cameras[i].getViewProj();

		// Apply the scale/offset/bias matrix, which transforms from [-1,1]
		// post-projection space to [0,1] UV spac
		//const float bias = 0.003f;

		Matrix4x4 tex_scale_bias = Matrix4x4(Vector4(0.5f, 0.0f, 0.0f, 0.0f),
											 Vector4(0.0f, -0.5f, 0.0f, 0.0f),
											 Vector4(0.0f, 0.0f, 1.0f, 0.0f),
											 Vector4(0.5f, 0.5f, 0.0f/*-bias*/, 1.0f));

		shadow_matrix = shadow_matrix * tex_scale_bias;

		// Apply the cascade offset/scale matrix, which applies the offset and scale needed to
		// convert the UV coordinate into the proper coordinate for the cascade being sampled in
		// the atlas.

		Matrix4x4 cascade_offset_matrix = Matrix4x4::CreateScale(cascade_size, 1.0f, 1.0f);
		cascade_offset_matrix._41       = i*cascade_size;
		cascade_offset_matrix._42       = 0.0f;

		csm.cascades_view_proj[i] = shadow_matrix * cascade_offset_matrix;
	}
};