# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

quiche is Cloudflare's implementation of the QUIC transport protocol and HTTP/3 as specified by the IETF. It provides a low-level API for processing QUIC packets and handling connection state. The application is responsible for providing I/O (e.g., sockets handling) as well as an event loop with timers.

## Workspace Structure

This is a Cargo workspace with the following key crates:

- **quiche/** - Core QUIC transport protocol and HTTP/3 implementation
  - Provides both Rust and C FFI APIs
  - Uses BoringSSL (vendored by default) or OpenSSL/quictls for cryptographic operations
  - Supports multiple congestion control algorithms (CUBIC, Reno, BBR, BBR2)
  - Optional `gcongestion` feature uses Google's QUIC congestion control implementation

- **apps/** (quiche_apps) - Example client and server applications
  - `quiche-client` - HTTP/3 client binary
  - `quiche-server` - HTTP/3 server binary

- **h3i/** - Low-level HTTP/3 debugging and testing tool
  - Interactive HTTP/3 client for debugging
  - Supports both synchronous and async (tokio) modes with `async` feature

- **tokio-quiche/** - Asynchronous wrapper around quiche for tokio
  - Provides async QUIC primitives for tokio applications
  - Supports zero-copy operations with `zero-copy` feature
  - Optional `gcongestion` feature for Google's congestion control

- **octets/** - Zero-copy buffer management
- **qlog/** - QUIC logging format support
- **buffer-pool/** - Buffer pooling utilities
- **datagram-socket/** - Datagram socket abstractions
- **task-killswitch/** - Task cancellation primitives

- **fuzz/** - Fuzzing targets (excluded from workspace)

## Building

### Basic Build
```bash
# Clone with submodules (required for BoringSSL)
git clone --recursive https://github.com/cloudflare/quiche

# Build all workspace crates
cargo build

# Build with examples
cargo build --examples

# Build only the apps
cargo build --package=quiche_apps
```

### Prerequisites
- Rust 1.83 or later
- cmake (required for building BoringSSL)
- NASM (required on Windows for BoringSSL)

### Build Features

#### quiche crate features:
- `boringssl-vendored` (default) - Build vendored BoringSSL
- `boringssl-boring-crate` - Use BoringSSL from boring crate
- `openssl` - Use OpenSSL/quictls instead (note: 0-RTT not supported)
- `ffi` - Build C FFI API and generate libquiche.a static library
- `gcongestion` - Use Google's QUIC congestion control implementation
- `qlog` - Enable QUIC logging support
- `fuzzing` - Enable fuzzing mode (for use with libfuzzer)
- `internal` - Expose internal APIs (no stability guarantees)

#### Building with specific features:
```bash
# Build with FFI for C/C++ integration
cargo build --features ffi

# Build with OpenSSL instead of BoringSSL
cargo build --features openssl --no-default-features

# Build with Google's congestion control
cargo build --features gcongestion
```

### Using Custom BoringSSL
```bash
QUICHE_BSSL_PATH="/path/to/boringssl" cargo build --examples
```

## Testing

### Run All Tests
```bash
cargo test
```

### Run Tests for Specific Crate
```bash
cargo test --package quiche
cargo test --package tokio-quiche
cargo test --package h3i
```

### Run Specific Test
```bash
# Run tests matching a pattern
cargo test test_name

# Run a specific test in a specific crate
cargo test --package quiche test_name
```

### Run Tests with Features
```bash
# Test with qlog support
cargo test --features qlog

# Test with gcongestion
cargo test --features gcongestion
```

## Running Example Applications

### Client
```bash
# Basic HTTP/3 client
cargo run --bin quiche-client -- https://cloudflare-quic.com/

# See all options
cargo run --bin quiche-client -- --help
```

### Server
```bash
# Run HTTP/3 server (note: uses self-signed cert, not for production)
cargo run --bin quiche-server -- --cert apps/src/bin/cert.crt --key apps/src/bin/cert.key

# See all options
cargo run --bin quiche-server -- --help
```

### Examples
Located in `quiche/examples/`:
- `client.rs` / `client.c` - Basic QUIC client examples
- `server.rs` / `server.c` - Basic QUIC server examples
- `http3-client.rs` / `http3-client.c` - HTTP/3 client examples
- `http3-server.rs` / `http3-server.c` - HTTP/3 server examples
- `qpack-encode.rs` / `qpack-decode.rs` - QPACK encoder/decoder examples

## Code Architecture

### Core QUIC Implementation (quiche/src/)

**Connection Management:**
- `lib.rs` - Main connection API (`Connection`, `Config`, `connect()`, `accept()`)
- `packet.rs` - QUIC packet parsing and serialization
- `frame.rs` - QUIC frame types and encoding
- `cid.rs` - Connection ID management
- `path.rs` - Path validation and migration

**Stream Management:**
- `stream/mod.rs` - Stream state machine and API
- `stream/send_buf.rs` - Send buffer implementation
- `stream/recv_buf.rs` - Receive buffer implementation
- `flowcontrol.rs` - Flow control logic
- `dgram.rs` - Datagram support (unreliable datagrams over QUIC)

**Loss Recovery and Congestion Control:**
- `recovery/mod.rs` - Loss detection and recovery
- `recovery/rtt.rs` - RTT estimation
- `recovery/congestion/` - Congestion control algorithms:
  - `cubic.rs` - CUBIC congestion control
  - `reno.rs` - Reno congestion control
  - `bbr/` - BBR congestion control
  - `bbr2/` - BBR v2 congestion control
  - `pacer.rs` - Packet pacing
- `recovery/gcongestion/` - Google's congestion control (when `gcongestion` feature enabled)
  - Alternative implementations of BBR/BBR2

**Cryptography and TLS:**
- `tls/boringssl.rs` - BoringSSL integration
- `tls/openssl_quictls.rs` - OpenSSL/quictls integration
- `crypto/boringssl.rs` - QUIC-specific crypto with BoringSSL
- `crypto/openssl_quictls.rs` - QUIC-specific crypto with OpenSSL

**HTTP/3 Implementation:**
- `h3/mod.rs` - HTTP/3 connection API
- `h3/stream.rs` - HTTP/3 stream handling
- `h3/frame.rs` - HTTP/3 frame types
- `h3/qpack/` - QPACK header compression:
  - `encoder.rs` - QPACK encoder
  - `decoder.rs` - QPACK decoder
  - `static_table.rs` - Static table

**Other Modules:**
- `ranges.rs` / `range_buf.rs` - Range tracking for ACKs and data
- `pmtud.rs` - Path MTU Discovery
- `rand.rs` - Random number generation
- `minmax.rs` - Min-max windowed filter
- `ffi.rs` - C FFI bindings (when `ffi` feature enabled)

### Key Architectural Patterns

1. **Event-Driven Design**: quiche does not handle I/O directly. Applications must:
   - Provide their own socket handling
   - Implement an event loop with timers
   - Call `Connection::recv()` for incoming packets
   - Call `Connection::send()` to generate outgoing packets
   - Call `Connection::on_timeout()` when timers expire

2. **Zero-Copy Buffers**: Uses `octets` crate for efficient buffer management with minimal copying.

3. **Feature Flags**: Extensive use of conditional compilation for different crypto backends, congestion control algorithms, and optional features.

4. **Modular Congestion Control**: Congestion control is abstracted behind traits, allowing multiple implementations (CUBIC, Reno, BBR, BBR2) and even wholesale replacement via `gcongestion` feature.

5. **FFI Layer**: C API is a thin wrapper over Rust API, designed for integration with C/C++ applications (curl, Android DNS resolver, etc.).

## Fuzzing

### Build Fuzzers
```bash
# Build specific fuzz targets
make build-fuzz

# Or directly with cargo-fuzz
cargo +nightly fuzz build --release --debug-assertions packet_recv_client
```

### Available Fuzz Targets
Located in `fuzz/src/`:
- `packet_recv_client.rs` - Fuzz client packet reception
- `packet_recv_server.rs` - Fuzz server packet reception
- `packets_recv_server.rs` - Fuzz multiple packet reception
- `packets_posths_server.rs` - Fuzz post-handshake packets
- `qpack_decode.rs` - Fuzz QPACK decoder

## Linting and Formatting

### Format Code
```bash
# Format all code according to rustfmt.toml
cargo fmt
```

### Run Clippy
```bash
# Run clippy with workspace configuration (clippy.toml)
cargo clippy --all-targets --all-features
```

## Docker

### Build Docker Images
```bash
# Build both base and QNS images
make docker-build

# Build only base image (includes client/server binaries)
make docker-base

# Build only QNS image (for quic-interop-runner)
make docker-qns
```

## Important Notes for Development

- **Application Responsibilities**: When using quiche, applications must provide I/O, event loops, and timer management. quiche only handles QUIC protocol logic.

- **Connection Configuration**: Many `Config` settings default to zero and must be explicitly configured based on use case:
  - `set_initial_max_streams_bidi()`
  - `set_initial_max_streams_uni()`
  - `set_initial_max_data()`
  - `set_initial_max_stream_data_bidi_local()`
  - `set_initial_max_stream_data_bidi_remote()`
  - `set_initial_max_stream_data_uni()`

- **Crypto Backend**: Default is vendored BoringSSL. OpenSSL/quictls support exists but lacks 0-RTT support.

- **Congestion Control**: By default uses quiche's native implementation. The `gcongestion` feature replaces this with Google's implementation ported from google/quiche.

- **FFI Usage**: When building for C/C++ integration, enable the `ffi` feature to build `libquiche.a`. The C API is defined in `quiche/include/quiche.h`.

- **Test Organization**: Tests are co-located with implementation in `mod tests` blocks within source files. Main integration tests are in `quiche/src/tests.rs`.
