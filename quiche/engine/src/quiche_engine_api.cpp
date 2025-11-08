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

QuicheEngine::QuicheEngine()
    : mPImpl(new QuicheEngineImpl())
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

bool QuicheEngine::open(const ConfigMap& config) {
    return mPImpl->open(config);
}

bool QuicheEngine::setEventCallback(EventCallback callback, void* user_data) {
    return mPImpl->setEventCallback(callback, user_data);
}

std::string QuicheEngine::connect(const std::string& host, const std::string& port,
                                 uint64_t timeout_ms) {
    return mPImpl->connect(host, port, timeout_ms);
}

void QuicheEngine::close(uint64_t app_error, const std::string& reason) {
    mPImpl->close(app_error, reason);
}

ssize_t QuicheEngine::write(const uint8_t* data, size_t len, bool fin) {
    return mPImpl->write(data, len, fin);
}

ssize_t QuicheEngine::read(uint8_t* buf, size_t buf_len, bool& fin) {
    return mPImpl->read(buf, buf_len, fin);
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

std::string QuicheEngine::getScid() const {
    return mPImpl->getScid();
}

} // namespace quiche
