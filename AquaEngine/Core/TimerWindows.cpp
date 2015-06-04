#ifdef _WIN32

#include "Timer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif //WIN32_LEAN_AND_MEAN

#include <Windows.h>

using namespace aqua;

Timer::Timer()
{
	_frames_per_second     = 0;
	_max_frames_per_second = 0;
	_frame_count           = 0;

	_delta = 0;

	QueryPerformanceFrequency((LARGE_INTEGER*)&_ticks_per_second);
	QueryPerformanceCounter((LARGE_INTEGER*)&_current_ticks);
	_start_ticks   = _current_ticks;
	_one_sec_ticks = _current_ticks;
}

Timer::~Timer()
{}

void Timer::tick()
{
	_prev_ticks = _current_ticks;
	QueryPerformanceCounter((LARGE_INTEGER*)&_current_ticks);

	// Update the time increment
	_delta = (float)(_current_ticks - _prev_ticks) / _ticks_per_second;

	// Continue counting the frame rate regardless of the time step.

	if((float)(_current_ticks - _one_sec_ticks) / _ticks_per_second < 1.0f)
	{
		_frame_count++;
	}
	else
	{
		_frames_per_second = _frame_count;

		if(_frames_per_second > _max_frames_per_second)
			_max_frames_per_second = _frames_per_second;

		_frame_count = 0;
		_one_sec_ticks = _current_ticks;
	}
}

void Timer::start()
{
	_frames_per_second = 0;
	_frame_count       = 0;
	_delta             = 0;

	QueryPerformanceCounter((LARGE_INTEGER*)&_current_ticks);
	_one_sec_ticks = _current_ticks;
}

void Timer::reset()
{
	start();

	_max_frames_per_second = 0;
	_start_ticks           = _current_ticks;
}

float Timer::getRuntime()
{
	return (float)((_current_ticks - _start_ticks) / _ticks_per_second);
}

float Timer::getElapsedTime()
{
	return _delta;
}

u32 Timer::getFramerate()
{
	return _frames_per_second;
}

u32 Timer::getMaxFramerate()
{
	return _max_frames_per_second;
}

float Timer::getMillisecondsPerFrame()
{
	return 1000.0f / _frames_per_second;
}

#endif