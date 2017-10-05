/**
 * @file
 * @brief Condition variable synchronization primitive
 */
#ifndef CPEN333_PROCESS_CONDITION_VARIABLE_H
#define CPEN333_PROCESS_CONDITION_VARIABLE_H

/**
 * @brief Suffix to append to mutex names for uniqueness
 */
#define CONDITION_VARIABLE_NAME_SUFFIX "_cv"

#include <string>
#include <chrono>

#include "impl/condition_base.h"
#include "named_resource.h"

namespace cpen333 {
namespace process {

/**
 * @brief Allows multiple process to wait for a condition to become `true` depending on a shared variable
 *
 * The condition_variable is a named synchronization primitive that can be used to block a thread,
 * or multiple threads, until some thread or process both modifies a shared variable (the condition),
 * and notifies the condition_variable.
 *
 * The thread that modifies the variable needs to
 * <ul>
 * <li>acquire a cpen333::process::mutex (typically via std::lock_guard)</li>
 * <li>perform the modification while the lock is held</li>
 * <li>execute notify_one or notify_all on the condition_variable (does not need to be under lock)</li>
 * </ul>
 * Any thread waiting under the variable needs to
 * <ul>
 * <li>acquire a std::unique_lock<cpen333::process::mutex> on the same mutex that protects the shared variable</li>
 * <li>execute wait, wait_for, or wait_until</li>
 * </ul>
 * The wait operations atomically release the mutex and suspend the execution of the thread.  When the condition
 * variable is notified, or a timeout expires, the thread is awakened and the mutex is atomically reacquired.
 * Unlike std::condition_variable, this implementation is not prone to spurious wake-ups.
 */
class condition_variable : public condition_base, public virtual named_resource {

 public:
  /**
   * @brief Creates or connects to a named condition variable
   * @param name name identifier for creating or connecting to an existing inter-process condition_variable
   */
  condition_variable(const std::string &name) :
      condition_base(name + std::string(CONDITION_VARIABLE_NAME_SUFFIX)) {}

  /**
   * @brief Waits until the condition_variable is notified
   *
   * The current thread will block until the shared condition_variable is explicitly notified.
   * The lock must be acquired before the wait command.  As the thread is suspended, it will
   * atomically release the lock.  When it is awoken, the thread will atomically re-acquire
   * the lock.
   *
   * @param lock lock that protects the shared condition information.  All waiting threads
   * must lock the same shared mutex.
   */
  void wait(std::unique_lock<cpen333::process::mutex>& lock) {
    return condition_base::wait(lock);
  }

  /**
   * @brief Waits for the condition_variable to be notified or for a timeout period to elapse
   *
   * Causes the current thread to block until the condition_variable is notified, or until
   * the specified timeout period elapses, whichever comes first.  As the thread is suspended, it will
   * atomically release the lock.  When it is awoken, the thread will atomically re-acquire
   * the lock.
   *
   * @tparam Rep timeout duration representation
   * @tparam Period timeout clock period
   * @param lock lock that protects the shared condition information.  All waiting threads
   * must lock the same shared mutex.
   * @param rel_time maximum relative time to wait for condition to be set
   * @return true if successful, false if timed out
   */
  template<class Rep, class Period>
  bool wait_for( std::unique_lock<cpen333::process::mutex>& lock,
                           const std::chrono::duration<Rep, Period>& rel_time) {
    return condition_base::wait_for(lock, rel_time);
  }

  /**
   * @brief Waits for the condition_variable to be notified or for a time-point to be reached.
   *
   * Causes the current thread to block until the condition_variable is notified, or until
   * the specified timeout time has been reached, whichever comes first. As the thread is suspended, it will
   * atomically release the lock.  When it is awoken, the thread will atomically re-acquire
   * the lock.
   *
   * @tparam Clock clock type
   * @tparam Duration duration type
   * @param lock lock that protects the shared condition information.  All waiting threads
   * must lock the same shared mutex.
   * @param timeout_time absolute timeout time
   * @return `true` if condition_variable is notified, `false` if timeout time has been reached
   * without condition_variable being notified
   */
  template<class Clock, class Duration >
  bool wait_until( std::unique_lock<cpen333::process::mutex>& lock,
                   const std::chrono::time_point<Clock, Duration>& timeout_time ) {
    return condition_base::wait(lock, timeout_time);
  }

  /**
   * @brief Waits for the condition_variable to be notified and the predicate `pred()` to evaluate
   * to `true`.
   *
   * Causes the current thread to block until the condition_variable is notified and the predicate
   * `pred()` evaluates to true. As the thread is suspended, it will
   * atomically release the lock.   When it is awoken, the thread will atomically re-acquire
   * the lock and check the predicate.  If the predicate still evaluates to `false`, the thread
   * will again atomically release the lock and suspend.
   *
   * @tparam Predicate predicate type, which should have signature `bool operator()`
   * @param lock lock that protects the shared condition information used in `pred`.  All waiting threads
   * must lock the same shared mutex.
   * @param pred predicate to evaluate.  If `pred()` returns `false`, waiting will continue.
   */
  template<typename Predicate>
  void wait(std::unique_lock<cpen333::process::mutex>& lock, Predicate pred) {
    while (!pred()) {
      condition_base::wait(lock, false, std::chrono::steady_clock::now());
    }
  }

  /**
   * @brief Waits for the condition_variable to be notified and the predicate `pred()` to evaluate
   * to `true`, or for a timeout to elapse.
   *
   * Causes the current thread to block until the condition_variable is notified and the predicate
   * `pred()` evaluates to `true`, or until the specified time elapses, whichever happens first.
   * As the thread is suspended, it will atomically release the lock.   When it is awoken, the thread
   * will atomically re-acquire the lock and check the predicate.  If the predicate still evaluates
   * to `false` and if the timeout has not yet elapsed, the thread will again atomically release the lock
   * and suspend.
   *
   * @tparam Rep duration representation
   * @tparam Period duration's clock period
   * @tparam Predicate predicate type, which should have signature `bool operator()`
   * @param lock lock that protects the shared condition information used in `pred`.  All waiting threads
   * must lock the same shared mutex.
   * @param rel_time maximum relative time to wait for condition variable
   * @param pred predicate to evaluate.  If `pred()` returns `false`, waiting will continue.
   */
  template<class Rep, class Period, class Predicate>
  bool wait_for( std::unique_lock<cpen333::process::mutex>& lock,
                 const std::chrono::duration<Rep, Period>& rel_time,
                 Predicate pred) {
    return wait_until(lock, std::chrono::steady_clock::now()+rel_time, pred);
  }

  /**
   * @brief Waits for the condition_variable to be notified and the predicate `pred()` to evaluate
   * to `true`, or for a timeout time to be reached.
   *
   * Causes the current thread to block until the condition_variable is notified and the predicate
   * `pred()` evaluates to `true`, or until the specified timeout time has been reached.
   * As the thread is suspended, it will atomically release the lock.   When it is awoken, the thread
   * will atomically re-acquire the lock and check the predicate.  If the predicate still evaluates
   * to `false` and if the timeout time has not yet been reached, the thread will again atomically release the lock
   * and suspend.
   *
   * @tparam Clock timeout clock type
   * @tparam Duration timeout clock duration
   * @tparam Predicate predicate type, which should have signature `bool operator()`
   * @param lock lock that protects the shared condition information used in `pred`.  All waiting threads
   * must lock the same shared mutex.
   * @param timeout_time absolute timeout time
   * @param pred predicate to evaluate.  If `pred()` returns `false`, waiting will continue.
   */
  template< class Clock, class Duration, class Predicate >
  bool wait_until( std::unique_lock<cpen333::process::mutex>& lock,
                   const std::chrono::time_point<Clock, Duration>& timeout_time,
                   Predicate pred ) {
    while (!pred()) {
      if (condition_base::wait_until(lock, timeout_time)) {
        return pred();
      }
    }
    return true;
  }

  bool unlink() {
    return condition_base::unlink();
  }

  /**
   * @brief Unlinks any condition variable with the provided name
   *
   * Allows a name to be freed without needing to create a new condition_variable resource.  This is
   * useful for clean-up of previously terminated processes that failed to release the resource
   * properly.
   *
   * @param name name of condition_variable resource
   * @return `true` if unlink successful, `false` if an error occurred or if not supported
   */
  static bool unlink(const std::string& name) {
    return cpen333::process::condition_base::unlink(name + std::string(CONDITION_VARIABLE_NAME_SUFFIX));
  }

};

} // process
} // cpen333

// undefine local macros
#undef CONDITION_VARIABLE_NAME_SUFFIX

#endif //CPEN333_PROCESS_CONDITION_VARIABLE_H
