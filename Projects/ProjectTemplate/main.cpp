
#include <AquaGame.h>

#if AQUA_DEBUG || AQUA_DEVELOPMENT
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

using namespace aqua;

class BasicExample : public AquaGame
{
public:
	bool init() override
	{
		if(!AquaGame::init())
			return false;

		return true;
	}

	int update(float dt) override
	{
		//--------------------------------------------------------------------------

		render();

		return true;
	}

	int shutdown() override
	{
		//-----------------------------------------------------------

		AquaGame::shutdown();

		return true;
	}

private:

};

#ifdef _WIN32

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
#if _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(230); //Uncomment and change number to enable breakpoint at memory leak
#endif

#if _WIN32
	SetCurrentDirectory(lpCmdLine);
#endif

	BasicExample game;

	if(!game.init())
	{
		game.shutdown();
		return 0;
	}

	return game.run();
}

#endif