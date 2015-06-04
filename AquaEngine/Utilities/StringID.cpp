#include "StringID.h"

#include "Logger.h"

#include "..\Utilities\Debug.h"

#include <stdint.h>
#include <map>
#include <string>
#include <stdlib.h>

using namespace aqua;

#define FORCE_INLINE    __forceinline

#define ROTL32(x,y)     _rotl(x,y)
#define ROTL64(x,y)     _rotl64(x,y)

#define BIG_CONSTANT(x) (x)

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

FORCE_INLINE uint32_t getblock(const uint32_t * p, int i)
{
	return p[i];
}

FORCE_INLINE uint64_t getblock(const uint64_t * p, int i)
{
	return p[i];
}

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

FORCE_INLINE uint32_t fmix(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

//----------

FORCE_INLINE uint64_t fmix(uint64_t k)
{
	k ^= k >> 33;
	k *= BIG_CONSTANT(0xff51afd7ed558ccd);
	k ^= k >> 33;
	k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
	k ^= k >> 33;

	return k;
}

//-----------------------------------------------------------------------------

unsigned int MurmurHash3_x86_32(const void * key, int len, uint32_t seed)
{
	const uint8_t * data = (const uint8_t*)key;
	const int nblocks = len / 4;

	uint32_t h1 = seed;

	uint32_t c1 = 0xcc9e2d51;
	uint32_t c2 = 0x1b873593;

	//----------
	// body

	const uint32_t * blocks = (const uint32_t *)(data + nblocks * 4);

	for(int i = -nblocks; i; i++)
	{
		uint32_t k1 = getblock(blocks, i);

		k1 *= c1;
		k1 = ROTL32(k1, 15);
		k1 *= c2;

		h1 ^= k1;
		h1 = ROTL32(h1, 13);
		h1 = h1 * 5 + 0xe6546b64;
	}

	//----------
	// tail

	const uint8_t * tail = (const uint8_t*)(data + nblocks * 4);

	uint32_t k1 = 0;

	switch(len & 3)
	{
	case 3: k1 ^= tail[2] << 16;
	case 2: k1 ^= tail[1] << 8;
	case 1: k1 ^= tail[0];
		k1 *= c1; k1 = ROTL32(k1, 15); k1 *= c2; h1 ^= k1;
	};

	//----------
	// finalization

	h1 ^= len;

	h1 = fmix(h1);

	return h1;
}

static std::map<unsigned int, std::string> _Entries;

unsigned int aqua::getStringID(const char* string)
{
	size_t len = strlen(string);
	ASSERT(len < INT32_MAX);

	unsigned int key = MurmurHash3_x86_32(string, (int)len, 0);

	std::map<unsigned int, std::string>::iterator it = _Entries.find(key);

	if(it == _Entries.end())
	{
		_Entries.insert(std::pair<unsigned int, std::string>(key, string));
	}
	else if(strcmp(string, getString(key)) != 0)
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, "StringID collision: %s", string);
		return 0;
	}

	return key;
}

const char* aqua::getString(unsigned int id)
{
	std::map<unsigned int, std::string>::iterator it = _Entries.find(id);

	if(it != _Entries.end())
		return (*it).second.c_str();

	return NULL;
}