#include "TransformManager.h"

using namespace aqua;

const TransformManager::Instance TransformManager::INVALID_INSTANCE = { INVALID_INDEX };

TransformManager::TransformManager(Allocator& allocator, u32 inital_capacity)
	: _allocator(allocator), _map(allocator), _num_modified_transforms(0)
{
	_data.size     = 0;
	_data.capacity = 0;

	if(inital_capacity > 0)
		setCapacity(inital_capacity);
}

TransformManager::~TransformManager()
{
	if(_data.capacity > 0)
		_allocator.deallocate(_data.buffer);
}

TransformManager::Instance TransformManager::create(Entity e)
{
	if(_data.size > 0)
	{
		if(lookup(e).valid())
			return INVALID_INSTANCE; //Max one transform per entity
	}

	ASSERT(_data.size <= _data.capacity);

	if(_data.size == _data.capacity)
		setCapacity(_data.capacity * 2 + 8);

	_map.insert(e, _data.size);

	_data.entity[_data.size]         = e;
	_data.local_position[_data.size] = Vector3();
	_data.local_rotation[_data.size] = Quaternion();
	_data.local_scale[_data.size]    = Vector3(1, 1, 1);
	_data.parent[_data.size]         = INVALID_INSTANCE;
	_data.first_child[_data.size]    = INVALID_INSTANCE;
	_data.next_sibling[_data.size]   = INVALID_INSTANCE;
	_data.prev_sibling[_data.size]   = INVALID_INSTANCE;
	_data.world[_data.size]          = Matrix4x4();

	transform(_data.size);

	return{ _data.size++ };
}

TransformManager::Instance TransformManager::lookup(Entity e)
{
	u32 i = _map.lookup(e, INVALID_INDEX);

	return{ i };
}

void TransformManager::destroy(Instance i)
{
	u32 last      = _data.size - 1;
	Entity e      = _data.entity[i.i];
	Entity last_e = _data.entity[last];

	_data.entity[i.i]         = _data.entity[last];
	_data.local_position[i.i] = _data.local_position[last];
	_data.local_rotation[i.i] = _data.local_rotation[last];
	_data.local_scale[i.i]    = _data.local_scale[last];

	_map.insert(last_e, i.i);
	_map.remove(e);

	_data.size--;
}

u32 TransformManager::getNumModifiedTransforms() const
{
	return _num_modified_transforms;
}

const ModifiedTransform* TransformManager::getModifiedTransforms() const
{
	return _modified_transforms;
}

void TransformManager::clearModifiedTransforms()
{
	_num_modified_transforms = 0;
}

void TransformManager::setParent(Instance i, Instance parent)
{
	Instance old_parent = _data.parent[i];

	if(old_parent.valid())
	{
		//Remove instance from old parent child list

		Instance prev_sibling = _data.prev_sibling[i];
		Instance next_sibling = _data.next_sibling[i];

		if(_data.first_child[old_parent] == i)
			_data.first_child[old_parent] = next_sibling;
		else
		{
			ASSERT(prev_sibling.valid());

			_data.next_sibling[prev_sibling] = next_sibling;
		}

		if(next_sibling.valid())
			_data.prev_sibling[next_sibling] = prev_sibling;
	}

	//Add instance to new parent child list

	_data.parent[i] = parent;

	Instance first_child = _data.first_child[parent];

	_data.first_child[parent] = i;
	_data.next_sibling[i] = first_child;

	if(first_child.valid())
		_data.prev_sibling[first_child] = i;

	transform(i);
}

void TransformManager::setLocal(Instance i, const Vector3& position, const Quaternion& rotation, const Vector3& scale)
{
	_data.local_position[i.i] = position;
	_data.local_rotation[i.i] = rotation;
	_data.local_scale[i.i]    = scale;
	transform(i);
}

void TransformManager::setLocalPosition(Instance i, const Vector3& position)
{
	_data.local_position[i.i] = position;
	transform(i);
}

void TransformManager::setLocalRotation(Instance i, const Quaternion& rotation)
{
	_data.local_rotation[i.i] = rotation;
	transform(i);
}

void TransformManager::setLocalScale(Instance i, const Vector3& scale)
{
	_data.local_scale[i.i] = scale;
	transform(i);
}

void TransformManager::scale(Instance i, const Vector3& scale)
{
	_data.local_scale[i.i] *= scale;
	transform(i);
}

void TransformManager::rotate(Instance i, const Quaternion& rotation, bool world_space)
{
	if(world_space)
		_data.local_rotation[i.i] = Quaternion::Concatenate(rotation, _data.local_rotation[i.i]);
	else
		_data.local_rotation[i.i] = Quaternion::Concatenate(_data.local_rotation[i.i], rotation);

	transform(i);
}

void TransformManager::translate(Instance i, const Vector3& translation, bool world_space)
{
	if(world_space)
		_data.local_position[i.i] += translation;
	else
	{
		/*
		Vector3 right   = _data.world[i.i].Right();
		//right.Normalize();
		Vector3 up      = _data.world[i.i].Up();
		//up.Normalize();
		Vector3 forward = _data.world[i.i].Backward();
		//forward.Normalize();

		Vector3 t = Vector3::Transform(translation, Matrix4x4(right, up, forward));
		*/
		Vector3 t = Vector3::TransformNormal(translation, _data.world[i.i]);

		_data.local_position[i.i] += t;
	}

	transform(i);
}

void TransformManager::transform(Instance i)
{
	Instance parent = _data.parent[i.i];

	Matrix4x4 parent_tm = parent.valid() ? _data.world[parent.i] : Matrix4x4();
	transform(parent_tm, i);
}

void TransformManager::transform(const Matrix4x4& parent, Instance i)
{
	Matrix4x4 local = Matrix4x4::CreateScale(_data.local_scale[i.i]) *
					  Matrix4x4::CreateFromQuaternion(_data.local_rotation[i.i]);

	local._41 = _data.local_position[i.i].x;
	local._42 = _data.local_position[i.i].y;
	local._43 = _data.local_position[i.i].z;

	_data.world[i.i] = local * parent;
	
	//Add transform to modified list
	//TODO: Check if this instance is already in modified list

	ASSERT(_num_modified_transforms != MAX_NUM_MODIFIED_TRANSFORMS);

	ModifiedTransform& mt = _modified_transforms[_num_modified_transforms++];
	mt.entity             = _data.entity[i.i];
	mt.transform          = &_data.world[i.i];
	
	Instance child = _data.first_child[i.i];

	while(child.valid())
	{
		transform(_data.world[i.i], child);
		child = _data.next_sibling[child.i];
	}
}

const Matrix4x4& TransformManager::getWorld(Instance i) const
{
	return _data.world[i.i];
}

void TransformManager::setCapacity(u32 new_capacity)
{
	size_t new_buffer_size = new_capacity*(sizeof(Entity) +
										   sizeof(Vector3) +
										   sizeof(Quaternion) +
										   sizeof(Vector3) +
										   4 * sizeof(Instance) +
										   sizeof(Matrix4x4));

	InstanceData new_data;
	new_data.size     = _data.size;
	new_data.capacity = new_capacity;
	new_data.buffer   = _allocator.allocate(new_buffer_size, __alignof(Entity));

	new_data.entity         = (Entity*)(new_data.buffer);
	new_data.local_position = (Vector3*)(new_data.entity + new_capacity);
	new_data.local_rotation = (Quaternion*)(new_data.local_position + new_capacity);
	new_data.local_scale    = (Vector3*)(new_data.local_rotation + new_capacity);
	new_data.parent         = (Instance*)(new_data.local_scale + new_capacity);
	new_data.first_child    = (Instance*)(new_data.parent + new_capacity);
	new_data.next_sibling   = (Instance*)(new_data.first_child + new_capacity);
	new_data.prev_sibling   = (Instance*)(new_data.next_sibling + new_capacity);
	new_data.world          = (Matrix4x4*)(new_data.prev_sibling + new_capacity);

	//CHECK IF ALL ARRAYS ARE PROPERLY ALIGNED
	ASSERT(pointer_math::isAligned(new_data.entity));
	ASSERT(pointer_math::isAligned(new_data.local_position));
	ASSERT(pointer_math::isAligned(new_data.local_rotation));
	ASSERT(pointer_math::isAligned(new_data.local_scale));
	ASSERT(pointer_math::isAligned(new_data.parent));
	ASSERT(pointer_math::isAligned(new_data.first_child));
	ASSERT(pointer_math::isAligned(new_data.next_sibling));
	ASSERT(pointer_math::isAligned(new_data.prev_sibling));
	ASSERT(pointer_math::isAligned(new_data.world));

	if(_data.size > 0)
	{
		memcpy(new_data.entity, _data.entity, _data.size * sizeof(decltype(*new_data.entity)));
		memcpy(new_data.local_position, _data.local_position, _data.size * sizeof(decltype(*new_data.local_position)));
		memcpy(new_data.local_rotation, _data.local_rotation, _data.size * sizeof(decltype(*new_data.local_rotation)));
		memcpy(new_data.local_scale, _data.local_scale, _data.size * sizeof(decltype(*new_data.local_scale)));
		memcpy(new_data.parent, _data.parent, _data.size * sizeof(decltype(*new_data.parent)));
		memcpy(new_data.first_child, _data.first_child, _data.size * sizeof(decltype(*new_data.first_child)));
		memcpy(new_data.next_sibling, _data.next_sibling, _data.size * sizeof(decltype(*new_data.next_sibling)));
		memcpy(new_data.world, _data.world, _data.size * sizeof(decltype(*new_data.world)));
	}

	if(_data.capacity > 0)
		_allocator.deallocate(_data.buffer);

	_data = new_data;
}