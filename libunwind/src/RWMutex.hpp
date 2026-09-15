//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//
// Abstract interface to shared reader/writer log, hiding platform and
// configuration differences.
//
//===----------------------------------------------------------------------===//

#ifndef __RWMUTEX_HPP__
#define __RWMUTEX_HPP__

#if defined(_WIN32)
#include <windows.h>
#elif defined(__MOLLENOS__) || defined(__VALI__)
#include <threads.h>
#elif !defined(_LIBUNWIND_HAS_NO_THREADS)
#include <pthread.h>
#if defined(__ELF__) && defined(_LIBUNWIND_LINK_PTHREAD_LIB)
#pragma comment(lib, "pthread")
#endif
#endif

namespace libunwind {

#if defined(_LIBUNWIND_HAS_NO_THREADS)

class _LIBUNWIND_HIDDEN RWMutex {
public:
  RWMutex() { mtx_init(&_lock, mtx_plain); }
  ~RWMutex() { mtx_destroy(&_lock); }

  bool lock_shared() { return mtx_lock(&_lock) == thrd_success; }
  bool unlock_shared() { return mtx_unlock(&_lock) == thrd_success; }
  bool lock() { return mtx_lock(&_lock) == thrd_success; }
  bool unlock() { return mtx_unlock(&_lock) == thrd_success; }

private:
  mtx_t _lock;
};

#elif defined(__MOLLENOS__) || defined(__VALI__)

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() { return true; }
  bool unlock_shared() { return true; }
  bool lock() { return true; }
  bool unlock() { return true; }
};

#elif defined(_WIN32)

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() {
    AcquireSRWLockShared(&_lock);
    return true;
  }
  bool unlock_shared() {
    ReleaseSRWLockShared(&_lock);
    return true;
  }
  bool lock() {
    AcquireSRWLockExclusive(&_lock);
    return true;
  }
  bool unlock() {
    ReleaseSRWLockExclusive(&_lock);
    return true;
  }

private:
  SRWLOCK _lock = SRWLOCK_INIT;
};

#elif !defined(LIBUNWIND_USE_WEAK_PTHREAD)

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() { return pthread_rwlock_rdlock(&_lock) == 0;  }
  bool unlock_shared() { return pthread_rwlock_unlock(&_lock) == 0; }
  bool lock() { return pthread_rwlock_wrlock(&_lock) == 0; }
  bool unlock() { return pthread_rwlock_unlock(&_lock) == 0; }

private:
  pthread_rwlock_t _lock = PTHREAD_RWLOCK_INITIALIZER;
};

#else

extern "C" int __attribute__((weak))
pthread_create(pthread_t *thread, const pthread_attr_t *attr,
               void *(*start_routine)(void *), void *arg);
extern "C" int __attribute__((weak))
pthread_rwlock_rdlock(pthread_rwlock_t *lock);
extern "C" int __attribute__((weak))
pthread_rwlock_wrlock(pthread_rwlock_t *lock);
extern "C" int __attribute__((weak))
pthread_rwlock_unlock(pthread_rwlock_t *lock);

// Calls to the locking functions are gated on pthread_create, and not the
// functions themselves, because the data structure should only be locked if
// another thread has been created. This is what similar libraries do.

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() {
    return !pthread_create || (pthread_rwlock_rdlock(&_lock) == 0);
  }
  bool unlock_shared() {
    return !pthread_create || (pthread_rwlock_unlock(&_lock) == 0);
  }
  bool lock() {
    return !pthread_create || (pthread_rwlock_wrlock(&_lock) == 0);
  }
  bool unlock() {
    return !pthread_create || (pthread_rwlock_unlock(&_lock) == 0);
  }

private:
  pthread_rwlock_t _lock = PTHREAD_RWLOCK_INITIALIZER;
};

#endif

} // namespace libunwind

#endif // __RWMUTEX_HPP__
