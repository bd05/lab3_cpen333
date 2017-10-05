/**
 * @file
 * @brief Inter-process shared memory (a.k.a. datapool) implementation
 */
#ifndef CPEN333_PROCESS_SHARED_MEMORY_H
#define CPEN333_PROCESS_SHARED_MEMORY_H

#include "../os.h"           // identify OS

#ifdef WINDOWS
#include "impl/windows/shared_memory.h"
#else
#include "impl/posix/shared_memory.h"
#endif

namespace cpen333 {
namespace process {

/**
 * @brief Shared memory with a a specific stored type
 *
 * With typed shared memory, the size of the required memory block is automatically
 * computed, and the data pointer is automatically cast to the correct type in get().
 * Member access operators are also overloaded for convenience so the shared object can be used
 * as if it is a direct pointer to the underlying data.
 *
 * @tparam T data type
 */
template<typename T>
class shared_object : private shared_memory, public virtual named_resource {

 public:
  /**
   * @brief Construct shared memory object
   * @param name   identifier for creating or connecting to an existing inter-process shared_object
   * @param readonly whether to treat the memory as read-only or read-write
   */
  shared_object(const std::string &name, bool readonly = false) :
      shared_memory(name, sizeof(T), readonly) {}

  /**
   * @brief Get a reference to the internal shared memory object
   *
   * @return reference to shared data object
   */
  T& operator*() {
    return *shared_memory::get<T>();
  }

  /**
   * @brief Get a pointer to the underlying shared memory object
   *
   * @return pointer to shared data object
   */
  T *operator->() {
    return shared_memory::get<T>();
  }

  /**
   * @brief Get a pointer to the underlying shared memory object
   * @return pointer to shared data object
   */
  T *get() {
    return shared_memory::get<T>();
  }

  bool unlink() {
    return shared_memory::unlink();
  }

  /**
  * @copydoc cpen333::process::named_resource::unlink(const std::string&)
  */
  static bool unlink(const std::string &name) {
    return shared_memory::unlink(name);
  }

};

} // process
} // cpen333

#endif //CPEN333_PROCESS_SHARED_MEMORY_H
