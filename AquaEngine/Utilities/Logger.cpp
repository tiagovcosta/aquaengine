#include "Logger.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif //WIN32_LEAN_AND_MEAN

#include <Windows.h>

#endif //_WIN32

using namespace aqua;

Logger& Logger::get()
{
	static Logger logger;

	return logger;
}

Logger::Logger()
{
	_min_message_level = MESSAGE_LEVEL::WARNING_MESSAGE;
	_channels_filter = 0ULL;
}

Logger::~Logger()
{}

bool Logger::open(const char* log_name)
{
	_log.open(log_name);

	writeSeparator();

	return true;
}

void Logger::close()
{
	writeSeparator();
	_log.close();
}

void Logger::setChannel(CHANNEL channel, bool enable)
{
	if(enable)
	{
		_channels_filter |= (u64)channel;
	}
	else
	{
		_channels_filter &= ~((u64)channel);
	}
}

void Logger::write(const char* text)
{
	_log << text << "\n";

#if _DEBUG
	OutputDebugString(text);
	OutputDebugString("\n");
#endif

	_log.flush();
}

bool Logger::write(MESSAGE_LEVEL level, CHANNEL channel, const char* format, ...)
{
	if((int)level < (int)_min_message_level || (_channels_filter & (u64)channel) == 0)
		return false;

	const unsigned int MAX_CHARS = 1023;
	static char buffer[MAX_CHARS + 1];

	int chars_written = 0;

	if(level == MESSAGE_LEVEL::WARNING_MESSAGE)
		chars_written = sprintf_s(buffer, "WARNING: ");
	else if(level == MESSAGE_LEVEL::ERROR_MESSAGE)
		chars_written = sprintf_s(buffer, "ERROR: ");

	va_list arg_list;
	va_start(arg_list, format);

	chars_written += vsnprintf_s(buffer + chars_written, MAX_CHARS - chars_written, _TRUNCATE, format, arg_list);
	buffer[MAX_CHARS] = '\0';

	va_end(arg_list);

	write(buffer);

	if(chars_written < 0)
		return false;

	return true;
}

void Logger::writeSeparator()
{
	write("------------------------------------------------------------");
}

void Logger::setMinMessageLevel(MESSAGE_LEVEL level)
{
	_min_message_level = level;
}