#include <stdio.h>
#include <string.h>
#include "system.h"
#include "threads.h"

// Main worker thread function, this does the actual calling of various job functions in the thread
void *Thread_Worker(void *data)
{
	// Get pointer to thread data
	ThreadWorker_t *worker=(ThreadWorker_t *)data;

	// If there's a constructor function assigned, call it
	if(worker->constructor)
		worker->constructor(worker->constructorArg);

	// Loop until stop is set
	for(;;)
	{
		// Lock the mutex
		pthread_mutex_lock(&worker->mutex);

		// Check if there are any jobs
		while((worker->numJobs==0)&&((!worker->stop)||(worker->pause)))
			pthread_cond_wait(&worker->condition, &worker->mutex);

		if(worker->stop)
			break;

		if(worker->numJobs==0)
			continue;

		// Get a copy of the current job
		ThreadJob_t job=worker->jobs[0];

		// Remove it from the job list
		worker->numJobs--;

		// Shift job list
		for(uint32_t i=0;i<worker->numJobs;i++)
			worker->jobs[i]=worker->jobs[i+1];

		// Unlock the mutex
		pthread_mutex_unlock(&worker->mutex);

		// If there's a valid pointer on the job item, run it
		if(job.function)
			job.function(job.arg);
	}

	// If there's a destructor function assigned, call that.
	if(worker->destructor)
		worker->destructor(worker->destructorArg);

	pthread_mutex_unlock(&worker->mutex);
	pthread_exit(NULL);

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

		pthread_mutex_lock(&worker->mutex);
		worker->jobs[worker->numJobs++]=(ThreadJob_t){ jobFunc, arg };
		pthread_cond_signal(&worker->condition);
		pthread_mutex_unlock(&worker->mutex);
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
	if(pthread_mutex_init(&worker->mutex, NULL))
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to create mutex.\r\n");
		return false;
	}

	// Initialize the condition
	if(pthread_cond_init(&worker->condition, NULL))
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

	if(pthread_create(&worker->thread, NULL, Thread_Worker, (void *)worker))
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to create worker thread.\r\n");
		return false;
	}

//	pthread_detach(worker->Thread);

	return true;
}

// Pauses thread (if a job is currently running, it will finish first)
void Thread_Pause(ThreadWorker_t *worker)
{
	pthread_mutex_lock(&worker->mutex);
	worker->pause=true;
	pthread_mutex_unlock(&worker->mutex);
}

// Resume running jobs
void Thread_Resume(ThreadWorker_t *worker)
{
	pthread_mutex_lock(&worker->mutex);
	worker->pause=false;
	pthread_cond_broadcast(&worker->condition);
	pthread_mutex_unlock(&worker->mutex);
}

// Stops thread and waits for it to exit and destroys objects.
bool Thread_Destroy(ThreadWorker_t *worker)
{
	if(worker==NULL)
		return false;

	pthread_mutex_lock(&worker->mutex);

	// Stop thread
	worker->stop=true;

	// Wake up thread
	worker->pause=false;
	pthread_cond_broadcast(&worker->condition);
	pthread_mutex_unlock(&worker->mutex);

	// Wait for thread to join back with calling thread
	pthread_join(worker->thread, NULL);

	// Destroy the mutex and condition variable
	pthread_mutex_lock(&worker->mutex);
	pthread_mutex_destroy(&worker->mutex);
	pthread_cond_destroy(&worker->condition);

	return true;
}
