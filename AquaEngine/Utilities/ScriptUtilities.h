#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include <lua.hpp>

namespace aqua
{
	namespace script_utilities
	{
		//If sandbox = true, the function expects a table on the top of the stack
		//to use as the script environment
		bool loadScript(lua_State* lua_state, const char* filename, bool sandbox = true);
		bool unloadScript(lua_State* lua_state, const char* filename);

		bool getScriptTable(lua_State* lua_state, const char* filename);

		void stackDump(lua_State* lua_state);

		double getMemoryUsage(lua_State* lua_state);
	}
}