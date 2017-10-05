/**
 * @file
 * @brief Named-resource wrapper for RAII-style unlinking of the resource name
 */
#ifndef CPEN333_PROCESS_UNLINKER_H
#define CPEN333_PROCESS_UNLINKER_H

namespace cpen333 {
namespace process {

/**
 * @brief A named-resource wrapper that provides a convenient RAII-style unlinking of the resource name
 *
 * Used to ensure that a resource's name is unlinked when the unlinker object drops out of scope.  This should
 * prevent resource leaking in case of exceptions.
 *
 * @tparam T named-resource type, must extend cpen333::process::named_resource and support `bool unlink()` and
 *         `static bool unlink(std::string&)`
 */
template<typename T>
class unlinker {
 public:
  /**
   * @brief named_resource type
   */
  using type = T;

  /**
   * @brief Constructs the object, wrapping the provided resource
   *
   * @param resource resource to unlink when unlinker drops out of scope
   */
  unlinker(T &resource) : resource_{resource} {}

 private:
  unlinker(const unlinker &) DELETE_METHOD;
  unlinker(unlinker &&) DELETE_METHOD;
  unlinker &operator=(const unlinker &) DELETE_METHOD;
  unlinker &operator=(unlinker &&) DELETE_METHOD;

 public:
  /**
   * @brief Destructor, calls the `unlink()` function of the wrapped resource
   */
  ~unlinker() {
    resource_.unlink();
  }

  /**
   * @brief Statically calls the `unlink(name)` function for the underlying type
   * @param name name of resource to unlink
   * @return always `false`, since unlinking is not supported on Windows
   */
  static bool unlink(const std::string &name) {
    return type::unlink(name);
  }

 private:
  T &resource_;
};

} // process
} // cpen333

#endif //CPEN333_PROCESS_UNLINKER_H
