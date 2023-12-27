#ifndef __THREADS_H__
#define __THREADS_H__

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

#define THREAD_MAXJOBS 128

typedef void (*ThreadFunction_t)(void *arg);

// Structure that holds the function pointer and argument
// to store in a list that can be iterated as a job list.
typedef struct
{
	ThreadFunction_t function;
	void *arg;
} ThreadJob_t;

// Structure for worker context
typedef struct
{
	bool pause;
	bool stop;

	ThreadJob_t jobs[THREAD_MAXJOBS];
	uint32_t numJobs;

	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t condition;

	ThreadFunction_t constructor;
	void *constructorArg;

	ThreadFunction_t destructor;
	void *destructorArg;
} ThreadWorker_t;

uint32_t Thread_GetJobCount(ThreadWorker_t *worker);
bool Thread_AddJob(ThreadWorker_t *worker, ThreadFunction_t jobFunc, void *arg);
void Thread_AddConstructor(ThreadWorker_t *worker, ThreadFunction_t constructorFunc, void *arg);
void Thread_AddDestructor(ThreadWorker_t *worker, ThreadFunction_t destructorFunc, void *arg);
bool Thread_Init(ThreadWorker_t *worker);
bool Thread_Start(ThreadWorker_t *worker);
void Thread_Pause(ThreadWorker_t *worker);
void Thread_Resume(ThreadWorker_t *worker);
bool Thread_Destroy(ThreadWorker_t *worker);

#endif
