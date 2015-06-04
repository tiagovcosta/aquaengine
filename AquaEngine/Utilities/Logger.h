#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\AquaTypes.h"

#include <fstream>

namespace aqua
{
	enum class MESSAGE_LEVEL
	{
		INFO_MESSAGE    = 0,
		WARNING_MESSAGE = 1,
		ERROR_MESSAGE   = 2
	};

	enum class CHANNEL : u64
	{
		GENERAL  = 1 << 0,
		RENDERER = 1 << 1
	};

	class Logger
	{
	public:
		~Logger();

		static Logger& get();

		bool open(const char* log_name);
		void close();

		void setChannel(CHANNEL channel, bool enable);

		bool write(MESSAGE_LEVEL level, CHANNEL channel, const char* format, ...);
		void writeSeparator();

		void setMinMessageLevel(MESSAGE_LEVEL level);

	private:
		Logger();

		void write(const char* text);

		std::ofstream _log;
		MESSAGE_LEVEL _min_message_level;

		u64           _channels_filter;
	};
}