#include "FontManager.h"

#include "..\Renderer\RenderDevice\RenderDevice.h"
#include "..\Renderer\RenderDevice\RenderDeviceEnums.h"

#include "..\Utilities\BinaryReader.h"
#include "..\Core\Allocators\Allocator.h"

#include <algorithm>

using namespace aqua;

static const char SPRITE_FONT_MAGIC[] = "DXTKfont";

// Describes a single character glyph.
struct Glyph
{
	u32   character;
	RECT  subrect;
	float x_offset;
	float y_offset;
	float x_advance;
};

static inline bool operator<(const Glyph& left, const Glyph& right)
{
	return left.character < right.character;
}

static inline bool operator<(wchar_t left, const Glyph& right)
{
	return left < right.character;
}

static inline bool operator<(const Glyph& left, wchar_t right)
{
	return left.character < right;
}

struct FontManager::Font
{
	ShaderResourceH sprite_font;
	Glyph*          glyphs;
	u32             num_glyphs;
	float           line_spacing;
	Glyph*          default_glyph;
	u32             sprite_font_width;
	u32             sprite_font_height;

	const Glyph* findGlyph(wchar_t character) const;
};

// Looks up the requested glyph, falling back to the default character if it is not in the font.
const Glyph* FontManager::Font::findGlyph(wchar_t character) const
{
	auto glyph = std::lower_bound(glyphs, glyphs + num_glyphs, character);

	if(glyph != glyphs + num_glyphs && glyph->character == character)
		return glyph;

	if(default_glyph)
		return default_glyph;

	//DebugTrace("SpriteFont encountered a character not in the font (%u, %C), and no default glyph was provided\n", character, character);

	return nullptr;
}

FontManager::FontManager(Allocator& allocator, RenderDevice& render_device)
	: _allocator(allocator), _render_device(render_device), _num_fonts(0), _capacity(0)
{

}

FontManager::~FontManager()
{
	if(_capacity > 0)
	{
		allocator::deallocateArray(_allocator, _fonts);
		allocator::deallocateArray(_allocator, _fonts_names);
	}
}

bool FontManager::loadFont(u32 name, const void* data, size_t size)
{
	BinaryReader br(data, size);

	// Validate the header.
	for(char const* magic = SPRITE_FONT_MAGIC; *magic; magic++)
	{
		if(br.next<u8>() != *magic)
		{
			//DebugTrace("SpriteFont provided with an invalid .spritefont file\n");
			return false;
		}
	}

	_fonts_names = allocator::allocateArrayNoConstruct<u32>(_allocator, 1);
	_fonts       = allocator::allocateArrayNoConstruct<Font>(_allocator, 1);
	_capacity    = 1;

	_fonts_names[_num_fonts] = name;

	Font& font = _fonts[0];

	// Read the glyph data.
	font.num_glyphs = br.next<u32>();

	auto glyph_data = br.nextArray<Glyph>(font.num_glyphs);

	font.glyphs = allocator::allocateArrayNoConstruct<Glyph>(_allocator, font.num_glyphs);
	memcpy(font.glyphs, glyph_data, sizeof(Glyph)*font.num_glyphs);

	//glyphs.assign(glyph_data, glyph_data + font.num_glyphs);

	// Read font properties.
	font.line_spacing = br.next<float>();

	//SetDefaultCharacter((wchar_t)br.next<u32>());
	br.next<u32>();

	// Read the texture data.
	auto texture_width     = br.next<u32>();
	auto texture_height    = br.next<u32>();
	//auto textureFormat   = br.next<DXGI_FORMAT>();
	auto texture_format2   = br.next<RenderResourceFormat>();
	br.nextRawData(3); //skip 3 bytes
	auto texture_stride    = br.next<u32>();
	auto texture_rows      = br.next<u32>();
	auto texture_data      = br.nextArray<u8>(texture_stride * texture_rows);

	Texture2DDesc texture_desc;
	texture_desc.width          = texture_width;
	texture_desc.height         = texture_height;
	texture_desc.mip_levels     = 1;
	texture_desc.array_size     = 1;
	texture_desc.format         = texture_format2;
	texture_desc.sample_count   = 1;
	texture_desc.sample_quality = 0;
	texture_desc.update_mode    = UpdateMode::NONE;
	texture_desc.generate_mips  = false;

	TextureViewDesc view_desc;
	view_desc.format            = texture_format2;
	view_desc.most_detailed_mip = 0;
	view_desc.mip_levels        = -1;

	SubTextureData initial_data;
	initial_data.data        = texture_data;
	initial_data.line_offset = texture_stride;

	if(!_render_device.createTexture2D(texture_desc, &initial_data, 1, &view_desc, 0, nullptr, 0, nullptr,
		0, nullptr, &font.sprite_font, nullptr, nullptr, nullptr))
	{
		return false;
	}

#if _DEBUG
	RenderDevice::setDebugName(font.sprite_font, "SpriteFont");
#endif

	font.sprite_font_width = texture_width;
	font.sprite_font_height = texture_height;

	_num_fonts++;

	return true;
}

bool FontManager::unloadFont(u32 name)
{
	for(u32 i = 0; i < _num_fonts; i++)
	{
		if(_fonts_names[i] == name)
		{
			Font& f = _fonts[i];

			_render_device.release(f.sprite_font);

			allocator::deallocateArray(_allocator, f.glyphs);

			_num_fonts--;

			_fonts_names[i] = _fonts_names[_num_fonts];
			_fonts[i]       = _fonts[_num_fonts];

			return true;
		}
	}

	return false;
}

FontManager::RenderString FontManager::buildStrings(u32 font_name, const char* string, FontVertex* out_vertices)
{
	ASSERT(string != nullptr && out_vertices != nullptr);

	Font* font = nullptr;

	for(u32 i = 0; i < _num_fonts; i++)
	{
		if(_fonts_names[i] == font_name)
		{
			font = &_fonts[i];
			break;
		}
	}

	static Vector2 corner_offsets[] =
	{
		{ 0, 0 },
		{ 0, 1 },
		{ 1, 0 },
		{ 1, 1 },
	};

	RenderString rs;
	rs.num_quads = 0;
	rs.width = 0;

	if(font != nullptr)
	{
		rs.sprite_font = font->sprite_font;

		float x = 0;
		float y = 0;

		for(; *string; string++)
		{
			wchar_t character = *string;

			switch(character)
			{
			case '\r':
				// Skip carriage returns.
				break;

			case '\n':
				// New line.
				x = 0;
				y -= font->line_spacing;
				break;

			default:
				// Output this character.
				auto glyph = font->findGlyph(character);

				x += glyph->x_offset;

				if(x < 0)
					x = 0;

				if(!iswspace(character))
				{
					u32 glyph_width = glyph->subrect.right - glyph->subrect.left;
					u32 glyph_height = glyph->subrect.bottom - glyph->subrect.top;

					float glyph_left = (float)glyph->subrect.left / font->sprite_font_width;
					float glyph_right = (float)glyph->subrect.right / font->sprite_font_width;
					float glyph_top = (float)glyph->subrect.top / font->sprite_font_height;
					float glyph_bottom = (float)glyph->subrect.bottom / font->sprite_font_height;

					Vector2 glyph_position = { x, y };
					Vector2 glyph_size = { (float)glyph_width, (float)glyph_height };

					*out_vertices = { { x, y - glyph_height - glyph->y_offset },
					{ glyph_left, glyph_bottom } };

					*(out_vertices + 1) = { { x, y - glyph->y_offset },
					{ glyph_left, glyph_top } };

					*(out_vertices + 2) = { { x + glyph_width, y - glyph_height - glyph->y_offset },
					{ glyph_right, glyph_bottom } };

					*(out_vertices + 3) = { { x + glyph_width, y - glyph->y_offset },
					{ glyph_right, glyph_top } };

					out_vertices += 4;

					rs.num_quads++;
				}

				x += glyph->subrect.right - glyph->subrect.left + glyph->x_advance;

				rs.width = x > rs.width ? x : rs.width;

				break;
			}
		}

		rs.height = -y + font->line_spacing;
	}

	return rs;
}