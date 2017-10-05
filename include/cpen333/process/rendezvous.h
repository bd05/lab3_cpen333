/**
 * @file
 * @brief Basic inter-process rendezvous implementation
 */
#ifndef CPEN333_PROCESS_RENDEZVOUS_H
#define CPEN333_PROCESS_RENDEZVOUS_H

/**
 * @brief Suffix to add to the rendezvous' name for uniqueness
 */
#define RENDEZVOUS_NAME_SUFFIX "_rdv"
/**
 * @brief Magic number to ensure rendezvous is initialized
 */
#define RENDEZVOUS_INITIALIZED 0x38973823

#include "named_resource.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "mutex.h"

namespace cpen333 {
namespace process {

/**
 * @brief Inter-process rendezvous implementation
 *
 * A synchronization primitive that allows a certain number of threads/processes to wait for others to arrive, then
 * proceed together.
 */
class rendezvous : public virtual named_resource {

 public:
  /**
   * @brief Creates or connects to a named rendezvous primitive
   * @param name  identifier for creating or connecting to an existing inter-process rendezvous
   * @param size  number of processes in group
   */
  rendezvous(const std::string &name, size_t size) :
      shared_(name + std::string(RENDEZVOUS_NAME_SUFFIX)),
      semaphore_(name + std::string(RENDEZVOUS_NAME_SUFFIX), 0),
      mutex_(name + std::string(RENDEZVOUS_NAME_SUFFIX)){

    // initialize data
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    if (shared_->initialized != RENDEZVOUS_INITIALIZED) {
      shared_->size = size;
      shared_->count = size;
      shared_->initialized = RENDEZVOUS_INITIALIZED;
    }

  }

  /**
   * @brief Waits until all other processes are also waiting
   *
   * Will cause the current process to block until all (# size) processes are waiting, then will release so that
   * the processes are synchronized.
   */
  void wait() {
    // lock shared data to prevent concurrent modification
    std::unique_lock<decltype(mutex_)> lock(mutex_);
    if (shared_->count == 0) {
      return;  // no need to wait
    }

    if (--(shared_->count) == 0) {
      // reset count
      shared_->count = shared_->size;
      // release size-1 (since we are the last to arrive)
      auto release = (shared_->size)-1;
      for (size_t i=0; i<release; ++i) {
        semaphore_.notify();
      }
    } else {
      // unlock shared data and wait
      lock.unlock();
      semaphore_.wait();
    }
  }

  bool unlink() {
    bool b1 = shared_.unlink();
    bool b2 = semaphore_.unlink();
    bool b3 = mutex_.unlink();
    return b1 && b2 && b3;
  }

  /**
  * @copydoc cpen333::process::named_resource::unlink(const std::string&)
  */
  static bool unlink(const std::string& name) {

    bool b1 = cpen333::process::shared_object<shared_data>::unlink(name + std::string(RENDEZVOUS_NAME_SUFFIX));
    bool b2 = cpen333::process::semaphore::unlink(name + std::string(RENDEZVOUS_NAME_SUFFIX));
    bool b3 = cpen333::process::mutex::unlink(name + std::string(RENDEZVOUS_NAME_SUFFIX));

    return b1 && b2 && b3;
  }

 private:

  struct shared_data {
    size_t size;
    size_t count;
    int initialized;
  };
  cpen333::process::shared_object<shared_data> shared_;
  cpen333::process::semaphore semaphore_;
  cpen333::process::mutex mutex_;

};

} // process
} // cpen333

// undef local macros
#undef RENDEZVOUS_NAME_SUFFIX
#undef RENDEZVOUS_INITIALIZED

#endif //CPEN333_PROCESS_RENDEZVOUS_H
