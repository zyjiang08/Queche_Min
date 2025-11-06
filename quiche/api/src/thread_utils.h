#ifndef __THREAD_UTILS_H__
#define __THREAD_UTILS_H__

#include <string>
#include <thread>

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #define PLATFORM_IOS
    #else
        #define PLATFORM_MACOS
    #endif
#elif defined(__ANDROID__)
    #define PLATFORM_ANDROID
#elif defined(__linux__)
    #define PLATFORM_LINUX
#endif

namespace quiche {
namespace thread_utils {

/**
 * Set thread name for the calling thread (cross-platform)
 *
 * Platform support:
 * - Linux: pthread_setname_np (max 15 chars + null terminator)
 * - macOS: pthread_setname_np (max 63 chars + null terminator)
 * - iOS: pthread_setname_np (max 63 chars + null terminator)
 * - Android: pthread_setname_np (max 15 chars + null terminator)
 * - Windows: SetThreadDescription (Windows 10 1607+, no length limit)
 *
 * @param name Thread name (will be truncated if too long)
 * @return true on success, false on failure
 */
bool setCurrentThreadName(const std::string& name);

/**
 * Set thread name for a specific thread (cross-platform)
 *
 * @param thread The std::thread to name
 * @param name Thread name (will be truncated if too long)
 * @return true on success, false on failure
 */
bool setThreadName(std::thread& thread, const std::string& name);

} // namespace thread_utils
} // namespace quiche

#endif // __THREAD_UTILS_H__
