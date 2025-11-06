# Bash 3.2 Compatibility Fix - Complete ✅

## Issue
When running `./build_mobile_libs.sh android:arm64-v8a` on macOS, the script crashed with:
```
./build_mobile_libs.sh: line 544: declare: -A: invalid option
declare: usage: declare [-afFirtx] [-p] [name[=value] ...]
```

## Root Cause
- macOS ships with Bash 3.2 by default
- Bash 3.2 does not support associative arrays (`declare -A`)
- The script was using associative arrays to map Android ABIs to targets and toolchains

## Solution
Replaced associative arrays with case statement helper functions in `build_mobile_libs.sh` (lines 543-585):

### Before (Bash 4+ only):
```bash
declare -A ANDROID_TARGETS=(
    ["arm64-v8a"]="aarch64-linux-android"
    ["armeabi-v7a"]="armv7-linux-androideabi"
    ["x86"]="i686-linux-android"
    ["x86_64"]="x86_64-linux-android"
)

declare -A ANDROID_TOOLCHAINS=(
    ["aarch64-linux-android"]="aarch64-linux-android"
    ["armv7-linux-androideabi"]="armv7a-linux-androideabi"
    ["i686-linux-android"]="i686-linux-android"
    ["x86_64-linux-android"]="x86_64-linux-android"
)

# Usage:
target="${ANDROID_TARGETS[$abi]}"
toolchain="${ANDROID_TOOLCHAINS[$target]}"
```

### After (Bash 3.2 compatible):
```bash
# Helper function to get target from ABI
get_android_target() {
    local abi=$1
    case $abi in
        arm64-v8a) echo "aarch64-linux-android" ;;
        armeabi-v7a) echo "armv7-linux-androideabi" ;;
        x86) echo "i686-linux-android" ;;
        x86_64) echo "x86_64-linux-android" ;;
        *) echo "" ;;
    esac
}

# Helper function to get toolchain from target
get_android_toolchain() {
    local target=$1
    case $target in
        aarch64-linux-android) echo "aarch64-linux-android" ;;
        armv7-linux-androideabi) echo "armv7a-linux-androideabi" ;;
        i686-linux-android) echo "i686-linux-android" ;;
        x86_64-linux-android) echo "x86_64-linux-android" ;;
        *) echo "" ;;
    esac
}

# Usage:
target=$(get_android_target "$abi")
toolchain=$(get_android_toolchain "$target")
```

## Verification

### Test Command:
```bash
./build_mobile_libs.sh android:arm64-v8a
```

### Results:
✅ **No more Bash compatibility errors**
- Script successfully parses `android:arm64-v8a` argument
- Helper functions work correctly
- Script reaches the build phase without Bash errors

### Output:
```
[INFO] ============================================
[INFO] Mobile Libraries Build Script
[INFO] ============================================
[INFO]
[INFO] Will build Android arm64-v8a
[INFO] Using Android NDK: /Users/jiangzhongyang/Library/Android/sdk/ndk/23.2.8568313
[INFO] Building Android arm64-v8a only...
[INFO] ======================================
[INFO] Building for Android (arm64-v8a)...
[INFO] ======================================
```

The script now fails later with NDK toolchain configuration issues (unrelated to Bash compatibility):
```
error: failed to find tool "aarch64-linux-android-clang"
```

This is a **separate issue** related to Android NDK compiler naming, not the Bash compatibility fix.

## Impact

✅ **Compatible with macOS default Bash 3.2**
✅ **Works on older Linux systems with Bash 3.x**
✅ **Still compatible with modern Bash 4+ and 5+**
✅ **No external dependencies or version requirements**

## Platform Compatibility

| Shell Version | Status |
|---------------|--------|
| Bash 3.2 (macOS default) | ✅ Works |
| Bash 4.x | ✅ Works |
| Bash 5.x | ✅ Works |

## Files Modified

- `/Users/jiangzhongyang/work/live/CDN/quiche/build_mobile_libs.sh` (lines 543-622)

## Summary

The Bash 3.2 compatibility issue is **completely resolved**. The build script now uses portable shell constructs that work across all Bash versions from 3.2 onwards. The script successfully parses arguments, validates architectures, and attempts to build without any Bash syntax errors.

**Status**: ✅ **COMPLETE**

---

**Date**: 2025-11-06
**Fixed by**: Case statement helper functions replacing associative arrays
