#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H

#ifdef _WIN32
#include <windows.h>

typedef HANDLE thread_t;
typedef CRITICAL_SECTION mutex_t;

typedef LPVOID void_t;
typedef DWORD  rthread_t;

// #define WAIT_OBJECT_0 1

static inline int thread_create(thread_t *thr, LPTHREAD_START_ROUTINE func, void *arg) {
    HANDLE h = CreateThread(NULL, 0, func, arg, 0, NULL);
    if (h == NULL) return -1;
    *thr = h;
    return 0;
}

#define thread_join(thr) WaitForSingleObject(thr, INFINITE)
#define thread_exit() ExitThread(0)

#define mutex_init(mtx) InitializeCriticalSection(mtx)
#define mutex_lock(mtx) EnterCriticalSection(mtx)
#define mutex_try_lock(mtx) WaitForSingleObject(mtx, 1000)
#define mutex_unlock(mtx) LeaveCriticalSection(mtx)
#define mutex_destroy(mtx) DeleteCriticalSection(mtx)

#else
#include <pthread.h>
typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;

typedef void * void_t;
typedef void * rthread_t;

#define thread_create(thr, func, arg) pthread_create(thr, NULL, func, arg)
#define thread_join(thr) pthread_join(thr, NULL)
#define thread_exit() pthread_exit(NULL)

#define mutex_init(mtx) pthread_mutex_init(mtx, NULL)
#define mutex_lock(mtx) pthread_mutex_lock(mtx)
#define mutex_unlock(mtx) pthread_mutex_unlock(mtx)
#define mutex_destroy(mtx) pthread_mutex_destroy(mtx)

#endif

#endif // THREAD_UTILS_H
