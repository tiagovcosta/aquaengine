#pragma once

#include "..\Renderer\Renderer.h"

#include "..\AquaTypes.h"

namespace aqua
{
	class TextureManager
	{
	public:

		TextureManager();

		bool init(Allocator& allocator, Allocator& temp_allocator, Renderer& renderer);
		bool shutdown();

		bool create(u32 name, const char* data, size_t data_size);
		bool destroy(u32 name);

		ShaderResourceH getTexture(u32 name);

	private:

		bool setCapacity(u32 new_capacity);

		Allocator* _allocator;
		Allocator* _temp_allocator;
		Renderer*  _renderer;

		u32              _num_textures;
		u32              _capacity;
		u32*             _textures_names;
		ShaderResourceH* _textures;
		u32*			 _ref_count;

	};
};