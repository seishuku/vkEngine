#include <stdio.h>
#include "threads.h"

#ifdef WIN32
#include <Windows.h>
#define DBGPRINTF(...) { char buf[512]; snprintf(buf, sizeof(buf), __VA_ARGS__); OutputDebugString(buf); }
#else
#define DBGPRINTF(...) { fprintf(stderr, __VA_ARGS__); }
#endif

// Structure that holds the function pointer and argument
// to store in a list that can be iterated as a job list.
typedef struct
{
    ThreadFunction_t Function;
    void *Arg;
} ThreadJob_t;

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
		while((List_GetCount(&Worker->Jobs)==0&&!Worker->Stop)||Worker->Pause)
			pthread_cond_wait(&Worker->Condition, &Worker->Mutex);

		ThreadJob_t Job={ NULL, NULL };

		// Get a copy of the current job
		List_GetCopy(&Worker->Jobs, 0, (void *)&Job);

		// Remove it from the job list
		List_Del(&Worker->Jobs, 0);

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
	uint32_t Count=0;

	if(Worker)
		Count=(uint32_t)List_GetCount(&Worker->Jobs);

	return Count;
}

// Adds a job function and argument to the job list
void Thread_AddJob(ThreadWorker_t *Worker, ThreadFunction_t JobFunc, void *Arg)
{
	if(Worker)
	{
		pthread_mutex_lock(&Worker->Mutex);
		List_Add(&Worker->Jobs, &(ThreadJob_t) { JobFunc, Arg });
		pthread_cond_signal(&Worker->Condition);
		pthread_mutex_unlock(&Worker->Mutex);
	}
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

	// initialize the job list, reserve memory for 10 jobs
	if(!List_Init(&Worker->Jobs, sizeof(ThreadJob_t), 10, NULL))
	{
		DBGPRINTF("Unable to create job list.\r\n");
		return false;
	}

	// Initalize the mutex
	if(pthread_mutex_init(&Worker->Mutex, NULL))
	{
		DBGPRINTF("Unable to create mutex.\r\n");
		return false;
	}

	// Initalize the condition
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

// Stops thread and waits for it to exit and destorys objects.
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

	// Destroy the job list
	List_Destroy(&Worker->Jobs);

	return true;
}
