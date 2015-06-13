#pragma once

#include "ShadowMapGenerator.h"

#include "..\DevTools\Profiler.h"

#include "..\Renderer\Renderer.h"
#include "..\Renderer\RendererStructs.h"

#include "..\Renderer\Camera.h"

#include "..\Utilities\StringID.h"

using namespace aqua;


void ShadowMapGenerator::init(aqua::Renderer& renderer, lua_State* lua_state, Allocator& allocator, LinearAllocator& scratchpad)
{
	_renderer             = &renderer;
	_allocator            = &allocator;
	_scratchpad_allocator = &scratchpad;
};

void ShadowMapGenerator::shutdown()
{

};

u32 ShadowMapGenerator::getSecondaryViews(const Camera& camera, RenderView* out_views)
{
	return 0;
}

void ShadowMapGenerator::generate(const void* args_, const VisibilityData* visibility)
{
	const Args& args = *(const Args*)args_;

	Profiler* profiler = _renderer->getProfiler();

	u32 scope_id;

	//_renderer->getRenderDevice()->clearDepthStencilTarget(args.dsv, 1.0f);

	_renderer->bindFrameParameters();

	//BUILD QUEUES

	u32 passes_names[] = { getStringID("shadow"), getStringID("shadow_alpha_masked") };

	enum class PassNameIndex : u8
	{
		SHADOW_MAP,
		SHADOW_MAP_ALPHA_MASKED,
		NUM_PASSES
	};

	RenderQueue queues[(u8)PassNameIndex::NUM_PASSES];

	_renderer->buildRenderQueues((u8)PassNameIndex::NUM_PASSES, passes_names, visibility, queues);

	u8 pass_index;

	//-----------------------------------------------
	//Shadow Map pass
	//-----------------------------------------------

	scope_id = profiler->beginScope("shadow_map");

	_renderer->setRenderTarget(0, nullptr, args.dsv);

	//_renderer->bindViewParameters();

	_renderer->setViewport(*args.viewport, args.shadow_map_width, args.shadow_map_height);

	pass_index = _renderer->getShaderManager()->getPassIndex(passes_names[(u8)PassNameIndex::SHADOW_MAP]);

	_renderer->render(pass_index, queues[(u8)PassNameIndex::SHADOW_MAP]);

	//-----------------------------------------------
	//Shadow Map Alpha Masked pass
	//-----------------------------------------------

	pass_index = _renderer->getShaderManager()->getPassIndex(passes_names[(u8)PassNameIndex::SHADOW_MAP_ALPHA_MASKED]);

	_renderer->render(pass_index, queues[(u8)PassNameIndex::SHADOW_MAP_ALPHA_MASKED]);

	profiler->endScope(scope_id);
};

void ShadowMapGenerator::generate(lua_State* lua_state)
{
	//NOT IMPLEMENTED
};