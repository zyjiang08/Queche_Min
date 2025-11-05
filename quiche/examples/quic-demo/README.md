# QUIC Demo - Standalone Client and Server

This directory contains standalone QUIC client and server examples that can be compiled independently from the main quiche project.

## 📁 Directory Structure

```
quic-demo/
├── Makefile          # Build configuration
├── README.md         # This file
├── src/
│   ├── client.c      # QUIC client implementation
│   └── server.c      # QUIC server implementation
├── include/
│   ├── quiche.h      # quiche library header
│   └── uthash.h      # Hash table implementation (for server)
├── lib/
│   └── libquiche.a   # quiche static library (~43MB)
├── certs/
│   ├── cert.crt      # Self-signed certificate
│   └── cert.key      # Private key
├── build/            # Build artifacts (generated)
├── quic-client       # Client binary (generated)
└── quic-server       # Server binary (generated)
```

## 🔧 Prerequisites

### macOS
```bash
brew install libev

# If uthash is not found, it will be automatically downloaded
# Alternatively: brew install uthash
```

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install libev-dev

# uthash.h is included in this directory
```

### Linux (CentOS/RHEL)
```bash
sudo yum install libev-devel
```

## 🏗️ Building

### Build Both Client and Server
```bash
make
```

### Build Individual Components
```bash
# Build only client
make client

# Build only server
make server
```

### Clean Build Artifacts
```bash
make clean
```

## 🚀 Running

### Running the QUIC Client

The client connects to a QUIC server and sends a simple HTTP/0.9 request.

**Basic usage:**
```bash
./quic-client <host> <port>
```

**Examples:**
```bash
# Connect to Cloudflare's QUIC test server
./quic-client cloudflare-quic.com 443

# Connect to local server
./quic-client 127.0.0.1 4433
```

**Using Makefile:**
```bash
make run-client HOST=cloudflare-quic.com PORT=443
```

**Expected output:**
```
connection established: hq-29
sent HTTP request
stream 4 is readable
<!DOCTYPE html>...
connection closed
```

### Running the QUIC Server

The server listens for incoming QUIC connections and echoes back "byez\n" when it receives data.

**Basic usage:**
```bash
./quic-server <host> <port>
```

**Examples:**
```bash
# Listen on localhost:4433
./quic-server 127.0.0.1 4433

# Listen on all interfaces
./quic-server 0.0.0.0 4433
```

**Using Makefile:**
```bash
make run-server HOST=127.0.0.1 PORT=4433
```

**Expected output:**
```
new connection
recv 1200 bytes
stream 4 is readable
sent 1200 bytes
connection closed, recv=1250 sent=2400 lost=0 rtt=12345ns cwnd=14520
```

## 🔐 Certificates

The `certs/` directory contains a self-signed certificate for testing purposes:
- `cert.crt` - Self-signed X.509 certificate
- `cert.key` - RSA private key

**⚠️ WARNING:** These certificates are for **testing only** and should **NOT** be used in production!

### Generating Your Own Certificates

```bash
cd certs
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout cert.key -out cert.crt -days 365 \
  -subj "/C=US/ST=State/L=City/O=Organization/CN=localhost"
```

## 🧪 Testing Client-Server Communication

### Terminal 1 - Start Server
```bash
cd quiche/examples/quic-demo
./quic-server 127.0.0.1 4433
```

### Terminal 2 - Run Client
```bash
cd quiche/examples/quic-demo
./quic-client 127.0.0.1 4433
```

You should see:
- **Server:** Accepts connection, receives data, sends response
- **Client:** Establishes connection, sends request, receives "byez\n"

## 📝 Implementation Details

### Client Features
- Connects to a QUIC server using quiche library
- Sends HTTP/0.9 GET request on stream 4
- Receives and prints response
- Uses libev for event loop
- Supports debug logging via `SSLKEYLOGFILE` environment variable

### Server Features
- Accepts multiple QUIC connections
- Implements stateless retry for DDoS protection
- Handles version negotiation
- Manages multiple streams per connection
- Uses uthash for connection management
- Supports RENO congestion control

### Supported QUIC Versions
Both client and server support:
- `hq-interop` (HTTP/0.9 over QUIC)
- `hq-29`, `hq-28`, `hq-27` (draft versions)

## 🔍 Debugging

### Enable Debug Logging
```bash
# Client
SSLKEYLOGFILE=/tmp/keys.log ./quic-client cloudflare-quic.com 443

# Server
SSLKEYLOGFILE=/tmp/keys.log ./quic-server 127.0.0.1 4433
```

The debug logs will show:
- Connection establishment
- Packet send/receive
- Stream operations
- Timeout events

### Check Binary Size
```bash
ls -lh quic-client quic-server
```

Expected sizes:
- **quic-client:** ~600-800 KB
- **quic-server:** ~600-800 KB

## 🛠️ Makefile Targets

| Target | Description |
|--------|-------------|
| `make` or `make all` | Build both client and server |
| `make client` | Build only the client |
| `make server` | Build only the server |
| `make clean` | Remove build artifacts and binaries |
| `make run-client HOST=... PORT=...` | Build and run client |
| `make run-server HOST=... PORT=...` | Build and run server |
| `make help` | Show help message |

## 📦 Dependencies

### Included in This Directory
- **quiche.h** - quiche library API
- **libquiche.a** - quiche static library (includes BoringSSL)
- **uthash.h** - Lightweight hash table library (header-only)
- **cert.crt / cert.key** - Test certificates

### System Dependencies (must be installed)
- **libev** - Event loop library
- **pthread** - POSIX threads
- **dl** - Dynamic linker
- **m** - Math library

### macOS Specific
- **Security.framework** - macOS security APIs
- **Foundation.framework** - macOS foundation APIs

## 🔄 Updating libquiche.a

If you modify the quiche library, rebuild `libquiche.a`:

```bash
# From the quiche root directory
cd ../../../  # Go to quiche root

# Build with FFI support
cargo build --release --features ffi,boringssl-vendored --no-default-features

# Copy to quic-demo
cp target/release/libquiche.a quiche/examples/quic-demo/lib/

# Rebuild examples
cd quiche/examples/quic-demo
make clean
make
```

## ⚠️ Known Issues

1. **macOS Version Warnings:** When linking, you may see warnings like:
   ```
   ld: warning: object file was built for newer 'macOS' version (15.2) than being linked (14.0)
   ```
   These are harmless and can be ignored.

2. **Unused Parameter Warnings:** Compile-time warnings about unused parameters are expected and do not affect functionality.

## 📚 Additional Resources

- **quiche Documentation:** https://docs.rs/quiche/
- **QUIC Specification:** https://www.rfc-editor.org/rfc/rfc9000.html
- **HTTP/3 Specification:** https://www.rfc-editor.org/rfc/rfc9114.html
- **libev Documentation:** http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod

## 📄 License

This code is part of the quiche project and follows the same BSD 2-Clause License.

See the license headers in `src/client.c` and `src/server.c` for full details.

## 🤝 Contributing

For issues or improvements to these examples, please refer to the main quiche repository:
https://github.com/cloudflare/quiche

## 💡 Tips

1. **Performance Testing:** Use `quic-client` to test connection latency to public QUIC servers
2. **Development:** Modify `src/client.c` or `src/server.c` and recompile with `make`
3. **Integration:** Link against `lib/libquiche.a` in your own C projects
4. **Port Conflicts:** If port 4433 is in use, try 4434, 5433, etc.

---

**Last Updated:** 2025-11-04
**quiche Version:** 0.24.6
**Compiler:** GCC/Clang compatible
