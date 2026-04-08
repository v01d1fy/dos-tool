#ifndef COMPAT_H
#define COMPAT_H

/*
 * Cross-platform threading compatibility layer.
 * On POSIX: uses pthreads directly.
 * On Windows: maps pthread API to Win32 threading primitives.
 */

#ifdef _WIN32

#include <windows.h>

/* --- Thread --- */
typedef HANDLE pthread_t;
typedef void *pthread_attr_t; /* unused, always NULL */

typedef struct {
    void *(*start_routine)(void *);
    void *arg;
} _thread_trampoline_t;

static DWORD WINAPI _thread_trampoline(LPVOID param) {
    _thread_trampoline_t *t = (_thread_trampoline_t *)param;
    void *(*fn)(void *) = t->start_routine;
    void *arg = t->arg;
    free(t);
    fn(arg);
    return 0;
}

static inline int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                                 void *(*start_routine)(void *), void *arg) {
    (void)attr;
    _thread_trampoline_t *t = (_thread_trampoline_t *)malloc(sizeof(_thread_trampoline_t));
    if (!t) return -1;
    t->start_routine = start_routine;
    t->arg = arg;
    *thread = CreateThread(NULL, 0, _thread_trampoline, t, 0, NULL);
    if (*thread == NULL) { free(t); return -1; }
    return 0;
}

static inline int pthread_join(pthread_t thread, void **retval) {
    (void)retval;
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}

/* --- Mutex --- */
typedef CRITICAL_SECTION pthread_mutex_t;
typedef void *pthread_mutexattr_t; /* unused */

static inline int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    (void)attr;
    InitializeCriticalSection(mutex);
    return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    DeleteCriticalSection(mutex);
    return 0;
}

#else
/* POSIX — just use real pthreads */
#include <pthread.h>
#endif

#endif /* COMPAT_H */
