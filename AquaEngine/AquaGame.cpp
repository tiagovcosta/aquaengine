#include "AquaGame.h"

//#include "Renderer\RendererStructs.h"

//#include "Renderer\Commands\CommandGroupWriter.h"

#include "Core\Allocators\FreeListAllocator.h"
#include "Core\Allocators\DynamicLinearAllocator.h"
#include "Core\Allocators\ProxyAllocator.h"

//#include "Core\Allocators\BlockAllocator.h"
//#include "Core\Allocators\BlockAllocatorProxy.h"
//#include "Core\Allocators\SmallBlockAllocator.h"

#include "Utilities\Logger.h"
#include "Utilities\StringID.h"
#include "Utilities\Debug.h"
#include "Utilities\ScriptUtilities.h"

#include <AntTweakBar.h>

using namespace aqua;

#ifdef _WIN32
LRESULT CALLBACK WindowProc(HWND hWnd, u32 message, WPARAM wParam, LPARAM lParam);
#endif

AquaGame::AquaGame() : _title("Title not found!"), _state(GAME_STATE::GAME_RUNNING),
					   _wnd_width(1280), _wnd_height(720), _main_allocator(nullptr)
{
	for(u32 i = 0; i < NUM_KEYS; i++)
	{
		_keys_pressed[i]  = false;
		_keys_released[i] = false;
		_keys_down[i]     = false;
	}
}

AquaGame::~AquaGame()
{}

bool AquaGame::init()
{
	Logger::get().open("log.txt");
	Logger::get().setMinMessageLevel(MESSAGE_LEVEL::INFO_MESSAGE);
	Logger::get().setChannel(CHANNEL::GENERAL, true);
	Logger::get().setChannel(CHANNEL::RENDERER, true);

	size_t memory_size = 1024ULL * 1024 * 1024; //1GB
	_memory            = malloc(memory_size);

	if(_memory == nullptr)
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, "Not enough memory! 1GB of RAM needed.");
		return false;
	}

	_main_allocator = new (_memory)FreeListAllocator(memory_size - sizeof(FreeListAllocator),
													 pointer_math::add(_memory, sizeof(FreeListAllocator)));

	ASSERT(_main_allocator != nullptr);
	/*
	_temp_allocator = allocator::allocateNew<EndAllocator>(*_main_allocator,
	_main_allocator->getSize(),
	pointer_math::add(_memory, memory_size));

	ASSERT(_temp_allocator != nullptr);

	*/
	_scratchpad_allocator = allocator::allocateNew<DynamicLinearAllocator>(*_main_allocator,
																		   *_main_allocator, 4 * 1024, 32);

	ASSERT(_scratchpad_allocator != nullptr);

	//-----------

	/*_memory2 = malloc(memory_size);

	if(_memory2 == nullptr)
	{
	Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, "Not enough memory! 1GB of RAM needed.");
	return false;
	}

	_main_block_allocator = new (_memory2)BlockAllocator(memory_size - sizeof(BlockAllocator),
	pointer_math::add(_memory2, sizeof(BlockAllocator)),
	1024*16, 32);

	_game_allocator = new (pointer_math::add(_memory2, sizeof(BlockAllocator))) SmallBlockAllocator(*_main_block_allocator, 16*1024, 32);

	_renderer_allocator_proxy = NEW_BLOCK_ALLOCATOR_PROXY(*_game_allocator, *_main_block_allocator);
	*/
	//-----------

	_lua_state = luaL_newstate();
	luaL_openlibs(_lua_state);

	Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::RENDERER,
						"Initial lua memory usage: %f KB", script_utilities::getMemoryUsage(_lua_state));

	if(!loadConfig())
	{
		//Logger::get().write(MESSAGE_LEVEL::WARNING_MESSAGE, CHANNEL::GENERAL, "Error loading config!");
	}
	else
		Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::GENERAL, "Config loaded!");

	if(!createWindow())
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, "Error creating window!");
		return false;
	}

	Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::GENERAL, "Window created!");

	_renderer_allocator = allocator::allocateNew<ProxyAllocator>(*_main_allocator, *_main_allocator);

	ASSERT(_renderer_allocator != nullptr);
	
	if(!_renderer.init(_wnd_width, _wnd_height, _wnd, true, _lua_state, *_renderer_allocator, *_scratchpad_allocator))
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, "Error initializing Renderer!");
		return false;
	}

	Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::GENERAL, "Renderer initialized!");

	TwInit(TW_DIRECT3D11, _renderer.getRenderDevice()->getLowLevelDevice());
	TwWindowSize(_wnd_width, _wnd_height);

	//_cmd_writer_scratchpad = _main_allocator->allocate(1024 * 1024);
	//_cmd_writer            = allocator::allocateNew<CommandGroupWriter>(*_main_allocator, _cmd_writer_scratchpad, 1024 * 1024);

	/*
	#if _DEBUG
	if(!_debug_renderer.init(_renderer, _lua_state, *_main_allocator, *_temp_allocator))
	{
	Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, "Error initializing Debug Renderer!");
	return false;
	}

	Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::GENERAL, "Debug Renderer initialized!");

	_renderer.addRenderQueueGenerator(getStringID("debug_renderer"), &_debug_renderer);
	#endif
	*/

	Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::GENERAL, "Initialization finished!");
	Logger::get().writeSeparator();

	//_texture_manager.init(*_main_allocator, *_scratchpad_allocator, _renderer);

	return true;
}

bool AquaGame::loadConfig()
{
	bool error = false;

	double init_lua_memory = script_utilities::getMemoryUsage(_lua_state);

	lua_newtable(_lua_state);

	if(!script_utilities::loadScript(_lua_state, "data/config.lua"))
	{
		Logger::get().write(MESSAGE_LEVEL::WARNING_MESSAGE, CHANNEL::GENERAL, "Error loading config.lua! Using default config.");
		return false;
	}

	script_utilities::getScriptTable(_lua_state, "data/config.lua");

	lua_getfield(_lua_state, -1, "title");

	if(!lua_isstring(_lua_state, -1))
	{
		Logger::get().write(MESSAGE_LEVEL::WARNING_MESSAGE, CHANNEL::GENERAL, "'title' not found!");
		error = true;
	}
	else
	{
		const char* title = lua_tostring(_lua_state, -1);
		size_t title_length = strlen(title) + 1;

		char* temp = allocator::allocateArrayNoConstruct<char>(*_main_allocator, title_length);
		memcpy(temp, title, sizeof(char)*title_length);

		_title = temp;
	}

	lua_getfield(_lua_state, -2, "window_width");

	if(!lua_isnumber(_lua_state, -1))
	{
		Logger::get().write(MESSAGE_LEVEL::WARNING_MESSAGE, CHANNEL::GENERAL, "'width' not found!");
		error = true;
	}
	else
		_wnd_width = static_cast<int>(lua_tointeger(_lua_state, -1));

	lua_getfield(_lua_state, -3, "window_height");

	if(!lua_isnumber(_lua_state, -1))
	{
		Logger::get().write(MESSAGE_LEVEL::WARNING_MESSAGE, CHANNEL::GENERAL, "'height' not found!");
		error = true;
	}
	else
		_wnd_height = static_cast<int>(lua_tointeger(_lua_state, -1));

	lua_pop(_lua_state, 4); //pop title, width and height and config table

	script_utilities::unloadScript(_lua_state, "data/config.lua");

#if _DEBUG
	if(script_utilities::getMemoryUsage(_lua_state) - init_lua_memory > 0)
		Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::RENDERER, "Lua memory leak");
#endif

	return !error;
}

int AquaGame::update(float dt)
{
	render();

	return 1;
}

int AquaGame::render()
{
	//TwDraw();

	_renderer.present();

	//_temp_allocator->clear();
	_scratchpad_allocator->clear();

	return 1;
}

bool AquaGame::render(Camera& camera, int generator)
{
	//Cache frame params

	//Generate secondary views

	//Sort views


	/*

	foreach view
	{
		cache view params

		foreach render_queue_generator
		{
			cull
		}
	}

	extract (cache objects params)

	start render job in a different thread

	return

	//-----------
	// RENDER JOB
	//-----------

	foreach render_queue_generator and other systems
	{
		prepare
	}

	foreach view
	{
		foreach pass
		{
			foreach render_queue_generator
			{
				get render items
			}

			Merge, sort and submit render items
		}
	}

	*/

	TwDraw();

	_renderer.present();

	_scratchpad_allocator->clear();

	return true;
}

int AquaGame::shutdown()
{
	//_texture_manager.shutdown();

#if _DEBUG
	//_debug_renderer.shutdown();
#endif

	if(_main_allocator != nullptr)
	{
		/*
		if(_cmd_writer != nullptr)
		allocator::deallocateDelete(*_main_allocator, _cmd_writer);

		if(_cmd_writer_scratchpad != nullptr)
		_main_allocator->deallocate(_cmd_writer_scratchpad);
		*/
	}

	TwTerminate();

	_renderer.shutdown();

	lua_close(_lua_state);

	if(_main_allocator != nullptr)
	{
		if(_renderer_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _renderer_allocator);

		allocator::deallocateArray(*_main_allocator, (char*)_title);

		if(_scratchpad_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _scratchpad_allocator);
		/*
		if(_temp_allocator != nullptr)
		allocator::deallocateDelete(*_main_allocator, _temp_allocator);
		*/
		_main_allocator->~Allocator();
	}

	/*
	if(_main_block_allocator != nullptr)
	{
		if(_renderer_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _renderer_allocator);

		allocator::deallocateArray(*_main_allocator, (char*)_title);

		if(_temp_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _temp_allocator);

		_main_block_allocator->~BlockAllocator();
	}

	free(_memory2);
	*/
	free(_memory);

	Logger::get().close();

	return 1;
}

Timer* AquaGame::getTimer()
{
	return &_timer;
}

void AquaGame::setGameState(GAME_STATE state)
{
	_state = state;
}

bool AquaGame::isPaused()
{
	return _state == GAME_STATE::GAME_PAUSED;
}

int AquaGame::run()
{
#ifdef _WIN32
	MSG msg = { 0 };

	_timer.reset();

	while(msg.message != WM_QUIT)
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			_timer.tick();

			if(_state == GAME_STATE::GAME_RUNNING)
			{
				update(_timer.getElapsedTime());

				for(u32 i = 0; i < NUM_KEYS; i++)
				{
					_keys_pressed[i]  = false;
					_keys_released[i] = false;
				}
			}
			else
			{
				Sleep(50);
			}
		}

	}

	shutdown();

	return (int)msg.wParam;
#endif
}

bool AquaGame::createWindow()
{
#ifdef _WIN32
	WNDCLASSEX wc;

	ZeroMemory(&wc, sizeof(WNDCLASSEX));

	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = WindowProc;
	wc.hInstance     = NULL;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszClassName = "AquaWindowClass";

	RegisterClassEx(&wc);

	RECT rect;
	rect.left   = 0;
	rect.top    = 0;
	rect.right  = _wnd_width;
	rect.bottom = _wnd_height;

	DWORD style = /*WS_OVERLAPPEDWINDOW*/ WS_CAPTION | /*WS_MAXIMIZEBOX |*/ WS_MINIMIZEBOX | WS_SYSMENU;

	if(AdjustWindowRectEx(&rect, style, false, 0) == 0)
		return false;

	_wnd = CreateWindowEx(NULL,
		"AquaWindowClass",
		_title,
		style,
		5, 5,
		rect.right - rect.left, rect.bottom - rect.top,
		NULL,
		NULL,
		NULL,
		NULL);

	if(_wnd == NULL)
	{
		return false;
	}

	SetWindowLongPtr(_wnd, GWLP_USERDATA, (LONG_PTR)this);

	ShowWindow(_wnd, SW_SHOWNORMAL);

#endif

	return true;
}

#ifdef _WIN32
LRESULT CALLBACK WindowProc(HWND hWnd, u32 message, WPARAM wParam, LPARAM lParam)
{
	AquaGame* game = (AquaGame*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	if(game != nullptr)
	{
		if(TwEventWin(hWnd, message, wParam, lParam)) // send event message to AntTweakBar
			return 0; // event has been handled by AntTweakBar

		switch(message)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_SIZE:
			if(wParam == SIZE_MINIMIZED)
			{
				game->setGameState(GAME_STATE::GAME_PAUSED);
				//AquaGame::get().getTimer()->reset();
			}
			else if(wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
			{
				game->setGameState(GAME_STATE::GAME_RUNNING);
				game->getTimer()->start();
			}

			return 0;

		case WM_ACTIVATEAPP:
			if(wParam == FALSE)
			{
				game->setGameState(GAME_STATE::GAME_PAUSED);
				//AquaGame::get().getTimer()->reset();
			}
			else
			{
				game->setGameState(GAME_STATE::GAME_RUNNING);
				game->getTimer()->start();
			}

			return 0;

			/*
			case WM_SETFOCUS:
			break;

			case WM_KILLFOCUS:
			break;
			*/

		case WM_MOUSEMOVE:

			return 0;

		case WM_KEYDOWN:

			game->_keys_down[wParam]    = true;
			game->_keys_pressed[wParam] = true;

			return 0;

		case WM_KEYUP:

			game->_keys_down[wParam]     = false;
			game->_keys_released[wParam] = true;

			return 0;

		case WM_LBUTTONDOWN:

			game->_keys_down[VK_LBUTTON]    = true;
			game->_keys_pressed[VK_LBUTTON] = true;

			return 0;

		case WM_LBUTTONUP:

			game->_keys_down[VK_LBUTTON]     = false;
			game->_keys_released[VK_LBUTTON] = true;

			return 0;

		case WM_MBUTTONDOWN:

			game->_keys_down[VK_MBUTTON]    = true;
			game->_keys_pressed[VK_MBUTTON] = true;

			return 0;

		case WM_MBUTTONUP:

			game->_keys_down[VK_MBUTTON]     = false;
			game->_keys_released[VK_MBUTTON] = true;

			return 0;

		case WM_RBUTTONDOWN:

			game->_keys_down[VK_RBUTTON]    = true;
			game->_keys_pressed[VK_RBUTTON] = true;

			return 0;

		case WM_RBUTTONUP:

			game->_keys_down[VK_RBUTTON]     = false;
			game->_keys_released[VK_RBUTTON] = true;

			return 0;
		}
	}
	else
	{
		switch(message)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
#endif