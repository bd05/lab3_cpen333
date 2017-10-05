/**
 * @file
 * @brief Event synchronization primitive
 */
#ifndef CPEN333_PROCESS_EVENT_H
#define CPEN333_PROCESS_EVENT_H

/**
 * @brief Suffix to append to event names for uniqueness
 */
#define EVENT_NAME_SUFFIX "_ev"

#include <string>
#include <chrono>
#include <condition_variable>

#include "named_resource.h"
#include "impl/condition_base.h"
#include "mutex.h"

namespace cpen333 {
namespace process {

/**
 * @brief Event primitive, acting like a turnstile
 *
 * A named synchronization primitive that allows multiple threads and processes to wait until the
 * event is notified.  The notifier can either `notify_one()` to let a single waiter through (if any),
 * or `notify_all()` to let everyone currently waiting through.
 *
 */
class event : private condition_base, public virtual named_resource {
 public:

  /**
   * @brief Creates or connects to a named event
   * @param name name identifier for creating or connecting to an existing inter-process event
   */
  event(const std::string &name) :
      condition_base(name + std::string(EVENT_NAME_SUFFIX)),
      mutex_(name + std::string(EVENT_NAME_SUFFIX)) {}

  /**
   * @brief Waits for the event to be triggered
   *
   * Causes the current thread to block until either `notify_all()` is called, or `notify_one()` and this thread
   * happens to be the one awoken.  Note that order of wakes is system-dependent, and not necessarily in order of
   * arrival.  This event will <em>not</em> exhibit spurious wake-ups.  A thread will be forced
   * to wait here indefinitely until the event is triggered.
   */
  void wait() {
    std::unique_lock<decltype(mutex_)> lock(mutex_);
    condition_base::wait(lock);
  }

  /**
   * @brief Waits for the event to be triggered or for a timeout period to elapse
   *
   * Causes the current thread to block until `notify_all()` is called, or `notify_one()` and this thread
   * happens to be the one awoken, or until the specified timeout period elapses, whichever comes first.
   * @tparam Rep timeout duration representation
   * @tparam Period timeout clock period
   * @param rel_time maximum relative time to wait for condition to be set
   * @return `true` if event is triggered, `false` if timeout has elapsed without event being triggered
   */
  template<class Rep, class Period>
  bool wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
    return wait_until(std::chrono::steady_clock::now()+rel_time);
  }

  /**
   * @brief Waits for the event to be triggered or for a time-point to be reached
   *
   * Causes the current thread to block until `notify_all()` is called, or `notify_one()` and this thread
   * happens to be the one awoken, or until the specified timeout time has been reached, whichever comes first.
   *
   * @tparam Clock clock type
   * @tparam Duration clock duration type
   * @param timeout_time absolute timeout time
   * @return `true` if event is triggered, `false` if timeout time has been reached without event
   */
  template< class Clock, class Duration >
  bool wait_until( const std::chrono::time_point<Clock, Duration>& timeout_time ) {
    std::unique_lock<cpen333::process::mutex> lock(mutex_, std::defer_lock);
    if (!lock.try_lock_until(timeout_time)) {
      return false;
    }
    return condition_base::wait_until(lock, timeout_time);
  }

  /**
   * @brief Wake a single thread waiting for the event to be triggered
   *
   * Note that the choice of thread to be awoken is up to the underlying system.  Threads are not necessarily
   * notified in order of arrival.
   */
  void notify_one() {
    condition_base::notify_one();
  }

  /**
   * @brief Wake all threads waiting for the event to be triggered
   *
   * All threads waiting for the event will be awoken and will continue.
   */
  void notify_all() {
    condition_base::notify_all();
  }

  virtual bool unlink() {
    bool b1 = condition_base::unlink();
    bool b2 = mutex_.unlink();
    return (b1 && b2);
  }

  /**
   * @copydoc cpen333::process::named_resource::unlink(const std::string&)
   */
  static bool unlink(const std::string& name) {
    bool b1 = cpen333::process::condition_base::unlink(name + std::string(EVENT_NAME_SUFFIX));
    bool b2 = cpen333::process::mutex::unlink(name + std::string(EVENT_NAME_SUFFIX));
    return b1 && b2;
  }

 private:
  cpen333::process::mutex mutex_;

};

} // process
} // cpen333

// undef local macros
#undef EVENT_NAME_SUFFIX

#endif //CPEN333_PROCESS_EVENT_H
