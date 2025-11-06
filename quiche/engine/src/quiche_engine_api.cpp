// quiche_engine_api.cpp
// QUIC Engine API - Engine class wrapper implementation
//
// Copyright (C) 2025, Cloudflare, Inc.
// All rights reserved.

#include <quiche_engine.h>
#include "quiche_engine_impl.h"

namespace quiche {

// ============================================================================
// QuicheEngine Constructor/Destructor
// ============================================================================

QuicheEngine::QuicheEngine(const std::string& host, const std::string& port, const ConfigMap& config)
    : mPImpl(new QuicheEngineImpl(host, port, config))
{
    mPImpl->setWrapper(this);
}

QuicheEngine::~QuicheEngine() {
    delete mPImpl;
}

// ============================================================================
// QuicheEngine Move Semantics
// ============================================================================

QuicheEngine::QuicheEngine(QuicheEngine&& other) noexcept
    : mPImpl(other.mPImpl)
{
    other.mPImpl = nullptr;
}

QuicheEngine& QuicheEngine::operator=(QuicheEngine&& other) noexcept {
    if (this != &other) {
        delete mPImpl;
        mPImpl = other.mPImpl;
        other.mPImpl = nullptr;
    }
    return *this;
}

// ============================================================================
// Engine Public API - Delegates to Impl
// ============================================================================

bool QuicheEngine::setEventCallback(EventCallback callback, void* user_data) {
    return mPImpl->setEventCallback(callback, user_data);
}

ssize_t QuicheEngine::write(uint64_t stream_id, const uint8_t* data, size_t len, bool fin) {
    return mPImpl->write(stream_id, data, len, fin);
}

ssize_t QuicheEngine::read(uint64_t stream_id, uint8_t* buf, size_t buf_len, bool& fin) {
    return mPImpl->read(stream_id, buf, buf_len, fin);
}

bool QuicheEngine::start() {
    return mPImpl->start();
}

void QuicheEngine::shutdown(uint64_t app_error, const std::string& reason) {
    mPImpl->shutdown(app_error, reason);
}

bool QuicheEngine::isConnected() const {
    return mPImpl->isConnected();
}

bool QuicheEngine::isRunning() const {
    return mPImpl->isRunning();
}

EngineStats QuicheEngine::getStats() const {
    return mPImpl->getStats();
}

std::string QuicheEngine::getLastError() const {
    return mPImpl->getLastError();
}

} // namespace quiche
