#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#if AQUA_RELEASE
	#ifndef NDEBUG
		#define NDEBUG
	#endif
#endif

#include <cassert>

#define ASSERT1(cond) assert(cond)
#define ASSERT2(cond, message) assert(message && cond)

#define GET_ASSERT_MACRO(_1,_2,NAME,...) NAME
#define ASSERT(...) GET_ASSERT_MACRO(__VA_ARGS__, ASSERT2, ASSERT1)(__VA_ARGS__)

//#include <Windows.h>
//#include <intrin.h>

/*#if _DEBUG
#define ASSERT(x) ASSERT2(x, CHAR(x), __FILE__, CHAR2(__LINE__))
#else
#define ASSERT(x)
#endif

#define ASSERT2(x, t, f, l) \
if(!x) \
{ \
::OutputDebugString("Assert " t " failed. In file " f " at line " l "."); \
::OutputDebugString( "\n" ); \
DebugBreak(); \
}

//#define debugBreak() asm{ int 3 }
//#define debugBreak() __debugbreak();

#define _T(x) L ## x
#define CHAR(x) #x
#define CHAR2(x) CHAR(x)*/