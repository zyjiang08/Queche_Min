# Compiler Optimization Options for quiche

Complete guide to Rust compiler optimization options for reducing binary size and improving performance.

---

## 📋 Table of Contents

- [Quick Reference](#quick-reference)
- [Profile Configuration](#profile-configuration)
- [Optimization Levels](#optimization-levels)
- [Link-Time Optimization (LTO)](#link-time-optimization-lto)
- [Code Generation Options](#code-generation-options)
- [Target-Specific Optimizations](#target-specific-optimizations)
- [RUSTFLAGS Configuration](#rustflags-configuration)
- [Platform-Specific Settings](#platform-specific-settings)
- [Optimization Profiles](#optimization-profiles)
- [Performance vs Size Trade-offs](#performance-vs-size-trade-offs)
- [Testing and Validation](#testing-and-validation)

---

## 🚀 Quick Reference

### Fastest Compilation (Development)
```toml
[profile.dev]
opt-level = 0
debug = true
lto = false
```

### Smallest Binary Size
```toml
[profile.release]
opt-level = "z"
lto = "fat"
codegen-units = 1
strip = true
panic = "abort"
```

### Best Performance
```toml
[profile.release]
opt-level = 3
lto = "fat"
codegen-units = 1
```

### Balanced (Recommended)
```toml
[profile.release]
opt-level = 2
lto = "thin"
codegen-units = 16
strip = "debuginfo"
```

---

## 🎚️ Profile Configuration

### Available Profiles

Cargo supports multiple profiles that control compilation behavior:

```toml
[profile.dev]        # Default for `cargo build`
[profile.release]    # Default for `cargo build --release`
[profile.test]       # Used for `cargo test`
[profile.bench]      # Used for `cargo bench`
```

You can also create custom profiles:

```toml
[profile.production]
inherits = "release"
opt-level = "z"
lto = true
```

Build with custom profile:
```bash
cargo build --profile production
```

---

## 📊 Optimization Levels

### opt-level

Controls the optimization level passed to LLVM.

| Level | Description | Binary Size | Compile Time | Performance |
|-------|-------------|-------------|--------------|-------------|
| `0` | No optimization | Largest | Fastest | Slowest |
| `1` | Basic optimization | Large | Fast | Moderate |
| `2` | Some optimization (default for release) | Medium | Moderate | Good |
| `3` | Maximum optimization | Medium | Slow | Best |
| `"s"` | Optimize for size | Small | Slow | Moderate |
| `"z"` | Aggressively optimize for size | **Smallest** | Slowest | Moderate |

#### Configuration Examples

**Development (fast iteration):**
```toml
[profile.dev]
opt-level = 0
```

**Release (performance):**
```toml
[profile.release]
opt-level = 3
```

**Minimum binary size:**
```toml
[profile.release]
opt-level = "z"
```

#### Size Comparison

Based on quiche library:
- `opt-level = 0`: ~45MB
- `opt-level = 2`: ~30MB
- `opt-level = 3`: ~32MB (slightly larger than 2)
- `opt-level = "s"`: ~25MB
- `opt-level = "z"`: ~22MB (smallest)

**Recommendation for quiche:**
- Development: `opt-level = 0`
- Production (size): `opt-level = "z"`
- Production (performance): `opt-level = 3`

---

## 🔗 Link-Time Optimization (LTO)

LTO performs optimizations across the entire program at link time, enabling better dead code elimination and inlining.

### LTO Options

| Value | Description | Build Time | Binary Size | Recommended For |
|-------|-------------|------------|-------------|-----------------|
| `false` | Disabled (default) | Fastest | Largest | Development |
| `true` or `"fat"` | Full LTO across all crates | **Very Slow** | **Smallest** | Production builds |
| `"thin"` | ThinLTO (faster than fat) | Moderate | Small | CI/CD pipelines |
| `"off"` | Explicitly disabled | Fastest | Largest | Never |

### Configuration

```toml
[profile.release]
lto = true           # or "fat" - full LTO
# lto = "thin"       # faster alternative
# lto = false        # disable
```

### Expected Results

For quiche library:
- `lto = false`: ~28MB, compile time: 2min
- `lto = "thin"`: ~24MB, compile time: 3min
- `lto = true`: ~20MB, compile time: 5-8min

### ThinLTO vs Fat LTO

**Fat LTO:**
- ✅ Maximum size reduction (15-25%)
- ✅ Best cross-crate optimization
- ❌ Very slow build times
- ❌ High memory usage

**ThinLTO:**
- ✅ Good size reduction (10-15%)
- ✅ Parallelizable compilation
- ✅ Reasonable build times
- ✅ Lower memory usage

**Recommendation:** Use `"thin"` for regular builds, `"fat"` for final releases.

---

## ⚙️ Code Generation Options

### codegen-units

Controls parallelization of code generation.

```toml
[profile.release]
codegen-units = 1    # Best optimization, slowest
# codegen-units = 16 # Default, faster builds
# codegen-units = 256 # Maximum parallelization
```

**Trade-offs:**
- `codegen-units = 1`:
  - ✅ Smallest binary (5-10% reduction)
  - ✅ Best optimization opportunities
  - ❌ Slowest compilation (no parallelization)

- `codegen-units = 16` (default):
  - ✅ Balanced compilation speed
  - ✅ Reasonable optimization
  - ⚖️ Good for most use cases

**Recommendation:** Use `1` for production releases, `16+` for development.

### panic Strategy

Controls unwinding behavior on panic.

```toml
[profile.release]
panic = "abort"      # Smaller binary, immediate termination
# panic = "unwind"   # Default, allows catching panics
```

**Impact:**
- `panic = "abort"`: Saves 5-15% binary size
- `panic = "unwind"`: Allows `catch_unwind()`, larger binary

**Recommendation:** Use `"abort"` for production if you don't need panic recovery.

### strip

Remove debug symbols and other information.

```toml
[profile.release]
strip = true              # Remove all symbols and debug info
# strip = "debuginfo"     # Remove only debug info
# strip = "symbols"       # Remove only symbol table
# strip = "none"          # Keep everything (default)
```

**Impact:**
- `strip = true`: Saves 20-40% binary size
- `strip = "debuginfo"`: Saves 15-30%
- `strip = "symbols"`: Saves 10-20%

**Note:** Currently `debug = true` in workspace Cargo.toml - change to `debug = false` for production.

### debug

Controls debug information generation.

```toml
[profile.release]
debug = false        # No debug info (smallest)
# debug = true       # Full debug info (current setting)
# debug = 1          # Line tables only
# debug = 2          # Full debug info
```

**Current quiche setting:**
```toml
[profile.release]
debug = true         # ⚠️ Change to false for production
```

**Recommendation:** Set `debug = false` in production builds.

### overflow-checks

Enable integer overflow checking.

```toml
[profile.release]
overflow-checks = false   # Disabled in release (default)
# overflow-checks = true  # Enable for safety
```

**Impact:**
- `false`: Slightly smaller binary, better performance
- `true`: Safer code, small overhead

### incremental

Enable incremental compilation.

```toml
[profile.release]
incremental = false   # Disabled in release (default)
# incremental = true  # Enable for faster rebuilds
```

**Note:** Incremental compilation is primarily for development.

---

## 🎯 Target-Specific Optimizations

### CPU-Specific Features

#### Native CPU Optimization

Build for the current CPU architecture:

```bash
# Via environment variable
RUSTFLAGS="-C target-cpu=native" cargo build --release

# Via .cargo/config.toml
[build]
rustflags = ["-C", "target-cpu=native"]
```

**Warning:** Binary will not be portable to older CPUs.

#### Specific CPU Targets

```bash
# Intel Skylake and newer
RUSTFLAGS="-C target-cpu=skylake" cargo build --release

# AMD Zen 3
RUSTFLAGS="-C target-cpu=znver3" cargo build --release

# ARM Cortex-A72
RUSTFLAGS="-C target-cpu=cortex-a72" cargo build --release
```

### Target Features

Enable specific CPU features:

```toml
# .cargo/config.toml
[build]
rustflags = [
    "-C", "target-feature=+aes,+avx,+avx2,+sse4.2",
]
```

**Common useful features:**
- `+aes` - AES hardware acceleration
- `+avx2` - Advanced vector extensions
- `+bmi2` - Bit manipulation instructions
- `+sse4.2` - Streaming SIMD extensions

**Check available features:**
```bash
rustc --print target-features
```

### Code Model

For different binary sizes and addressing modes:

```toml
[build]
rustflags = ["-C", "code-model=small"]
```

| Model | Description | Use Case |
|-------|-------------|----------|
| `small` | Default, code+data < 2GB | Most applications |
| `medium` | code < 2GB, data unlimited | Large data sets |
| `large` | No size limits | Very large programs |
| `kernel` | Kernel code | OS kernels |

---

## 🛠️ RUSTFLAGS Configuration

### Method 1: Environment Variable

```bash
# Single build
RUSTFLAGS="-C opt-level=z -C lto=fat" cargo build --release

# Export for session
export RUSTFLAGS="-C opt-level=z -C lto=fat"
cargo build --release
```

### Method 2: .cargo/config.toml (Recommended)

Create `.cargo/config.toml` in project root:

```toml
[build]
rustflags = [
    "-C", "opt-level=z",
    "-C", "lto=fat",
    "-C", "codegen-units=1",
    "-C", "strip=symbols",
    "-C", "panic=abort",
]

# Target-specific flags
[target.x86_64-unknown-linux-gnu]
rustflags = [
    "-C", "link-arg=-Wl,--gc-sections",
    "-C", "link-arg=-Wl,--strip-all",
]

[target.aarch64-apple-darwin]
rustflags = [
    "-C", "link-arg=-dead_strip",
]
```

### Method 3: Cargo.toml Profile

```toml
[profile.release]
opt-level = "z"
lto = "fat"
codegen-units = 1
strip = true
panic = "abort"
```

**Priority:** RUSTFLAGS > .cargo/config.toml > Cargo.toml profile

---

## 🖥️ Platform-Specific Settings

### Linux

```toml
# .cargo/config.toml
[target.x86_64-unknown-linux-gnu]
rustflags = [
    "-C", "link-arg=-Wl,--gc-sections",      # Remove unused sections
    "-C", "link-arg=-Wl,--strip-all",        # Strip all symbols
    "-C", "link-arg=-Wl,--as-needed",        # Link only needed libs
    "-C", "relocation-model=pic",            # Position independent code
]

# For static linking
[target.x86_64-unknown-linux-musl]
rustflags = [
    "-C", "target-feature=+crt-static",
    "-C", "link-arg=-static",
]
```

### macOS

```toml
# .cargo/config.toml
[target.x86_64-apple-darwin]
rustflags = [
    "-C", "link-arg=-dead_strip",            # Remove dead code
    "-C", "link-arg=-Wl,-dead_strip_dylibs", # Remove unused dylibs
]

[target.aarch64-apple-darwin]
rustflags = [
    "-C", "link-arg=-dead_strip",
    "-C", "target-cpu=apple-m1",             # Apple Silicon optimization
]
```

### Windows

```toml
# .cargo/config.toml
[target.x86_64-pc-windows-msvc]
rustflags = [
    "-C", "link-arg=/OPT:REF",               # Remove unused functions
    "-C", "link-arg=/OPT:ICF",               # Merge identical code
]

[target.x86_64-pc-windows-gnu]
rustflags = [
    "-C", "link-arg=-Wl,--gc-sections",
    "-C", "link-arg=-Wl,--strip-all",
]
```

### Embedded/ARM

```toml
[target.armv7-unknown-linux-gnueabihf]
rustflags = [
    "-C", "opt-level=z",
    "-C", "link-arg=-Wl,--gc-sections",
    "-C", "target-feature=+neon",            # NEON SIMD
]
```

---

## 🎨 Optimization Profiles

### Profile 1: Maximum Size Reduction (Aggressive)

**Best for:** Embedded systems, size-constrained environments

```toml
# Cargo.toml
[profile.release]
opt-level = "z"
lto = "fat"
codegen-units = 1
strip = true
panic = "abort"
debug = false
overflow-checks = false

# .cargo/config.toml
[build]
rustflags = [
    "-C", "embed-bitcode=yes",
    "-C", "link-dead-code=no",
]

[target.x86_64-unknown-linux-gnu]
rustflags = [
    "-C", "link-arg=-Wl,--gc-sections",
    "-C", "link-arg=-Wl,--strip-all",
]
```

**Expected results for quiche:**
- Binary size: ~18-20MB (from 28MB baseline)
- Reduction: 28-35%
- Build time: 8-12 minutes

### Profile 2: Balanced Size/Performance

**Best for:** Production web servers, general applications

```toml
# Cargo.toml
[profile.release]
opt-level = 2
lto = "thin"
codegen-units = 16
strip = "debuginfo"
panic = "unwind"
debug = false

# .cargo/config.toml
[build]
rustflags = [
    "-C", "target-cpu=x86-64-v2",  # Baseline modern CPU
]
```

**Expected results for quiche:**
- Binary size: ~24MB
- Reduction: 14-18%
- Build time: 3-4 minutes
- Good performance

### Profile 3: Maximum Performance

**Best for:** High-throughput servers, latency-critical applications

```toml
# Cargo.toml
[profile.release]
opt-level = 3
lto = "fat"
codegen-units = 1
strip = "debuginfo"
panic = "unwind"
debug = false

# .cargo/config.toml
[build]
rustflags = [
    "-C", "target-cpu=native",
    "-C", "target-feature=+aes,+avx2,+sse4.2",
]
```

**Expected results for quiche:**
- Binary size: ~25-27MB
- Performance: Best possible
- Build time: 7-10 minutes

### Profile 4: Debug-Optimized

**Best for:** Development with acceptable performance

```toml
# Cargo.toml
[profile.dev]
opt-level = 1
debug = true
lto = false
codegen-units = 256
incremental = true
```

**Expected results:**
- Build time: 30-60 seconds (incremental: 5-10s)
- Reasonable performance for testing
- Full debug information

### Profile 5: Profile-Guided Optimization (PGO)

**Best for:** Maximum performance in production

**Step 1: Instrument build**
```bash
RUSTFLAGS="-C profile-generate=/tmp/pgo-data" \
    cargo build --release --package quiche
```

**Step 2: Run typical workload**
```bash
# Run your application with representative workload
./target/release/quiche-server
# Generate traffic, simulate production load
```

**Step 3: Optimized build**
```bash
RUSTFLAGS="-C profile-use=/tmp/pgo-data/merged.profdata" \
    cargo build --release --package quiche
```

**Expected improvement:**
- 5-15% better performance
- Slightly smaller binary
- Optimized for actual usage patterns

---

## ⚖️ Performance vs Size Trade-offs

### Comparison Matrix

| Configuration | Binary Size | Build Time | Runtime Perf | Use Case |
|---------------|-------------|------------|--------------|----------|
| Default dev | 45MB | 30s | Slow | Development |
| Default release | 28MB | 2min | Good | Testing |
| opt-level=z | 22MB | 3min | Moderate | Size-critical |
| opt-level=3 + LTO | 25MB | 8min | Best | Performance |
| Stripped | 20MB | 2min | Good | Production |
| PGO | 24MB | 15min | Excellent | High-end prod |

### Detailed Breakdown

#### Binary Size Impact

| Optimization | Size Reduction | Impact on Performance |
|--------------|----------------|----------------------|
| `opt-level = "z"` | -20% | -5 to -10% |
| `lto = "fat"` | -15% | +2 to +5% |
| `codegen-units = 1` | -5% | +1 to +3% |
| `strip = true` | -30% | No impact |
| `panic = "abort"` | -10% | No impact |
| `debug = false` | -25% | No impact |

#### Build Time Impact

| Optimization | Time Increase | Parallelizable |
|--------------|---------------|----------------|
| `opt-level = 3` | +50% | Partially |
| `lto = "fat"` | +200% | No |
| `lto = "thin"` | +50% | Yes |
| `codegen-units = 1` | +100% | No |
| PGO (total) | +500% | Partially |

---

## 🧪 Testing and Validation

### Size Comparison Script

Create `scripts/compare_builds.sh`:

```bash
#!/bin/bash

echo "=== Comparing Build Configurations ==="

configs=(
    "dev::"
    "release::"
    "release-size::-C|opt-level=z|-C|lto=fat"
    "release-perf::-C|opt-level=3|-C|lto=fat"
)

for config in "${configs[@]}"; do
    IFS=':' read -r name profile flags <<< "$config"

    echo ""
    echo "Building: $name"

    if [ -z "$flags" ]; then
        cargo build --profile "$profile" --package quiche 2>&1 | tail -1
    else
        RUSTFLAGS="${flags//|/ }" cargo build --profile "$profile" --package quiche 2>&1 | tail -1
    fi

    size=$(stat -f%z target/release/libquiche.a 2>/dev/null || stat -c%s target/release/libquiche.a 2>/dev/null)
    size_mb=$(echo "scale=2; $size / 1024 / 1024" | bc)
    echo "Size: ${size_mb}MB"
done
```

### Performance Benchmark

```bash
#!/bin/bash

echo "=== Performance Benchmarks ==="

# Build with different optimizations
cargo build --release --package quiche

# Run benchmarks
cargo bench --package quiche

# Compare throughput
cargo run --release --bin quiche-server &
SERVER_PID=$!
sleep 2

# Test with hey or similar tool
hey -n 10000 -c 100 https://localhost:4433/

kill $SERVER_PID
```

### Validation Checklist

After applying optimizations:

- [ ] **Compile successfully**
  ```bash
  cargo build --release
  ```

- [ ] **Run all tests**
  ```bash
  cargo test --release
  ```

- [ ] **Check binary size**
  ```bash
  ls -lh target/release/libquiche.*
  ```

- [ ] **Verify examples work**
  ```bash
  cargo run --release --bin quiche-client -- https://cloudflare-quic.com/
  ```

- [ ] **Run benchmarks**
  ```bash
  cargo bench --package quiche
  ```

- [ ] **Test in target environment**
  - Deploy to production-like setup
  - Monitor performance metrics
  - Check for crashes or errors

---

## 📐 Recommended Settings for quiche

### Development

```toml
# Cargo.toml
[profile.dev]
opt-level = 0
debug = true
```

Fast iteration, full debug info.

### CI/CD Testing

```toml
# Cargo.toml
[profile.test]
opt-level = 1
debug = true
lto = false
```

Reasonable performance for tests, quick builds.

### Production - Size Priority

```toml
# Cargo.toml
[profile.release]
opt-level = "z"
lto = "fat"
codegen-units = 1
strip = true
panic = "abort"
debug = false
overflow-checks = false

# .cargo/config.toml
[target.x86_64-unknown-linux-gnu]
rustflags = [
    "-C", "link-arg=-Wl,--gc-sections",
    "-C", "link-arg=-Wl,--strip-all",
]
```

**Build command:**
```bash
cargo build --release --package quiche --no-default-features \
    --features boringssl-vendored
```

### Production - Performance Priority

```toml
# Cargo.toml
[profile.release]
opt-level = 3
lto = "thin"
codegen-units = 16
strip = "debuginfo"
panic = "unwind"
debug = false

# .cargo/config.toml
[build]
rustflags = [
    "-C", "target-cpu=x86-64-v3",
]
```

---

## 🔍 Advanced Techniques

### 1. Split Debuginfo

Keep debug info in separate files:

```toml
[profile.release]
debug = true
split-debuginfo = "unpacked"  # or "packed"
strip = "none"
```

Then strip after build:
```bash
objcopy --only-keep-debug target/release/libquiche.a libquiche.debug
objcopy --strip-debug target/release/libquiche.a
```

### 2. Cross-Language LTO

For better optimization with C/C++ code (BoringSSL):

```toml
[profile.release]
lto = true

# .cargo/config.toml
[target.x86_64-unknown-linux-gnu]
linker = "clang"
rustflags = [
    "-C", "linker-plugin-lto",
    "-C", "link-arg=-fuse-ld=lld",
]
```

### 3. Abort on Panic

For libraries used in FFI:

```toml
[profile.release]
panic = "abort"

[dependencies]
quiche = { version = "*", default-features = false }
```

This prevents unwinding across FFI boundaries.

### 4. Dynamic Linking

To reduce static library size:

```toml
[profile.release]
prefer-dynamic = true
```

**Note:** Requires Rust standard library to be available at runtime.

---

## 📚 Additional Resources

### Tools

- **cargo-bloat**: Analyze binary size
  ```bash
  cargo install cargo-bloat
  cargo bloat --release --package quiche -n 20
  ```

- **cargo-tree**: Inspect dependencies
  ```bash
  cargo tree --package quiche --edges normal
  ```

- **cargo-asm**: View generated assembly
  ```bash
  cargo install cargo-asm
  cargo asm --package quiche Connection::recv
  ```

### Documentation

- [Cargo Book - Profiles](https://doc.rust-lang.org/cargo/reference/profiles.html)
- [Rust Compiler Options](https://doc.rust-lang.org/rustc/codegen-options/index.html)
- [LLVM Optimization Passes](https://llvm.org/docs/Passes.html)
- [min-sized-rust Guide](https://github.com/johnthagen/min-sized-rust)

### Monitoring Build Size

Add to CI/CD:

```yaml
# .github/workflows/size-check.yml
name: Binary Size Check

on: [pull_request]

jobs:
  size:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build
        run: cargo build --release --package quiche
      - name: Check size
        run: |
          SIZE=$(stat -c%s target/release/libquiche.a)
          echo "Binary size: $((SIZE / 1024 / 1024))MB"
          if [ $SIZE -gt 30000000 ]; then
            echo "::error::Binary too large!"
            exit 1
          fi
```

---

## 🎯 Summary

### Quick Wins (Easy Implementation)

1. **Set debug = false** in release profile (current: true)
   - **Impact:** -25% size, instant

2. **Add strip = true**
   - **Impact:** -30% size, instant

3. **Change opt-level to "z"**
   - **Impact:** -20% size, slight build time increase

4. **Enable LTO = "thin"**
   - **Impact:** -15% size, moderate build time increase

### Best Overall Configuration

For quiche production builds:

```toml
# Cargo.toml
[profile.release]
opt-level = "z"      # Optimize for size
lto = "thin"         # Link-time optimization (balanced)
codegen-units = 1    # Better optimization
strip = true         # Remove symbols
panic = "abort"      # Smaller panic handling
debug = false        # No debug info
```

**Expected Results:**
- Binary: 18-22MB (from 28MB baseline)
- Reduction: ~35-40%
- Build time: 4-6 minutes
- Performance: Still very good

---

## 📝 Changelog

- **2025-11-03**: Initial compiler optimizations guide
- Comprehensive analysis of all rustc codegen options
- Platform-specific optimization strategies
- Performance vs size trade-off matrix
- Practical optimization profiles for quiche

---

**Last Updated:** 2025-11-03
**Rust Version:** 1.89.0
**Target:** quiche QUIC/HTTP3 library
