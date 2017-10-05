/**
 * @file
 * @brief POSIX implementation of an inter-process named shared memory (i.e. datapool)
 *
 * Uses a POSIX shared memory object (shm_open)
 */
#ifndef CPEN333_PROCESS_POSIX_SHARED_MEMORY_H
#define CPEN333_PROCESS_POSIX_SHARED_MEMORY_H

/**
 *  @brief Suffix to append to shared memory names for uniqueness
 */
#define SHARED_MEMORY_NAME_SUFFIX "_shm"

#include <string>
#include <mutex>  // for lock

#include "../../../util.h"
#include "../named_resource_base.h"
#include "mutex.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>        // for mode constants
#include <fcntl.h>           // for O constants

namespace cpen333 {
namespace process {
namespace posix {

/**
 * @brief Inter-process shared memory implementation
 *
 * Creates and shares a block of memory between threads/processes, accessible using a unique name.  The block
 * of memory is mapped to DIFFERENT address spaces on each process.  This is essentially a
 * memory-mapped file.
 *
 * This shared memory has KERNEL PERSISTENCE, meaning if not unlink()-ed, will continue to exist in its current state
 * until the system is shut down (persisting beyond the life of the initiating program)
 */
class shared_memory : public impl::named_resource_base {
 public:
  /**
   * @brief Alias to native handle for shared memory
   */
  using native_handle_type = int;

  /**
   * @brief Constructs or connects to a block of shared memory
   *
   * @param name  identifier for creating or connecting to an existing inter-process shared memory block
   * @param size  if creating, the size of the memory block.  This size should be consistent between users
   * @param readonly  whether or not to map the memory as read-only
   */
  shared_memory(const std::string &name, size_t size, bool readonly = false) :
    impl::named_resource_base{name+std::string(SHARED_MEMORY_NAME_SUFFIX)}, fid_{-1},
    data_{nullptr}, size_{size} {

    // try opening new
    bool initialize = true;
    int mode = S_IRWXU | S_IRWXG; // user/group +rw permissions
    errno = 0;

    {
      // protect initialization
      cpen333::process::mutex mutex(name + std::string(SHARED_MEMORY_NAME_SUFFIX));
      std::lock_guard<cpen333::process::mutex> lock(mutex);

      fid_ = shm_open(id_ptr(), O_RDWR | O_CREAT | O_EXCL, mode);
      if (fid_ < 0 && errno == EEXIST) {
        // create for open
        initialize = false;
        fid_ = shm_open(id_ptr(), readonly ? O_RDONLY : O_RDWR, mode);
      }

      if (fid_ < 0) {
        cpen333::perror(std::string("Cannot create shared memory with id ") + this->name());
        return;
      }

      // truncate and initialize
      if (initialize) {
        int resize = ftruncate(fid_, size_);
        if (resize < 0) {
          cpen333::perror(std::string("Cannot allocate shared memory with id ") + this->name());
          return;
        }
      }
    } // end of critical section

    int flags = readonly ? PROT_READ : PROT_WRITE;
    data_ = mmap(nullptr, size_, flags, MAP_SHARED, fid_, 0);
    if (data_ == (void*) -1) {
      data_ = nullptr;
      cpen333::perror(std::string("Cannot map shared memory with id ") + this->name());
      return;
    }
  }

  /**
   * @brief Destructor, unmaps this instance of the shared memory block (but does not unmap memory from other users)
   */
  ~shared_memory() {
    // unmap
    if (data_ != nullptr) {
      if (munmap(data_, size_) != 0) {
        cpen333::perror(std::string("Cannot unmap shared memory with id ") + name());
      }
    }

    // close
    if (fid_ != -1) {
      if (close(fid_) != 0) {
        cpen333::perror(std::string("Cannot close shared memory with id ") + name());
      }
    }
  }

  /**
   * @brief Pointer to memory at a particular offset from the block
   * @param offset memory offset (in bytes)
   * @return pointer to memory offset
   */
  void* get(size_t offset = 0) {
    return (void*)((char*)data_ + offset);
  }

  /**
   * @brief Byte access, by reference
   * @param offset memory offset (in bytes)
   * @return byte at particular offset
   */
  uint8_t& operator[](size_t offset) {
    return *((uint8_t*)get(offset));
  }

  /**
   * @brief Retrieves a pointer to an object of specified type starting at a particular offset
   *
   * @tparam T type of pointer to return
   * @param offset memory offset (in bytes)
   * @return pointer to object
   */
  template<typename T>
  T* get(size_t offset) {
    return (T*)get(offset);
  }

  /**
   * @brief Retrieves a pointer to the underlying memory, cast to a specified type
   * @tparam T type of pointer to return
   * @return pointer to object
   */
  template<typename T>
  T* get() {
    return (T*)data_;
  }

  bool unlink() {
    errno = 0;
    int status = shm_unlink(id_ptr());
    if (status != 0) {
      cpen333::perror(std::string("Failed to unlink shared memory with id ") + name());
    }
    return status == 0;
  }

  /**
   * @copydoc cpen333::process::named_resource::unlink(const std::string&)
   */
  static bool unlink(const std::string& name) {
    char nm[MAX_RESOURCE_ID_SIZE];
    impl::named_resource_base::make_resource_id(name+std::string(SHARED_MEMORY_NAME_SUFFIX), nm);
    int status = shm_unlink(&nm[0]);
    if (status != 0) {
      cpen333::perror(std::string("Failed to unlink shared memory with id ") + std::string(nm));
    }
    return status == 0;
  }

  /**
   * @brief Native handle to underlying shared memory block
   *
   * On POSIX systems, this is a POSIX shm id
   *
   * @return native handle to shared memory block
   */
  native_handle_type native_handle() {
    return fid_;
  }

  private:
    native_handle_type fid_;
    void* data_;
    size_t size_;
};


} // native implementation

/**
 * @brief Alias to POSIX native implementation of inter=process shared memory
 */
using shared_memory = posix::shared_memory;

} // process
} // cpen333

// undef local macros
#undef SHARED_MEMORY_NAME_SUFFIX

#endif //CPEN333_PROCESS_POSIX_SHARED_MEMORY_H
