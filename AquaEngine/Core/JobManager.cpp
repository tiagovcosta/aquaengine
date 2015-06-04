#include "JobManager.h"

#include <algorithm>

#include "..\Utilities\Debug.h"

using namespace aqua;

__declspec(thread) u8 aqua::THREAD_ID;

JobManager& JobManager::get()
{
	static JobManager manager;

	return manager;
}

JobManager::JobManager() : _stop(false), _num_workers(std::thread::hardware_concurrency()),
_num_jobs_in_queue(0), _first_free(0), _next_job(0)
{
	if(_num_workers < MIN_NUM_WORKERS)
		_num_workers = MIN_NUM_WORKERS;
	else if(_num_workers > MAX_NUM_WORKERS)
		_num_workers = MAX_NUM_WORKERS;

	//Init job's ids
	for(u32 i = 0; i < MAX_NUM_JOBS; i++)
	{
		_jobs[i].id = _next_job++ << 32;
		_jobs[i].id |= i;

		_free_list[i] = i;
	}

	//Create worker threads
	for(unsigned int i = 0; i < _num_workers; ++i)
		_workers[i] = std::thread([this](int thread_id)
	{
		//init THREAD_ID
		THREAD_ID = thread_id;

		while(true)
		{
			std::unique_lock<std::mutex> queue_lock(_jobs_queue_mutex);

			while(!_stop && _num_jobs_in_queue == 0) //Check spurious wakeup
			{
				_condition.wait(queue_lock);
			}

			if(_stop)
			{
				queue_lock.unlock();
				return;
			}
				

			//Get job with highest priority
			Job& job = _jobs[_jobs_queue[0]];

			std::pop_heap(std::begin(_jobs_queue), _jobs_queue + _num_jobs_in_queue, [this](const u32 x, const u32 y)
			{
				return _jobs[x].priority < _jobs[y].priority;
			});

			_num_jobs_in_queue--;

			queue_lock.unlock();

			/*
			//Wait for dependency
			if(job.dependency != NULL_JOB)
			{
				wait(job.dependency);
			}
			*/

			//Run job

			job.func(job.id, job.data);

			finishJob(job.id);
		}
	}, i+1);
}

JobManager::~JobManager()
{
	ASSERT(_stop);
}

JobId JobManager::addJob(JobFunc func, void* data, JobId dependency, JobId parent, u32 priority)
{
	/*
	u32 index;

	//Get free job from free list
	{
		std::lock_guard<std::mutex> queue_lock(_jobs_mutex);

		ASSERT("JobManager full!" && _first_free < MAX_NUM_JOBS);

		index = _free_list[_first_free++];
	}
	*/

	u32 first_free = _first_free++;
	
	ASSERT("JobManager full!" && first_free < MAX_NUM_JOBS);

	u32 index = _free_list[first_free];

	Job& job            = _jobs[index];
	job.func            = func;
	job.data            = data;
	job.parent          = parent;
	job.open_jobs       = 1;
	//job.dependency    = dependency;
	job.priority        = priority;
	job.first_dependent = NULL_JOB & INDEX_MASK;
	job.sibling         = NULL_JOB & INDEX_MASK;

	if(parent != NULL_JOB)
	{
		_jobs[parent & INDEX_MASK].open_jobs++;
	}

	//Check dependency
	if(dependency != NULL_JOB)
	{
		u32 dependency_index = dependency & INDEX_MASK;

		std::lock_guard<std::mutex> lock(_jobs_mutexes[dependency_index]);

		if(!isFinished(dependency))
		{
			Job& dependency_job = _jobs[dependency_index];

			job.sibling                    = dependency_job.first_dependent;
			dependency_job.first_dependent = index;

			return job.id;
		}
	}

	//Add job to queue
	{
		std::lock_guard<std::mutex> queue_lock(_jobs_queue_mutex);

		_jobs_queue[_num_jobs_in_queue++] = index;

		std::push_heap(std::begin(_jobs_queue), _jobs_queue + _num_jobs_in_queue, [this](const u32 x, const u32 y)
		{
			return _jobs[x].priority < _jobs[y].priority;
		});
	}

	JobId id = job.id; //Copy id before notifying to prevent errors

	_condition.notify_one();

	return id;
}

bool JobManager::isFinished(JobId job) const
{
	return _jobs[job & INDEX_MASK].id != job;
}

void JobManager::wait(JobId job)
{
	while(!isFinished(job))
	{
		if(_stop)
			return;

		u32 job_index = MAX_NUM_JOBS;

		{
			std::lock_guard<std::mutex> queue_lock(_jobs_queue_mutex);

			if(_num_jobs_in_queue > 0)
			{
				//Get job with highest priority
				job_index = _jobs_queue[0];

				std::pop_heap(std::begin(_jobs_queue), _jobs_queue + _num_jobs_in_queue, [this](const u32 x, const u32 y)
				{
					return _jobs[x].priority < _jobs[y].priority;
				});

				_num_jobs_in_queue--;
			}
		}

		//If there was a job in queue
		if(job_index != MAX_NUM_JOBS)
		{
			Job& job = _jobs[job_index];

			/*
			//Wait for dependency
			if(job.dependency != NULL_JOB)
			{
				wait(job.dependency);
			}
			*/

			//Run job
			job.func(job.id, job.data);

			finishJob(job.id);
		}
		else
		{
			std::this_thread::yield();
		}
	}
}

void JobManager::stop()
{
	_stop = true;

	_condition.notify_all();

	for(unsigned int i = 0; i < _num_workers; ++i)
	{
		bool k = _workers[i].joinable();
		_workers[i].join();
	}
}

u32 JobManager::getNumWorkers() const
{
	return _num_workers;
}

void JobManager::finishJob(JobId job_id)
{
	u32 job_index = job_id & INDEX_MASK;

	Job& job = _jobs[job_index];

	ASSERT(job.id == job_id);

	u32 open_jobs = --job.open_jobs;

	//Job finished, decrease parent open jobs (if job has a parent)
	if(open_jobs == 0)
	{
		if(job.parent != NULL_JOB)
			finishJob(job.parent);

		{
			std::lock_guard<std::mutex> lock(_jobs_mutexes[job_index]);

			//Add dependents to queue

			u32 dependent_index = job.first_dependent;

			if(dependent_index != (NULL_JOB & INDEX_MASK))
			{
				{
					std::lock_guard<std::mutex> queue_lock(_jobs_queue_mutex);

					while(dependent_index != (NULL_JOB & INDEX_MASK))
					{
						_jobs_queue[_num_jobs_in_queue++] = dependent_index;

						std::push_heap(std::begin(_jobs_queue), _jobs_queue + _num_jobs_in_queue, [this](const u32 x, const u32 y)
						{
							return _jobs[x].priority < _jobs[y].priority;
						});

						dependent_index = _jobs[dependent_index].sibling;
					}
				}

				_condition.notify_all();
			}

			//Set new id
			JobId new_id = job_index | (_next_job++ << 32);
			job.id       = new_id;
		}
		
		/*
		{
		std::lock_guard<std::mutex> lock(_jobs_mutex);

		_free_list[--_first_free] = job_index; //Add job to free list
		}
		*/

		_free_list[--_first_free] = job_index; //Add job to free list
	}
}