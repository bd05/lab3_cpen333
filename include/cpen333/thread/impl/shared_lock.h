/**
 * @file
 * @brief Basic replacement shared_lock implementation for old machines that don't support C++14
 */
#ifndef STD_SHARED_LOCK_H
#define STD_SHARED_LOCK_H

#include "../../util.h"

// replacement shared_lock
namespace std {

/**
 * @brief Basic shared_lock replacement, only supports simple locking
 * @tparam SharedMutexType mutex type supporting lock_shared()
 */
template<typename SharedMutexType>
class shared_lock {
 private:
  SharedMutexType &mutex_;

 public:
  /**
   * @brief Constructor, lock mutex in shared access mode
   * @param mutex shared mutex
   */
  shared_lock(SharedMutexType &mutex) : mutex_(mutex) {
    mutex_.lock_shared();
  }

 private:
  shared_lock(const shared_lock &) DELETE_METHOD;
  shared_lock(shared_lock &&) DELETE_METHOD;
  shared_lock &operator=(const shared_lock &) DELETE_METHOD;
  shared_lock &operator=(shared_lock &&) DELETE_METHOD;

 public:

  /**
   * @brief Locks mutex in shared access mode
   */
  void lock() {
    mutex_.lock_shared();
  }

  /**
   * @brief Unlocks mutex from shared access mode
   */
  void unlock() {
    mutex_.unlock_shared();
  }

  /**
   * @brief Destructor, automatically unlocks mutex
   */
  ~shared_lock() {
    mutex_.unlock_shared();
  }
};

} // std

#endif //CPEN333_PROCESS_SHARED_LOCK_H
