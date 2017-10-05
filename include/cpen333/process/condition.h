/**
 * @file
 * @brief Condition synchronization primitive
 */
#ifndef CPEN333_PROCESS_CONDITION_H
#define CPEN333_PROCESS_CONDITION_H

/**
 * @brief Suffix to append to mutex names for uniqueness
 */
#define CONDITION_NAME_SUFFIX "_con"

/**
 * @brief Magic number of testing initialization
 */
#define CONDITION_INITIALIZED 0x87621232

#include <string>
#include <chrono>

#include "named_resource.h"
#include "impl/condition_base.h"

namespace cpen333 {
namespace process {


/**
 * @brief Allows multiple processes to wait until the condition is set, acting like a gate.
 *
 * A named synchronization primitive that allows multiple threads and processes to wait until the
 * condition is notified as being set.  As long
 * as the condition remains set, any threads that wait on the condition will immediately proceed.
 * The condition must manually be reset in order to cause threads/processes to wait until the next
 * time the condition is set.
 */
class condition : private condition_base, public virtual named_resource {
 public:

  /**
   * @brief Creates or connects to the named condition
   * @param name name identifier for creating or connecting to an existing inter-process condition
   * @param value initial state, as either set (`true`) or reset (`false`)
   */
  condition(const std::string &name, bool value = false) :
      condition_base(name + std::string(CONDITION_NAME_SUFFIX)),
      storage_(name + std::string(CONDITION_NAME_SUFFIX)),
      mutex_(name + std::string(CONDITION_NAME_SUFFIX)) {

    // initialize data if we need to
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    if (storage_->initialized != CONDITION_INITIALIZED) {
      storage_->value = value;
      storage_->initialized = CONDITION_INITIALIZED;
    }

  }

  /**
   * @brief Waits until the condition is set
   *
   * Causes the current thread to block until the condition is set.
   * This condition will <em>not</em> exhibit spurious wake-ups.  A thread will be forced
   * to wait here indefinitely until the condition is set.
   */
  void wait() {
    std::unique_lock<cpen333::process::mutex> lock(mutex_);
    while (!storage_->value) {
      condition_base::wait(lock, false, std::chrono::steady_clock::now());
    }
  }

  /**
   * @brief Waits for the condition to be set or for a timeout period to elapse
   *
   * Causes the current thread to block until the condition is set, or until
   * the specified timeout period elapses, whichever comes first.
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
    std::unique_lock<cpen333::process::mutex> lock(mutex_, std::defer_lock);
    if (!lock.try_lock_until(timeout_time)) {
      return false;
    }
    while (!storage_->value) {
      if (condition_base::wait_until(lock, timeout_time)) {
        return storage_->value;
      }
      if (std::chrono::steady_clock::now() > timeout_time) {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief Sets the condition to `true` and wakes up threads
   *
   * Sets the condition's internal state to `true` and wakes up any threads waiting on
   * the condition.  The condition will remain in the `set` state until it is manually reset.
   */
  void notify() {
    // open gate
    {
      std::lock_guard<cpen333::process::mutex> lock(mutex_);
      storage_->value = true;
    }
    // wake everyone up to check the new value
    condition_base::notify(true);
  }

  /**
   * @brief Resets the condition
   *
   * Sets the condition's internal state to `false`.  This will call any future `wait` calls to block
   * until the condition is again notified.
   */
  void reset() {
    // reset the value
    std::lock_guard<cpen333::process::mutex> lock(mutex_);
    storage_->value = false;
  }

  virtual bool unlink() {
    bool b1 = condition_base::unlink();
    bool b2 = storage_.unlink();
    bool b3 = mutex_.unlink();
    return b1 && b2 && b3;
  }

  /**
   * @brief Unlinks any condition with the provided name
   *
   * Allows a name to be freed without needing to create a new condition resource.  This is
   * useful for clean-up of previously terminated processes that failed to release the resource
   * properly.
   *
   * @param name name of condition resource
   * @return `true` if unlink successful, `false` if an error occurred or if not supported
   */
  static bool unlink(const std::string& name) {
    bool b1 = cpen333::process::condition_base::unlink(name + std::string(CONDITION_NAME_SUFFIX));
    bool b2 = cpen333::process::shared_object<shared_data>::unlink(name + std::string(CONDITION_NAME_SUFFIX));
    bool b3 = cpen333::process::mutex::unlink(name + std::string(CONDITION_NAME_SUFFIX));
    return b1 && b2 && b3;
  }

 private:
  struct shared_data {
    bool value;
    size_t initialized;
  };
  cpen333::process::shared_object<shared_data> storage_;
  cpen333::process::mutex mutex_;

};

} // process
} // cpen333

// undefine local macros
#undef CONDITION_NAME_SUFFIX
#undef CONDITION_INITIALIZED

#endif //CPEN333_PROCESS_CONDITION_H
