/**
 * @file
 * @brief detects OS and sets appropriate macros
 *
 * By checking various macros defined by common compilers, this file tries to detect the current OS and sets
 * unified macros to be used in other platform-specific code.
 * <table>
 *  <caption>Platform-specific Macros</caption>
 *  <tr><th>Operating System</th><th>Macros</th></tr>
 *  <tr><td>Windows</td><td>`WINDOWS`</td></tr>
 *  <tr><td>Linux</td><td>`LINUX`, `POSIX`</td></tr>
 *  <tr><td>OSX</td><td>`APPLE`, `POSIX`</td></tr>
 * </table>
 */
#ifndef CPEN333_OS_H
#define CPEN333_OS_H

// Platform-dependent defines
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(_WIN64) || defined(WIN64) || defined(_WIN64)
/**
 * Defined on Windows platforms
 */
#define WINDOWS
#elif defined(__APPLE__)
/**
 * Defined on OSX platforms
 */
#define APPLE
 /**
 * Defined on POSIX-compliant platforms
 */
#define POSIX
#else
 /**
 * Defined on Linux platforms
 */
#define LINUX
 /**
 * Defined on POSIX-compliant platforms
 */
#define POSIX
#endif

#endif //CPEN333_OS_H
