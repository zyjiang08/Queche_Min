# Binary Size Optimization Guide for quiche

This document provides comprehensive guidance for reducing the binary size of the quiche QUIC/HTTP3 library.

## 📊 Current Status Analysis

### Code Size Breakdown

| Component | Lines of Code | Estimated Binary Impact |
|-----------|--------------|------------------------|
| HTTP/3 Module | ~11,679 | 30-40% |
| Google Congestion Control (gcongestion) | ~6,973 | 10-15% |
| Native BBR Implementation | ~1,437 | 3-5% |
| Native BBR2 Implementation | ~2,347 | 5-7% |
| FFI C Interface | ~2,605 | 5-8% |
| qlog Logging | 106 references | 15-20% |
| PMTUD | ~413 | 1-2% |
| DGRAM Support | ~117 | 1-2% |
| **Total Source Code** | **~68,785** | |

### Current Build Sizes

- `libquiche.a`: **28MB** (static library)
- `libquiche.rlib`: **15MB** (Rust library)
- Includes: vendored BoringSSL, all features enabled

---

## 🎯 Optimization Strategies

### 1. Remove HTTP/3 Support ⭐⭐⭐⭐⭐

**Expected Savings: 30-40%**

If you only need QUIC transport layer without HTTP/3:

#### Implementation

**Step 1: Add feature flag in `quiche/Cargo.toml`**

```toml
[features]
default = ["boringssl-vendored"]
http3 = []  # Make HTTP/3 optional
boringssl-vendored = []
# ... other features
```

**Step 2: Conditionally compile in `quiche/src/lib.rs`**

```rust
// Around line 9381
#[cfg(feature = "http3")]
pub mod h3;
```

**Step 3: Build without HTTP/3**

```bash
cargo build --release --no-default-features --features boringssl-vendored
```

#### What Gets Removed

- `quiche/src/h3/mod.rs` - HTTP/3 connection handling
- `quiche/src/h3/stream.rs` - HTTP/3 stream management
- `quiche/src/h3/frame.rs` - HTTP/3 frame types
- `quiche/src/h3/qpack/` - QPACK header compression (~11,679 lines total)
- HTTP/3 FFI bindings

---

### 2. Disable qlog Logging ⭐⭐⭐⭐

**Expected Savings: 15-20%**

qlog is primarily used for debugging and not needed in production.

#### Implementation

**Build without qlog:**

```bash
cargo build --release --no-default-features --features boringssl-vendored
```

#### What Gets Removed

- qlog crate dependency (with serde_json)
- 106 qlog-related code blocks across codebase
- Event serialization overhead
- JSON formatting code

#### Dependencies Removed

```toml
# These become unnecessary without qlog
serde = { workspace = true, features = ["derive"] }
serde_json = { workspace = true, features = ["preserve_order"] }
serde_with = { workspace = true, features = ["macros"] }
```

---

### 3. Remove Unnecessary Congestion Control Algorithms ⭐⭐⭐⭐

**Expected Savings: 10-15%**

Most deployments only need one congestion control algorithm.

#### Option A: Keep Only CUBIC (Most Common)

**Step 1: Add features in `quiche/Cargo.toml`**

```toml
[features]
default = ["boringssl-vendored", "cubic"]
cubic = []
reno = []
bbr = []
bbr2 = []
gcongestion = []
```

**Step 2: Modify `quiche/src/recovery/congestion/mod.rs`**

```rust
// Always compile CUBIC
mod cubic;
pub use cubic::Cubic;

// Conditionally compile others
#[cfg(feature = "reno")]
mod reno;
#[cfg(feature = "reno")]
pub use reno::Reno;

#[cfg(feature = "bbr")]
mod bbr;

#[cfg(feature = "bbr2")]
mod bbr2;

// Remove gcongestion entirely if not needed
#[cfg(feature = "gcongestion")]
mod gcongestion;
```

**Step 3: Update `quiche/src/recovery/mod.rs`**

Add feature gates around algorithm registrations.

**What Gets Removed:**
- `recovery/congestion/reno.rs` (~200 lines)
- `recovery/congestion/bbr/` (~1,437 lines)
- `recovery/congestion/bbr2/` (~2,347 lines)
- `recovery/gcongestion/` (~6,973 lines) - **Largest savings**

#### Option B: Use gcongestion, Remove Native Implementations

If you prefer Google's implementation:

```bash
cargo build --release --features gcongestion,boringssl-vendored --no-default-features
```

Then remove native BBR/BBR2 implementations.

---

### 4. Remove FFI C Interface ⭐⭐⭐

**Expected Savings: 5-8%**

If you only use Rust API:

#### Implementation

**Step 1: Modify `quiche/Cargo.toml`**

```toml
[lib]
crate-type = ["lib", "rlib"]  # Remove "staticlib", "cdylib"
```

**Step 2: Build without FFI**

```bash
cargo build --release --no-default-features --features boringssl-vendored
```

#### What Gets Removed

- `quiche/src/ffi.rs` (~2,005 lines)
- `quiche/src/h3/ffi.rs` (~600 lines)
- C header file generation logic
- FFI-specific error handling

---

### 5. Remove Optional Protocol Features ⭐⭐⭐

**Expected Savings: 3-5%**

#### DGRAM (Unreliable Datagrams)

If you don't need QUIC DATAGRAM extension:

**Add to `quiche/Cargo.toml`:**

```toml
[features]
dgram = []
```

**Modify `quiche/src/lib.rs`:**

```rust
#[cfg(feature = "dgram")]
mod dgram;

// Gate all dgram_* public methods
#[cfg(feature = "dgram")]
pub fn dgram_send(&mut self, buf: &[u8]) -> Result<()> { ... }

#[cfg(feature = "dgram")]
pub fn dgram_recv(&mut self, buf: &mut [u8]) -> Result<usize> { ... }

// ... all 15 dgram_* methods
```

#### PMTUD (Path MTU Discovery)

If your network environment is stable:

```rust
#[cfg(feature = "pmtud")]
mod pmtud;
```

---

### 6. Optimize Dependencies ⭐⭐

**Expected Savings: 5-10%**

#### Remove Unnecessary Feature Flags

**Modify `quiche/Cargo.toml`:**

```toml
[dependencies]
# Remove "union" feature if not needed
smallvec = { workspace = true }

# Use minimal log
log = { workspace = true, default-features = false }

# Remove huffman_hpack if not using HTTP/3
octets = { workspace = true }
```

#### Optimize octets Dependency

If not using HTTP/3, modify `octets/Cargo.toml`:

```toml
[features]
# Don't enable huffman_hpack by default
default = []
huffman_hpack = []
```

---

### 7. Compilation Optimization ⭐⭐⭐⭐

**Expected Savings: 20-30%**

#### Modify Root `Cargo.toml`

```toml
[profile.release]
opt-level = "z"          # Optimize for size instead of speed
lto = true               # Enable Link Time Optimization
codegen-units = 1        # Single codegen unit for better optimization
strip = true             # Strip symbols and debug info
panic = "abort"          # Use abort instead of unwind
debug = false            # Remove debug info (currently set to true)
```

#### Additional Compiler Flags

**Create `.cargo/config.toml`:**

```toml
[build]
rustflags = [
    "-C", "embed-bitcode=yes",
    "-C", "lto=fat",
]

[target.x86_64-unknown-linux-gnu]
rustflags = [
    "-C", "link-arg=-Wl,--gc-sections",  # Remove unused sections
    "-C", "link-arg=-Wl,--strip-all",    # Strip all symbols
]
```

---

### 8. Replace BoringSSL with System OpenSSL ⭐⭐⭐⭐⭐

**Expected Savings: 40-50% (LARGEST SAVINGS)**

Vendored BoringSSL significantly increases binary size.

#### Implementation

```bash
# Use system OpenSSL/quictls
cargo build --release --no-default-features --features openssl
```

#### Trade-offs

**Advantages:**
- ✅ Dramatically smaller binary size
- ✅ Uses system-provided SSL library
- ✅ Easier to audit and update

**Disadvantages:**
- ❌ **No 0-RTT support**
- ❌ Requires OpenSSL/quictls to be installed
- ❌ Version compatibility concerns

#### Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt-get install libssl-dev pkg-config
```

**macOS:**
```bash
brew install openssl@3
export PKG_CONFIG_PATH="/opt/homebrew/opt/openssl@3/lib/pkgconfig"
```

**Building with OpenSSL:**
```bash
cargo build --release --no-default-features --features openssl
```

---

## 🏆 Recommended Optimization Plans

### Plan A: Minimal QUIC Transport (Aggressive)

**Expected Size Reduction: 70-80%**

#### Use Case
- Pure QUIC transport layer
- No HTTP/3 needed
- Production environment
- Size is critical priority

#### Configuration

**`Cargo.toml`:**
```toml
[features]
default = []
minimal = ["openssl"]

[profile.release]
opt-level = "z"
lto = true
codegen-units = 1
strip = true
panic = "abort"
debug = false
```

#### Build Command

```bash
cargo build --release --no-default-features --features minimal
```

#### What's Removed

- ✂️ Entire HTTP/3 module
- ✂️ qlog logging
- ✂️ FFI interface
- ✂️ gcongestion
- ✂️ BBR/BBR2 (keep CUBIC only)
- ✂️ DGRAM support
- ✂️ Vendored BoringSSL → System OpenSSL

#### What's Kept

- ✅ Core QUIC transport
- ✅ CUBIC congestion control
- ✅ Stream management
- ✅ Loss recovery
- ✅ Connection migration

---

### Plan B: Streamlined QUIC + HTTP/3 (Balanced)

**Expected Size Reduction: 40-50%**

#### Use Case
- Need HTTP/3 support
- Production environment
- Want 0-RTT support
- Reasonable size constraints

#### Configuration

**Build Command:**
```bash
cargo build --release --no-default-features \
  --features boringssl-vendored
```

**`Cargo.toml` profile:**
```toml
[profile.release]
opt-level = "z"
lto = true
codegen-units = 1
strip = true
panic = "abort"
debug = false
```

#### What's Removed

- ✂️ qlog logging
- ✂️ FFI interface
- ✂️ gcongestion
- ✂️ BBR/BBR2 (keep CUBIC only)
- ✂️ DGRAM support

#### What's Kept

- ✅ HTTP/3 support
- ✅ Vendored BoringSSL (0-RTT support)
- ✅ CUBIC congestion control
- ✅ Full QUIC features

---

### Plan C: Production Optimization (Conservative)

**Expected Size Reduction: 25-35%**

#### Use Case
- Need all core features
- Minimal code changes
- Conservative approach
- Focus on build optimization

#### Configuration

**`Cargo.toml`:**
```toml
[profile.release]
opt-level = "z"
lto = true
strip = true
panic = "abort"
debug = false
```

#### Build Command

```bash
cargo build --release --no-default-features \
  --features boringssl-vendored
```

#### What's Removed

- ✂️ qlog logging (not needed in production)
- ✂️ FFI interface (if only using Rust)
- ✂️ Either gcongestion OR native BBR (choose one)

#### What's Kept

- ✅ HTTP/3 support
- ✅ Vendored BoringSSL
- ✅ Multiple congestion control options
- ✅ All protocol features

---

## 🔧 Implementation Steps

### Step 1: Backup Current Configuration

```bash
git checkout -b binary-optimization
git commit -am "Backup before optimization"
```

### Step 2: Choose Your Plan

Select Plan A, B, or C based on your requirements.

### Step 3: Modify Cargo.toml

```bash
# Edit the root Cargo.toml
vim Cargo.toml

# Edit quiche/Cargo.toml for features
vim quiche/Cargo.toml
```

### Step 4: Add Feature Gates (If Needed)

For advanced optimizations, add `#[cfg(feature = "...")]` guards:

```bash
vim quiche/src/lib.rs
vim quiche/src/recovery/congestion/mod.rs
```

### Step 5: Build and Test

```bash
# Clean previous builds
cargo clean

# Build with optimizations
cargo build --release --no-default-features --features boringssl-vendored

# Run tests to ensure functionality
cargo test --no-default-features --features boringssl-vendored

# Check binary size
ls -lh target/release/libquiche.*
```

### Step 6: Compare Sizes

```bash
# Before optimization
echo "Before: $(ls -lh target/release/libquiche.a | awk '{print $5}')"

# After optimization
echo "After: $(ls -lh target/release/libquiche.a | awk '{print $5}')"
```

### Step 7: Run Integration Tests

```bash
# Test with example client
cargo run --bin quiche-client --no-default-features \
  --features boringssl-vendored -- https://cloudflare-quic.com/

# Test with example server
cargo run --bin quiche-server --no-default-features \
  --features boringssl-vendored -- \
  --cert apps/src/bin/cert.crt --key apps/src/bin/cert.key
```

---

## 📋 Feature Comparison Matrix

| Feature | Plan A (Minimal) | Plan B (Balanced) | Plan C (Conservative) |
|---------|------------------|-------------------|----------------------|
| Binary Size Reduction | 70-80% | 40-50% | 25-35% |
| HTTP/3 | ❌ | ✅ | ✅ |
| QUIC Transport | ✅ | ✅ | ✅ |
| 0-RTT Support | ❌ (OpenSSL) | ✅ (BoringSSL) | ✅ (BoringSSL) |
| qlog Logging | ❌ | ❌ | ❌ |
| FFI C API | ❌ | ❌ | Optional |
| Congestion Control | CUBIC only | CUBIC only | CUBIC + BBR/BBR2 |
| DGRAM Support | ❌ | ❌ | ✅ |
| Code Changes | Extensive | Moderate | Minimal |
| Recommended For | Embedded/IoT | Web Servers | Enterprise |

---

## ⚠️ Important Considerations

### 1. Testing Requirements

After any optimization:

```bash
# Run full test suite
cargo test --all-features

# Run specific package tests
cargo test --package quiche --no-default-features --features boringssl-vendored

# Test examples
cargo test --package quiche_apps
```

### 2. 0-RTT Support

- **BoringSSL**: Supports 0-RTT
- **OpenSSL**: Does NOT support 0-RTT
- Consider your latency requirements

### 3. API Compatibility

When removing features, document breaking changes:

```rust
// In your application code
#[cfg(feature = "http3")]
use quiche::h3::Connection as H3Connection;
```

### 4. Dependency Management

Update your application's `Cargo.toml`:

```toml
[dependencies]
quiche = { version = "0.24", default-features = false, features = ["boringssl-vendored"] }
```

### 5. Documentation

Update README.md with minimum build instructions:

```markdown
## Minimum Build

For smallest binary size:

\`\`\`bash
cargo build --release --no-default-features --features minimal
\`\`\`
```

---

## 🧪 Verification and Benchmarking

### Size Verification Script

Create `scripts/check_size.sh`:

```bash
#!/bin/bash

echo "Building with different configurations..."

# Default build
cargo build --release
DEFAULT_SIZE=$(stat -f%z target/release/libquiche.a)

# Minimal build
cargo build --release --no-default-features --features boringssl-vendored
MINIMAL_SIZE=$(stat -f%z target/release/libquiche.a)

# Calculate reduction
REDUCTION=$(echo "scale=2; ($DEFAULT_SIZE - $MINIMAL_SIZE) * 100 / $DEFAULT_SIZE" | bc)

echo "Default build: $(numfmt --to=iec-i --suffix=B $DEFAULT_SIZE)"
echo "Minimal build: $(numfmt --to=iec-i --suffix=B $MINIMAL_SIZE)"
echo "Size reduction: ${REDUCTION}%"
```

### Performance Testing

Ensure optimizations don't significantly impact performance:

```bash
# Run benchmarks
cargo bench --package quiche

# Test throughput
cargo run --release --bin quiche-client -- \
  --benchmark https://cloudflare-quic.com/
```

---

## 📚 Additional Resources

### Related Files

- `Cargo.toml` - Root workspace configuration
- `quiche/Cargo.toml` - Main library configuration
- `quiche/src/lib.rs` - Main library source
- `quiche/src/recovery/congestion/mod.rs` - Congestion control

### Useful Commands

```bash
# Check dependency tree
cargo tree --package quiche

# Analyze binary size
cargo bloat --release --package quiche

# Check feature flags
cargo metadata --format-version 1 | jq '.packages[] | select(.name == "quiche") | .features'

# Profile binary size (requires cargo-bloat)
cargo install cargo-bloat
cargo bloat --release -n 10
```

### Further Reading

- [Cargo Book - Profiles](https://doc.rust-lang.org/cargo/reference/profiles.html)
- [Rust Performance Book](https://nnethercote.github.io/perf-book/)
- [min-sized-rust](https://github.com/johnthagen/min-sized-rust)

---

## 🤝 Contributing

If you find additional optimization opportunities, please:

1. Test the optimization thoroughly
2. Measure actual size impact
3. Document any trade-offs
4. Submit a pull request

---

## 📝 Changelog

- **2025-11-03**: Initial optimization guide created
- Analysis of 68,785 lines of source code
- Identified 8 major optimization strategies
- Provided 3 practical implementation plans

---

## ⚖️ License

This optimization guide is provided under the same BSD-2-Clause license as quiche.
