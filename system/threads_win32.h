#ifndef __THREADS_WIN32_H__
#define __THREADS_WIN32_H__

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef HANDLE thrd_t;
typedef int (*thrd_start_t)(void *);

enum {
    thrd_success	=0,
    thrd_error		=1,
    thrd_timedout	=2
};

typedef CRITICAL_SECTION mtx_t;

enum {
    mtx_plain		=0x1,
    mtx_recursive	=0x2,
    mtx_timed		=0x4
};

typedef CONDITION_VARIABLE cnd_t;

typedef struct
{
	volatile LONG state;
} once_flag;

#define ONCE_FLAG_INIT { 0 }

static inline DWORD _timespec_to_ms_timeout(const struct timespec *abstime)
{
    if(!abstime)
		return INFINITE;

	struct timespec now;

#if defined(_MSC_VER)
    time_t s = time(NULL);
    now.tv_sec = (long)s;
    now.tv_nsec = 0;
#else
    clock_gettime(CLOCK_REALTIME, &now);
#endif

    time_t sec=abstime->tv_sec-now.tv_sec;
    long nsec=(long)abstime->tv_nsec-(long)now.tv_nsec;

	if(sec<0||(sec==0&&nsec<=0))
		return 0;

	uint64_t ms=(uint64_t)sec*1000ULL+(uint64_t)(nsec/1000000L);

	if(ms>INFINITE-2)
		return INFINITE-2;

	return (DWORD)ms;
}

static unsigned __stdcall _thrd_wrapper(void *arg);

typedef struct {
    thrd_start_t func;
    void *arg;
    int retval;
} _thrd_wrapper_t;

static inline int thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
    if(!thr||!func)
		return thrd_error;

	_thrd_wrapper_t *w=( _thrd_wrapper_t *)HeapAlloc(GetProcessHeap(), 0, sizeof(_thrd_wrapper_t));

	if(!w)
		return thrd_error;

	w->func=func;
    w->arg=arg;
    w->retval=0;
    uintptr_t h=_beginthreadex(NULL, 0, (_beginthreadex_proc_type)_thrd_wrapper, w, 0, NULL);

	if(!h)
	{
        HeapFree(GetProcessHeap(), 0, w);
        return thrd_error;
    }

	*thr=(HANDLE)h;

	return thrd_success;
}

static inline int thrd_detach(thrd_t thr)
{
    if(thr==NULL)
		return thrd_error;

	if(!CloseHandle(thr))
		return thrd_error;

	return thrd_success;
}

static inline int thrd_join(thrd_t thr, int *res)
{
	if(thr==NULL)
		return thrd_error;

	DWORD r=WaitForSingleObject(thr, INFINITE);

	if(r!=WAIT_OBJECT_0)
		return thrd_error;

	DWORD exitcode=0;

	if(!GetExitCodeThread(thr, &exitcode))
	{
		CloseHandle(thr);
		return thrd_error;
    }

    if(res)
		*res=(int)exitcode;

	CloseHandle(thr);

	return thrd_success;
}

static inline thrd_t thrd_current(void)
{
    return GetCurrentThread();
}

static inline int thrd_equal(thrd_t a, thrd_t b)
{
#if(_WIN32_WINNT>=0x0600)||defined(GetThreadId)
    DWORD idA=GetThreadId(a);
    DWORD idB=GetThreadId(b);
    return idA==idB;
#else
    return a==b;
#endif
}

static inline void thrd_yield(void)
{
    SwitchToThread();
}

static inline int thrd_sleep(const struct timespec *duration, struct timespec *remaining)
{
	if(!duration)
		return thrd_error;

	uint64_t ms=(uint64_t)duration->tv_sec*1000ULL+(uint64_t)(duration->tv_nsec/1000000L);

	Sleep((DWORD)ms);

	if(remaining)
	{
		remaining->tv_sec=0;
		remaining->tv_nsec=0;
	}

	return thrd_success;
}

static unsigned __stdcall _thrd_wrapper(void *arg)
{
    _thrd_wrapper_t *w=(_thrd_wrapper_t *)arg;
    int r=w->func(w->arg);

	HeapFree(GetProcessHeap(), 0, w);
	_endthreadex((unsigned)r);

	return (unsigned)r;
}

static inline int mtx_init(mtx_t *mtx, int type)
{
	(void)type;

	if(!mtx)
		return thrd_error;

	InitializeCriticalSection(mtx);

	return thrd_success;
}

static inline void mtx_destroy(mtx_t *mtx)
{
    if(!mtx)
		return;

	DeleteCriticalSection(mtx);
}

static inline int mtx_lock(mtx_t *mtx)
{
	if(!mtx)
		return thrd_error;

	EnterCriticalSection(mtx);

	return thrd_success;
}

static inline int mtx_trylock(mtx_t *mtx)
{
	if(!mtx)
		return thrd_error;

    return TryEnterCriticalSection(mtx)?thrd_success:thrd_timedout;
}

static inline int mtx_timedlock(mtx_t *mtx, const struct timespec *abstime)
{
    if(!mtx)
		return thrd_error;

	DWORD timeout_ms=_timespec_to_ms_timeout(abstime);

	if(timeout_ms==INFINITE)
	{
		EnterCriticalSection(mtx);
		return thrd_success;
	}

	DWORD start=GetTickCount();

	while(1)
	{
		if(TryEnterCriticalSection(mtx))
			return thrd_success;

		if((GetTickCount()-start)>=timeout_ms)
			return thrd_timedout;

		Sleep(1);
    }
}

static inline int mtx_unlock(mtx_t *mtx)
{
	if(!mtx)
		return thrd_error;

	LeaveCriticalSection(mtx);

	return thrd_success;
}

static inline int cnd_init(cnd_t *cond)
{
	if(!cond)
		return thrd_error;

	InitializeConditionVariable(cond);

	return thrd_success;
}

static inline void cnd_destroy(cnd_t *cond)
{
	(void)cond;
}

static inline int cnd_signal(cnd_t *cond)
{
    if(!cond)
		return thrd_error;

	WakeConditionVariable(cond);

	return thrd_success;
}

static inline int cnd_broadcast(cnd_t *cond)
{
	if(!cond)
		return thrd_error;

	WakeAllConditionVariable(cond);

	return thrd_success;
}

static inline int cnd_wait(cnd_t *cond, mtx_t *mtx)
{
	if(!cond||!mtx)
		return thrd_error;

	return SleepConditionVariableCS(cond, mtx, INFINITE)?thrd_success:thrd_error;
}

static inline int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *abstime)
{
	if(!cond||!mtx)
		return thrd_error;

	DWORD ms=_timespec_to_ms_timeout(abstime);

	if(ms==INFINITE)
        return SleepConditionVariableCS(cond, mtx, INFINITE)?thrd_success:thrd_error;
	else if(ms==0)
		return SleepConditionVariableCS(cond, mtx, 0)?thrd_success:thrd_timedout;
    else
	{
		if(SleepConditionVariableCS(cond, mtx, ms))
			return thrd_success;

		return (GetLastError()==ERROR_TIMEOUT)?thrd_timedout:thrd_error;
	}
}

static inline void call_once(once_flag *flag, void (*func)(void))
{
	LONG expected=0;

	if(InterlockedCompareExchange(&flag->state, 1, 0)==0)
	{
		func();
		InterlockedExchange(&flag->state, 2);
	}
	else
	{
		while(flag->state!=2)
			Sleep(0);
	}
}

static inline void thrd_msleep(unsigned int ms)
{
	Sleep(ms);
}

#ifdef __cplusplus
}
#endif
#endif

#endif
