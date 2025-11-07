# Cargo Configuration

## Auto-Generated Config

The `config.toml` file in this directory is **automatically generated** by `quiche_engine_all.sh` during Android builds.

**Do NOT manually edit or commit this file.**

## How It Works

When you run:
```bash
ANDROID_NDK_HOME=/path/to/ndk ./quiche_engine_all.sh android:arm64-v8a
```

The script will automatically:
1. Detect your Android NDK location from `ANDROID_NDK_HOME`
2. Generate `.cargo/config.toml` with correct toolchain paths
3. Build the Android library

## Manual Configuration (If Needed)

If you need to build Android targets directly with `cargo` (without using the build script), create a `config.toml` file manually:

```toml
[target.aarch64-linux-android]
ar = "/path/to/ndk/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-ar"
linker = "/path/to/ndk/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang"

# Add other targets as needed...
```

**Note**: This file will be overwritten the next time you run `quiche_engine_all.sh`.

## Supported NDK Versions

- NDK 21.x (recommended)
- NDK 23.x
- NDK 25.x and later

The script automatically detects your NDK version and adjusts paths accordingly.

## Why Absolute Paths?

The generated `config.toml` contains absolute paths like:
```toml
ar = "/Users/jiangzhongyang/Library/Android/sdk/ndk/21.4.7075529/..."
```

**This is by design and is NOT a problem:**

1. ✅ File is auto-generated before each build
2. ✅ File is in `.gitignore` (not committed)
3. ✅ Each developer generates their own config based on `$ANDROID_NDK_HOME`
4. ✅ Works across different NDK versions and platforms

**Why not use relative paths or environment variables?**
- Cargo's `[target.*]` config does NOT support environment variable expansion
- Cargo's `[target.*]` config does NOT support relative paths
- Absolute paths are the only supported format

For detailed explanation, see [DESIGN.md](DESIGN.md).
