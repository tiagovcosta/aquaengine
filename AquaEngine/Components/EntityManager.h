#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\Core\Containers\Queue.h"
#include "..\Core\Containers\Array.h"

#include "..\Utilities\Debug.h"

#include "..\AquaTypes.h"

//Based on http://bitsquid.blogspot.pt/2014/08/building-data-oriented-entity-system.html

namespace aqua
{
	struct Entity
	{
	public:
		operator u32() const { return id; };
	private:
		u32 id;

		static const u8 NUM_INDEX_BITS = 24; // ~ 16.7 million entities
		static const u32 INDEX_MASK    = (1 << NUM_INDEX_BITS) - 1;

		static const u8 NUM_GENERATION_BITS = 8;
		static const u32 GENERATION_MASK    = (1 << NUM_GENERATION_BITS) - 1;

		u32 index() const { return id & INDEX_MASK; }
		u32 generation() const { return (id >> NUM_INDEX_BITS) & GENERATION_MASK; }

		static Entity create(u32 index, u32 generation)
		{
			Entity e;
			e.id = (generation << NUM_INDEX_BITS) | index;
			return e;
		}

		friend class EntityManager;
	};

	inline bool operator==(const Entity& x, const Entity& y)
	{
		return (u32)x == (u32)y;
	}

	static_assert(sizeof(Entity) == sizeof(u32), "Check Entity struct");

	class EntityManager
	{
	public:

		EntityManager(Allocator& allocator) : _generation(allocator), _free_indices(allocator)
		{

		}

		Entity create()
		{
			u32 index;

			if(_free_indices.size() > MINIMUM_FREE_INDICES)
			{
				index = _free_indices.front();
				_free_indices.pop();
			}
			else
			{
				_generation.push(0);

				index = static_cast<u32>(_generation.size() - 1);

				ASSERT("Error: Too many entities!" && index < (1 << Entity::NUM_INDEX_BITS));
			}

			return Entity::create(index, _generation[index]);
		}

		bool alive(Entity e) const
		{
			return _generation[e.index()] == e.generation();
		}

		void destroy(Entity e)
		{
			const u32 index = e.index();

			_generation[index]++;

			_free_indices.push(index);
		}

	private:

		static const u16 MINIMUM_FREE_INDICES = 1024;

		Array<u8>  _generation;
		Queue<u32> _free_indices;
	};
};