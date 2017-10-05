/**
 * @file
 * @brief Basic inter-process message queue implementation based on a FIFO
 */

#ifndef CPEN333_PROCESS_MESSAGE_QUEUE_H
#define CPEN333_PROCESS_MESSAGE_QUEUE_H

/**
 * @brief Suffix to add to the internal message queue name for uniqueness
 */
#define MESSAGE_QUEUE_SUFFIX "_mq"

#include <string>
#include <chrono>
#include "fifo.h"
#include "named_resource.h"

namespace cpen333 {
namespace process {

/**
 * @brief Basic inter-process named message queue based on a FIFO
 *
 * Allows sending/receiving of messages with a fixed type.  Unlike Windows messages, does not require you to know the
 * thread ID of the receiver, but also does not allow filtering on the receiving end.  Unlike a POSIX message queue,
 * does not allow variable length messages or message priorities.
 *
 * @tparam MessageType fixed type of messages.
 */
template<typename MessageType>
class message_queue : public virtual named_resource {
 public:
  /**
   * @brief Message type
   */
  typedef MessageType message_type;

  /**
   * @brief Constructs a named message queue
   *
   * @param name name identifier for creating or connecting to an existing inter-process message-queue
   * @param size if creating, the maximum number of elements that can be stored in the queue without blocking
   */
  message_queue(const std::string& name, size_t size = 1024) :
      fifo_(name + std::string(MESSAGE_QUEUE_SUFFIX), size) {}

  /**
   * @brief Sends a message to the queue
   * @param msg message to send
   */
  void send(const MessageType& msg) {
    fifo_.push(msg);
  }

  /**
   * @brief Tries to send a message without blocking
   *
   * If it is not possible to send the message without blocking, then will return immediately without
   * sending.
   *
   * @param msg message to send
   * @return `true` if message sent successfully, `false` if not sent
   */
  bool try_send(const MessageType& msg) {
    return fifo_.try_push(msg);
  }

  /**
   * @brief Tries to send a message, will wait for a maximum amount of time before aborting
   *
   * If it is not possible to send the message immediately, then the current thread
   * will block until either the message is sent successfully, or a timeout period has elapsed.
   *
   * @tparam Rep timeout duration representation
   * @tparam Period timeout duration period
   * @param msg message to send
   * @param rel_time relative timeout time
   * @return `true` if the message is sent successfully within the timeout period, `false` if not sent
   */
  template <typename Rep, typename Period>
  bool try_send_for(const MessageType& msg, std::chrono::duration<Rep, Period>& rel_time) {
    return try_send_until(msg, std::chrono::steady_clock::now()+rel_time);
  }

  /**
   * @brief Tries to send a message, will wait until a timeout time has been reached before aborting
   *
   * If it is not possible to send the message immediately, then the current thread will block until
   * either the message is sent successfully, or an absolute timeout time has been reached.
   *
   * @tparam Clock timeout clock type
   * @tparam Duration timeout duration type
   * @param msg message to send
   * @param timeout absolute timeout time
   * @return `true` if message sent successfully before timeout time has passed, `false` if message not sent
   */
  template<typename Clock, typename Duration>
  bool try_send_until(const MessageType& msg, const std::chrono::time_point<Clock,Duration>& timeout) {
    return fifo_.try_push_until(msg, timeout);
  }

  /**
   * @brief Retrieves and removes the next message from the message queue
   *
   * If there are currently no messages in the queue, then the current thread will block until
   * a message becomes available.
   *
   * @return the next message in the queue
   */
  MessageType receive() {
    return fifo_.pop();
  }

  /**
   * @brief Retrieves and removes the next message from the message queue
   *
   * If there are currently no messages in the queue, then the current thread will block until a
   * message becomes available.
   *
   * @param out destination.  If `nullptr`, the message is removed from the queue but not returned
   */
  void receive(MessageType* out) {
    fifo_.pop(out);
  }

  /**
   * @brief Tries to receive a message without blocking
   *
   * Populates memory pointed to by `out` with the next message, and removes the message from the queue.
   * If there are no messages, then this will return immediately.
   *
   * @param out destination.  If `nullptr`, the message is removed but not returned.
   * @return `true` if a message is returned, `false` otherwise
   */
  bool try_receive(MessageType* out) {
    return fifo_.try_pop(out);
  }

  /**
   * @brief Tries to receive a message, will wait for a maximum amount of time before aborting
   *
   * If it is not possible to receive a message immediately, then the current thread
   * will block until either a message becomes available, or a timeout period has elapsed.
   *
   * @tparam Rep duration representation
   * @tparam Period duration period
   * @param out destination.  If `nullptr`, the next message is removed but not returned
   * @param rel_time relative timeout time
   * @return `true` if message successfully returned, `false` if timeout elapsed
   */
  template <typename Rep, typename Period>
  bool try_receive_for(MessageType* out, std::chrono::duration<Rep, Period>& rel_time) {
    return try_receive_until(out, std::chrono::steady_clock::now()+rel_time);
  }

  /**
   * @brief Tries to receive a message, will wait for a maximum timeout time to be reached before aborting
   *
   * If it is not possible to receive a message immediately, then the current thread will block until either
   * a message becomes available, or a timeout time has been reached.
   *
   * @tparam Clock clock type
   * @tparam Duration clock duration type
   * @param out destination.  If `nullptr`, the next message is removed but not returned
   * @param timeout absolute timeout time
   * @return `true` if message successfully returned, `false` if timeout
   */
  template<typename Clock, typename Duration>
  bool try_receive_until(MessageType* out, const std::chrono::time_point<Clock,Duration>& timeout) {
    return fifo_.try_pop_until(out, timeout);
  }

  /**
   * @brief Peeks at the next message without removing it.
   *
   * Returns the next message in the queue.  If there are currently no messages, this method will block until one
   * becomes available.  The message will remain in the queue for the next `peek` or `receive` operation.
   *
   * @return next message in the queue
   */
  MessageType peek() {
    return fifo_.peek();
  }

  /**
   * @brief Peeks at the next message without removing it.
   *
   * Populates memory pointed to by `out` with next message in the queue.  If there are currently no messages,
   * this method will block until a message becomes available.  The message will remain in the
   * queue for the next `peek` or `receive` operation.
   *
   * @param out destination.  If `nullptr`, nothing happens.
   */
  void peek(MessageType* out) {
    fifo_.peek(out);
  }

  /**
   * @brief Tries to peek at the message without blocking
   *
   * Populates memory pointed to by `out` with the next message in the queue.  If there are no messages, then this
   * will return immediately without peeking. The message will remain in the queue for the next `peek` or
   * `receive` operation.
   *
   * @param out destination.  If `nullptr`, nothing happens.
   * @return `true` if message was successfully peeked, `false` otherwise
   */
  bool try_peek(MessageType* out) {
    return fifo_.try_peek(out);
  }

  /**
   * @brief Tries to peek at the next message, will wait for a maximum amount of time before aborting
   *
   * If it is not possible to peek at the next message immediately, then the current thread
   * will block until either a message becomes available, or a timeout period has elapsed.
   * The message will remain in the queue for the next `peek` or `receive` operation.
   *
   * @tparam Rep duration representation
   * @tparam Period duration period
   * @param out destination.  If `nullptr`, nothing happens
   * @param rel_time relative timeout time
   * @return `true` if message successfully peeked, `false` if timeout elapsed
   */
  template <typename Rep, typename Period>
  bool try_peek_for(MessageType* out, std::chrono::duration<Rep, Period>& rel_time) {
    return try_peek_until(out, std::chrono::steady_clock::now()+rel_time);
  }

  /**
   * @brief Tries to peek at the next message, will wait for a maximum timeout time before aborting
   *
   * If it is not possible to peek at the next message immediately, then the current thread will block
   * until either a message becomes available, or a timeout time has been reached.
   * The message will remain in the queue for the next `peek` or `receive` operation.
   *
   * @tparam Clock clock type
   * @tparam Duration clock duration type
   * @param out destination.  If `nullptr`, nothing happens
   * @param timeout absolute timeout time
   * @return `true` if message successfully peeked, `false` if timeout
   */
  template<typename Clock, typename Duration>
  bool try_peek_until(MessageType* out, const std::chrono::time_point<Clock,Duration>& timeout) {
    return fifo_.try_peek_until(out, timeout);
  }

  /**
   * @brief Number of messages currently in the queue
   *
   * This method should be used sparingly, since messages could be added/removed during or immediately after the call,
   * making the result potentially unreliable.
   *
   * @return number of messages
   */
  size_t size() {
    return fifo_.size();
  }

  /**
   * @brief Check if the message queue is currently empty
   *
   * This method should be used sparingly, since messages could be added/removed during or immediately after the call,
   * making the result potentially unreliable
   *
   * @return `true` if empty, `false` otherwise
   */
  bool empty() {
    return fifo_.empty();
  }

  bool unlink() {
    return fifo_.unlink();
  }

  /**
   * @copydoc cpen333::process::named_resource::unlink(const std::string&)
   */
  static bool unlink(const std::string& name) {
    return cpen333::process::fifo<MessageType>::unlink(name + std::string(MESSAGE_QUEUE_SUFFIX));
  }

 private:
  cpen333::process::fifo<MessageType> fifo_;

};

} // process
} // namespace

// undefine local macros
#undef MESSAGE_QUEUE_SUFFIX

#endif //CPEN333_PROCESS_MESSAGE_QUEUE_H
