/**
 * @file
 * @brief Inter-process shared semaphore implementation
 */
#ifndef CPEN333_PROCESS_SEMAPHORE_H
#define CPEN333_PROCESS_SEMAPHORE_H

#include "../os.h" // identify OS

#ifdef WINDOWS
#include "impl/windows/semaphore.h"
#else
#include "impl/posix/semaphore.h"
#endif

/**
 * @class cpen333::process::semaphore
 * @brief An inter-process semaphore synchronization primitive
 *
 * Used to protect access to a counted resource shared by multiple processes.  This is an alias to either
 * cpen333::process::posix::semaphore or cpen333::process::windows::semaphore depending on your platform.
 */

#include "impl/semaphore_guard.h"

#endif //CPEN333_PROCESS_SEMAPHORE_H
