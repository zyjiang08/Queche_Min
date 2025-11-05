# Thread-Safe Architecture for quiche_engine

## Overview

This document describes the thread-safe refactoring of the quiche_engine implementation to ensure all `quiche_*` function calls happen from a single thread (the libev event loop thread), as required by the QUIC protocol implementation.

## Problem Statement

The original implementation had a critical threading issue:
- `quiche_engine_run()` was blocking and ran the libev loop in the calling thread
- Event callbacks were invoked from the libev thread
- Functions like `quiche_engine_write()` and `quiche_engine_read()` could be called from any thread
- **CRITICAL BUG**: When `quiche_engine_write()` was called from a different thread, it would directly call `quiche_conn_stream_send()`, violating the QUIC requirement that all `quiche_*` functions must be called from the same thread

## Solution: Command Queue Pattern with Background Thread

### Architecture Components

1. **Background Event Loop Thread**
   - libev event loop runs in a dedicated pthread
   - All `quiche_*` function calls happen in this thread only
   - Thread is started by `quiche_engine_run()` and managed by the engine

2. **Thread-Safe Command Queue**
   - Mutex-protected FIFO queue for cross-thread communication
   - Application threads enqueue commands
   - libev thread processes commands

3. **Async Watcher (ev_async)**
   - Allows safe signaling from any thread to the libev thread
   - Wakes up libev loop to process pending commands

### Command Types

```c
typedef enum {
    CMD_WRITE,   // Write data to a stream
    CMD_CLOSE,   // Close the connection
    CMD_STOP,    // Stop the event loop
} command_type_t;
```

### Threading Model

```
Application Thread(s)              libev Event Loop Thread
------------------                 -----------------------
     |                                    |
     | quiche_engine_write()              |
     |    ↓                                |
     | Create CMD_WRITE                    |
     | Enqueue command                     |
     | Signal ev_async                     |
     |                                     ↓
     |                              async_cb triggered
     |                              process_commands()
     |                                     ↓
     |                              quiche_conn_stream_send()
     |                              (called in libev thread ✓)
     |                                     ↓
     |                              flush_egress()
```

## API Changes

### Modified Functions

#### `quiche_engine_run()`
**Before:**
- Blocking function that ran libev loop directly
- Returned only when connection closed

**After:**
- Non-blocking function that starts libev loop in background thread
- Returns immediately after thread starts successfully
- Returns 0 on success, -1 on error

#### `quiche_engine_join()` (NEW)
- Blocks until the event loop thread finishes
- Must be called after `quiche_engine_run()` if you want to wait for completion
- Returns 0 on success, -1 on error

#### `quiche_engine_write()`
**Before:**
```c
ssize_t quiche_engine_write(...) {
    // Directly called quiche_conn_stream_send()
    // UNSAFE if called from different thread!
    quiche_conn_stream_send(engine->conn, ...);
    flush_egress(engine);
}
```

**After:**
```c
ssize_t quiche_engine_write(...) {
    // Thread-safe: Enqueues command for processing
    command_t *cmd = malloc(sizeof(command_t));
    cmd->type = CMD_WRITE;
    // Copy data to command
    cmd_queue_push(&engine->cmd_queue, cmd);
    ev_async_send(engine->loop, &engine->async_watcher);
    return len;  // Returns immediately
}
```

#### `quiche_engine_close()`
**Before:**
- Directly called `quiche_conn_close()`

**After:**
- Enqueues CMD_CLOSE command
- Thread-safe, can be called from any thread

#### `quiche_engine_uninit()`
**Before:**
- Stopped event loop with `ev_break()`
- Cleaned up resources

**After:**
- Sends CMD_STOP command to libev thread
- Waits for thread to finish with `pthread_join()`
- Cleans up event loop with `ev_loop_destroy()`
- Destroys command queue
- Frees all resources

### Client Code Changes

**Before:**
```c
quiche_engine_run(engine);  // Blocks until connection closes
quiche_engine_uninit(engine);
```

**After:**
```c
quiche_engine_run(engine);   // Returns immediately
quiche_engine_join(engine);  // Wait for completion
quiche_engine_uninit(engine);
```

## Implementation Details

### Command Queue Structure

```c
typedef struct command {
    command_type_t type;
    union {
        struct {
            uint64_t stream_id;
            uint8_t data[MAX_WRITE_DATA_SIZE];  // 64KB max
            size_t len;
            bool fin;
        } write;
        struct {
            uint64_t error_code;
            char reason[256];
        } close;
    } params;
    struct command *next;
} command_t;

typedef struct {
    command_t *head;
    command_t *tail;
    pthread_mutex_t mutex;
} command_queue_t;
```

### Command Queue Operations

```c
// Thread-safe push (called from any thread)
void cmd_queue_push(command_queue_t *queue, command_t *cmd) {
    pthread_mutex_lock(&queue->mutex);
    // Add to tail
    pthread_mutex_unlock(&queue->mutex);
}

// Pop (called only from libev thread)
command_t* cmd_queue_pop(command_queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);
    // Remove from head
    pthread_mutex_unlock(&queue->mutex);
    return cmd;
}
```

### Event Loop Thread

```c
static void* event_loop_thread(void *arg) {
    quiche_engine_t *engine = (quiche_engine_t *)arg;

    // Run event loop (blocks until stopped)
    ev_run(engine->loop, 0);

    engine->is_running = false;
    return NULL;
}
```

### Async Callback

```c
static void async_cb(EV_P_ ev_async *w, int revents) {
    quiche_engine_t *engine = (quiche_engine_t *)w->data;
    process_commands(engine);  // Process all pending commands
}
```

### Command Processing

```c
static void process_commands(quiche_engine_t *engine) {
    command_t *cmd;
    while ((cmd = cmd_queue_pop(&engine->cmd_queue)) != NULL) {
        switch (cmd->type) {
            case CMD_WRITE:
                // Safe: called in libev thread
                quiche_conn_stream_send(engine->conn,
                    cmd->params.write.stream_id,
                    cmd->params.write.data,
                    cmd->params.write.len,
                    cmd->params.write.fin, &error_code);
                flush_egress(engine);
                break;

            case CMD_CLOSE:
                // Safe: called in libev thread
                quiche_conn_close(engine->conn, true,
                    cmd->params.close.error_code,
                    cmd->params.close.reason, ...);
                flush_egress(engine);
                break;

            case CMD_STOP:
                ev_break(engine->loop, EVBREAK_ONE);
                break;
        }
        free(cmd);
    }
}
```

## Thread Safety Guarantees

### ✓ Thread-Safe Operations (can be called from any thread)

- `quiche_engine_write()` - Enqueues command
- `quiche_engine_close()` - Enqueues command
- `quiche_engine_join()` - Blocks until thread finishes
- `quiche_engine_uninit()` - Properly stops thread and cleans up

### ⚠️ Callback Context (called in libev thread)

- `on_engine_event()` callbacks are invoked from libev thread
- `quiche_engine_read()` can be called safely from within callbacks
- All event processing happens in libev thread

### ✓ All `quiche_*` Calls in Single Thread

- `quiche_conn_stream_send()` - Only called in process_commands() (libev thread)
- `quiche_conn_stream_recv()` - Called in recv_cb() (libev thread)
- `quiche_conn_close()` - Only called in process_commands() (libev thread)
- `quiche_conn_send()` - Only called in flush_egress() (libev thread)
- `quiche_conn_recv()` - Only called in recv_cb() (libev thread)
- `quiche_conn_on_timeout()` - Only called in timeout_cb() (libev thread)

## Performance Considerations

### Overhead

1. **Command Allocation**: Each write requires malloc/free of command structure
2. **Memory Copy**: Data is copied into command structure (max 64KB per write)
3. **Mutex Lock/Unlock**: One lock per enqueue, one per dequeue
4. **Thread Context Switch**: ev_async_send() triggers context switch

### Optimization Opportunities

1. **Command Pool**: Pre-allocate command structures to avoid malloc/free
2. **Batch Processing**: Process multiple commands per async callback
3. **Zero-Copy for Large Data**: Use shared memory or reference counting for large writes

### When This Architecture is Needed

✓ **Use thread-safe version when:**
- Multiple application threads need to write data
- Background processing is required
- Non-blocking API is preferred
- Application needs to do work while QUIC connection runs

❌ **Original blocking version sufficient when:**
- Single-threaded application
- All operations happen in main thread
- Simpler call flow is preferred

## Files Modified

### Core Implementation
- `quiche_engine.c` - Added threading, command queue, async processing
- `quiche_engine.h` - Updated API documentation, added `quiche_engine_join()`

### Client Application
- `client.c` - Updated to call `quiche_engine_join()` after `quiche_engine_run()`

### Backup Files (for reference)
- `quiche_engine_old.c` - Original non-threaded version
- `quiche_engine_old.h` - Original header

## Testing

### Basic Functionality Test

```bash
# Start server
./quic-server 127.0.0.1 4433 ../cert.crt ../cert.key

# Run client (in another terminal)
./quic-client 127.0.0.1 4433
```

### Expected Output

Client should:
1. Connect successfully
2. Send HTTP request
3. Receive response
4. Close cleanly
5. Print connection statistics

### Thread Safety Verification

The implementation ensures:
- All `quiche_*` functions are called from libev thread
- Application threads safely enqueue commands via mutex
- ev_async provides thread-safe signaling

## Migration Guide

### For Users of the Engine

**Minimal Changes Required:**

```c
// Old code:
quiche_engine_run(engine);  // Was blocking

// New code:
quiche_engine_run(engine);   // Now non-blocking
quiche_engine_join(engine);  // Add this line to wait
```

**For Multi-threaded Applications:**

```c
// Now safe to call from any thread:
pthread_t thread1, thread2;

void* worker1(void *arg) {
    quiche_engine_t *engine = (quiche_engine_t *)arg;
    quiche_engine_write(engine, 4, data, len, false);
    return NULL;
}

void* worker2(void *arg) {
    quiche_engine_t *engine = (quiche_engine_t *)arg;
    quiche_engine_write(engine, 8, data, len, false);
    return NULL;
}

// Start engine
quiche_engine_run(engine);

// Spawn worker threads
pthread_create(&thread1, NULL, worker1, engine);
pthread_create(&thread2, NULL, worker2, engine);

// Wait for workers
pthread_join(thread1, NULL);
pthread_join(thread2, NULL);

// Wait for engine
quiche_engine_join(engine);

// Clean up
quiche_engine_uninit(engine);
```

## Limitations

1. **Write Size Limit**: Maximum 64KB per write (MAX_WRITE_DATA_SIZE)
   - Larger writes must be split by the application

2. **Non-Blocking Writes**: `quiche_engine_write()` returns immediately
   - Does not indicate actual send success
   - Error reporting happens asynchronously via error callback

3. **Read Operations**: `quiche_engine_read()` should only be called from callbacks
   - Not thread-safe for calling from arbitrary threads
   - Alternative: data is delivered via STREAM_READABLE event

## Conclusion

The thread-safe refactoring successfully addresses the critical threading violation where `quiche_*` functions could be called from multiple threads. The new architecture:

✓ Ensures all QUIC operations happen in a single dedicated thread
✓ Provides thread-safe write and close operations via command queue
✓ Uses libev's ev_async for safe cross-thread signaling
✓ Maintains API compatibility with minimal client code changes
✓ Enables multi-threaded applications to safely use the engine

This implementation follows the QUIC protocol's strict requirement that all connection operations must be serialized in a single thread, preventing potential race conditions, data corruption, and undefined behavior.
