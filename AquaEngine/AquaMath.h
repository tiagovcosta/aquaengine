#pragma once

#include "AquaTypes.h"

#include <SimpleMath.h>

namespace aqua
{
	using Matrix4x4 = DirectX::SimpleMath::Matrix;
	using Vector2 = DirectX::SimpleMath::Vector2;
	using Vector3 = DirectX::SimpleMath::Vector3;
	using Vector4 = DirectX::SimpleMath::Vector4;
	using Quaternion = DirectX::SimpleMath::Quaternion;
	using Plane = DirectX::SimpleMath::Plane;

	const Vector3 VECTOR3_UP      = Vector3(0.0f, 1.0f, 0.0f);
	const Vector3 VECTOR3_DOWN    = Vector3(0.0f, -1.0f, 0.0f);
	const Vector3 VECTOR3_LEFT    = Vector3(-1.0f, 0.0f, 0.0f);
	const Vector3 VECTOR3_RIGHT   = Vector3(1.0f, 0.0f, 0.0f);
	const Vector3 VECTOR3_BACK    = Vector3(0.0f, 0.0f, -1.0f);
	const Vector3 VECTOR3_FORWARD = Vector3(0.0f, 0.0f, 1.0f);
	const Vector3 VECTOR3_ZERO    = Vector3(0.0f, 0.0f, 0.0f);
	const Vector3 VECTOR3_ONE     = Vector3(1.0f, 1.0f, 1.0f);

	struct ColorRGBA8
	{
		u8 r;
		u8 g;
		u8 b;
		u8 a;
	};

	static inline float minf(float a, float b)
	{
		return a < b ? a : b;
	}

	static inline float maxf(float a, float b)
	{
		return a > b ? a : b;
	}

	inline Quaternion quaternionLookRotation(Vector3 forward, Vector3 up = Vector3(0.0f, 1.0f, 0.0f))
	{
		//forward.Normalize();
		Vector3 right = up.Cross(forward);
		up = forward.Cross(right);

		return Quaternion::CreateFromRotationMatrix(Matrix4x4(right, up, forward));
	}

	struct SphericalCoords
	{
		float phi;
		float theta;
	};

	inline SphericalCoords directionToSpherical(const Vector3& dir)
	{
		SphericalCoords out;
		out.phi = std::acos(dir.y);
		out.theta = std::atan2(dir.z, dir.x);

		return out;
	}

	inline Vector3 sphericalToDirection(const SphericalCoords& coords)
	{
		Vector3 out;
		out.x = sin(coords.phi)*cos(coords.theta);
		out.y = cos(coords.phi);
		out.z = sin(coords.phi)*sin(coords.theta);

		return out;
	}
};