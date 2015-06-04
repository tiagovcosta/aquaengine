#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\AquaTypes.h"

namespace aqua
{
	class Timer
	{
	public:
		Timer();
		~Timer();

		void tick();
		void start();
		void reset();

		float getRuntime();
		float getElapsedTime();

		u32 getFramerate();
		u32 getMaxFramerate();

		float getMillisecondsPerFrame();

	private:
		u64 _ticks_per_second; //How many ticks are incremented per second
		u64 _start_ticks; //When was start called
		u64 _current_ticks; //Current ammount of ticks
		u64 _prev_ticks; //Last frame ammount of ticks
		u64 _one_sec_ticks; //Ammount of ticks in last second

		float _delta;
		u32   _frames_per_second;
		u32   _max_frames_per_second;
		u32   _frame_count;
	};
};