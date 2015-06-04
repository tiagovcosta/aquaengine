#ifdef _WIN32

#include "FileLoader.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif //WIN32_LEAN_AND_MEAN

#include <Windows.h>

using namespace aqua;

bool file2::readFile(const char* filename, char* output)
{
	HANDLE file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, nullptr,
							 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if(file == INVALID_HANDLE_VALUE)
		return false;

	LARGE_INTEGER file_size = { 0 };
	GetFileSizeEx(file, &file_size);

	// Read the data in
	DWORD bytes_read = 0;

	if(!ReadFile(file, output, file_size.LowPart, &bytes_read, nullptr))
	{
		CloseHandle(file);
		return false;
	}

	output[static_cast<size_t>(file_size.LowPart)] = 0; //Terminator

	CloseHandle(file);

	// If not read complete file
	if(bytes_read < file_size.LowPart)
		return false;

	return true;
}

bool file2::openFileAsync(const char* filename, AsyncFileHandle& file)
{
	file.file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, nullptr,
						   OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

	if(file.file == INVALID_HANDLE_VALUE)
		return false;

	file.overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if(file.overlapped.hEvent == NULL)
		return false;

	return true;
}

void file2::closeFileAsync(AsyncFileHandle& file)
{
	CloseHandle(file.file);
	CloseHandle(file.overlapped.hEvent);
}

file2::AsyncReadState file2::readFileAsync(AsyncFileHandle& file, u64 offset, u32 num_bytes, char* output)
{
	ULARGE_INTEGER uli;
	uli.QuadPart = offset;

	file.overlapped.Offset     = uli.LowPart;
	file.overlapped.OffsetHigh = uli.HighPart;

	// Read the data
	if(!ReadFile(file.file, output, num_bytes, nullptr, &file.overlapped))
	{
		if(GetLastError() != ERROR_IO_PENDING)
		{
			//CloseHandle(file.file);
			return AsyncReadState::FAIL;
		}
		else
			return AsyncReadState::PENDING;

	}

	return AsyncReadState::DONE;
}

file2::AsyncReadState file2::checkAsyncFileRead(AsyncFileHandle& file)
{
	DWORD num_bytes_read;

	BOOL result = GetOverlappedResult(file.file, &file.overlapped, &num_bytes_read, FALSE);

	if(result)
	{
		ResetEvent(file.overlapped.hEvent);

		return AsyncReadState::DONE;
	}
	else if(GetLastError() != ERROR_IO_INCOMPLETE)
		return AsyncReadState::FAIL;

	return AsyncReadState::PENDING;
}

u64 file2::getFileSize(const char* filename)
{
	WIN32_FILE_ATTRIBUTE_DATA data;

	if(GetFileAttributesEx(filename, GetFileExInfoStandard, &data) == 0)
	{
		return 0; //Fail
	}

	ULARGE_INTEGER uli;
	uli.LowPart  = data.nFileSizeLow;
	uli.HighPart = data.nFileSizeHigh;

	return uli.QuadPart;
}

#endif