/**
 * @file
 * @brief Inter-process pipe implementation
 */
#ifndef CPEN333_PROCESS_PIPE_H
#define CPEN333_PROCESS_PIPE_H

#include "../os.h"
#ifdef WINDOWS
#include "impl/windows/pipe.h"
#else
#include "impl/posix/pipe.h"
#endif

/**
 * @class cpen333::process::pipe
 * @brief A client pipe implementation for inter-process communication
 *
 * Used to communicate between processes on the same machine over a bi-directional channel.  This is an alias to either
 * cpen333::process::posix::pipe or cpen333::process::windows::pipe
 * depending on your platform.
 */

/**
 * @class cpen333::process::pipe_server
 * @brief A pipe server implementation for inter-process communication
 *
 * Used to listen for connections from other processes.  This is an alias to either
 * cpen333::process::posix::pipe_server or cpen333::process::windows::pipe_server
 * depending on your platform.
 */

#include "impl/basic_pipe.h"

#endif //CPEN333_PROCESS_PIPE_H
