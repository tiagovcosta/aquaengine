#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "EntityManager.h"

#include "..\Core\Containers\HashMap.h"

#include "..\AquaMath.h"
#include "..\AquaTypes.h"

namespace aqua
{
	struct ModifiedTransform
	{
		Entity			 entity;
		const Matrix4x4* transform;
	};

	class TransformManager
	{
	public:
		struct Instance
		{
		public:
			operator u32() { return i; };

			bool valid() const
			{
				return i != INVALID_INDEX;
			}

		private:
			Instance(u32 index) : i(index) {}

			u32 i;

			friend class TransformManager;
		};

		static const u32 INVALID_INDEX = UINT32_MAX;

		static const Instance INVALID_INSTANCE;

		TransformManager(Allocator& allocator, u32 inital_capacity);
		~TransformManager();

		Instance create(Entity e);
		Instance lookup(Entity e);
		void	 destroy(Instance i);

		u32						 getNumModifiedTransforms() const;
		const ModifiedTransform* getModifiedTransforms() const;

		void clearModifiedTransforms();

		void setParent(Instance i, Instance parent);

		void setLocal(Instance i, const Vector3& position, const Quaternion& rotation, const Vector3& scale);
		void setLocalPosition(Instance i, const Vector3& position);
		void setLocalRotation(Instance i, const Quaternion& rotation);
		void setLocalScale(Instance i, const Vector3& scale);

		void scale(Instance i, const Vector3& scale);
		void rotate(Instance i, const Quaternion& rotation, bool world_space = false);
		void translate(Instance i, const Vector3& translation, bool world_space = false);

		void lookAt(Instance i, const Vector3& target, const Vector3& up);

		void transform(Instance i);
		void transform(const Matrix4x4& parent, Instance i);

		const Matrix4x4& getWorld(Instance i) const;

		void setCapacity(u32 new_capacity);

	private:

		//SoA
		struct InstanceData
		{
			u32         size;
			u32         capacity;

			void*       buffer;

			Entity*     entity;
			Vector3*    local_position;
			Quaternion* local_rotation;
			Vector3*    local_scale;
			Instance*   parent;
			Instance*   first_child;
			Instance*   next_sibling;
			Instance*   prev_sibling;

			Matrix4x4*  world;
		};

		Allocator& _allocator;

		HashMap<u32, u32> _map;

		InstanceData _data;

		static const u32 MAX_NUM_MODIFIED_TRANSFORMS = 16*1024;

		u32				  _num_modified_transforms;
		ModifiedTransform _modified_transforms[MAX_NUM_MODIFIED_TRANSFORMS];
	};
};