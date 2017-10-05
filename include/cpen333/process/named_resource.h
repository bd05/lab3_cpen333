/**
 * @file
 * @brief Base for all named inter-process resources
 */
#ifndef CPEN333_PROCESS_NAMED_RESOURCE_H
#define CPEN333_PROCESS_NAMED_RESOURCE_H

#include <string>
#include "../util.h"

namespace cpen333 {
namespace process {

/**
 * @brief Pure virtual base class for all inter-process resources
 *
 * Named resources are accessed using an identifying name during construction.  If the name and resource
 * exists, it will connecting to the existing version.  Otherwise, a new resource will be allocated.
 * The resource will persist as long as processes are using it.
 *
 * On POSIX systems, a named resource has kernel persistence, meaning it will continue to exist until
 * either the name is manually "unlinked" from the resource, or until the next reboot.
 * If unlinked, the name is detached from the resource, but the resource will continue to exist and be
 * accessible until all current users have finished using it.  If any new users try to access the resource
 * with the previous unlinked name, it will be viewed as a new usage, and a new separate resource will be
 * allocated.
 *
 * On Windows, the named resource will persist until all users have released it.  There is not concept
 * of "unlinking" in Windows.
 */
class named_resource {
 public:

  /**
   * @brief Virtual destructor
   */
  virtual ~named_resource() {};

  /**
 * @brief Detaches the name from the named resource
 *
 * On POSIX systems, named resources will persist beyond the lifetime of any process that uses them as
 * long as the name has not been unlinked (or until the system is rebooted).  Calling `unlink` will detach
 * the name, allowing the resource to be freed once all current users have exited.
 *
 * @return `true` if unlink is successful, `false` if unlinking is not supported or if an error has
 * occurred.
 */
  virtual bool unlink() = 0;

  /**
   * @brief Unlinks the name without needing to create a resource
   *
   * Implementers should also provide a static method for unlinking.  The purpose
   * is mainly for clean-up of existing resources.
   *
   * @param name desired resource name
   * @return `true` if unlink successful, `false` if not successful or not supported
   */
  static bool unlink(const std::string& name) {
    UNUSED(name);
    return false;
  }

};

}
} // cpen333

#endif //CPEN333_PROCESS_NAMED_RESOURCE_H
