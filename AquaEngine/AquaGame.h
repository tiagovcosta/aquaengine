#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015              
/////////////////////////////////////////////////////////////////////////////////////////////

#if _DEBUG
//#include "DebugTools\DebugRenderer.h"
#endif

//#include "Resources\TextureManager.h"

#include "Renderer\Renderer.h"

//#include "World\Camera.h"

//#include "Core\Allocators\AllocatorProxy.h"
#include "Core\Timer.h"

#include "AquaTypes.h"

#include "lua.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif //WIN32_LEAN_AND_MEAN

#include <Windows.h>

#endif //_WIN32

namespace aqua
{
	enum class GAME_STATE
	{
		GAME_RUNNING,
		GAME_PAUSED
	};

	class Allocator;
	class BlockAllocator;
	class SmallBlockAllocator;
	class DynamicLinearAllocator;
	class ProxyAllocator;

	//class CommandGroupWriter;

	class EndAllocator;

	class AquaGame
	{
	public:
		AquaGame();
		virtual ~AquaGame();

		int run();

		virtual bool init();
		virtual int  update(float dt);
		virtual int  render();
		virtual int  shutdown();

		bool render(Camera& camera, int generator);

		void   setGameState(GAME_STATE state);
		bool   isPaused();

		Timer* getTimer();

		/*
		enum KeyState : u8
		{
		UP,
		PRESSED,
		DOWN,
		RELEASED
		};
		*/

		static const u32 NUM_KEYS = 256;

		bool _keys_pressed[NUM_KEYS];
		bool _keys_released[NUM_KEYS];
		bool _keys_down[NUM_KEYS];

	protected:
		bool                       loadConfig();
		bool                       createWindow();

		void*                      _memory;

		Allocator*                 _main_allocator;
		DynamicLinearAllocator*    _scratchpad_allocator;
		ProxyAllocator*            _renderer_allocator;

		lua_State*                 _lua_state;

		Renderer                   _renderer;
		//TextureManager             _texture_manager;

		//void*                      _cmd_writer_scratchpad;
		//CommandGroupWriter*        _cmd_writer;

#if _DEBUG
		//DebugRenderer              _debug_renderer;
#endif

		const char*                _title;
		GAME_STATE                 _state;

		int                        _wnd_width;
		int                        _wnd_height;

		Timer                      _timer;

		WindowH                    _wnd;

		//bool _keys_state[256];
	};
};