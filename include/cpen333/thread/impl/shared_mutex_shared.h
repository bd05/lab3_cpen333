/**
 * @file
 * @brief Implementation of a mutex with shared access that gives priority to shared use
 * (read-priority)
 */
#ifndef CPEN333_THREAD_SHARED_MUTEX_SHARED_H
#define CPEN333_THREAD_SHARED_MUTEX_SHARED_H

#include <mutex>
#include "../semaphore.h"

namespace cpen333 {
namespace thread {

namespace impl {

/**
 * @brief A read-preferring shared mutex implementation
 *
 * Shared mutex implementation based on the mutex/condition variable pattern.  Gives priority to
 * shared (read) access.
 *
 * See https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock#Using_a_condition_variable_and_a_mutex
 * for details
 */
class shared_mutex_shared {
 private:

  std::mutex mutex_;     // mutex for shared access
  cpen333::thread::semaphore global_; // global semaphore
  size_t shared_;          // shared counter object

 public:
  /**
   * Constructor, creates a read-preferring shared mutex
   */
  shared_mutex_shared() :  mutex_(), global_(1), shared_(0) { }

  // disable copy/move constructors
 private:
  shared_mutex_shared(const shared_mutex_shared &) DELETE_METHOD;
  shared_mutex_shared(shared_mutex_shared &&) DELETE_METHOD;
  shared_mutex_shared &operator=(const shared_mutex_shared &) DELETE_METHOD;
  shared_mutex_shared &operator=(shared_mutex_shared &&) DELETE_METHOD;

 public:

  /**
   * @copydoc cpen333::thread::impl::shared_mutex_exclusive::lock_shared()
   */
  void lock_shared() {
    // may hold both mutex_ and global_ until writes are complete
    std::lock_guard <std::mutex> lock(mutex_);
    if (++shared_ == 1) {
      global_.wait();  // "lock" semaphore preventing write access
    }
  }

  /**
   * @copydoc cpen333::thread::impl::shared_mutex_exclusive::try_lock_shared()
   */
  bool try_lock_shared() {
    std::unique_lock <std::mutex> lock(mutex_, std::defer_lock); // do not try yet
    if (!lock.try_lock()) {
      return false;
    }

    if (shared_ == 0) {
      bool success = global_.try_wait();  // "lock" semaphore preventing writes
      // only increment if successful
      if (!success) {
        return false;
      }
      shared_ = 1;
    } else {
      ++shared_;
    }
    return true;
  }

  /**
   * @copydoc cpen333::thread::impl::shared_mutex_exclusive::unlock_shared()
   */
  void unlock_shared() {
    std::lock_guard <std::mutex> lock(mutex_);
    if (--shared_ == 0) {
      global_.notify(); // "unlock" semaphore allowing writes
    }
  }

  /**
   * @copydoc cpen333::thread::impl::shared_mutex_exclusive::lock()
   */
  void lock() {
    global_.wait(); // lock semaphore
  }

  /**
   * @copydoc cpen333::thread::impl::shared_mutex_exclusive::try_lock()
   */
  bool try_lock() {
    return global_.try_wait();
  }

  /**
   * @copydoc cpen333::thread::impl::shared_mutex_exclusive::unlock()
   */
  void unlock() {
    global_.notify(); // unlock semaphore
  }

  /**
   * @copydoc cpen333::thread::impl::shared_mutex_exclusive::try_lock_for()
   */
  template<class Rep, class Period>
  bool try_lock_for(const std::chrono::duration <Rep, Period> &timeout_duration) {
    return try_lock_until(std::chrono::steady_clock::now() + timeout_duration);
  }

  /**
   * @copydoc cpen333::thread::impl::shared_mutex_exclusive::try_lock_until()
   */
  template<class Clock, class Duration>
  bool try_lock_until(const std::chrono::time_point <Clock, Duration> &timeout_time) {
    return global_.wait_until(timeout_time);
  }

  /**
   * @copydoc cpen333::thread::impl::shared_mutex_exclusive::try_lock_shared_for()
   */
  template<class Rep, class Period>
  bool try_lock_shared_for(const std::chrono::duration <Rep, Period> &timeout_duration) {
    return try_lock_shared_until(std::chrono::steady_clock::now() + timeout_duration);
  }

  /**
   * @copydoc cpen333::thread::impl::shared_mutex_exclusive::try_lock_shared_until()
   */
  template<class Clock, class Duration>
  bool try_lock_shared_until(const std::chrono::time_point <Clock, Duration> &timeout_time) {
    std::unique_lock <std::mutex> lock(mutex_, std::defer_lock); // do not try yet
    if (!lock.try_lock_until(timeout_time)) {
      return false;
    }

    // locked, so safe to read/write to count
    if (shared_ == 0) {
      bool success = global_.wait_until(timeout_time);  // "lock" semaphore
      if (!success) {
        return false;
      }
      shared_ = 1;
    } else {
      ++shared_;
    }
    return true;
  }
};

} // implementation

/**
 * @brief Alias for default shared mutex with shared (read) priority
 */
typedef impl::shared_mutex_shared shared_mutex_shared;

/**
 * @brief Alias for default shared timed mutex with shared (read) priority
 */
typedef impl::shared_mutex_shared shared_timed_mutex_shared;

} // thread
} // cpen333

#endif //CPEN333_THREAD_SHARED_MUTEX_SHARED_H
