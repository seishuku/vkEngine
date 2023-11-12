#include <stdio.h>
#include <string.h>
#include "threads.h"
#include "system.h"

// Main worker thread function, this does the actual calling of various job functions in the thread
void *Thread_Worker(void *Data)
{
	// Get pointer to thread data
	ThreadWorker_t *Worker=(ThreadWorker_t *)Data;

	// If there's a constructor function assigned, call it
	if(Worker->Constructor)
		Worker->Constructor(Worker->ConstructorArg);

	// Loop until stop is set
	while(!Worker->Stop)
	{
		// Lock the mutex
		pthread_mutex_lock(&Worker->Mutex);

		// Check if there are any jobs
		while((Worker->NumJobs==0&&!Worker->Stop)||Worker->Pause)
			pthread_cond_wait(&Worker->Condition, &Worker->Mutex);

		if(Worker->NumJobs==0)
			continue;

		ThreadJob_t Job={ NULL, NULL };

		// Get a copy of the current job
		Job=Worker->Jobs[0];

		// Remove it from the job list
		Worker->NumJobs--;

		// Shift job list
		for(uint32_t i=0;i<Worker->NumJobs;i++)
			Worker->Jobs[i]=Worker->Jobs[i+1];

		// Unlock the mutex
		pthread_mutex_unlock(&Worker->Mutex);

		// If there's a valid pointer on the job item, run it
		if(Job.Function)
			Job.Function(Job.Arg);
	}

	// If there's a destructor function assigned, call that.
	if(Worker->Destructor)
		Worker->Destructor(Worker->DestructorArg);

	return 0;
}

// Get the number of current jobs
uint32_t Thread_GetJobCount(ThreadWorker_t *Worker)
{
	if(Worker)
		return Worker->NumJobs;

	return 0;
}

// Adds a job function and argument to the job list
bool Thread_AddJob(ThreadWorker_t *Worker, ThreadFunction_t JobFunc, void *Arg)
{
	if(Worker)
	{
		if(Worker->NumJobs>=THREAD_MAXJOBS)
			return false;

		pthread_mutex_lock(&Worker->Mutex);
		Worker->Jobs[Worker->NumJobs++]=(ThreadJob_t){ JobFunc, Arg };
		pthread_cond_signal(&Worker->Condition);
		pthread_mutex_unlock(&Worker->Mutex);
	}
	else
		return false;

	return true;
}

// Assigns a constructor function and argument to the thread
void Thread_AddConstructor(ThreadWorker_t *Worker, ThreadFunction_t ConstructorFunc, void *Arg)
{
	if(Worker)
	{
		Worker->Constructor=ConstructorFunc;
		Worker->ConstructorArg=Arg;
	}
}

// Assigns a destructor function and argument to the thread
void Thread_AddDestructor(ThreadWorker_t *Worker, ThreadFunction_t DestructorFunc, void *Arg)
{
	if(Worker)
	{
		Worker->Destructor=DestructorFunc;
		Worker->DestructorArg=Arg;
	}
}

// Set up initial parameters and objects
bool Thread_Init(ThreadWorker_t *Worker)
{
	if(Worker==NULL)
		return false;

	// Not stopped
	Worker->Stop=false;

	// Not paused
	Worker->Pause=false;

	// No constructor
	Worker->Constructor=NULL;
	Worker->ConstructorArg=NULL;

	// No destructor
	Worker->Destructor=NULL;
	Worker->DestructorArg=NULL;

	// initialize the job list
	memset(Worker->Jobs, 0, sizeof(ThreadJob_t)*THREAD_MAXJOBS);
	Worker->NumJobs=0;

	// Initialize the mutex
	if(pthread_mutex_init(&Worker->Mutex, NULL))
	{
		DBGPRINTF("Unable to create mutex.\r\n");
		return false;
	}

	// Initialize the condition
	if(pthread_cond_init(&Worker->Condition, NULL))
	{
		DBGPRINTF("Unable to create condition.\r\n");
		return false;
	}

	return true;
}

// Starts up worker thread
bool Thread_Start(ThreadWorker_t *Worker)
{
	if(Worker==NULL)
		return false;

	if(pthread_create(&Worker->Thread, NULL, Thread_Worker, (void *)Worker))
	{
		DBGPRINTF("Unable to create worker thread.\r\n");
		return false;
	}

//	pthread_detach(Worker->Thread);

	return true;
}

// Pauses thread (if a job is currently running, it will finish first)
void Thread_Pause(ThreadWorker_t *Worker)
{
	pthread_mutex_lock(&Worker->Mutex);
	Worker->Pause=true;
	pthread_mutex_unlock(&Worker->Mutex);
}

// Resume running jobs
void Thread_Resume(ThreadWorker_t *Worker)
{
	pthread_mutex_lock(&Worker->Mutex);
	Worker->Pause=false;
	pthread_cond_signal(&Worker->Condition); // TODO: Is this actually needed? It seems to pause and resume fine with out it.
	pthread_mutex_unlock(&Worker->Mutex);
}

// Stops thread and waits for it to exit and destroys objects.
bool Thread_Destroy(ThreadWorker_t *Worker)
{
	if(Worker==NULL)
		return false;

	// Wake up thread
	Thread_Resume(Worker);

	// Stop the thread
	Worker->Stop=true;

	pthread_join(Worker->Thread, NULL);

	// Destroy the mutex
	pthread_mutex_destroy(&Worker->Mutex);

	// Destroy the condition variable
	pthread_cond_destroy(&Worker->Condition);

	return true;
}
