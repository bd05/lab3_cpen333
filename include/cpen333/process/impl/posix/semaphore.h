/**
 * @file
 * @brief POSIX implementation of an inter-process named semaphore
 *
 * Uses a POSIX semaphore
 */
#ifndef CPEN333_PROCESS_POSIX_SEMAPHORE_H
#define CPEN333_PROCESS_POSIX_SEMAPHORE_H

/**
 * @brief Suffix to append to semaphore names for uniqueness
 */
#define SEMAPHORE_NAME_SUFFIX "_sem"

#include <string>
#include <chrono>
#include <thread>       // for yield
#include <fcntl.h>      // for O_* constants
#include <sys/stat.h>   // for mode constants
#include <semaphore.h>

#include "../../../util.h"
#include "../named_resource_base.h"

#ifdef APPLE
#include "../osx/sem_timedwait.h" // missing sem_timedwait functionality
#endif

namespace cpen333 {
namespace process {
namespace posix {

/**
 * @brief Inter-process named semaphore primitive
 *
 * Used to limit access to a number of resources.  Contains an integer whose value is never allowed to fall below zero.
 * There are two main supported actions: wait(), which decrements the internal value, and notify() which increments the
 * value.  If the value of the semaphore is zero, then wait() will cause the thread to block until the value becomes
 * greater than zero.
 *
 * This implementation has no explicit maximum value
 *
 * This semaphore has KERNEL PERSISTENCE, meaning if not unlink()-ed, will continue to exist in its current state
 * until the system is shut down (persisting beyond the life of the initiating program)
 *
 */
class semaphore : public impl::named_resource_base {
 public:
  /**
   * @brief Alias to native handle type for semaphore
   *
   * In this case, a POSIX sem_t*
   */
  using native_handle_type = sem_t*;

  /**
   * @brief Constructs or connects to a named semaphore
   *
   * @param name  identifier for creating or connecting to an existing inter-process semaphore
   * @param value initial value (defaults to 1)
   */
  semaphore(const std::string& name, size_t value = 1) :
      impl::named_resource_base{name+std::string(SEMAPHORE_NAME_SUFFIX)}, handle_{nullptr} {
    // create named semaphore
    errno = 0;
    // has O_CREAT | O_RDWR, but latter not documented for OSX
    handle_ = sem_open(id_ptr(), O_CREAT, S_IRWXU | S_IRWXG, value);
    if (handle_ == SEM_FAILED) {
      cpen333::perror(std::string("Cannot create semaphore ")+ name
                          + ", system name: " + this->name());
    }
  }

  /**
   * @brief Destructor
   */
  ~semaphore() {
    // release the semaphore
    if (sem_close(handle_) != 0) {
        cpen333::perror(std::string("Cannot destroy semaphore with id ")+name());
    }
  }

  /**
   * @brief Determines the current stored value of the semaphore
   *
   * This should never be used, except for possibly debugging, as the value may change without notice from other
   * threads.  This method will cause an error on OSX.
   *
   * @return
   */
  size_t value() {
    int val = 0;
#ifdef __APPLE__
    int success = -1;
    errno = ENOSYS; // not supported on OSX
#else
    int success = sem_getvalue(handle_, &val);
#endif
    if (success != 0) {
      cpen333::perror(std::string("Failed to get semaphore value ")+name());
    }
    return val;
  }

  /**
   * @brief Waits for and decrements the semaphore value
   *
   * If the value is greater than zero, will decrement it and return immediately.  Otherwise, the thread will
   * block until it becomes possible to perform the decrement.
   */
  void wait() {
    int success = 0;
    // continuously loop until we have the lock
    do {
      success = sem_wait(handle_);
      std::this_thread::yield();   // yield to prevent from hogging resources
    } while (success == -1 && errno == EINTR);
    if (success != 0) {
      cpen333::perror(std::string("Failed to wait on semaphore ")+name());
    }
  }

  /**
   * @brief Tries to wait for the semaphore, returning immediately
   *
   * If the value is greater than zero, will decrement the semaphore and return true.  Otherwise, will return false.
   *
   * @return true if decrement successful, false otherwise
   */
  bool try_wait() {
    int success = sem_trywait(handle_);
    if (errno == EINVAL) {
      cpen333::perror(std::string("Failed to wait on semaphore ")+name());
    }
    return (success == 0);
  }

  /**
   * @brief Increments the semaphore value
   *
   * If the semaphore's value consequently becomes greater than zero, then one process or thread that is currently
   * blocked in a wait() operation will be woken up and will proceed.
   */
  void notify() {
    // std::cout << "notifying sem " << name() << std::endl;
    int success = sem_post(handle_);
    if (success != 0) {
      cpen333::perror(std::string("Failed to post semaphore ")+name());
    }
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
  template< class Rep, class Period >
  bool wait_for( const std::chrono::duration<Rep,Period>& timeout_duration ) {
    return wait_until(std::chrono::steady_clock::now()+timeout_duration);
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
  template< class Clock, class Duration >
  bool wait_until( const std::chrono::time_point<Clock,Duration>& timeout_time ) {
    auto duration = timeout_time.time_since_epoch();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(duration);
    timespec ts;
    ts.tv_sec = sec.count();
    ts.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration-sec).count();
    int success = sem_timedwait(handle_, &ts);
    if (errno == EINVAL) {
      cpen333::perror(std::string("Failed to wait on semaphore ")+name());
    }
    return (success == 0);
  }

  /**
   * @brief Returns a native handle to the semaphore
   *
   * The native handle has a type aliased to semaphore::native_handle_type
   *
   * On POSIX systems, is of type sem_t*.
   *
   * @return native semaphore handle
   */
  native_handle_type native_handle() const {
    return handle_;
  }

  bool unlink() {
    int status = sem_unlink(id_ptr());
    if (status != 0) {
      cpen333::perror(std::string("Failed to unlink semaphore with id ")+name());
    }
    return (status == 0);
  }

  /**
   * @copydoc cpen333::process::named_resource::unlink(const std::string&)
   */
  static bool unlink(const std::string& name) {
    char nm[MAX_RESOURCE_ID_SIZE];
    impl::named_resource_base::make_resource_id(name+std::string(SEMAPHORE_NAME_SUFFIX), nm);
    int status = sem_unlink(&nm[0]);
    if (status != 0) {
      cpen333::perror(std::string("Failed to unlink semaphore with id ")+std::string(nm));
    }
    return (status == 0);
  }

 private:
  native_handle_type handle_;

};

} // native implementation

/**
 * @brief Alias to POSIX native implementation of inter-process semaphore
 */
using semaphore = posix::semaphore;

} // process
} // cpen333

// undef local macros
#undef SEMAPHORE_NAME_SUFFIX

#endif //CPEN333_PROCESS_POSIX_SEMAPHORE_H
