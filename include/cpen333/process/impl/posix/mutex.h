/**
 * @file
 * @brief POSIX implementation of an inter-process named mutex
 *
 * Uses a binary POSIX semaphore (rather than pthreads, which doesn't seem to easily support named mutexes).
 */

#ifndef CPEN333_PROCESS_MUTEX_POSIX_H
#define CPEN333_PROCESS_MUTEX_POSIX_H

/**
 * @brief Suffix to append to mutex names for uniqueness
 */
#define MUTEX_NAME_SUFFIX "_mux"

#include <string>
#include <chrono>
#include <thread>

#include "../../../util.h"
#include "semaphore.h"
#include "../named_resource_base.h"

namespace cpen333 {
namespace process {
namespace posix {

/**
 * @brief Inter-process named mutual exclusion primitive
 *
 * Used to limit resource access to one thread at a time
 *
 * Based on a named semaphore.  Unlike a true mutex, this implementation does NOT
 * enforce that the same thread unlock the mutex.  However, in practice, it should
 * be treated as a true mutex by using std::lock_guard or std::unique_lock.
 *
 * This mutex has KERNEL PERSISTENCE, meaning if not unlink()-ed, will continue to exist in its current state
 * until the system is shut down (persisting beyond the life of the initiating program)
 */
class mutex : public impl::named_resource_base {
 public:
  /**
   * @brief Alias to the underlying native mutex handle
   */
  using native_handle_type = semaphore::native_handle_type;

  /**
   * @brief Constructs or connects to the named mutex
   * @param name  identifier for creating or connecting to an existing inter-process mutex
   */
  mutex(const std::string& name) :
    impl::named_resource_base{name + std::string(MUTEX_NAME_SUFFIX)},
    semaphore_{name + std::string(MUTEX_NAME_SUFFIX), 1}
    // thread_{}
  {}

  /**
   * @brief Locks the mutex
   *
   * This will block the current thread until the mutex is available to be locked, preventing multiple
   * threads from accessing a protected resource.
   */
  void lock() {
    semaphore_.wait();
    // thread_ = std::this_thread::get_id();  // set id
  }

  /**
   * @brief Tries to lock the mutex
   *
   * This will attempt to lock the mutex, returning immediately.
   *
   * @return true if locked successfully, false if already locked
   */
  bool try_lock() {
    bool success = semaphore_.try_wait();
    //    if (success) {
    //      thread_ = std::this_thread::get_id();
    //    }
    return success;
  }

  /**
   * @brief Tries to lock the mutex until a relative time has elapsed
   *
   * Blocks until the specified timeout duration has elapsed or the lock is acquired, whichever comes first.
   *
   * @tparam Rep timer representation
   * @tparam Period timeout period type
   * @param timeout_duration maximum relative time to block for
   * @return true if lock was acquired successfully, false otherwise
   */
  template< class Rep, class Period >
  bool try_lock_for( const std::chrono::duration<Rep,Period>& timeout_duration ) {
    bool success = semaphore_.wait_for(timeout_duration);
    //    if (success) {
    //      thread_ = std::this_thread::get_id();
    //    }
    return success;
  }

  /**
   * @brief Tries to lock the mutex until an absolute time has passed
   * @tparam Clock timeout clock type
   * @tparam Duration timeout duration type
   * @param timeout_time absolute timeout time
   * @return true if the lock was acquired successfully, false otherwise
   */
  template< class Clock, class Duration >
  bool try_lock_until( const std::chrono::time_point<Clock,Duration>& timeout_time ) {
    bool success = semaphore_.wait_until(timeout_time);
    //    if (success) {
    //      thread_ = std::this_thread::get_id();
    //    }
    return success;
  }

  /**
   * @brief Unlocks the mutex
   *
   * Puts the mutex in a state to be relocked, freeing the protected resource to be used by
   * another (or the same) thread
   */
  void unlock() {
    //    if (thread_ != std::this_thread::get_id()) {
    //      cpen333::perror(std::string("Cannot unlock mutex locked by different thread, ") + name());
    //      return;
    //    }
    semaphore_.notify();
    //  thread_ = {};  // clear thread id
  }

  /**
   * @brief Returns a native handle
   *
   * In this case, a POSIX sem_t
   *
   * @return native handle to underlying mutex
   */
  native_handle_type native_handle() {
    return semaphore_.native_handle();
  }

  bool unlink() {
    return semaphore_.unlink();
  }

  /**
   * @copydoc cpen333::process::named_resource::unlink(const std::string&)
   */
  static bool unlink(const std::string& name) {
   return cpen333::process::posix::semaphore::unlink(name + std::string(MUTEX_NAME_SUFFIX));
  }

 private:
  cpen333::process::posix::semaphore semaphore_;  // binary semaphore
  // XXX Tracking ID fails if multiple threads are actually using this mutex simultaneously
  //     We *could* prevent this by using yet another mutex, but that would make this much slower
  // std::thread::id thread_;  // for tracking thread id

};

} // native implementation

/**
 * @brief Alias to POSIX implementation of inter-process mutex
 */
using mutex = posix::mutex;

/**
 * @brief Alias to POSIX implementation of inter-process mutex allowing timed waits
 */
using timed_mutex = posix::mutex;

} // process
} // cpen333

// undef local macros
#undef MUTEX_NAME_SUFFIX

#endif //CPEN333_PROCESS_MUTEX_POSIX_H
