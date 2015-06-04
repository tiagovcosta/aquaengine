#pragma once

//#include "..\Renderer\Renderer.h"
//#include "..\Renderer\RenderDevice\RenderDeviceTypes.h"
#include "..\Renderer\RendererInterfaces.h"
//#include "..\Renderer\RendererStructs.h"
#include "..\Renderer\ShaderManager.h"

#include "..\AquaMath.h"
#include "..\AquaTypes.h"

namespace aqua
{
	class Renderer;
	class RenderShader;
	class ParameterGroupDesc;
	class ParameterGroup;
	struct RenderTexture;
	struct Viewport;

	class FontManager;

	class LinearAllocator;

	class TextRenderer : public ResourceGenerator
	{
	public:
		struct Args
		{
			const Viewport*      viewport;
			const RenderTexture* target;
			const char*			 text;
			float				 x;
			float				 y;
		};

		TextRenderer(Allocator& allocator, LinearAllocator& temp_allocator, Renderer& renderer, FontManager& font_manager);
		~TextRenderer();

		// ResourceGenerator interface
		u32 getSecondaryViews(const Camera& camera, RenderView* out_views) override final;
		void generate(const void* args, const VisibilityData* visibility) override final;
		void generate(lua_State* lua_state) override final;

	private:
		/*
		struct TextMesh
		{
			Mesh*    mesh;
			DrawCall draw_call;
		};
		*/
		Allocator&       _allocator;
		LinearAllocator* _temp_allocator;

		Renderer* _renderer;

		FontManager* _font_manager;

		const RenderShader* _shader;
		ShaderPermutation   _shader_permutation;

		const ParameterGroupDesc* _material_params_desc;
		ParameterGroup*           _material_params;

		BufferH _vertex_buffer;
		BufferH _index_buffer;

		Mesh* _mesh;
	};
};