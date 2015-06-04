#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015
/////////////////////////////////////////////////////////////////////////////////////////////

#include "..\AquaTypes.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

//////////////////////////////////////////////
//Should the JobManager be a singleton?
//////////////////////////////////////////////

//---------------------------
//-- JOB GROUP ID
//---------------------------

namespace aqua
{
	extern __declspec(thread) aqua::u8 THREAD_ID;

	typedef u64 JobId; //<unique id (32 bits)><index (32 bits)>
	typedef void(*JobFunc)(JobId, void*);

	class JobManager
	{
	public:
		~JobManager();

		static const JobId NULL_JOB = UINT64_MAX;

		static JobManager& get();

		JobId addJob(JobFunc func, void* data, JobId dependency = NULL_JOB, JobId parent = NULL_JOB, u32 priority = 0);

		bool isFinished(JobId job) const;

		void wait(JobId job);

		void stop();

		u32 getNumWorkers() const;

	private:
		JobManager();
		JobManager(const JobManager&); //Disable copies
		JobManager& operator=(JobManager);

		static const unsigned int MIN_NUM_WORKERS = 4;
		static const unsigned int MAX_NUM_WORKERS = 32;
		static const u32          MAX_NUM_JOBS    = 2048;

		static const u32          INDEX_MASK = UINT32_MAX;

		struct Job
		{
			std::atomic<JobId> id;
			JobId              parent;
			//JobId              dependency;
			JobFunc            func;
			void*              data;
			std::atomic<u32>   open_jobs;
			u32                priority;

			u32 first_dependent;
			u32 sibling;
		};

		void finishJob(JobId job);

		u32          _num_workers;
		std::thread  _workers[MAX_NUM_WORKERS];

		u32              _jobs_queue[MAX_NUM_JOBS]; //priority queue of jobs
		Job              _jobs[MAX_NUM_JOBS];
		std::mutex       _jobs_mutexes[MAX_NUM_JOBS];
		u32              _free_list[MAX_NUM_JOBS]; //list of free entries in jobs array
		u32              _num_jobs_in_queue;
		std::atomic<u32> _first_free;

		std::atomic<u64> _next_job;

		//std::mutex              _jobs_mutex;
		std::mutex              _jobs_queue_mutex;
		std::condition_variable _condition;
		std::atomic<bool>       _stop;
	};
};