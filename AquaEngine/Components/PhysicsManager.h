#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "EntityManager.h"

#include "..\Core\Containers\HashMap.h"

#include "..\AquaMath.h"
#include "..\AquaTypes.h"

namespace physx
{
	class PxFoundation;
	class PxProfileZoneManager;
	class PxPhysics;
	//class PxVisualDebuggerConnection;
	class PxDefaultCpuDispatcher;
	class PxScene;
	class PxRigidActor;
	class PxMaterial;
	class PxShape;
}

namespace aqua
{
	struct PhysicMaterialDesc
	{
		float static_friction;
		float dynamic_friction;
		float restitution;
	};

	struct PhysicMaterial
	{
	private:
		physx::PxMaterial* mat;

		friend class PhysicsManager;
	};

	struct PhysicShape
	{
	private:
		physx::PxShape* shape;

		friend class PhysicsManager;
	};

	struct PhysicPose
	{
		Vector3    position;
		Quaternion rotation;
	};

	struct ActiveTransform
	{
		Entity     entity;
		Vector3    position;
		Quaternion rotation;
	};

	enum class PhysicActorType
	{
		STATIC,
		DYNAMIC
	};

	class PhysicsManager
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

			friend class PhysicsManager;
		};

		static const u32 INVALID_INDEX = UINT32_MAX;

		static const Instance INVALID_INSTANCE;

		PhysicsManager(Allocator& allocator, u32 inital_capacity, u32 num_threads);
		~PhysicsManager();

		bool simulate(float dt, Allocator& frame_allocator);

		Instance createStatic(Entity e, PhysicShape shape, const Vector3& position = Vector3(), const Quaternion& rot = Quaternion());
		Instance create(Entity e, PhysicActorType type, const Vector3& position = Vector3(), const Quaternion& rot = Quaternion());
		Instance lookup(Entity e);
		void	 destroy(Instance i);

		void addShape(Instance i, PhysicShape shape);
		void setKinematic(Instance i, bool enable);

		void setKinematicTarget(Instance i, const Vector3& position, const Quaternion& rotation = Quaternion());

		PhysicPose getWorldPose(Instance i) const;

		//------------------------------------------------------------------------------------

		PhysicMaterial createMaterial(const PhysicMaterialDesc& desc);

		PhysicShape createPlaneShape(PhysicMaterial material);
		PhysicShape createBoxShape(float hx, float hy, float hz, PhysicMaterial material);
		PhysicShape createSphereShape(float radius, PhysicMaterial material);
		PhysicShape createMeshShape(const PhysicMaterialDesc& desc);

		void setShapeLocalPose(PhysicShape shape, const Vector3& local_position = Vector3(), const Quaternion& local_rot = Quaternion());

		//------------------------------------------------------------------------------------

		u32					   getNumModifiedTransforms() const;
		const ActiveTransform* getModifiedTransforms() const;

		void setCapacity(u32 new_capacity);

	private:
		//SoA
		struct InstanceData
		{
			u32         size;
			u32         capacity;

			void*       buffer;

			Entity*				  entity;
			physx::PxRigidActor** actor;
		};

		Allocator& _allocator;

		HashMap<u32, u32> _map;

		InstanceData _data;

		u32				 _num_modified_transforms;
		ActiveTransform* _modified_transforms;

		physx::PxFoundation*               _foundation;
		physx::PxProfileZoneManager*       _profile_zone_manager;
		physx::PxPhysics*	               _physics;
		void* _debugger_connection;
		physx::PxDefaultCpuDispatcher*     _cpu_dispatcher;
		physx::PxScene*				       _scene;

		float _accumulator;
		float _step_size;
	};
};