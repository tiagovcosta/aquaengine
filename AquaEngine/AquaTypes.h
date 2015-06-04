#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include <cstdint>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif //WIN32_LEAN_AND_MEAN

#include <Windows.h>

#endif //_WIN32

namespace aqua
{
	typedef unsigned char uchar;

	typedef uint8_t  u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;

	typedef int8_t  s8;
	typedef int16_t s16;
	typedef int32_t s32;
	typedef int64_t s64;

	typedef uintptr_t uptr;

	static_assert(sizeof(UINT) == sizeof(u32), "Check Conversions");

#ifdef _WIN32
	typedef HWND WindowH;
#else
	typedef void* WindowH;
#endif
}