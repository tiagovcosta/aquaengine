#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\AquaMath.h"

namespace aqua
{
	class Camera
	{
	public:
		Camera();
		~Camera();

		void setPosition(const Vector3& position);
		void setDirection(const Vector3& direction, const Vector3& up = VECTOR3_UP);

		//void setView(const Vector3& position, const Vector3& direction, const Vector3& up = VECTOR3_UP);

		void setPerspective(float fov, float aspect, float near_z, float far_z);
		void setOrthograpic(float left, float right, float top, float bottom, float near_z, float far_z);

		void setProjection(const Matrix4x4& proj);

		void setNearPlane(float distance);
		void setFarPlane(float distance);
		void setFieldOfView(float fov);
		void setAspect(float aspect);

		void update();

		const Vector3&   getPosition() const;
		const Vector3&   getLookDirection() const;
		const Vector3&   getUp() const;
		const Vector3&   getRight() const;

		const Matrix4x4& getView() const;
		const Matrix4x4& getProj() const;
		const Matrix4x4& getViewProj() const;

		const Vector3*   getFrustumCorners() const;

		//Returns Vector2(near_z, far_z)
		Vector2          getClipDistances() const;

		float			 getFOV() const;

		Vector2          getFarPlaneSize() const;

		const Plane*     getPlanes() const;

	public:
		void      updateCornersAndPlanes();

		bool      _update;

		Vector3   _position;
		Vector3   _right;
		Vector3   _up;
		Vector3   _direction;

		Matrix4x4 _view;
		Matrix4x4 _proj;
		Matrix4x4 _view_proj;

		float     _fov;

		float     _near_width;
		float     _near_height;
		float     _far_width;
		float     _far_height;
		float     _near_z;
		float     _far_z;

		Vector3   _corners[8];
		Plane     _planes[6];

		bool _custom_projection;
	};
};