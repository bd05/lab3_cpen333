/**
 * @file
 * @brief POSIX implementation of a pipe
 */
#ifndef CPEN333_PROCESS_POSIX_PIPE_H
#define CPEN333_PROCESS_POSIX_PIPE_H

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <mutex>
#include <thread>
#include <chrono>

#include "mutex.h"
#include "../named_resource_base.h"
#include "../../../util.h"

/**
 * @brief create pipe in temporary directory
 */
#define PIPE_NAME_PREFIX "/tmp"

/**
 * @brief Name suffix for pipe from server to client
 */
#define PIPE_SC_NAME_SUFFIX "_scpipe"

/**
 * @brief Name suffix for pipe client to server
 */
#define PIPE_CS_NAME_SUFFIX "_cspipe"

/**
 * @brief Name suffix for client mutex 
 */
#define PIPE_CLIENT_MUTEX_NAME_SUFFIX "_cm"

/**
 * @brief Name suffix for server mutex 
 */
#define PIPE_SERVER_MUTEX_NAME_SUFFIX "_sm"

/**
 * @brief Default name for uninitialized pipes
 */
#define DEFAULT_PIPE_NAME "uninitialized_pipe"

/**
 * @brief Invalid pipe handle
 */
#define INVALID_PIPE -1

/**
 * @brief Default timeout for trying connections 
 */
#define PIPE_TIMEOUT 10000

namespace cpen333 {
namespace process {

namespace posix {

class pipe_server;

/**
 * @brief Interprocess pipe
 */
class pipe : private impl::named_resource_base {
 private:
  cpen333::process::mutex mutex_;
  int pipe_in_;
  int pipe_out_;
  bool open_;

  friend class pipe_server;

  /**
   * @brief Initialize a pipe with provided info
   *
   * For use by the pipe server when creating pipe
   *
   * @param name pipe name
   * @param pipe_in pipe input file id
   * @param pipe_out pipe output file id
   * @param open whether the pipe is open
   */
  void __initialize(std::string name, int pipe_in, int pipe_out, bool open) {
    set_name(name);
    pipe_in_ = pipe_in;
    pipe_out_ = pipe_out;
    open_ = open;
  }

 public:
  /**
   * @brief Default constructor, for use with a server
   */
  pipe() : impl::named_resource_base(DEFAULT_PIPE_NAME), mutex_(DEFAULT_PIPE_NAME), 
           pipe_in_(INVALID_PIPE), pipe_out_(INVALID_PIPE), open_(false) {}

  /**
   * @brief Main constructor, creates a named pipe
   * @param name identifier for connecting to an existing inter-process pipe
   */
  pipe(const std::string& name) : impl::named_resource_base(name),
      mutex_(name + std::string(PIPE_CLIENT_MUTEX_NAME_SUFFIX)),
      pipe_in_(INVALID_PIPE), pipe_out_(INVALID_PIPE), open_(false) {}

 private:
  pipe(const pipe &) DELETE_METHOD;
  pipe &operator=(const pipe &) DELETE_METHOD;

 public:

  /**
   * @brief Move-constructor
   * @param other pipe to move
   */
  pipe(pipe&& other) : impl::named_resource_base(DEFAULT_PIPE_NAME), mutex_(DEFAULT_PIPE_NAME),
                       pipe_in_(INVALID_PIPE), pipe_out_(INVALID_PIPE), open_(false) {
    *this = std::move(other);
  }

  /**
   * @brief Move-assignment
   * @param other pipe to copy
   * @return reference to myself
   */
  pipe &operator=(pipe&& other) {
    __initialize(other.name(), other.pipe_in_, other.pipe_out_, other.open_);
    other.set_name(DEFAULT_PIPE_NAME);
    other.pipe_in_ = INVALID_PIPE;
    other.pipe_out_ = INVALID_PIPE;
    other.open_ = false;
    return *this;
  }
  
  /**
   * @brief Destructor, closes pipe if not already closed
   */
  ~pipe() {
    close();
  }

  /**
   * @brief Opens pipe if not already open, attempts to connect
   * to pipe server.
   *
   * @return true if connection established, false otherwise
   */
  bool open() {

    // don't open if already opened
    if (open_) {
      return false;
    }

    // server to client
    std::string name_sc = PIPE_NAME_PREFIX;
    name_sc.append(id_ptr());
    name_sc.append(PIPE_SC_NAME_SUFFIX);

    // client to server
    std::string name_cs = PIPE_NAME_PREFIX;
    name_cs.append(id_ptr());
    name_cs.append(PIPE_CS_NAME_SUFFIX);

    // protect opening of connection
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    // create client-to-server pipe
    ::unlink(name_cs.c_str());  // unlink outgoing
    int status_cs = mkfifo(name_cs.c_str(), S_IRWXU);
    if (status_cs != 0) {
      ::close(pipe_in_);
      pipe_in_ = INVALID_PIPE;
      cpen333::perror(std::string("Failed to create client-to-server pipe ") + name_cs);
      return false;
    }

    // try to opening pipe from server
    auto started = std::chrono::system_clock::now();
    while(pipe_in_ == INVALID_PIPE) {
      // server to client first, then client to server
      pipe_in_ = ::open(name_sc.c_str(), O_RDONLY);
      if (pipe_in_ < 0) {
        auto t = std::chrono::system_clock::now();
        auto l = std::chrono::duration_cast<std::chrono::milliseconds>(t-started);
        if (l.count() > PIPE_TIMEOUT) {
          // failed to open pipe
          cpen333::perror("Failed to open server-to-client pipe (timeout)");
          ::unlink(name_cs.c_str());
          return false;
        }
      }
    }

    pipe_out_ = ::open(name_cs.c_str(), O_WRONLY);
    if (pipe_out_ < 0) {
      // failed to open pipe
      cpen333::perror("Failed to open client-to-server pipe");
      ::close(pipe_in_);
      pipe_in_ = INVALID_PIPE;
      pipe_out_ = INVALID_PIPE;
      ::unlink(name_sc.c_str());  // unlink outgoing pipe
      return false;
    }

    ::unlink(name_cs.c_str());  // unlink outgoing pipe

    open_ = true;
    return true;
  }

  /**
   * @brief Writes a string to the pipe, including the terminating zero
   * @param str string to send, length+1 must fit into a signed integer
   * @return true if send successful, false otherwise
   */
  bool write(const std::string& str) {
    return write(str.c_str(), (int)(str.length()+1));
  }

  /**
   * @brief Writes bytes to the pipe
   *
   * @param buff pointer to data buffer to send
   * @param size number of bytes to send
   * @return true if send successful, false otherwise
   */
  bool write(const void* buff, int size) {

    if (!open_) {
      return false;
    }

    // write all contents
    int nwrite = 0;
    const char* cbuff = (const char*)buff;

    while (nwrite < size) {
      int lwrite = (int)(::write(pipe_out_, &cbuff[nwrite], size-nwrite));
      if (lwrite < 0) {
        cpen333::perror("Pipe write(...) failed");
        return false;
      }
      nwrite += lwrite;
    }

    return true;
  }

  /**
   * @brief Reads bytes of data from a pipe
   * @param buff pointer to data buffer to populate
   * @param size size of buffer
   * @return number of bytes read, 0 if pipe is closed, or -1 if error
   */
  int read(void* buff, int size) {

    if (!open_) {
      return -1;
    }

    int nread = (int)(::read(pipe_in_, buff, size));
    if ( nread < 0 ) {
      cpen333::perror("Pipe read(...) failed");
      return -1;
    }

    return nread;
  }

  /**
   * @brief Closes the pipe
   * @return true if successful, false otherwise
   */
  bool close() {

    if (!open_) {
      return false;
    }

    // cleanup
    int success_in = ::close(pipe_in_);
    if (success_in < 0) {
      cpen333::perror("Failed to close input pipe");
    }
    int success_out = ::close(pipe_out_);
    if (success_out < 0) {
      cpen333::perror("Failed to close output pipe");
    }
    pipe_in_ = INVALID_PIPE;
    pipe_out_ = INVALID_PIPE;
    open_ = false;

    return (success_in == 0) && (success_out == 0);
  }

  /**
   * @copydoc impl::named_resource_base::unlink()
   */
  bool unlink() {
    std::string name_cs = PIPE_NAME_PREFIX;
    name_cs.append(id_ptr());
    name_cs.append(PIPE_CS_NAME_SUFFIX);
    int success_cs = ::unlink(name_cs.c_str());
    if (success_cs != 0) {
      cpen333::perror(std::string("Failed to unlink pipe with id ")+name_cs);
    }

    std::string name_sc = PIPE_NAME_PREFIX;
    name_sc.append(id_ptr());
    name_sc.append(PIPE_SC_NAME_SUFFIX);
    int success_sc = ::unlink(name_sc.c_str());
    if (success_sc != 0) {
      cpen333::perror(std::string("Failed to unlink pipe with id ")+name_sc);
    }

    bool success = mutex_.unlink();

    return success && (success_cs == 0) && (success_sc == 0);
  }

  /**
   * @copydoc impl::named_resource_base::unlink(const std::string&)
   */
  static bool unlink(const std::string& name) {
    char nm[MAX_RESOURCE_ID_SIZE];
    impl::named_resource_base::make_resource_id(name, nm);

    std::string name_cs = PIPE_NAME_PREFIX;
    name_cs.append(nm);
    name_cs.append(PIPE_CS_NAME_SUFFIX);
    int success_cs = ::unlink(name_cs.c_str());
    if (success_cs != 0) {
      cpen333::perror(std::string("Failed to unlink pipe with id ")+name_cs);
    }

    std::string name_sc = PIPE_NAME_PREFIX;
    name_sc.append(nm);
    name_sc.append(PIPE_SC_NAME_SUFFIX);
    int success_sc = ::unlink(name_sc.c_str());
    if (success_sc != 0) {
      cpen333::perror(std::string("Failed to unlink pipe with id ")+name_sc);
    }

    bool b1 = cpen333::process::mutex::unlink(name + std::string(PIPE_SERVER_MUTEX_NAME_SUFFIX));
    bool b2 = cpen333::process::mutex::unlink(name + std::string(PIPE_CLIENT_MUTEX_NAME_SUFFIX));

    return b1 && b2 && (success_cs == 0) && (success_sc == 0);
  }
};


/**
 * @brief Pipe server
 *
 * Implementation of a named pipe server that listens
 * for connections.  The server is NOT started automatically.
 * To start listening for connections, call the start() function.
 */
class pipe_server : private impl::named_resource_base {
  cpen333::process::mutex mutex_;
  bool open_;

 public:
  /**
   * @brief Default constructor, creates a pipe server that listens for new connections
   */
  pipe_server(const std::string& name) : impl::named_resource_base(name),
      mutex_(name + std::string(PIPE_SERVER_MUTEX_NAME_SUFFIX)), open_(false) { }

 private:
  pipe_server(const pipe_server &) DELETE_METHOD;
  pipe_server(pipe_server &&) DELETE_METHOD;
  pipe_server &operator=(const pipe_server &) DELETE_METHOD;
  pipe_server &operator=(pipe_server &&) DELETE_METHOD;

 public:
  /**
   * @brief Destructor, closes the pipe server
   */
  ~pipe_server() {
    close();
  }

  /**
   * @brief Starts listening for connections.
   * @return true if successful, false otherwise.
   */
  bool start() {

    if (open_){
      return false;
    }

    // nothing really to do...

    open_ = true;
    return true;
  }

  /**
   * @brief Accepts a client connection
   *
   * This method will block until a client connects to the pipe server.
   * Upon connection, the provide client is populated and automatically
   * opened.
   *
   * @param client connected pipe client
   * @return true if successful, false otherwise
   */
  bool accept(pipe& client) {
    if (!open_) {
      return false;
    }

    std::string name_sc = PIPE_NAME_PREFIX;
    name_sc.append(id_ptr());
    name_sc.append(PIPE_SC_NAME_SUFFIX);

    std::lock_guard<decltype(mutex_)> lock(mutex_);

    // create named pipe
    ::unlink(name_sc.c_str());
    int status_sc = mkfifo(name_sc.c_str(), S_IRWXU);
    if (status_sc != 0) {
      cpen333::perror(std::string("Failed to create server-to-client pipe ") + name_sc);
      return false;
    }

    // connect pipe
    int pipe_sc = open(name_sc.c_str(), O_WRONLY);
    if (pipe_sc < 0) {
      ::unlink(name_sc.c_str());  // unlink outgoing
      cpen333::perror(std::string("Failed to open server-to-client pipe ") + name_sc);
      return false;
    }

    std::string name_cs = PIPE_NAME_PREFIX;
    name_cs.append(id_ptr());
    name_cs.append(PIPE_CS_NAME_SUFFIX);

    int pipe_cs = open(name_cs.c_str(), O_RDONLY);
    if (pipe_cs < 0) {
      ::unlink(name_sc.c_str());  // unlink outgoing
      ::close(pipe_sc);
      cpen333::perror(std::string("Failed to open client-to-server pipe ") + name_cs);
      return false;
    }
    ::unlink(name_sc.c_str());

    client.close();
    client.__initialize(id_ptr(), pipe_cs, pipe_sc, true);

    return true;
  }

  /**
   * @brief Close the server socket
   * @return true if successful, false otherwise
   */
  bool close() {
    if (!open_) {
      return false;
    }
    open_ = false;
    return true;
  }

  bool unlink() {
    std::string name_cs = PIPE_NAME_PREFIX;
    name_cs.append(id_ptr());
    name_cs.append(PIPE_CS_NAME_SUFFIX);
    int success_cs = ::unlink(name_cs.c_str());
    if (success_cs != 0) {
      cpen333::perror(std::string("Failed to unlink pipe with id ")+name_cs);
    }

    std::string name_sc = PIPE_NAME_PREFIX;
    name_sc.append(id_ptr());
    name_sc.append(PIPE_SC_NAME_SUFFIX);
    int success_sc = ::unlink(name_sc.c_str());
    if (success_sc != 0) {
      cpen333::perror(std::string("Failed to unlink pipe with id ")+name_sc);
    }

    bool success = mutex_.unlink();

    return success && (success_cs == 0) && (success_sc == 0);
  }

  /**
   * @copydoc impl::named_resource_base::unlink(const std::string&)
   */
  static bool unlink(const std::string& name) {
    char nm[MAX_RESOURCE_ID_SIZE];
    impl::named_resource_base::make_resource_id(name, nm);

    std::string name_cs;
    name_cs.append(nm) = PIPE_NAME_PREFIX;
    name_cs.append(PIPE_CS_NAME_SUFFIX);
    int success_cs = ::unlink(name_cs.c_str());
    if (success_cs != 0) {
      cpen333::perror(std::string("Failed to unlink pipe with id ")+name_cs);
    }

    std::string name_sc = PIPE_NAME_PREFIX;
    name_sc.append(nm);
    name_sc.append(PIPE_SC_NAME_SUFFIX);
    int success_sc = ::unlink(name_sc.c_str());
    if (success_sc != 0) {
      cpen333::perror(std::string("Failed to unlink pipe with id ")+name_sc);
    }

    bool b1 = cpen333::process::mutex::unlink(name + std::string(PIPE_SERVER_MUTEX_NAME_SUFFIX));
    bool b2 = cpen333::process::mutex::unlink(name + std::string(PIPE_CLIENT_MUTEX_NAME_SUFFIX));

    return b1 && b2 && (success_cs == 0) && (success_sc == 0);
  }
};

} // posix

/**
 * @brief POSIX implementation of a pipe client
 */
typedef posix::pipe pipe;

/**
 * @brief POSIX implementation of a pipe server
 */
typedef posix::pipe_server pipe_server;

} // process
} // cpen333

// undef local macros
#undef PIPE_NAME_PREFIX
#undef PIPE_SC_NAME_SUFFIX
#undef PIPE_CS_NAME_SUFFIX
#undef PIPE_CLIENT_MUTEX_NAME_SUFFIX
#undef PIPE_SERVER_MUTEX_NAME_SUFFIX
#undef DEFAULT_PIPE_NAME
#undef INVALID_PIPE
#undef PIPE_TIMEOUT

#endif