/**
 * @file
 * @brief Semaphore synchronization primitive implementation
 */
#ifndef CPEN333_THREAD_SEMAPHORE_H
#define CPEN333_THREAD_SEMAPHORE_H

#include <mutex>
#include <condition_variable>
#include <chrono>
#include "../util.h"

namespace cpen333 {
namespace thread {


/**
 * @brief A local semaphore synchronization primitive
 *
 * Used to protect access to a counted resource shared by multiple threads. Contains an integer whose value
 * is never allowed to fall below zero.
 * There are two main supported actions: wait(), which decrements the internal value, and notify() which increments the
 * value.  If the value of the semaphore is zero, then wait() will cause the thread to block until the value becomes
 * greater than zero.
 *
 * This implementation has no explicit maximum value.
 *
 * Adapted from http://stackoverflow.com/questions/4792449/c0x-has-no-semaphores-how-to-synchronize-threads
 *
 * @tparam Mutex mutex type
 * @tparam CondVar condition variable type
 */
template <typename Mutex, typename CondVar>
class basic_semaphore {
 public:
  /**
   * @brief Alias to condition variable native handle type
   */
  typedef typename CondVar::native_handle_type native_handle_type;

  /**
   * @brief Simple constructor that allows setting the initial count
   * @param count resource count (default 1)
   */
  explicit basic_semaphore(size_t count = 1) : mutex_(), cv_(), count_(count) {}

 private:
  // do not allow copying or moving
  basic_semaphore(const basic_semaphore&) DELETE_METHOD;
  basic_semaphore(basic_semaphore&&) DELETE_METHOD;
  basic_semaphore& operator=(const basic_semaphore&) DELETE_METHOD;
  basic_semaphore& operator=(basic_semaphore&&) DELETE_METHOD;

 public:

  /**
   * @brief Increments the semaphore value
   *
   * If the semaphore's value consequently becomes greater than zero, then one process or thread that is currently
   * blocked in a wait() operation will be woken up and will proceed.
   */
  void notify() {
    std::lock_guard<Mutex> lock(mutex_);
    ++count_;
    cv_.notify_one();
  }

  /**
   * @brief Waits for and decrements the semaphore value
   *
   * If the value is greater than zero, will decrement it and return immediately.  Otherwise, the thread will
   * block until it becomes possible to perform the decrement.
   */
  void wait() {
    std::unique_lock<Mutex> lock(mutex_);
    cv_.wait(lock, [&]{ return count_ > 0; });
    --count_;
  }

  /**
   * @brief Tries to wait for the semaphore, returning immediately
   *
   * If the value is greater than zero, will decrement the semaphore and return true.  Otherwise, will return false.
   *
   * @return true if decrement successful, false otherwise
   */
  bool try_wait() {
    std::lock_guard<Mutex> lock(mutex_);
    if (count_ > 0) {
      --count_;
      return true;
    }
    return false;
  }

  /**
   * @brief Tries to wait for the semaphore for up to a maximum timeout duration
   *
   * If the semaphore's value is greater than zero, will decrement it and return true immediately.  Otherwise,
   * will wait (blocking) up to a maximum relative timeout period.
   *
   * @tparam Rep time representation
   * @tparam Period timeout period type
   * @param timeout_duration maximum relative duration for waiting
   * @return true if semaphore successfully decremented, false if timed-out
   */
  template<class Rep, class Period>
  bool wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) {
    std::unique_lock<Mutex> lock{mutex_};
    bool finished = cv_.wait_for(lock, timeout_duration, [&]{ return count_ > 0; });
    if (finished) {
      --count_;
    }

    return finished;
  }

  /**
   * @brief Tries to wait for the semaphore for up to a maximum absolute time
   *
   * If the semaphore's value is greater than zero, will decrement it and return true immediately.  Otherwise,
   * will wait (blocking) up to a maximum relative timeout period.
   *
   * @tparam Clock timeout clock type
   * @tparam Duration timeout duration type
   * @param timeout_time maximum absolute time for waiting
   * @return true if semaphore successfully decremented, false if timed-out
   */
  template<class Clock, class Duration>
  bool wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) {
    std::unique_lock<Mutex> lock(mutex_);
    auto finished = cv_.wait_until(lock, timeout_time, [&]{ return count_ > 0; });
    if (finished) {
      --count_;
    }
    return finished;
  }

  /**
   * @brief Returns a native handle to the semaphore
   *
   * The native handle has a type aliased to semaphore::native_handle_type.
   *
   * @return native semaphore handle
   */
  native_handle_type native_handle() {
    return cv_.native_handle();
  }

 private:
  Mutex   mutex_;
  CondVar cv_;
  size_t  count_;
};

/**
 * @brief Alias to default semaphore implementation with std::mutex and std::condition_variable
 */
typedef basic_semaphore<std::mutex, std::condition_variable> semaphore;

/**
 * @brief Semaphore guard, similar to std::lock_guard
 *
 * Protects a semaphore's wait/notify using RAII to ensure all resources
 * are returned to the system
 * @tparam SemaphoreType basic semaphore that supports wait() and notify()
 */
template <typename SemaphoreType>
class semaphore_guard {
  SemaphoreType& sem_;
 public:
  /**
   * @brief Constructor, waits on semaphore
   * @param sem semaphore to wait on
   */
  semaphore_guard(SemaphoreType& sem) : sem_(sem) {
    sem_.wait();
  }

  /**
   * @brief Destructor, automatically notifies semaphore
   */
  ~semaphore_guard() {
    sem_.notify();
  }

  // do not allow copying or moving
 private:
  semaphore_guard(const semaphore_guard&);
  semaphore_guard(semaphore_guard&&);
  semaphore_guard& operator=(const semaphore_guard&);
  semaphore_guard& operator=(semaphore_guard&&);
  
};

} // thread
} // cpen333

#endif //CPEN333_THREAD_SEMAPHORE_H
