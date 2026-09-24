// SPDX-License-Identifier: MIT
// Counts lock acquisitions in the test process (macOS). Linked as a dylib, whose __interpose
// section dyld applies to every image: calls from libc++ (std::mutex) are counted too.
#include <os/lock.h>
#include <pthread.h>
#include <stdatomic.h>

static _Atomic long gLocks;

long lockcount_get(void) { return atomic_load(&gLocks); }

static int countedLock(pthread_mutex_t* m) {
  atomic_fetch_add(&gLocks, 1);
  return pthread_mutex_lock(m);
}

static int countedTryLock(pthread_mutex_t* m) {
  atomic_fetch_add(&gLocks, 1);
  return pthread_mutex_trylock(m);
}

static void countedUnfairLock(os_unfair_lock_t l) {
  atomic_fetch_add(&gLocks, 1);
  os_unfair_lock_lock(l);
}

static int countedRdLock(pthread_rwlock_t* l) {
  atomic_fetch_add(&gLocks, 1);
  return pthread_rwlock_rdlock(l);
}

static int countedWrLock(pthread_rwlock_t* l) {
  atomic_fetch_add(&gLocks, 1);
  return pthread_rwlock_wrlock(l);
}

struct Interpose {
  const void* replacement;
  const void* original;
};

__attribute__((used)) static const struct Interpose kInterpose[] __attribute__((section("__DATA,__interpose"))) = {
    {(const void*)&countedLock, (const void*)&pthread_mutex_lock},
    {(const void*)&countedTryLock, (const void*)&pthread_mutex_trylock},
    {(const void*)&countedUnfairLock, (const void*)&os_unfair_lock_lock},
    {(const void*)&countedRdLock, (const void*)&pthread_rwlock_rdlock},
    {(const void*)&countedWrLock, (const void*)&pthread_rwlock_wrlock},
};
