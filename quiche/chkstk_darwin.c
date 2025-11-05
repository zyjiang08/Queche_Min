// chkstk_darwin.c
// Provides __chkstk_darwin implementation for iOS builds
// This function is used by LLVM for stack probing on Darwin platforms

#ifdef __APPLE__
#include <stdint.h>

// Stack checking function for Darwin (macOS/iOS)
// This is needed when compiling with -fstack-check or when the compiler
// generates stack probe calls for large stack allocations

#if defined(__x86_64__) || defined(__i386__)
// For x86/x86_64, __chkstk_darwin is expected by some BoringSSL code
__attribute__((visibility("default")))
void __chkstk_darwin(void) {
    // On x86_64, the amount to probe is passed in %rax
    // On i386, the amount is passed on the stack
    // For iOS, we typically don't need to do anything here
    // as the stack is already set up correctly
    __asm__ volatile("" ::: "memory");
}
#endif

#if defined(__arm64__) || defined(__aarch64__)
// For ARM64/AArch64, we might need a similar stub
// Though typically ARM doesn't use __chkstk_darwin
__attribute__((visibility("default")))
void __chkstk_darwin(void) {
    __asm__ volatile("" ::: "memory");
}
#endif

#if defined(__arm__)
// For 32-bit ARM
__attribute__((visibility("default")))
void __chkstk_darwin(void) {
    __asm__ volatile("" ::: "memory");
}
#endif

#endif // __APPLE__
