#include <stdio.h>
#include <string.h>
#include "system.h"
#include "threads.h"

// Main worker thread function, this does the actual calling of various job functions in the thread
int Thread_Worker(void *data)
{
	// Get pointer to thread data
	ThreadWorker_t *worker=(ThreadWorker_t *)data;

	// If there's a constructor function assigned, call it
	if(worker->constructor)
		worker->constructor(worker->constructorArg);

	for(;;)
	{
		mtx_lock(&worker->mutex);

		if(!(worker->stop||worker->numJobs>0)||worker->pause)
			cnd_wait(&worker->condition, &worker->mutex);

		if(worker->numJobs>0)
		{
			// Get a copy of the current job
			ThreadJob_t job=worker->jobs[0];

			// Remove it from the job list
			worker->numJobs--;

			// Shift job list
			for(uint32_t i=0;i<worker->numJobs;i++)
				worker->jobs[i]=worker->jobs[i+1];

			// Unlock the mutex
			mtx_unlock(&worker->mutex);

			// If there's a valid pointer on the job item, run it
			if(job.function)
				job.function(job.arg);
		}
		else if(worker->stop)
		{
			// Unlock the mutex
			mtx_unlock(&worker->mutex);
			break;
		}
	}

	// If there's a destructor function assigned, call that.
	if(worker->destructor)
		worker->destructor(worker->destructorArg);

	return 0;
}

// Get the number of current jobs
uint32_t Thread_GetJobCount(ThreadWorker_t *worker)
{
	if(worker)
		return worker->numJobs;

	return 0;
}

// Adds a job function and argument to the job list
bool Thread_AddJob(ThreadWorker_t *worker, ThreadFunction_t jobFunc, void *arg)
{
	if(worker)
	{
		if(worker->numJobs>=THREAD_MAXJOBS)
			return false;

		mtx_lock(&worker->mutex);
		worker->jobs[worker->numJobs++]=(ThreadJob_t){ jobFunc, arg };
		cnd_signal(&worker->condition);
		mtx_unlock(&worker->mutex);
	}
	else
		return false;

	return true;
}

// Assigns a constructor function and argument to the thread
void Thread_AddConstructor(ThreadWorker_t *worker, ThreadFunction_t constructorFunc, void *arg)
{
	if(worker)
	{
		worker->constructor=constructorFunc;
		worker->constructorArg=arg;
	}
}

// Assigns a destructor function and argument to the thread
void Thread_AddDestructor(ThreadWorker_t *worker, ThreadFunction_t destructorFunc, void *arg)
{
	if(worker)
	{
		worker->destructor=destructorFunc;
		worker->destructorArg=arg;
	}
}

// Set up initial parameters and objects
bool Thread_Init(ThreadWorker_t *worker)
{
	if(worker==NULL)
		return false;

	// Not stopped
	worker->stop=false;

	// Not paused
	worker->pause=false;

	// No constructor
	worker->constructor=NULL;
	worker->constructorArg=NULL;

	// No destructor
	worker->destructor=NULL;
	worker->destructorArg=NULL;

	// initialize the job list
	memset(worker->jobs, 0, sizeof(ThreadJob_t)*THREAD_MAXJOBS);
	worker->numJobs=0;

	// Initialize the mutex
	if(mtx_init(&worker->mutex, mtx_plain))
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to create mutex.\r\n");
		return false;
	}

	// Initialize the condition
	if(cnd_init(&worker->condition))
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to create condition.\r\n");
		return false;
	}

	return true;
}

// Starts up worker thread
bool Thread_Start(ThreadWorker_t *worker)
{
	if(worker==NULL)
		return false;

	if(thrd_create(&worker->thread, Thread_Worker, (void *)worker))
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to create worker thread.\r\n");
		return false;
	}

	return true;
}

// Pauses thread (if a job is currently running, it will finish first)
void Thread_Pause(ThreadWorker_t *worker)
{
	mtx_lock(&worker->mutex);
	worker->pause=true;
	mtx_unlock(&worker->mutex);
}

// Resume running jobs
void Thread_Resume(ThreadWorker_t *worker)
{
	mtx_lock(&worker->mutex);
	worker->pause=false;
	cnd_broadcast(&worker->condition);
	mtx_unlock(&worker->mutex);
}

// Stops thread and waits for it to exit and destroys objects.
bool Thread_Destroy(ThreadWorker_t *worker)
{
	if(worker==NULL)
		return false;

	mtx_lock(&worker->mutex);

	// Stop thread
	worker->stop=true;

	// Wake up thread
	worker->pause=false;
	cnd_broadcast(&worker->condition);

	mtx_unlock(&worker->mutex);

	// Wait for thread to join back with calling thread
	thrd_join(worker->thread, NULL);

	// Destroy the mutex and condition variable
	mtx_lock(&worker->mutex);
	mtx_destroy(&worker->mutex);
	cnd_destroy(&worker->condition);

	return true;
}

bool ThreadBarrier_Init(ThreadBarrier_t *barrier, uint32_t count)
{
	if(count==0)
		return false;

	if(cnd_init(&barrier->cond)!=thrd_success)
		return false;

	if(mtx_init(&barrier->mutex, mtx_plain)!=thrd_success)
		return false;

	barrier->count=count;
	barrier->initCount=count;
	barrier->check=0;

	return true;
}

bool ThreadBarrier_Wait(ThreadBarrier_t *barrier)
{
	mtx_lock(&barrier->mutex);
	uint32_t check=barrier->check;

	if(--barrier->count==0)
	{
		barrier->check++;
		barrier->count=barrier->initCount;

		cnd_broadcast(&barrier->cond);
		mtx_unlock(&barrier->mutex);

		return false;
	}

	while(check==barrier->check)
		cnd_wait(&barrier->cond, &barrier->mutex);

	mtx_unlock(&barrier->mutex);

	return true;
}
