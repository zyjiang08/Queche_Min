// quiche_engine_api.cpp
// QUIC Engine API - Engine class wrapper implementation
//
// Copyright (C) 2025, Cloudflare, Inc.
// All rights reserved.

#include "quiche_engine.h"
#include "quiche_engine_impl.h"

namespace quiche {

// ============================================================================
// QuicheEngine Constructor/Destructor
// ============================================================================

QuicheEngine::QuicheEngine(const std::string& host, const std::string& port, const ConfigMap& config)
    : pImpl(new QuicheEngineImpl(host, port, config))
{
    pImpl->setWrapper(this);
}

QuicheEngine::~QuicheEngine() {
    delete pImpl;
}

// ============================================================================
// QuicheEngine Move Semantics
// ============================================================================

QuicheEngine::QuicheEngine(QuicheEngine&& other) noexcept
    : pImpl(other.pImpl)
{
    other.pImpl = nullptr;
}

QuicheEngine& QuicheEngine::operator=(QuicheEngine&& other) noexcept {
    if (this != &other) {
        delete pImpl;
        pImpl = other.pImpl;
        other.pImpl = nullptr;
    }
    return *this;
}

// ============================================================================
// Engine Public API - Delegates to Impl
// ============================================================================

bool QuicheEngine::setEventCallback(EventCallback callback, void* user_data) {
    return pImpl->setEventCallback(callback, user_data);
}

ssize_t QuicheEngine::write(uint64_t stream_id, const uint8_t* data, size_t len, bool fin) {
    return pImpl->write(stream_id, data, len, fin);
}

ssize_t QuicheEngine::write(uint64_t stream_id, const std::string& data, bool fin) {
    return pImpl->write(stream_id, reinterpret_cast<const uint8_t*>(data.c_str()), data.size(), fin);
}

ssize_t QuicheEngine::read(uint64_t stream_id, uint8_t* buf, size_t buf_len, bool& fin) {
    return pImpl->read(stream_id, buf, buf_len, fin);
}

std::string QuicheEngine::read(uint64_t stream_id, bool& fin) {
    uint8_t buf[65535];
    ssize_t len = pImpl->read(stream_id, buf, sizeof(buf), fin);
    if (len > 0) {
        return std::string(reinterpret_cast<char*>(buf), len);
    }
    return "";
}

bool QuicheEngine::run() {
    return pImpl->run();
}

bool QuicheEngine::join() {
    return pImpl->join();
}

void QuicheEngine::stop() {
    pImpl->stop();
}

bool QuicheEngine::close(uint64_t app_error, const std::string& reason) {
    return pImpl->close(app_error, reason);
}

bool QuicheEngine::isConnected() const {
    return pImpl->isConnected();
}

bool QuicheEngine::isRunning() const {
    return pImpl->isRunning();
}

EngineStats QuicheEngine::getStats() const {
    return pImpl->getStats();
}

std::string QuicheEngine::getLastError() const {
    return pImpl->getLastError();
}

} // namespace quiche
