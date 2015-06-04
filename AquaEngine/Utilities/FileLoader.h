#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\AquaTypes.h"

namespace aqua
{
	namespace file2
	{
		bool readFile(const char* filename, char* output);

		struct AsyncFileHandle
		{
#if _WIN32
			HANDLE     file;
			OVERLAPPED overlapped;
#endif
		};

		enum class AsyncReadState : u8
		{
			FAIL,
			PENDING,
			DONE
		};

		bool openFileAsync(const char* filename, AsyncFileHandle& file);
		void closeFileAsync(AsyncFileHandle& file);

		AsyncReadState readFileAsync(AsyncFileHandle& file, u64 offset, u32 num_bytes, char* output);
		AsyncReadState checkAsyncFileRead(AsyncFileHandle& file);

		u64 getFileSize(const char* filename);
	};
};