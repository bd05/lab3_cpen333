/**
 * @file
 * @brief Semaphore guard, similar to std::lock_guard
 */
#ifndef CPEN333_PROCESS_SEMAPHORE_GUARD_H
#define CPEN333_PROCESS_SEMAPHORE_GUARD_H

namespace cpen333 {
namespace process {

/**
 * @brief Semaphore guard, similar to std::lock_guard
 *
 * Protects a semaphore's wait/notify using RAII to ensure all resources
 * are returned to the system
 * @tparam Semaphore basic semaphore that supports wait() and notify()
 */
template<typename Semaphore>
class semaphore_guard {
  Semaphore& sem_;
 public:
  /**
   * @brief Constructor, waits on semaphore
   * @param sem semaphore to wait on
   */
  semaphore_guard(Semaphore& sem) : sem_(sem) {
    sem_.wait();
  }

  // disable copy/move constructors
 private:
  semaphore_guard(const semaphore_guard&) DELETE_METHOD;
  semaphore_guard(semaphore_guard&&) DELETE_METHOD;
  semaphore_guard& operator=(const semaphore_guard&) DELETE_METHOD;
  semaphore_guard& operator=(semaphore_guard&&) DELETE_METHOD;

 public:

  /**
   * @brief Destructor, automatically notifies semaphore
   */
  ~semaphore_guard() {
    sem_.notify();
  }
};

} // process
} // cpen333

#endif //CPEN333_PROCESS_SEMAPHORE_GUARD_SEMAPHORE_H
