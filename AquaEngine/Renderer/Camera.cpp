#include "Camera.h"

using namespace aqua;

Camera::Camera() : _position(0.0f, 0.0f, 0.0f), /*_right(1.0f, 0.0f, 0.0f),*/ _up(0.0f, 1.0f, 0.0f),
					_direction(0.0f, 0.0f, 1.0f), _view(), _proj(), _view_proj(), _fov(0.0f),
					_near_width(0.0f), _near_height(0.0f), _far_width(0.0f), _far_height(0.0f),
					_near_z(0.0f), _far_z(0.0f)
{}

Camera::~Camera()
{}

void Camera::setPosition(const Vector3& position)
{
	_position = position;

	_update = true;
}

void Camera::setDirection(const Vector3& direction, const Vector3& up)
{
	_direction = direction;
	_direction.Normalize();

	//_up = up;

	_right = up.Cross(_direction);
	_right.Normalize();

	_up = _direction.Cross(_right);
	_up.Normalize();

	_update = true;
}
/*
void Camera::setView(const Vector3& position, const Vector3& direction, const Vector3& up)
{
	_position  = position;
	_direction = direction;
	_up        = up;

	//DirectX::XMStoreFloat4x4(&_view, DirectX::XMMatrixLookToLH(_position, _direction, _up));
}
*/
void Camera::setPerspective(float fov, float aspect, float near_z, float far_z)
{
	_fov = fov;

	_near_height = 2 * tan(fov / 2) * near_z;
	_near_width  = _near_height * aspect;

	_far_height = 2 * tan(fov / 2) * far_z;
	_far_width  = _far_height * aspect;

	_near_z = near_z;
	_far_z  = far_z;

	//_proj = Matrix4x4::CreatePerspectiveFieldOfView(fov, aspect, near_z, far_z);

	DirectX::XMStoreFloat4x4(&_proj, DirectX::XMMatrixPerspectiveFovLH(fov, aspect, near_z, far_z));

	_update = true;
}

void Camera::setOrthograpic(float left, float right, float top, float bottom, float near_z, float far_z)
{
	_near_height = std::abs(top - bottom);
	_near_width  = std::abs(right - left);

	_far_height = _near_height;
	_far_width  = _near_width;

	_near_z = near_z;
	_far_z  = far_z;

	//_proj = Matrix4x4::CreateOrthographicOffCenter(left, right, bottom, top, near_z, far_z);

	DirectX::XMStoreFloat4x4(&_proj, DirectX::XMMatrixOrthographicOffCenterLH(left, right, bottom, top, near_z, far_z));

	_update = true;
}

void Camera::setProjection(const Matrix4x4& proj)
{
	_proj = proj;

	_update = true;
}

void Camera::update()
{
	if(_update)
	{
		/*
		// Keep camera's axes orthogonal to each other and of unit length.
		_direction.Normalize();

		_up = _direction.Cross(_right);
		_up.Normalize();

		_right = _up.Cross(_direction);
		_right.Normalize();

		// Fill in the view matrix entries.

		_view._11 = _right.x;
		_view._21 = _right.y;
		_view._31 = _right.z;
		_view._41 = -_position.Dot(_right);

		_view._12 = _up.x;
		_view._22 = _up.y;
		_view._32 = _up.z;
		_view._42 = -_position.Dot(_up);

		_view._13 = _direction.x;
		_view._23 = _direction.y;
		_view._33 = _direction.z;
		_view._43 = -_position.Dot(_direction);

		_view._14 = 0.0f;
		_view._24 = 0.0f;
		_view._34 = 0.0f;
		_view._44 = 1.0f;
		*/
		DirectX::XMStoreFloat4x4(&_view, DirectX::XMMatrixLookToLH(_position, _direction, _up));

		_view_proj = _view * _proj;

		updateCornersAndPlanes();

		_update = false;
	}
}

const Vector3& Camera::getPosition() const
{
	return _position;
}

const Vector3& Camera::getLookDirection() const
{
	return _direction;
}

const Vector3& Camera::getUp() const
{
	return _up;
}

const Vector3& Camera::getRight() const
{
	return _right;
	//return _up.Cross(_direction);
}

const Matrix4x4& Camera::getView()const
{
	return _view;
}

const Matrix4x4& Camera::getProj()const
{
	return _proj;
}

const Matrix4x4& Camera::getViewProj()const
{
	return _view_proj;
}

const Vector3* Camera::getFrustumCorners() const
{
	return _corners;
}

Vector2 Camera::getClipDistances() const
{
	return Vector2(_near_z, _far_z);
}

float Camera::getFOV() const
{
	return _fov;
}

Vector2 Camera::getFarPlaneSize() const
{
	return Vector2(_far_width, _far_height);
}

const Plane* Camera::getPlanes() const
{
	return _planes;
}

void Camera::updateCornersAndPlanes()
{
	////// UPDATE CORNERS POSITION

	Vector3 nearCenter = _position + (_direction * _near_z);

	float half_near_height = _near_height / 2;
	float half_near_width  = _near_width / 2;

	//Near Top Left
	_corners[0] = nearCenter + (_up * half_near_height) - (_right * half_near_width);
	//Near Top Right
	_corners[1] = nearCenter + (_up * half_near_height) + (_right * half_near_width);
	//Near Down Left
	_corners[2] = nearCenter - (_up * half_near_height) - (_right * half_near_width);
	//Near Down Right
	_corners[3] = nearCenter - (_up * half_near_height) + (_right * half_near_width);

	Vector3 farCenter = _position + (_direction * _far_z);

	float half_far_height = _far_height / 2;
	float half_far_width  = _far_width / 2;

	//Far Top Left
	_corners[4] = farCenter + (_up * half_far_height) - (_right * half_far_width);
	//Far Top Right
	_corners[5] = farCenter + (_up * half_far_height) + (_right * half_far_width);
	//Far Down Left
	_corners[6] = farCenter - (_up * half_far_height) - (_right * half_far_width);
	//Far Down Right
	_corners[7] = farCenter - (_up * half_far_height) + (_right * half_far_width);

	// Planes face inward.

	// Top Plane
	_planes[0].x = _view_proj._14 - _view_proj._12;
	_planes[0].y = _view_proj._24 - _view_proj._22;
	_planes[0].z = _view_proj._34 - _view_proj._32;
	_planes[0].w = _view_proj._44 - _view_proj._42;

	// Bottom Plane
	_planes[1].x = _view_proj._14 + _view_proj._12;
	_planes[1].y = _view_proj._24 + _view_proj._22;
	_planes[1].z = _view_proj._34 + _view_proj._32;
	_planes[1].w = _view_proj._44 + _view_proj._42;

	// Left Plane
	_planes[2].x = _view_proj._14 + _view_proj._11;
	_planes[2].y = _view_proj._24 + _view_proj._21;
	_planes[2].z = _view_proj._34 + _view_proj._31;
	_planes[2].w = _view_proj._44 + _view_proj._41;

	// Right Plane
	_planes[3].x = _view_proj._14 - _view_proj._11;
	_planes[3].y = _view_proj._24 - _view_proj._21;
	_planes[3].z = _view_proj._34 - _view_proj._31;
	_planes[3].w = _view_proj._44 - _view_proj._41;

	// Near Plane
	_planes[4].x = _view_proj._13;
	_planes[4].y = _view_proj._23;
	_planes[4].z = _view_proj._33;
	_planes[4].w = _view_proj._43;

	// Far Plane
	_planes[5].x = _view_proj._14 - _view_proj._13;
	_planes[5].y = _view_proj._24 - _view_proj._23;
	_planes[5].z = _view_proj._34 - _view_proj._33;
	_planes[5].w = _view_proj._44 - _view_proj._43;

	for(int i = 0; i < 6; i++)
		_planes[i].Normalize();
}