/**
 * @file
 * @brief Inter-process shared-access mutex implementations, allowing for multi-read/write access
 */
#ifndef CPEN333_PROCESS_SHARED_MUTEX_H
#define CPEN333_PROCESS_SHARED_MUTEX_H


#include "mutex.h"
#include "semaphore.h"
#include "shared_memory.h"
#include "impl/shared_mutex_exclusive.h"
#include "impl/shared_mutex_fair.h"
#include "impl/shared_mutex_shared.h"

#if __cplusplus >= 201402L
#include <shared_mutex>
#else
#include "impl/shared_lock.h"
#endif

namespace cpen333 {
namespace process {

/**
 * @brief Default shared mutex that uses fair priority
 */
typedef impl::shared_mutex_fair shared_mutex;

/**
 * @brief Default shared timed mutex uses fair priority
 */
typedef impl::shared_mutex_fair shared_timed_mutex;

/**
 * @class cpen333::process::shared_mutex
 * @brief An inter-process mutual exclusion synchronization primitive allowing for shared access
 *
 * Used to protect access to a resource shared by multiple processes, allowing shared
 * `read' access, mirroring std::shared_mutex in c++17.  This is an alias to
 * cpen333::process::impl::shared_mutex_fair.
 */

/**
 * @class cpen333::process::shared_timed_mutex
 * @brief An inter-process mutual exclusion synchronization primitive allowing for shared access with timeouts
 *
 * Used to protect access to a resource shared by multiple processes, allowing shared
 * `read' access, mirroring std::shared_timed_mutex in c++14.  This is an alias to
 * cpen333::process::impl::shared_mutex_fair.
 */

/**
 * @brief Shared lock guard, similar to std::lock_guard but for shared locks
 * @tparam SharedMutex  shared mutex type
 */
template<typename SharedMutex>
class shared_lock_guard {
  SharedMutex& mutex_;
 public:

  /**
   * @brief Construct the shared lock guard
   * @param mutex  mutex to lock on constructions
   */
  shared_lock_guard(SharedMutex& mutex) : mutex_(mutex) {
    mutex_.lock_shared();
  }

 private:
  // disable copy/move constructors
  shared_lock_guard(const shared_lock_guard&) DELETE_METHOD;
  shared_lock_guard(shared_lock_guard&&) DELETE_METHOD;
  shared_lock_guard& operator=(const shared_lock_guard&) DELETE_METHOD;
  shared_lock_guard& operator=(shared_lock_guard&&) DELETE_METHOD;

 public:
  /**
   * @brief Destructor, unlock shared mutex
   */
  ~shared_lock_guard() {
    mutex_.unlock_shared();
  }
};

} // process
} // cpen333

#endif //CPEN333_PROCESS_SHARED_MUTEX_H
