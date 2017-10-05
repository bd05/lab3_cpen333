/**
 * @file
 * @brief Basic rendezvous implementation
 */
#ifndef CPEN333_THREAD_RENDEZVOUS_H
#define CPEN333_THREAD_RENDEZVOUS_H

#include <mutex>
#include <condition_variable>
#include "../util.h"

namespace cpen333 {
namespace thread {

/**
 * @brief Rendezvous synchronization primitive implementation
 *
 * A synchronization primitive that allows a certain number of threads to wait for others to arrive, then
 * proceed together.
 */
class rendezvous {
  std::mutex mutex_;
  std::condition_variable cv_;
  size_t countdown_;
  size_t countup_;
  size_t size_;

 public:
  /**
   * @brief Constructs a rendezvous primitive
   * @param size  number of threads in group
   */
  rendezvous(size_t size) : mutex_(), countdown_(size), countup_(0), size_(size) {}

 private:
  rendezvous(const rendezvous &) DELETE_METHOD;
  rendezvous(rendezvous &&) DELETE_METHOD;
  rendezvous &operator=(const rendezvous &) DELETE_METHOD;
  rendezvous &operator=(rendezvous &&) DELETE_METHOD;

 public:
  
  /**
   * @brief Waits until all other threads are also waiting
   *
   * Will cause the current thread to block until all (# size) threads are waiting, then will release so that
   * the threads are synchronized.
   */
  void wait() {
    // lock to protect access to data
    std::unique_lock<std::mutex> lock(mutex_);

    //check if we are done
    if (countdown_ <= 1) {
      // release all threads
      countdown_ = 0;
      cv_.notify_all();
    } else {
      --countdown_;

      // wait until everybody has arrived
      // We use a simple lambda to check condition
      cv_.wait(lock, [&](){return countdown_ == 0;});
    }

    // reset count upwards
    ++countup_;

    // when last thread exists method, reset countdown_
    if (countup_ == size_) {
      countdown_ = size_;
      countup_ = 0;
    }
  }
};

} // thread
} // cpen333

#endif //CPEN333_THREAD_RENDEZVOUS_H
