#ifndef __THREADS_H__
#define __THREADS_H__

#include <threads.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

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

	thrd_t thread;
	mtx_t mutex;
	cnd_t condition;

	ThreadFunction_t constructor;
	void *constructorArg;

	ThreadFunction_t destructor;
	void *destructorArg;
} ThreadWorker_t;

typedef struct
{
    mtx_t mutex;
    cnd_t cond;
	uint32_t initCount;
	uint32_t count;
	uint32_t check;
} ThreadBarrier_t;

uint32_t Thread_GetJobCount(ThreadWorker_t *worker);
bool Thread_AddJob(ThreadWorker_t *worker, ThreadFunction_t jobFunc, void *arg);
void Thread_AddConstructor(ThreadWorker_t *worker, ThreadFunction_t constructorFunc, void *arg);
void Thread_AddDestructor(ThreadWorker_t *worker, ThreadFunction_t destructorFunc, void *arg);
bool Thread_Init(ThreadWorker_t *worker);
bool Thread_Start(ThreadWorker_t *worker);
void Thread_Pause(ThreadWorker_t *worker);
void Thread_Resume(ThreadWorker_t *worker);
bool Thread_Destroy(ThreadWorker_t *worker);

bool ThreadBarrier_Init(ThreadBarrier_t *barrier, uint32_t count);
bool ThreadBarrier_Wait(ThreadBarrier_t *barrier);

#endif
