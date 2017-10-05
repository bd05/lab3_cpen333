/**
 * @file
 * @brief Inter-process shared mutex implementation
 */

#ifndef CPEN333_PROCESS_SOCKET_H
#define CPEN333_PROCESS_SOCKET_H

#include "../os.h"
#ifdef WINDOWS
#include "impl/windows/socket.h"
#else
#include "impl/posix/socket.h"
#endif

/**
 * @class cpen333::process::socket
 * @brief A client socket implementation for inter-process communication
 *
 * Used to communicate between processes over IP.  This is an alias to either
 * cpen333::process::posix::socket or cpen333::process::windows::socket
 * depending on your platform.
 */


/**
 * @class cpen333::process::socket_server
 * @brief A server socket implementation for inter-process communication
 *
 * Used to listen for connections from remote clients.  This is an alias to either
 * cpen333::process::posix::socket_server or cpen333::process::windows::socket_server
 * depending on your platform.
 */

#endif //CPEN333_PROCESS_SOCKET_H
