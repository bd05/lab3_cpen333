/**
 * @file
 * @brief Basic inter-process pipe implementation based on a FIFO
 */
#ifndef CPEN333_PROCESS_BASIC_PIPE_H
#define CPEN333_PROCESS_BASIC_PIPE_H

/**
 * @brief Suffix to add to the pipe's internal memory name for uniqueness
 */
#define BASIC_PIPE_NAME_SUFFIX "_pp"
/**
 * @brief Suffix to add to the pipe's writer's semaphore/mutex
 */
#define BASIC_PIPE_WRITE_SUFFIX "_ppw"
/**
 * @brief Suffix to add to the pipe's reader's semaphore/mutex
 */
#define BASIC_PIPE_READ_SUFFIX "_ppr"
/**
 * @brief Suffix to add to the pipe's information block
 */
#define BASIC_PIPE_INFO_SUFFIX "_ppi"
/**
 * @brief Suffix to add to the pipe's "open" mutex for ensuring valid pipe connections
 */
#define BASIC_PIPE_OPEN_SUFFIX "_ppo"
/**
 * @brief Magic number for ensuring pipe has been initialized
 */
#define BASIC_PIPE_INITIALIZED 0x18763023

#include "../named_resource.h"
#include "../mutex.h"
#include "../semaphore.h"
#include "../shared_memory.h"

// simulated pipe using shared memory and semaphores
namespace cpen333 {
namespace process {

/**
 * @brief Inter-process pipe emulated using a shared FIFO-style queue
 *
 * Allows sending/receiving of unstructured information between two connected processes.
 *
 */
class basic_pipe : public virtual named_resource {
 public:
  /**
   * @brief Constructs a named pipe instance
   *
   * @param name  identifier for creating or connecting to an existing inter-process pipe
   * @param size  if creating, the maximum number of bytes that can be stored in the pipe without blocking
   */
  basic_pipe(const std::string& name, size_t size = 1024) :
      wmutex_(name + std::string(BASIC_PIPE_WRITE_SUFFIX)),
      rmutex_(name + std::string(BASIC_PIPE_READ_SUFFIX)),
      omutex_(name + std::string(BASIC_PIPE_OPEN_SUFFIX)),
      info_(name + std::string(BASIC_PIPE_INFO_SUFFIX)),
      pipe_(name + std::string(BASIC_PIPE_NAME_SUFFIX), size),
      producer_(name + std::string(BASIC_PIPE_WRITE_SUFFIX), 0),
      consumer_(name + std::string(BASIC_PIPE_READ_SUFFIX), size) {

    // potentially initialize info
    std::lock_guard<decltype(wmutex_)> lock(omutex_);
    if (info_->initialized != BASIC_PIPE_INITIALIZED) {
      info_->size = size;
      info_->read = 0;
      info_->write = 0;
      info_->reof = 0;            // marks 1 past the final written index
      info_->weof = 0;            // marks 1 past the final read index
      info_->initialized = BASIC_PIPE_INITIALIZED; // mark as initialized
    }
  }

  /**
   * @brief Destructor
   */
  virtual ~basic_pipe() {}

  /**
   * @brief Writes data to the pipe
   *
   * If the pipe becomes full, will block until there is room to complete the message.
   *
   * @param data data to write
   * @param size number of bytes to write
   * @return true if write is successful
   */
  bool write(const void* data, size_t size) {

    uint8_t *ptr = (uint8_t *) data;

    // try to write bytes
    std::unique_lock<decltype(wmutex_)> lock(wmutex_, std::defer_lock);
    for (size_t i = 0; i < size; ++i) {
      consumer_.wait();  // wait until there is space in the pipe

      // write next byte and advance write index
      lock.lock();
      size_t pos = info_->write;

      // check for EOF
      if (info_->weof > 0) {
        // Do not start write if pipe is closed, otherwise finish writing
        if ( (i == 0)  // start
            || (pos == 0 && info_->weof == info_->size)  // wrap
            || (pos == info_->weof)) {                   // regular
          consumer_.notify();  // notify any other writer threads
          return false;
        }
      }

      // next byte location to write, wrapping around if need be
      if ((++(info_->write)) == info_->size) {
        info_->write = 0;
      }
      lock.unlock();

      // do actual write outside of lock for efficiency
      // (though here it is only one byte...)
      pipe_[pos] = *ptr;
      ++ptr;

      producer_.notify();  // byte available for read
    }

    return true;
  }

  /**
   * @brief Writes an object to the pipe
   *
   * Convenience method for writing objects to the pipe, auto-detecting the appropriate number of bytes.  This method
   * will block until there is sufficient room to finish writing the object
   *
   * @tparam T type of object to write
   * @param data reference to data
   * @return true if write is successful, false otherwise
   */
  template<typename T>
  bool write(const T& data) {
    return this->write<T>(&data);
  }

  /**
   * @brief Writes an object to the pipe
   *
   * Convenience method for writing objects to the pipe, auto-detecting the appropriate number of bytes.  This method
   * will block until there is sufficient room to finish writing the object
   *
   * @tparam T type of object to write
   * @param data pointer to data
   * @return true if write is successful, false otherwise
   */
  template<typename T>
  bool write(const T* data) {
    return this->write((void*)data, sizeof(T));
  }

  /**
   * @brief Reads data from the pipe
   *
   * Reads the specified number of bytes from the head of the pipe.  This method will block until the desired number of
   * bytes are read.
   *
   * @param data memory address to fill with pipe contents
   * @param size number of bytes
   * @return true if successful, false otherwise
   */
  bool read(void* data, size_t size) {
    uint8_t *ptr = (uint8_t *) data;

    std::unique_lock<decltype(rmutex_)> lock(rmutex_, std::defer_lock);
    for (size_t i = 0; i < size; ++i) {
      producer_.wait();  // wait until there is data in the pipe

      // read next byte and advance read index
      lock.lock();
      size_t pos = info_->read;

      // check for EOF
      if (info_->reof > 0) {
        if ( ((pos == 0) && (info_->reof == info_->size))
            || (pos == info_->reof)) {
          producer_.notify();  // notify any other reader threads
          return false;
        }
      }

      // next byte location to read, wrapping around if need be
      if ((++(info_->read)) == info_->size) {
        info_->read = 0;
      }
      lock.unlock();

      // do the actualy write
      *ptr = pipe_[pos];
      ++ptr;  // advance ptr

      consumer_.notify();  // byte available for writing
    }

    return true;
  }

  /**
   * @brief Reads an object from the pipe
   *
   * Convenience method for reading an object from the pipe, auto-detecting the appropriate number of bytes to read.
   * This method will block until the complete object is read.
   *
   * @tparam T type of object
   * @param data pointer to object to populate
   * @return true if successful, false otherwise
   */
  template<typename T>
  bool read(T* data) {
    return read((void*)data, sizeof(T));
  }

  /**
   * @brief Determines the number of bytes currently remaining in the pipe.
   *
   * This method should rarely be used, as the number of bytes is subject to change rapidly.  One possible use-case
   * is if there is a single reader, and the reader wants to check if there is any data available.
   *
   * @return number of bytes currently remaining in the pipe
   */
  size_t available() {
    // lock both read and write to get indices, don't want them changing between here
    std::lock_guard<decltype(rmutex_)> rlock(rmutex_);
    std::lock_guard<decltype(wmutex_)> wlock(wmutex_);
    auto r = info_->read;
    auto w = info_->write;
    if (w < r) {
      return info_->size-r+w;
    }
    return w-r;
  }

  bool unlink() {
    bool b1 = wmutex_.unlink();
    bool b2 = rmutex_.unlink();
    bool b3 = omutex_.unlink();
    bool b4 = info_.unlink();
    bool b5 = pipe_.unlink();
    bool b6 = producer_.unlink();
    bool b7 = consumer_.unlink();
    return b1 && b2 && b3 && b4 && b5 && b6 && b7;
  }

  /**
  * @copydoc cpen333::process::named_resource::unlink(const std::string&)
  */
  static bool unlink(const std::string& name) {

    bool b1 = cpen333::process::mutex::unlink(name + std::string(BASIC_PIPE_WRITE_SUFFIX));
    bool b2 = cpen333::process::mutex::unlink(name + std::string(BASIC_PIPE_READ_SUFFIX));
    bool b3 = cpen333::process::mutex::unlink(name + std::string(BASIC_PIPE_OPEN_SUFFIX));
    bool b4 = cpen333::process::shared_object<pipe_info>::unlink(name + std::string(BASIC_PIPE_INFO_SUFFIX));
    bool b5 = cpen333::process::shared_memory::unlink(name + std::string(BASIC_PIPE_NAME_SUFFIX));
    bool b6 = cpen333::process::semaphore::unlink(name + std::string(BASIC_PIPE_WRITE_SUFFIX));
    bool b7 = cpen333::process::semaphore::unlink(name + std::string(BASIC_PIPE_READ_SUFFIX));

    return b1 && b2 && b3 && b4 && b5 && b6 && b7;
  }

 private:
  struct pipe_info {
    int initialized;
    size_t read;
    size_t write;
    size_t size;
    size_t reof;
    size_t weof;
  };

  cpen333::process::mutex wmutex_;
  cpen333::process::mutex rmutex_;
  cpen333::process::mutex omutex_;  // for checking if pipe is open
  cpen333::process::shared_object<pipe_info> info_;
  cpen333::process::shared_memory pipe_;
  cpen333::process::semaphore producer_;
  cpen333::process::semaphore consumer_;

};

} // process
} // cpen333

// undef local macros
#undef BASIC_PIPE_NAME_SUFFIX
#undef BASIC_PIPE_WRITE_SUFFIX
#undef BASIC_PIPE_READ_SUFFIX
#undef BASIC_PIPE_INFO_SUFFIX
#undef BASIC_PIPE_OPEN_SUFFIX
#undef BASIC_PIPE_INITIALIZED

#endif //CPEN333_PROCESS_BASIC_PIPE_H