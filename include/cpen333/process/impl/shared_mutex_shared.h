/**
 * @file
 * @brief Implementation of an inter-process mutex with shared access that gives priority to shared use
 * (read-priority)
 */

#ifndef CPEN333_PROCESS_SHARED_MUTEX_SHARED_H
#define CPEN333_PROCESS_SHARED_MUTEX_SHARED_H

/**
 * @brief Name suffix for internals to guarantee uniqueness
 */
#define SHARED_MUTEX_SHARED_NAME_SUFFIX "_sms"
/**
 * @brief Magic number for testing initialization
 */
#define SHARED_MUTEX_SHARED_INITIALIZED 0x98271238

#include "../mutex.h"
#include "../semaphore.h"
#include "../shared_memory.h"
#include "../named_resource.h"

namespace cpen333 {
namespace process {

namespace impl {

/**
 * @brief A read-preferring inter-process shared mutex implementation
 *
 * Inter-process shared mutex implementation based on the mutex/condition variable pattern.  Gives priority to
 * shared (read) access.
 *
 * See https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock#Using_a_condition_variable_and_a_mutex
 * for details
 */
class shared_mutex_shared : public virtual named_resource {
 private:

  struct shared_data {
    size_t shared;
    size_t initialized;
  };

  cpen333::process::mutex shared_;     // mutex for shared access
  cpen333::process::semaphore global_; // global semaphore
  cpen333::process::shared_object<shared_data> count_;     // shared counter object

 public:
  /**
   * Constructor, creates or connects to a read-preferring shared mutex
   * @param name identifier for creating or connecting to an existing inter-process shared mutex
   */
  shared_mutex_shared(const std::string &name) :
      shared_(name + std::string(SHARED_MUTEX_SHARED_NAME_SUFFIX)),
      global_(name + std::string(SHARED_MUTEX_SHARED_NAME_SUFFIX), 1),
      count_(name + std::string(SHARED_MUTEX_SHARED_NAME_SUFFIX)) {

    // initialize storage
    std::lock_guard<decltype(shared_)> lock(shared_);
    if (count_->initialized != SHARED_MUTEX_SHARED_INITIALIZED) {
      count_->shared = 0;
      count_->initialized = SHARED_MUTEX_SHARED_INITIALIZED;
    }
  }

 private:
  // disable copy/move constructors
  shared_mutex_shared(const shared_mutex_shared &);
  shared_mutex_shared(shared_mutex_shared &&);
  shared_mutex_shared &operator=(const shared_mutex_shared &);
  shared_mutex_shared &operator=(shared_mutex_shared &&);

 public:

  /**
   * @copydoc cpen333::process::impl::shared_mutex_exclusive::lock_shared()
   */
  void lock_shared() {

    // may hold both shared_ and global_ until writes are complete
    std::lock_guard <cpen333::process::mutex> lock(shared_);
    if (++(count_->shared) == 1) {
      global_.wait();  // "lock" semaphore preventing write access
    }
  }

  /**
   * @copydoc cpen333::process::impl::shared_mutex_exclusive::try_lock_shared()
   */
  bool try_lock_shared() {
    std::unique_lock <cpen333::process::mutex> lock(shared_, std::defer_lock); // do not try yet
    if (!lock.try_lock()) {
      return false;
    }

    if (count_->shared == 0) {
      bool success = global_.try_wait();  // "lock" semaphore preventing writes
      // only increment if successful
      if (!success) {
        return false;
      }
      count_->shared = 1;
    } else {
      ++(count_->shared);
    }
    return true;
  }

  /**
   * @copydoc cpen333::process::impl::shared_mutex_exclusive::unlock_shared()
   */
  void unlock_shared() {
    std::lock_guard <cpen333::process::mutex> lock(shared_);
    if (--(count_->shared) == 0) {
      global_.notify(); // "unlock" semaphore allowing writes
    }
  }

  /**
   * @copydoc cpen333::process::impl::shared_mutex_exclusive::lock()
   */
  void lock() {
    global_.wait(); // lock semaphore
  }

  /**
   * @copydoc cpen333::process::impl::shared_mutex_exclusive::try_lock()
   */
  bool try_lock() {
    return global_.try_wait();
  }

  /**
   * @copydoc cpen333::process::impl::shared_mutex_exclusive::unlock()
   */
  void unlock() {
    global_.notify(); // unlock semaphore
  }

  /**
   * @copydoc cpen333::process::impl::shared_mutex_exclusive::try_lock_for()
   */
  template<class Rep, class Period>
  bool try_lock_for(const std::chrono::duration <Rep, Period> &timeout_duration) {
    return try_lock_until(std::chrono::steady_clock::now() + timeout_duration);
  }

  /**
   * @copydoc cpen333::process::impl::shared_mutex_exclusive::try_lock_until()
   */
  template<class Clock, class Duration>
  bool try_lock_until(const std::chrono::time_point <Clock, Duration> &timeout_time) {
    return global_.wait_until(timeout_time);
  }

  /**
   * @copydoc cpen333::process::impl::shared_mutex_exclusive::try_lock_shared_for()
   */
  template<class Rep, class Period>
  bool try_lock_shared_for(const std::chrono::duration <Rep, Period> &timeout_duration) {
    return try_lock_shared_until(std::chrono::steady_clock::now() + timeout_duration);
  }

  /**
   * @copydoc cpen333::process::impl::shared_mutex_exclusive::try_lock_shared_until()
   */
  template<class Clock, class Duration>
  bool try_lock_shared_until(const std::chrono::time_point <Clock, Duration> &timeout_time) {
    std::unique_lock <cpen333::process::mutex> lock(shared_, std::defer_lock); // do not try yet
    if (!lock.try_lock_until(timeout_time)) {
      return false;
    }

    // locked, so safe to read/write to count
    if (count_->shared == 0) {
      bool success = global_.wait_until(timeout_time);  // "lock" semaphore
      if (!success) {
        return false;
      }
      count_->shared = 1;
    } else {
      ++(count_->shared);
    }
    return true;
  }

  bool unlink() {
    bool b1 = shared_.unlink();
    bool b2 = global_.unlink();
    bool b3 = count_.unlink();
    return (b1 && b2 && b3);
  }

  /**
   * @copydoc cpen333::process::named_resource::unlink(const std::string&)
   */
  static bool unlink(const std::string& name) {
    bool b1 = cpen333::process::mutex::unlink(name + std::string(SHARED_MUTEX_SHARED_NAME_SUFFIX));
    bool b2 = cpen333::process::semaphore::unlink(name + std::string(SHARED_MUTEX_SHARED_NAME_SUFFIX));
    bool b3 = cpen333::process::shared_object<shared_data>::unlink(name + std::string(SHARED_MUTEX_SHARED_NAME_SUFFIX));
    return (b1 && b2 && b3);
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

} // process
} // cpen333

// undef local macros
#undef SHARED_MUTEX_SHARED_NAME_SUFFIX
#undef SHARED_MUTEX_SHARED_INITIALIZED

#endif //CPEN333_PROCESS_SHARED_MUTEX_SHARED_H
