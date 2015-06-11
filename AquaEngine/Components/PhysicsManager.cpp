#include "PhysicsManager.h"

#include "..\Utilities\Blob.h"
#include "..\Utilities\Logger.h"

#if !AQUA_DEBUG
#define NDEBUG
#endif

#include <PxPhysicsAPI.h>

using namespace aqua;

using namespace physx;

static physx::PxDefaultErrorCallback gDefaultErrorCallback;
static physx::PxDefaultAllocator gDefaultAllocatorCallback;

const PhysicsManager::Instance PhysicsManager::INVALID_INSTANCE = { INVALID_INDEX };

PhysicsManager::PhysicsManager(Allocator& allocator, u32 inital_capacity, u32 num_threads)
	: _allocator(allocator), _map(allocator)
{
	_data.size     = 0;
	_data.capacity = 0;

	if(inital_capacity > 0)
		setCapacity(inital_capacity);

	_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);

	if(!_foundation)
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, "PxCreateFoundation failed!");
		//fatalError("PxCreateFoundation failed!");
		return;
	}

	#if AQUA_DEBUG || AQUA_DEVELOPMENT

		bool record_memory_allocations = true;

		_profile_zone_manager = &physx::PxProfileZoneManager::createProfileZoneManager(_foundation);

		if(!_profile_zone_manager)
		{
			Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, 
								"PxProfileZoneManager::createProfileZoneManager failed!");
			//fatalError("PxProfileZoneManager::createProfileZoneManager failed!");
			return;
		}

	#else
		bool record_memory_allocations = false;
		_profile_zone_manager          = nullptr;
	#endif

	_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *_foundation, 
							   physx::PxTolerancesScale(), record_memory_allocations, _profile_zone_manager);

	if(!_physics)
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, "PxCreatePhysics failed!");
		//fatalError("PxCreatePhysics failed!");
		return;
	}

	if(!PxInitExtensions(*_physics))
	{
		//fatalError("PxInitExtensions failed!");
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, "PxInitExtensions failed!");
		return;
	}
	
	// setup connection parameters
	const char*     pvd_host_ip = "127.0.0.1";  // IP of the PC which is running PVD
	int             port        = 5425;         // TCP port to connect to, where PVD is listening
	unsigned int    timeout     = 100;          // timeout in milliseconds to wait for PVD to respond,

	// consoles and remote PCs need a higher timeout.
	physx::PxVisualDebuggerConnectionFlags connectionFlags = physx::PxVisualDebuggerExt::getAllConnectionFlags();
	
	// and now try to connect
	_debugger_connection = physx::PxVisualDebuggerExt::createConnection(_physics->getPvdConnectionManager(),
		pvd_host_ip, port, timeout, connectionFlags);

	PxSceneDesc scene_desc(_physics->getTolerancesScale());
	scene_desc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	scene_desc.flags |= PxSceneFlag::eENABLE_ACTIVETRANSFORMS;

	if(!scene_desc.cpuDispatcher)
	{
		_cpu_dispatcher = PxDefaultCpuDispatcherCreate(num_threads);

		if(!_cpu_dispatcher)
		{
			Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, "PxDefaultCpuDispatcherCreate failed!");
			//fatalError("PxDefaultCpuDispatcherCreate failed!");
			return;
		}
			
		scene_desc.cpuDispatcher = _cpu_dispatcher;
	}
	
	if(!scene_desc.filterShader)
		scene_desc.filterShader = physx::PxDefaultSimulationFilterShader;
	/*
#if PX_SUPPORT_GPU_PHYSX
	if(!scene_desc.gpuDispatcher && mCudaContextManager)
	{
		scene_desc.gpuDispatcher = mCudaContextManager->getGpuDispatcher();
	}
#endif
	*/
	_scene = _physics->createScene(scene_desc);

	_accumulator = 0.0f;
	_step_size   = 1.0f / 60;
}

PhysicsManager::~PhysicsManager()
{
	_scene->release();

	_cpu_dispatcher->release();

	if(_debugger_connection != nullptr)
		((PxVisualDebuggerConnection*)_debugger_connection)->release();

	PxCloseExtensions();

	_physics->release();

#if AQUA_DEBUG || AQUA_DEVELOPMENT

	_profile_zone_manager->release();

#endif

	_foundation->release();

	if(_data.capacity > 0)
		_allocator.deallocate(_data.buffer);
}

bool PhysicsManager::simulate(float dt, Allocator& frame_allocator)
{
	_num_modified_transforms = 0;

	_accumulator += dt;

	if(_accumulator < _step_size)
		return false;

	_accumulator -= _step_size;

	_scene->simulate(_step_size);

	_scene->fetchResults(true);

	PxU32 num_active_transforms;
	const physx::PxActiveTransform* active_transforms = _scene->getActiveTransforms(num_active_transforms);

	_num_modified_transforms = num_active_transforms;

	if(num_active_transforms > 0)
	{
		_modified_transforms = allocator::allocateArrayNoConstruct<ActiveTransform>(frame_allocator, num_active_transforms);

		for(u32 i = 0; i < num_active_transforms; i++)
		{
			_modified_transforms[i].entity = *(Entity*)active_transforms[i].userData;

			auto p = active_transforms[i].actor2World.p;

			_modified_transforms[i].position = Vector3(p.x, p.y, p.z);

			auto q = active_transforms[i].actor2World.q;

			_modified_transforms[i].rotation = Quaternion(q.x, q.y, q.z, q.w);
		}
	}

	return true;
}

PhysicsManager::Instance PhysicsManager::createStatic(Entity e, PhysicShape shape, const Vector3& position, const Quaternion& rot)
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

	u32 index = _data.size++;

	_data.entity[index] = e;

	_data.actor[index] = _physics->createRigidStatic(PxTransform(position.x, position.y, position.z,
													 physx::PxQuat(rot.x, rot.y, rot.z, rot.w)));

	_data.actor[index]->attachShape(*shape.shape);

	_scene->addActor(*_data.actor[index]);

	return{ index };
}

PhysicsManager::Instance PhysicsManager::create(Entity e, PhysicActorType type, const Vector3& position, const Quaternion& rot)
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

	u32 index = _data.size++;

	_data.entity[index]         = e;

	if(type == PhysicActorType::STATIC)
	{
		_data.actor[index] = _physics->createRigidStatic(PxTransform(position.x, position.y, position.z,
																	 physx::PxQuat(rot.x, rot.y, rot.z, rot.w)));
	}
	else
	{
		_data.actor[index] = _physics->createRigidDynamic(PxTransform(position.x, position.y, position.z,
																	  physx::PxQuat(rot.x, rot.y, rot.z, rot.w)));
	}

	_data.actor[index]->userData = &_data.entity[index];

	_scene->addActor(*_data.actor[index]);

	return{ index++ };
}

PhysicsManager::Instance PhysicsManager::lookup(Entity e)
{
	u32 i = _map.lookup(e, INVALID_INDEX);

	return{ i };
}

void PhysicsManager::destroy(Instance i)
{
	u32 last      = _data.size - 1;
	Entity e      = _data.entity[i.i];
	Entity last_e = _data.entity[last];

	_data.entity[i.i] = _data.entity[last];
	_data.actor[i.i]  = _data.actor[last];

	_map.insert(last_e, i.i);
	_map.remove(e);

	_data.size--;
}

void PhysicsManager::addShape(Instance i, PhysicShape shape)
{
	_data.actor[i]->attachShape(*shape.shape);

	physx::PxRigidBodyExt::updateMassAndInertia(*((physx::PxRigidBody*)_data.actor[i]), 1.0f);
}

void PhysicsManager::setKinematic(Instance i, bool enable)
{
	((physx::PxRigidBody*)_data.actor[i])->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, enable);
}

void PhysicsManager::setKinematicTarget(Instance i, const Vector3& position, const Quaternion& rotation)
{
	((physx::PxRigidDynamic*)_data.actor[i])->setKinematicTarget(PxTransform(position.x, position.y, position.z,
																			 PxQuat(rotation.x, rotation.y, rotation.z, rotation.w)));
}

const Matrix4x4& PhysicsManager::getWorld(Instance i) const
{
	//return _data.actor[i.i];
	return Matrix4x4();
}

PhysicMaterial PhysicsManager::createMaterial(const PhysicMaterialDesc& desc)
{
	PhysicMaterial m;
	m.mat = _physics->createMaterial(desc.static_friction, desc.dynamic_friction, desc.restitution);

	return m;
}

PhysicShape PhysicsManager::createPlaneShape(PhysicMaterial material)
{
	PhysicShape shape;
	shape.shape = _physics->createShape(PxPlaneGeometry(), *material.mat);

	//PxCreatePlane(_physics, PxPlane()

	return shape;
}

PhysicShape PhysicsManager::createBoxShape(float hx, float hy, float hz, PhysicMaterial material)
{
	PhysicShape shape;
	shape.shape = _physics->createShape(PxBoxGeometry(hx, hy, hz), *material.mat);

	return shape;
}

PhysicShape PhysicsManager::createSphereShape(float radius, PhysicMaterial material)
{
	PhysicShape shape;
	shape.shape = _physics->createShape(PxSphereGeometry(radius), *material.mat);

	return shape;
}

void PhysicsManager::setShapeLocalPose(PhysicShape shape, const Vector3& local_position, const Quaternion& local_rot)
{
	shape.shape->setLocalPose(PxTransform(local_position.x, local_position.y, local_position.z,
										  physx::PxQuat(local_rot.x, local_rot.y, local_rot.z, local_rot.w)));
}

u32 PhysicsManager::getNumModifiedTransforms() const
{
	// retrieve array of actors that moved
	PxU32 num_active_transforms;
	const physx::PxActiveTransform* active_transforms = _scene->getActiveTransforms(num_active_transforms);

	//active_transforms[0].actor->us

	return _num_modified_transforms;
}

const ActiveTransform* PhysicsManager::getModifiedTransforms() const
{
	return _modified_transforms;
}

void PhysicsManager::setCapacity(u32 new_capacity)
{
	InstanceData new_data;
	new_data.size     = _data.size;
	new_data.capacity = new_capacity;
	new_data.buffer   = allocateBlob(_allocator, new_capacity, new_data.entity, new_data.actor);

	//CHECK IF ALL ARRAYS ARE PROPERLY ALIGNED
	ASSERT(pointer_math::isAligned(new_data.entity));
	ASSERT(pointer_math::isAligned(new_data.actor));

#define COPY_DATA(data) memcpy(new_data.data, _data.data, _data.size * sizeof(decltype(*new_data.data)));

	if(_data.size > 0)
	{
		COPY_DATA(entity);
		COPY_DATA(actor);
	}

	if(_data.capacity > 0)
		_allocator.deallocate(_data.buffer);

	_data = new_data;

	for(u32 i = 0; i < _data.size; i++)
		_data.actor[i]->userData = &_data.entity[i];
}