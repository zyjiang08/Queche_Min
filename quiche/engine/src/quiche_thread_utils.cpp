#include "quiche_thread_utils.h"

#include <cstring>

// Platform-specific includes
#if defined(PLATFORM_WINDOWS)
    #include <windows.h>
    #include <processthreadsapi.h>
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_ANDROID) || defined(PLATFORM_MACOS) || defined(PLATFORM_IOS)
    #include <pthread.h>
    #if defined(PLATFORM_LINUX) || defined(PLATFORM_ANDROID)
        #include <sys/prctl.h>
    #endif
#endif

namespace quiche {
namespace thread_utils {

bool setCurrentThreadName(const std::string& name) {
    if (name.empty()) {
        return false;
    }

#if defined(PLATFORM_WINDOWS)
    // Windows 10 1607+ (Anniversary Update)
    // SetThreadDescription supports wide strings, no length limit
    std::wstring wname(name.begin(), name.end());
    HRESULT hr = SetThreadDescription(GetCurrentThread(), wname.c_str());
    return SUCCEEDED(hr);

#elif defined(PLATFORM_MACOS) || defined(PLATFORM_IOS)
    // macOS/iOS: pthread_setname_np takes only the name (sets current thread)
    // Max length: 63 chars + null terminator
    constexpr size_t MAX_NAME_LEN = 63;
    std::string truncated = name.substr(0, MAX_NAME_LEN);
    return pthread_setname_np(truncated.c_str()) == 0;

#elif defined(PLATFORM_LINUX) || defined(PLATFORM_ANDROID)
    // Linux/Android: pthread_setname_np takes thread and name
    // Max length: 15 chars + null terminator
    constexpr size_t MAX_NAME_LEN = 15;
    std::string truncated = name.substr(0, MAX_NAME_LEN);
    return pthread_setname_np(pthread_self(), truncated.c_str()) == 0;

#else
    // Unsupported platform
    (void)name;  // Suppress unused warning
    return false;
#endif
}

bool setThreadName(std::thread& thread, const std::string& name) {
    if (name.empty() || !thread.joinable()) {
        return false;
    }

#if defined(PLATFORM_WINDOWS)
    // Windows: Get native handle and set description
    HANDLE handle = static_cast<HANDLE>(thread.native_handle());
    std::wstring wname(name.begin(), name.end());
    HRESULT hr = SetThreadDescription(handle, wname.c_str());
    return SUCCEEDED(hr);

#elif defined(PLATFORM_MACOS) || defined(PLATFORM_IOS)
    // macOS/iOS: Cannot set name of another thread from outside
    // The thread must call setCurrentThreadName() itself
    // We'll return false to indicate this limitation
    (void)thread;
    (void)name;
    return false;

#elif defined(PLATFORM_LINUX) || defined(PLATFORM_ANDROID)
    // Linux/Android: Can set name of another thread
    constexpr size_t MAX_NAME_LEN = 15;
    std::string truncated = name.substr(0, MAX_NAME_LEN);
    pthread_t native_handle = thread.native_handle();
    return pthread_setname_np(native_handle, truncated.c_str()) == 0;

#else
    // Unsupported platform
    (void)thread;
    (void)name;
    return false;
#endif
}

} // namespace thread_utils
} // namespace quiche
