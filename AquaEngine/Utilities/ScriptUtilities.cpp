#include "ScriptUtilities.h"

#include "Logger.h"

using namespace aqua;

bool script_utilities::loadScript(lua_State* lua_state, const char* filename, bool sandbox)
{
	// load the lua script into memory
	if(luaL_loadfile(lua_state, filename) != 0)
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, lua_tostring(lua_state, -1));
		return false;
	}

	if(sandbox)
	{
		// Store the function environment in the registry
		lua_pushvalue(lua_state, -2);
		lua_setfield(lua_state, LUA_REGISTRYINDEX, filename);

		// set the environment for the loaded script and execute it
		lua_pushvalue(lua_state, -2);
		lua_setfenv(lua_state, -2);
	}

	if(lua_pcall(lua_state, 0, 0, 0) != 0)
	{
		Logger::get().write(MESSAGE_LEVEL::ERROR_MESSAGE, CHANNEL::GENERAL, lua_tostring(lua_state, -1));
		return false;
	}

	if(sandbox)
		lua_pop(lua_state, 1);

	return true;
}

bool script_utilities::getScriptTable(lua_State* lua_state, const char* filename)
{
	// retrieve the environment from the registry
	lua_getfield(lua_state, LUA_REGISTRYINDEX, filename);

	return true;
}

bool script_utilities::unloadScript(lua_State* lua_state, const char* filename)
{
	lua_pushnil(lua_state);
	lua_setfield(lua_state, LUA_REGISTRYINDEX, filename);

	return true;
}

void script_utilities::stackDump(lua_State* lua_state)
{
	int i;
	int top = lua_gettop(lua_state);
	for(i = 1; i <= top; i++) /* repeat for each level */
	{
		int t = lua_type(lua_state, i);

		switch(t)
		{
		case LUA_TSTRING: /* strings */
		{
			Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::GENERAL, lua_tostring(lua_state, i));
			break;
		}
		case LUA_TBOOLEAN: /* booleans */
		{
			const char* x = lua_toboolean(lua_state, i) ? "true" : "false";
			Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::GENERAL, x);
			break;
		}
		case LUA_TNUMBER: /* numbers */
		{
			Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::GENERAL, lua_tostring(lua_state, i));
			//printf("%g", lua_tonumber(L, i));
			break;
		}
		default: /* other values */
		{
			Logger::get().write(MESSAGE_LEVEL::INFO_MESSAGE, CHANNEL::GENERAL, lua_typename(lua_state, t));
			break;
		}
		}

		//printf(" "); /* put a separator */
	}
	//printf("\n"); /* end the listing */
}

double script_utilities::getMemoryUsage(lua_State* lua_state)
{
	/*
	lua_getglobal(lua_state, "collectgarbage");  // function to be called
	lua_pcall(lua_state, 0, 0, 0);

	lua_getglobal(lua_state, "collectgarbage");  // function to be called
	lua_pushstring(lua_state, "count");   // push 1st argument

	lua_pcall(lua_state, 1, 1, 0);

	double x = lua_tonumber(lua_state, -1);

	lua_pop(lua_state, 1);
	*/

	lua_gc(lua_state, LUA_GCCOLLECT, 0);

	double x = (double)lua_gc(lua_state, LUA_GCCOUNT, 0);

	x += (double)lua_gc(lua_state, LUA_GCCOUNT, 0) / 1024;

	return x;
}