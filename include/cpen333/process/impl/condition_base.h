/**
 * @file
 * @brief Base class for condition, condition_variable, and event classes
 */
#ifndef CPEN333_PROCESS_CONDITION_BASE_H
#define CPEN333_PROCESS_CONDITION_BASE_H

/**
 * @brief Suffix to append to shared storage identifier for uniqueness
 */
#define CONDITION_BASE_STORAGE_SUFFIX "_cbs"
/**
 * @brief Suffix to append to lock semaphore identifier
 */
#define CONDITION_BASE_BLOCK_LOCK_SUFFIX "_cbl"
/**
 * @brief Suffix to append to queue semaphore identifier
 */
#define CONDITION_BASE_BLOCK_QUEUE_SUFFIX "_cbq"
/**
 * @brief Suffix to append to unblock mutex identifier
 */
#define CONDITION_BASE_UNBLOCK_LOCK_SUFFIX "_cbu"

/**
 * @brief Magic number for testing initialization
 */
#define CONDITION_BASE_INITIALIZED 0x09812312

#include <string>
#include <chrono>
#include <condition_variable>

#include "../named_resource.h"
#include "../mutex.h"
#include "../semaphore.h"
#include "../shared_memory.h"  // for keeping a "waiters" count needed for notify_all()

namespace cpen333 {
namespace process {

/**
 * @brief Lock inverter
 *
 * Inverts lock/unlock operations on a lock
 *
 * @tparam BasicLock lock type that supports lock() and unlock()
 */
template<typename BasicLock>
class lock_inverter {
  BasicLock &lock_;
 public:
  /**
   * @brief Constructor
   * @param lock lock to invert
   */
  lock_inverter(BasicLock& lock) : lock_(lock){}

 private:
  // disable assignment
  lock_inverter(const lock_inverter&) DELETE_METHOD;
  lock_inverter(lock_inverter &&) DELETE_METHOD;
  lock_inverter &operator=(const lock_inverter &) DELETE_METHOD;
  lock_inverter &operator=(lock_inverter &&) DELETE_METHOD;

 public:

  /**
   * @brief Unlocks the underlying lock
   */
  void lock() {
    lock_.unlock();
  }

  /**
   * @brief Locks the underlying lock
   */
  void unlock() {
    lock_.lock();
  }
};

//
// Implementation based on boost's boost/interpress/sync/detail/condition_algorithm_8a.hpp
// Their implementation guarantees not to have spurious wake-ups
/**
 * @brief Base-class for conditions, condition variables, and events
 *
 * Like an event, has the ability to wait for ownership of a lock, and for
 * notifying waiting threads.  This condition base DOES NOT suffer from spurious
 * wake-ups.
 */
class condition_base : public virtual named_resource {
  
 public:
  /**
   * @brief Constructor
   * @param name unique identifier
   */
  condition_base(const std::string &name) :
      waiters_(name + std::string(CONDITION_BASE_STORAGE_SUFFIX)),
      block_lock_(name + std::string(CONDITION_BASE_BLOCK_LOCK_SUFFIX),1),
      block_queue_(name + std::string(CONDITION_BASE_BLOCK_QUEUE_SUFFIX),0),
      unblock_lock_(name + std::string(CONDITION_BASE_UNBLOCK_LOCK_SUFFIX)) {

    // initialize data
    std::lock_guard<decltype(unblock_lock_)> lock(unblock_lock_);
    if (waiters_->initialized != CONDITION_BASE_INITIALIZED) {
      waiters_->blocked = 0;
      waiters_->unblock = 0;
      waiters_->gone = 0;
      waiters_->initialized = CONDITION_BASE_INITIALIZED;
    }

  }

 private:
  // disable copy/move constructors
  condition_base(const condition_base&);
  condition_base(condition_base&&);
  condition_base& operator=(const condition_base&);
  condition_base& operator=(condition_base&&);

 public:

  /**
   * @brief Wait until the thread is notified
   * @param lock external lock
   */
  void wait(std::unique_lock<cpen333::process::mutex>& lock) {
    wait(lock, false, std::chrono::steady_clock::now());
  }

  /**
   * @brief Wait until the thread is notified, or until a timeout period has elapsed
   * @tparam Rep duration clock representation
   * @tparam Period duration clock period
   * @param lock external lock
   * @param rel_time relative time to wait
   * @return true if successful, false iftimeout has occured
   */
  template<class Rep, class Period>
  bool wait_for( std::unique_lock<cpen333::process::mutex>& lock,
                           const std::chrono::duration<Rep, Period>& rel_time) {
    if (wait(lock, true, std::chrono::steady_clock::now()+rel_time)) {
      return true;
    }
    return false;
  }

  /**
   * @brief Wait until the thread is notified, or until a timeout time has been reached
   * @tparam Clock timeout clock representation
   * @tparam Duration timeout duration
   * @param lock external lock
   * @param timeout_time absolute timeout time
   * @return true if wait is successful, false if timeout has been reached
   */
  template<class Clock, class Duration >
  bool wait_until( std::unique_lock<cpen333::process::mutex>& lock,
                             const std::chrono::time_point<Clock, Duration>& timeout_time ) {
    return wait(lock, true, timeout_time);
  }

  /**
   * @brief Notify one waiting thread
   *
   * Wake up a single waiting thread and notify them of a potential change
   */
  void notify_one() {
    notify(false);
  }

  /**
   * @brief Notify all waiting threads
   *
   * Wake up all waiting threads and notify them of a potential change
   */
  void notify_all() {
    notify(true);
  }

  virtual bool unlink() {
    bool b1 = waiters_.unlink();
    bool b2 = block_lock_.unlink();
    bool b3 = block_queue_.unlink();
    bool b4 = unblock_lock_.unlink();
    return (b1 && b2 && b3 && b4);
  }

  /**
  * @copydoc cpen333::process::named_resource::unlink(const std::string&)
  */
  static bool unlink(const std::string& name) {
    bool b1 = cpen333::process::shared_object<shared_data>::unlink(name + std::string(CONDITION_BASE_STORAGE_SUFFIX));
    bool b2 = cpen333::process::semaphore::unlink(name + std::string(CONDITION_BASE_BLOCK_LOCK_SUFFIX));
    bool b3 = cpen333::process::semaphore::unlink(name + std::string(CONDITION_BASE_BLOCK_QUEUE_SUFFIX));
    bool b4 = cpen333::process::mutex::unlink(name + std::string(CONDITION_BASE_UNBLOCK_LOCK_SUFFIX));
    return b1 && b2 && b3 && b4;
  }

 protected:

  /**
   * @brief Notify (wake-up) waiting threads
   * @param broadcast if true, wakes up all threads
   */
  void notify(bool broadcast) {
    int signals;
    {
      // lock prcondition_baseing new waiters from being signalled
      std::lock_guard<decltype(unblock_lock_)>  unblocklock(unblock_lock_);

      if ( waiters_->unblock != 0 ) {
        if ( waiters_->blocked == 0) {
          // nothing to signal
          return;
        }
        if (broadcast) {
          // move blocked to signals/unblock
          signals = waiters_->blocked;
          waiters_->unblock += signals;
          waiters_->blocked = 0;
        } else {
          // only signal one
          signals = 1;
          waiters_->unblock++;
          waiters_->blocked--;
        }
      } else if ( waiters_->blocked > waiters_->gone ) {
        block_lock_.wait();                      // close the gate
        if ( waiters_->gone != 0) {
          // waiters gone are no longer blocked
          waiters_->blocked -= waiters_->gone;
          waiters_->gone = 0;
        }
        if (broadcast) {
          // move blocked to unblock/signals
          signals = waiters_->blocked;
          waiters_->unblock = signals;
          waiters_->blocked = 0;
        } else {
          signals = 1;
          waiters_->unblock = 1;
          waiters_->blocked--;
        }
      } else {
        return;
      }
    }
    // post a number of signals
    while (signals > 0) {
      --signals;
      block_queue_.notify();
    }
  }

  /**
   * @brief Waits for condition to be notified
   * @tparam Clock clock type
   * @tparam Duration duration type
   * @param lock external lock to ensure no simultaneous waits/notifies
   * @param timeout whether or not to wait with a timeout
   * @param abs_time absolute timeout time
   * @return true if wait was successful, false if timeout occurred
   */
  template<class Clock, class Duration>
  bool wait(std::unique_lock<cpen333::process::mutex>& lock,
            bool timeout, const std::chrono::time_point<Clock, Duration>& abs_time) {

    long signals_left = 0;
    long waiters_gone = 0;

    // scoped lock on memory to atomically change waiters count
    {
      cpen333::process::semaphore_guard<decltype(block_lock_)> blocklock(block_lock_);
      ++(waiters_->blocked);
    }

    // release mutex and wait on semaphore until notify_all or notify_one called
    // unlock lock, and protect so will be relocked out of scope
    lock_inverter<std::unique_lock<cpen333::process::mutex>> ilock(lock);
    std::lock_guard<lock_inverter<std::unique_lock<cpen333::process::mutex>>> external_unlock(ilock);

    // wait on queue and see if timed out
    bool timed_out = false;
    if (timeout) {
      timed_out = !block_queue_.wait_until(abs_time);
    } else {
      block_queue_.wait();
    }

    // reduce waiters count and check if we were the last in a notify_all
    {
      std::lock_guard<cpen333::process::mutex> unblocklock(unblock_lock_);
      signals_left = waiters_->unblock;

      if (signals_left != 0) {
        if (timed_out) {
          if (waiters_->blocked != 0) {
            --(waiters_->blocked);
          } else {
            ++(waiters_->gone);    // spurious wakeup
          }
        }

        if (--(waiters_->unblock) == 0) {
          if (waiters_->blocked != 0) {
            block_lock_.notify();   // allow one more through
            signals_left = 0;     // let him take care of rest
          } else {
            // take waiters_->gone out of shared memory
            waiters_gone = waiters_->gone;
            if (waiters_gone != 0) {
              waiters_->gone = 0;
            }
          } // blocked
        } // last to unblock
      } // signals left

        // timeout/cancelled or spurious semaphore
      else if ((std::numeric_limits<long>::max)() / 2 == ++(waiters_->gone)) {
        // reduce both blocked and gone
        cpen333::process::semaphore_guard<decltype(block_lock_)> blocklock(block_lock_);
        waiters_->blocked -= waiters_->gone;
        waiters_->gone = 0;
      }
    } // unblock lock is unlocked

    // one signal left
    if (signals_left == 1) {
      if (waiters_gone != 0) {
        while (waiters_gone > 0) {
          --waiters_gone;
          block_queue_.wait();
        }
      }
      block_lock_.notify();  // allow next through
    }

    return !timed_out;
  }

 private:
  struct shared_data {
    long blocked;   // number of waiters blocked
    long unblock;   // number of waiters to unblock
    long gone;      // number of waiters gone
    int initialized;  // magic initialized number
  };

  cpen333::process::shared_object<shared_data> waiters_;
  cpen333::process::semaphore block_lock_;
  cpen333::process::semaphore block_queue_;
  cpen333::process::mutex unblock_lock_;

};

} // process
} // cpen333

// undef local macros
#undef CONDITION_BASE_STORAGE_SUFFIX
#undef CONDITION_BASE_BLOCK_LOCK_SUFFIX
#undef CONDITION_BASE_BLOCK_QUEUE_SUFFIX
#undef CONDITION_BASE_UNBLOCK_LOCK_SUFFIX
#undef CONDITION_BASE_INITIALIZED

#endif //CPEN333_PROCESS_CONDITION_BASE_H
