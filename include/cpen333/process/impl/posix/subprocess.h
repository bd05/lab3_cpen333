/**
 * @file
 * @brief POSIX implementation of a child process
 */
#ifndef CPEN333_PROCESS_POSIX_SUBPROCESS_H
#define CPEN333_PROCESS_POSIX_SUBPROCESS_H

#include <string>
#include <vector>
#include <chrono>
#include <cstdlib> // for std::quick_exit
#include <thread>  // for yield

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <wordexp.h>

#include "../../../util.h"

namespace cpen333 {
namespace process {
namespace posix {

/**
 * @brief A child process
 *
 * Allows launching of a child process, with environment inherited from the current process.
 */
class subprocess {
 private:
  pid_t pid_;
  std::vector<std::string> exec_;
  bool detached_;
  bool started_;
  bool terminated_;

 public:
  /**
   * @brief Alias to native handle type, on POSIX is pid_t
   */
  using native_handle_type = pid_t;

  /**
   * @brief Constructs a new subprocess
   *
   * The new process will run the command exec[0] with argv parameters {exec[1], exec[2], ...}.  A detached child
   * process will run in a separate thread, concurrently with the parent process.  If not detached, the parent will wait
   * for a running child to complete.
   *
   * @param exec command and arguments to execute
   * @param start whether to start the subprocess immediately
   * @param detached run the subprocess in `detached' mode
   */
  subprocess(const std::vector<std::string> &exec, bool start = true, bool detached = false) :
      pid_{-1}, exec_{exec}, detached_{detached}, started_{false}, terminated_{false} {
    if (start) {
      this->start();
    }
  }

  /**
   * @brief Constructs a new subprocess from a command string
   *
   * The new process will run the command cmd with argv parameters parsed from the string.  A detached child
   * process will run in a separate thread, concurrently with the parent process.  If not detached, the parent will wait
   * for a running child to complete.
   *
   * NOTE: this is much less safe than subprocess(const std::vector<std::string>,bool,bool), which should
   * be preferred when there are multiple arguments or if string parsing might be ambiguous
   *
   * @param cmd command, potentially containing arguments to parse
   * @param start whether to start the subprocess immediately
   * @param detached run the process in `detached' mode
   */
  subprocess(const std::string &cmd, bool start = true, bool detached = false) :
      pid_{-1}, exec_{}, detached_{detached}, started_{false}, terminated_{false} {

    wordexp_t p;
    char **w;

    wordexp(cmd.c_str(), &p, 0);
    w = p.we_wordv;
    for (size_t i = 0; i < p.we_wordc; i++) {
      printf("%s\n", w[i]);
      exec_.push_back(w[i]);
    }
    wordfree(&p);

    if (start) {
      this->start();
    }
  }

  /**
   * @brief Starts the subprocess
   * @return true if started, false if process already started or an error occurs
   */
  bool start() {

    if (started_) {
      return false;
    }

    // fork/exec
    pid_ = fork();
    if (pid_ == 0) {
      // we are in child
      std::vector<char*> c;
      for(size_t i = 0; i < exec_.size(); ++i){
        char *str = (char*)(&exec_[i][0]);
        c.push_back(str);
      }
      c.push_back(nullptr);  // null-terminated array of strings for execvp

      if (detached_) {
        // pid_t sid =
        setsid(); // detach process
      }
      int status = execvp(&(exec_[0][0]), c.data());
      cpen333::perror("Cannot create subprocess ");
      //std::quick_exit(status); // execvp failed, terminate child
      std::_Exit(status); // OSX doesn't seem to have quick_exit() defined
    }

    bool success = (pid_ >= 0);
    if (!success) {
      cpen333::perror("Failed to create process ");
    } else {
      started_ = true;
    }
    return success;
  }

  /**
   * @brief Waits for the subprocess to complete execution
   * @return true if joined successfully, false if the process has already been joined or if an error occurs
   */
  bool join() {
    if (pid_ < 0 || terminated_) {
      return false;
    }

    int status = 0;
    errno = 0;
    // loop while pid still running or interrupted
    while (waitpid(pid_, &status, 0) != -1 || errno == EINTR) {
    };

    if ( !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      cpen333::perror(std::string("Failed to wait for process "));
      return false;
    }
    terminated_ = true;
    return true;
  }

  /**
   * @brief Waits for the subprocess to complete execution
   *
   * Similar to join(), but will return true if the process has already been joined
   *
   * @return true if process terminates or is already terminated
   */
  bool wait() {
    if (terminated_) {
      return true;
    }
    return join();
  }

  /**
   * @brief Waits for the process to terminate up to a maximum amount of time
   * @tparam Rep  duration representation
   * @tparam Period  duration tick period
   * @param duration maximum relative time to wait for
   * @return true if process terminates or is already terminated
   */
  template<typename Rep, typename Period>
  bool wait_for(const std::chrono::duration<Rep,Period>& duration) {
    return wait_until(std::chrono::steady_clock::now()+duration);
  }

  /**
   * @brief Waits for the process to terminate up to a maximum absolute time
   * @tparam Clock timeout clock type
   * @tparam Duration timeout clock duration type
   * @param timeout_time absolute timeout time
   * @return true if process terminates or is already terminated
   */
  template< class Clock, class Duration >
  bool wait_until( const std::chrono::time_point<Clock,Duration>& timeout_time ) {

    // There is self-pipe trick to try to catch SIGCHLD, but here we will just poll
    // http://stackoverflow.com/questions/282176/waitpid-equivalent-with-timeout

    // already terminated
    if (terminated_) {
      return true;
    }

    int status = 0;
    int r = 0;
    do {
      errno = 0;
      r = waitpid(pid_, &status, WNOHANG);
      if (r < 0) {
        // error
        cpen333::perror(std::string("Failed to wait for process "));
        return false;
      }

      std::this_thread::yield();  // yield to other threads to prevent excessive polling
    } while (r != pid_ && std::chrono::steady_clock::now() < timeout_time);

    if ( r == pid_ && WIFEXITED(status) ) {
      terminated_ = true;
      int exstat = WEXITSTATUS(status);
      if (exstat != 0) {
        cpen333::perror(std::string("Process terminated with exit code ") + std::to_string(exstat));
      }
    }

    return terminated_;

  }

  /**
   * @brief Checks if the process has already been terminated
   * @return true if process is terminated
   */
  bool terminated() {
    if (terminated_) {
      return true;
    }
    // check wait
    return wait_for(std::chrono::milliseconds(0));
  }

  /**
   * @brief Force the process to terminate
   * @return true if successful
   */
  bool terminate() {
    int status = kill(pid_, SIGKILL);
    if (status != 0) {
      cpen333::perror("Failed to terminate process.");
      return false;
    }
    return true;
  }

};

} // native implementation

/**
 * @brief Alias to POSIX implementation of a child process
 */
using subprocess = posix::subprocess;

} // process
} // cpen333

#endif //CPEN333_PROCESS_POSIX_SUBPROCESS_H
