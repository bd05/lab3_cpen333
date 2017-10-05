/**
 * @file
 * @brief Timer implementation with callback function capability
 */
#ifndef CPEN333_THREAD_TIMER_H
#define CPEN333_THREAD_TIMER_H

#include <functional>  // for storing std::function<void()>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "semaphore.h"

namespace cpen333 {
namespace thread {

namespace detail {
/**
 * @brief No-op functor
 */
class noop_function_t {
 public:
  noop_function_t() {}    // user-defined constructor for const type
  void operator () () const {}
};

/**
 * @brief Basic no-operation function
 */
const noop_function_t noop_function;  // constant instance

/**
 * @brief runs events in a separate thread
 * @tparam T functor type to run, supports operator ()
 */
template<typename T>
class runner {

  T func_;
  std::mutex mutex_;
  std::condition_variable cv_;
  size_t count_;   // run count
  bool terminate_; // not accepting any more
  std::unique_ptr<std::thread> thread_;

  void run() {

    // do not run just yet
    std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
    size_t cc = 0;  // number of times run
    
    while(!terminate_) {
      // wait until terminate or one more item
      lock.lock();
      {
        // reduce count by number of times we have since ran func_()
        count_ -= cc;
        cv_.wait(lock, [&]() { return terminate_ || count_ > 0; });
        cc = count_;    // current count is the # of times to run func_() now
      }
      lock.unlock();  // relinquish lock
      
      // call function cc times in a row, outside of lock so notify() will not block
      // for long-lasting functions
      for (size_t i=0; i<cc; ++i) {
        func_();
      }
    }
  }

 public:
  runner(T&& func) : func_(std::move(func)), mutex_(), cv_(), 
                     count_(0), terminate_(0), thread_(nullptr) {}

  void start() {
    // spawn new thread
    thread_ = std::unique_ptr<std::thread>(new std::thread(&runner::run, this));
  }
  
  void terminate() {
    std::lock_guard<std::mutex> lock(mutex_);
    terminate_ = true;
    cv_.notify_one();
  }

  ~runner() {
    terminate();
    if (thread_.get() != nullptr) {
      thread_->join();  // need to "join", otherwise thread will refer to non-existent variables
    }
  }

  void notify() {
    { // localized lock
      std::lock_guard<std::mutex> lock(mutex_);
      ++count_;
    }
    cv_.notify_one();
  }
};

}

/**
 * @brief Timer implementation
 *
 * Allows tracking of timer ticks or running a callback functor
 * at a regular tick interval.
 *
 * The timer is NOT started automatically.  It must be started by calling
 * start().
 *
 * @tparam Duration tick duration type
 */
template<typename Duration>
class timer {
 public:

  /**
   * @brief Creates a basic timer
   *
   * The timer is NOT started automatically.  It must be started by calling
   * start().
   *
   * @param period tick interval
   */
  timer(const Duration& period) : timer(period, detail::noop_function) {}

  /**
   * @brief Creates a timer with a callback function
   *
   * The callback function func() is executed on every tick.  The function
   * execution time should be well within a single tick period.  Callbacks are
   * executed in a single thread, meaning that if one execution does not
   * finish before the next tick, calls will be accumulated and run
   * sequentially.
   *
   * The timer is NOT started automatically.  It must be started by calling
   * start().
   *
   * @tparam Func callback function type
   * @param period tick interval
   * @param func callback function
   */
  template<typename Func>
  timer(const Duration& period, Func &&func) :
    time_(period), ring_(false), run_(false), terminate_(false),
    runner_(std::forward<Func>(func)),
    mutex_(), cv_(),
    thread_(nullptr) {
    runner_.start();  // start new thread running
    // ensure created after all memory is allocated, otherwise may run into errors in run()
    thread_ = new std::thread(&timer::run, this);
  }

 private:
  timer(const timer &) DELETE_METHOD;
  timer(timer &&) DELETE_METHOD;
  timer &operator=(const timer &) DELETE_METHOD;
  timer &operator=(timer &&) DELETE_METHOD;

 public:
  
  /**
   * @brief Start timer running
   *
   * Resets clock to zero and "test" flag
   */
  void start() {
    {
      std::lock_guard<decltype(mutex_)> lock(mutex_);
      run_ = true;    // start your races
      ring_ = false;  // turn off last ring
    }
    cv_.notify_all();
  }

  /**
   * @brief Stops timer running
   *
   * Leaves "test" flag intact to see if timer has gone off
   */
  void stop() {
    {
      std::lock_guard<decltype(mutex_)> lock(mutex_);
      run_ = false;  // stop running
    }
    cv_.notify_all();
  }

  /**
   * @brief Checks if timer is running
   * @return true if running, false otherwise
   */
  bool running() {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    return run_;
  }

  /**
   * @brief Waits until the next tick event
   *
   * Blocks the current thread until the next tick event, or the
   * timer is stopped.
   */
  void wait() {
    // wait until next event
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    cv_.wait(lock);
  }

  /**
   * @brief Tests if timer has gone off since last reset
   * @return true if timer has gone off
   */
  bool test() {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    return ring_;
  }

  /**
   * @brief Test if timer has gone off since last call, and resets flag
   * @return true if timer has gone off since last call
   */
  bool test_and_reset() {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    if (ring_) {
      ring_ = false;
      return true;
    }
    return false;
  }

  /**
   * @brief Destructor
   *
   * Runs any pending callbacks and terminates the timer
   */
  ~timer() {
    // signal thread to terminate
    {
      std::lock_guard<decltype(mutex_)> lock(mutex_);
      terminate_ = true;
      cv_.notify_all();
    }
    // let thread finish, since refers to member data
    thread_->join();
    delete thread_;
    thread_ = nullptr;
  }

 private:

  void run() {

    // wait for first start or until terminated
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&](){return run_ || terminate_;});

    auto tick_ = std::chrono::steady_clock::now() + time_;
    while(!terminate_) {
      // wait until awoken on purpose, or time-out
      if (cv_.wait_until(lock, tick_,
                     [&](){ return !run_ || terminate_; })) {

        // we've hit stop, wait again until start is hit
        cv_.wait(lock, [&](){return run_ || terminate_;});
        std::chrono::steady_clock::now() + time_;  // start time from now

      } else {
        // timeout, run callback
        tick_ += time_;    // set up next tick immediately so no time is wasted
        ring_ = true;      // let people know timer has gone off
        cv_.notify_all();  // wake up anyone waiting for event

        // notify runner to run another instance
        runner_.notify();
      }
    }
  }

  Duration time_;
  bool ring_;
  bool run_;
  bool terminate_;         // signal to terminate
  detail::runner<std::function<void()>>   runner_;  // callback
  std::mutex mutex_;                // controls terminate
  std::condition_variable cv_;      // controls waiting
  std::thread* thread_;             // timer thread
};

} // thread
} // cpen333

#endif //CPEN333_THREAD_TIMER_H
