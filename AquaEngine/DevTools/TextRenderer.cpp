#include "TextRenderer.h"

#include "..\Resources\FontManager.h"

#include "..\Renderer\RendererUtilities.h"
#include "..\Renderer\Renderer.h"
#include "..\Renderer\RendererStructs.h"

#include "..\Core\Allocators\ScopeStack.h"
//#include "..\Utilities\Allocators\LinearAllocator.h"
#include "..\Core\Allocators\Allocator.h"

#include "..\Utilities\StringID.h"

using namespace aqua;

static const u32 MAX_NUM_CHARS = 1024;

TextRenderer::TextRenderer(Allocator& allocator, LinearAllocator& temp_allocator, Renderer& renderer, FontManager& font_manager)
	: _allocator(allocator), _temp_allocator(&temp_allocator), _renderer(&renderer), _font_manager(&font_manager)
{
	//Get shaders

	_shader = _renderer->getShaderManager()->getRenderShader(getStringID("data/shaders/text.cshader"));

	_shader_permutation = _shader->getPermutation(0);

	auto params_desc_set  = _shader->getMaterialParameterGroupDescSet();
	_material_params_desc = getParameterGroupDesc(*params_desc_set, 0);

	_material_params = _renderer->getRenderDevice()->createParameterGroup(_allocator, RenderDevice::ParameterGroupType::MATERIAL,
																		  *_material_params_desc, UINT32_MAX, 0, nullptr);

	BufferDesc buffer_desc;
	buffer_desc.type         = BufferType::DEFAULT;
	buffer_desc.num_elements = MAX_NUM_CHARS * 4;
	buffer_desc.stride       = sizeof(FontVertex);
	buffer_desc.bind_flags   = (u8)BufferBindFlag::VERTEX;
	buffer_desc.update_mode  = UpdateMode::CPU;

	_mesh = createMesh(1, _allocator);

	VertexBuffer& vb = _mesh->getVertexBuffer(0);
	vb.offset        = 0;
	vb.stride        = buffer_desc.stride;

	_renderer->getRenderDevice()->createBuffer(buffer_desc, nullptr, vb.buffer, nullptr, nullptr);

	//indices
	static const u32 num_indices = MAX_NUM_CHARS * 6;

	u16* indices = allocator::allocateArray<u16>(*_temp_allocator, num_indices);

	u16* current_index = indices;

	for(u32 i = 0; i < MAX_NUM_CHARS * 4; i += 4)
	{
		*(current_index++) = i;
		*(current_index++) = i + 1;
		*(current_index++) = i + 2;

		*(current_index++) = i + 2;
		*(current_index++) = i + 1;
		*(current_index++) = i + 3;
	}

	buffer_desc.type         = BufferType::DEFAULT;
	buffer_desc.num_elements = num_indices;
	buffer_desc.stride       = sizeof(u16);
	buffer_desc.bind_flags   = (u8)BufferBindFlag::INDEX;
	buffer_desc.update_mode  = UpdateMode::NONE;

	_renderer->getRenderDevice()->createBuffer(buffer_desc, indices, _index_buffer, nullptr, nullptr);

	_mesh->index_buffer        = _index_buffer;
	_mesh->index_format        = IndexBufferFormat::UINT16;
	_mesh->index_buffer_offset = 0;
	_mesh->topology            = PrimitiveTopology::TRIANGLE_LIST;
}

TextRenderer::~TextRenderer()
{
	RenderDevice* render_device = _renderer->getRenderDevice();

	render_device->release(_index_buffer);

	VertexBuffer& vb = _mesh->getVertexBuffer(0);

	render_device->release(vb.buffer);

	_allocator.deallocate(_mesh);

	_allocator.deallocate(_material_params);
}

u32 TextRenderer::getSecondaryViews(const Camera& camera, RenderView* out_views)
{
	return 0;
}

void TextRenderer::generate(const void* args_, const VisibilityData* visibility)
{
	const Args& args = *(const Args*)args_;

	_renderer->setViewport(*args.viewport, args.target->width, args.target->height);
	_renderer->setRenderTarget(1, args.target, nullptr);

	FontVertex vex[MAX_NUM_CHARS * 4];

	auto len = strlen(args.text);

	FontManager::RenderString render_string = _font_manager->buildStrings(getStringID("data/fonts/consolas14.spritefont"),
																		  args.text, (FontVertex*)vex);

	//render_string.width /= args.target->width;
	//render_string.height /= args.target->height;

	float x = args.x * 2;
	float y = -args.y * 2;

	for(u32 i = 0; i < render_string.num_quads * 4; i++)
	{
		vex[i].position.x = x + (vex[i].position.x / args.target->width) * 2 - 1;
		vex[i].position.y = y + (vex[i].position.y / args.target->height) * 2 + 1;

		//vex[i].position.x = (vex[i].position.x / args.target->width) * 2 - 1;
		//vex[i].position.y = (vex[i].position.y / args.target->height) * 2 + 1;

		//vex[i].position.x = (vex[i].position.x / args.target->width) * 2 - 1;
		//vex[i].position.y = (vex[i].position.y / args.target->height) * 2 - 1 + (2 - render_string.height * 2);
	}

	BufferH vb = _mesh->getVertexBuffer(0).buffer;

	auto m = _renderer->getRenderDevice()->map(vb, 0, MapType::DISCARD);

	memcpy(m.data, vex, sizeof(FontVertex) * render_string.num_quads * 4);

	/*
	FontManager::RenderString render_string = _font_manager->buildStrings(getStringID("data/fonts/arial16.spritefont"),
	"Test", (FontVertex*)m.data);
	*/

	_renderer->getRenderDevice()->unmap(vb, 0);

	DrawCall draw_call = createDrawCall(true, render_string.num_quads * 6, 0, 0);

	u8 index = _material_params_desc->getSRVIndex(getStringID("sprite_font"));

	if(index != UINT8_MAX)
		_material_params->setSRV(render_string.sprite_font, index);

	RenderItem render_item;
	render_item.mesh            = _mesh;
	render_item.material_params = _renderer->getRenderDevice()->cacheTemporaryParameterGroup(*_material_params);
	render_item.instance_params = nullptr;
	render_item.draw_call       = &draw_call;
	render_item.num_instances   = 1;
	render_item.shader          = _shader_permutation[0];

	_renderer->render(render_item);
}

void TextRenderer::generate(lua_State* lua_state)
{
	//NOT IMPLEMENTED
};