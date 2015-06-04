#include "File.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif //WIN32_LEAN_AND_MEAN

#include <Windows.h>
#endif

using namespace aqua;

u32 file::readFile(const char* filename, bool async, char* output)
{
#ifdef _WIN32

	HANDLE file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if(file == INVALID_HANDLE_VALUE)
		return 0;

	LARGE_INTEGER file_size = { 0 };
	GetFileSizeEx(file, &file_size);

	// Read the data in
	DWORD bytes_read = 0;
	if(!ReadFile(file, output, file_size.LowPart, &bytes_read, nullptr))
	{
		CloseHandle(file);
		return 0;
	}

	output[static_cast<size_t>(file_size.LowPart)] = 0; //Terminator

	CloseHandle(file);

	// If not read complete file
	if(bytes_read < file_size.LowPart)
		return 0;

#endif //_WIN32

	return 1;
}

size_t file::getFileSize(const char* filename)
{
#ifdef _WIN32

	HANDLE file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if(INVALID_HANDLE_VALUE == file)
	{
		return 0;
	}

	LARGE_INTEGER file_size = { 0 };
	GetFileSizeEx(file, &file_size);

	// If file is too big for 32-bit allocation reject read
	if(file_size.HighPart > 0)
	{
		CloseHandle(file);
		return 0;
	}

	return static_cast<size_t>(file_size.LowPart) + 1;

#endif
}