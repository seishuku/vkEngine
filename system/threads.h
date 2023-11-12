#ifndef __THREADS_H__
#define __THREADS_H__

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

#define THREAD_MAXJOBS 128

typedef void (*ThreadFunction_t)(void *Arg);

// Structure that holds the function pointer and argument
// to store in a list that can be iterated as a job list.
typedef struct
{
	ThreadFunction_t Function;
	void *Arg;
} ThreadJob_t;

// Structure for worker context
typedef struct
{
	bool Pause;
	bool Stop;

	ThreadJob_t Jobs[THREAD_MAXJOBS];
	uint32_t NumJobs;

	pthread_t Thread;
	pthread_mutex_t Mutex;
	pthread_cond_t Condition;

	ThreadFunction_t Constructor;
	void *ConstructorArg;

	ThreadFunction_t Destructor;
	void *DestructorArg;
} ThreadWorker_t;

uint32_t Thread_GetJobCount(ThreadWorker_t *Worker);
bool Thread_AddJob(ThreadWorker_t *Worker, ThreadFunction_t JobFunc, void *Arg);
void Thread_AddConstructor(ThreadWorker_t *Worker, ThreadFunction_t ConstructorFunc, void *Arg);
void Thread_AddDestructor(ThreadWorker_t *Worker, ThreadFunction_t DestructorFunc, void *Arg);
bool Thread_Init(ThreadWorker_t *Worker);
bool Thread_Start(ThreadWorker_t *Worker);
void Thread_Pause(ThreadWorker_t *Worker);
void Thread_Resume(ThreadWorker_t *Worker);
bool Thread_Destroy(ThreadWorker_t *Worker);

#endif
