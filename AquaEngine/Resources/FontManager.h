#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\Renderer\RenderDevice\RenderDeviceTypes.h"

#include "..\AquaMath.h"
#include "..\AquaTypes.h"

namespace aqua
{
	class RenderDevice;
	class Allocator;

	struct FontVertex
	{
		Vector2 position;
		Vector2 tex_coord;
	};

	class FontManager
	{
	public:
		FontManager(Allocator& allocator, RenderDevice& render_device);
		~FontManager();

		bool loadFont(u32 name, const void* data, size_t size);
		bool unloadFont(u32 name);

		struct RenderString
		{
			ShaderResourceH sprite_font;
			u32             num_quads;
			float           width;
			float           height;
		};

		RenderString buildStrings(u32 font_name, const char* string, FontVertex* out_vertices);

	private:

		struct Font;

		Allocator&    _allocator;
		RenderDevice& _render_device;

		u32*  _fonts_names;
		Font* _fonts;
		u32	  _num_fonts;
		u32	  _capacity;

	};
};