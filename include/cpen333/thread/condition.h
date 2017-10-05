/**
 * @file
 * @brief Condition synchronization primitive
 */
#ifndef CPEN333_THREAD_CONDITION_H
#define CPEN333_THREAD_CONDITION_H

#include <mutex>
#include <condition_variable>

namespace cpen333 {
namespace thread {

/**
 * @brief Allows multiple threads to wait until the condition is set, acting like a gate.
 *
 * As long as the condition remains set, any threads that wait on the condition will immediately proceed.
 * The condition must manually be reset in order to cause threads/processes to wait until the next
 * time the condition is set.
 */
class condition {

 public:

  /**
 * @brief Creates the condition
 * @param value initial state, as either set (`true`) or reset (`false`)
 */
  condition(bool value = false) : open_(value), cv_(), mutex_() {}

 private:
  condition(const condition &) DELETE_METHOD;
  condition(condition &&) DELETE_METHOD;
  condition &operator=(const condition &) DELETE_METHOD;
  condition &operator=(condition &&) DELETE_METHOD;

 public:
  
  /**
   * @brief Waits until the condition is set
   *
   * Causes the current thread to block until the condition is set.
   * This condition will <em>not</em> exhibit spurious wake-ups.  A thread will be forced
   * to wait here indefinitely until the condition is set.
   */
  void wait() {
    // wait on condition variable until gate is open
    std::unique_lock<decltype(mutex_)> lock(mutex_);
    cv_.wait(lock, [&](){return open_;});
  }

  /**
   * @brief Waits for the condition to be set or for a timeout period to elapse
   *
   * Causes the current thread to block until the condition is set, or until
   * the specified timeout period elapses, whichever comes first.
   *
   * @tparam Rep timeout duration representation
   * @tparam Period timeout clock period
   * @param rel_time maximum relative time to wait for condition to be set
   * @return `true` if condition is set, `false` if timeout has elapsed without condition being set
   */
  template<class Rep, class Period>
  bool wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
    return wait_until(std::chrono::steady_clock::now()+rel_time);
  }

  /**
   * @brief Waits for the condition to be set or for a time-point to be reached
   *
   * Causes the current thread to block until the condition is set, or until the
   * specified timeout time has been reached, whichever comes first.
   *
   * @tparam Clock clock type
   * @tparam Duration clock duration type
   * @param timeout_time absolute timeout time
   * @return `true` if condition is set, `false` if timeout time has been reached without condition being set
   */
  template< class Clock, class Duration >
  bool wait_until( const std::chrono::time_point<Clock, Duration>& timeout_time ) {
    std::unique_lock<decltype(mutex_)> lock(mutex_, std::defer_lock);
    if (!lock.try_lock_until(timeout_time)) {
      return false;
    }
    return cv_.wait_until(lock, timeout_time, [&](){return open_;});
  }

  /**
   * @brief Sets the condition to `true` and wakes up threads
   *
   * Sets the condition's internal state to `true` and wakes up any threads waiting on
   * the condition.  The condition will remain in the `set` state until it is manually reset.
   */
  void notify() {
    // protect data and open gate
    {
      std::lock_guard<decltype(mutex_)> lock(mutex_);
      open_ = true;
    }
    // notify all that gate is now open
    cv_.notify_all();
  }

  /**
   * @brief Resets the condition
   *
   * Sets the condition's internal state to `false`.  This will call any future `wait` calls to block
   * until the condition is again notified.
   */
  void reset() {
    // protect data and close gate
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    open_ = false;
  }

 private:
  bool open_;  // gate
  std::condition_variable_any cv_;
  std::timed_mutex mutex_;
};

} // thread
} // cpen333

#endif //CPEN333_THREAD_CONDITION_H
