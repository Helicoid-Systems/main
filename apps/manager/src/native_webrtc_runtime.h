#pragma once

#include "hs_webrtc.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace helicoid::manager {

enum class NativeWebRtcLoadErrorCode {
    EmptyPath,
    PathMustBeAbsolute,
    LibraryUnavailable,
    MissingRequiredExport,
    AbiVersionMismatch,
    CapabilityQueryFailed,
    RequiredCapabilitiesMissing,
};

struct NativeWebRtcLoadError {
    NativeWebRtcLoadErrorCode code;
    std::uint32_t status = HS_STATUS_OK;
    std::uint32_t actualAbiVersion = 0;
    std::uint32_t missingCapabilities = 0;
};

struct NativeWebRtcLoadResult;
struct NativeWebRtcModule;

/// One tightly packed BGRA frame copied from the native receive mailbox.
struct NativeScreenFrame final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> bgra;

    NativeScreenFrame() = default;
    NativeScreenFrame(const NativeScreenFrame&) = delete;
    NativeScreenFrame& operator=(const NativeScreenFrame&) = delete;
    NativeScreenFrame(NativeScreenFrame&& other) noexcept;
    NativeScreenFrame& operator=(NativeScreenFrame&& other) noexcept;
    ~NativeScreenFrame();

    void clear() noexcept;
};

class NativeWebRtcRuntime;
class NativeWebRtcPeer;

/// Owns one initialized engine handle and keeps its loaded library alive.
class NativeWebRtcEngine final {
public:
    NativeWebRtcEngine(const NativeWebRtcEngine&) = delete;
    NativeWebRtcEngine& operator=(const NativeWebRtcEngine&) = delete;
    NativeWebRtcEngine(NativeWebRtcEngine&& other) noexcept;
    NativeWebRtcEngine& operator=(NativeWebRtcEngine&& other) noexcept;
    ~NativeWebRtcEngine();

    bool isOpen() const noexcept { return handle_ != nullptr; }
    std::uint32_t drain(std::uint32_t timeoutMs) noexcept;
    std::uint32_t shutdown() noexcept;
    std::unique_ptr<NativeWebRtcPeer> createPeer(std::uint32_t& outStatus) const;

private:
    friend class NativeWebRtcRuntime;
    NativeWebRtcEngine(std::shared_ptr<NativeWebRtcModule> module,
                       hs_engine* handle) noexcept;

    std::shared_ptr<NativeWebRtcModule> module_;
    hs_engine* handle_ = nullptr;
};

/// Owns one peer token and keeps the native library loaded until it is retired.
class NativeWebRtcPeer final {
public:
    NativeWebRtcPeer(const NativeWebRtcPeer&) = delete;
    NativeWebRtcPeer& operator=(const NativeWebRtcPeer&) = delete;
    NativeWebRtcPeer(NativeWebRtcPeer&& other) noexcept;
    NativeWebRtcPeer& operator=(NativeWebRtcPeer&& other) noexcept;
    ~NativeWebRtcPeer();

    bool isOpen() const noexcept { return handle_ != nullptr; }
    std::uint32_t drain(std::uint32_t timeoutMs) noexcept;
    std::uint32_t generation(std::uint32_t& outGeneration) noexcept;
    std::uint32_t state(std::uint32_t& outState) noexcept;
    std::uint32_t prepareVideo(std::uint32_t role) noexcept;
    std::uint32_t beginObservation(std::span<const std::uint8_t> channelBinding,
                                  std::uint32_t leaseMs) noexcept;
    std::uint32_t endObservation() noexcept;
    std::uint32_t beginCapture(std::uint32_t expectedGeneration) noexcept;
    std::uint32_t endCapture() noexcept;
    std::uint32_t pushScreenFrame(std::span<const std::uint8_t> bgra,
                                  std::uint32_t width,
                                  std::uint32_t height,
                                  std::uint32_t stride) noexcept;
    std::uint32_t createProductDataChannel() noexcept;
    std::uint32_t closeProductDataChannel() noexcept;
    std::uint32_t createOffer(std::string& outSdp);
    std::uint32_t createAnswer(std::string& outSdp);
    std::uint32_t setRemoteDescription(std::string_view type,
                                       std::string_view sdp) noexcept;
    std::uint32_t addIceCandidate(std::string_view candidate) noexcept;
    std::uint32_t pollIceCandidate(std::string& outCandidate);
    std::uint32_t setExpectedRemoteCertificate(std::string_view certificatePem) noexcept;
    std::uint32_t exportLocalCertificate(std::string& outCertificatePem);
    std::uint32_t exportVerifiedRemoteCertificate(std::string& outCertificatePem);
    std::uint32_t exportVerifiedChannelBinding(std::vector<std::uint8_t>& outBinding);
    std::uint32_t sendProductMessage(std::span<const std::uint8_t> message) noexcept;
    std::uint32_t pollProductMessage(std::vector<std::uint8_t>& outMessage);
    std::uint32_t acquireScreenFrame(NativeScreenFrame& outFrame);

private:
    friend class NativeWebRtcEngine;
    NativeWebRtcPeer(std::shared_ptr<NativeWebRtcModule> module,
                     hs_peer* handle) noexcept;

    std::shared_ptr<NativeWebRtcModule> module_;
    hs_peer* handle_ = nullptr;
};

/// Dynamically loaded, app-owned WebRTC ABI. Production loading always uses
/// the fixed library location inside the running application's bundle.
class NativeWebRtcRuntime final {
public:
    NativeWebRtcRuntime(const NativeWebRtcRuntime&) = delete;
    NativeWebRtcRuntime& operator=(const NativeWebRtcRuntime&) = delete;
    NativeWebRtcRuntime(NativeWebRtcRuntime&& other) noexcept;
    NativeWebRtcRuntime& operator=(NativeWebRtcRuntime&& other) noexcept;
    ~NativeWebRtcRuntime() = default;

    static NativeWebRtcLoadResult loadBundled();
#if defined(HELICOID_MANAGER_NATIVE_WEBRTC_TESTING)
    static NativeWebRtcLoadResult loadForTesting(const std::filesystem::path& library);
#endif

    std::uint32_t abiVersion() const noexcept { return abiVersion_; }
    std::uint32_t capabilities() const noexcept { return capabilities_; }
    bool supportsConnection() const noexcept;
    bool supportsObservation() const noexcept;
    std::unique_ptr<NativeWebRtcEngine> createEngine(std::uint32_t& outStatus) const;

private:
    friend class NativeWebRtcEngine;
    static NativeWebRtcLoadResult loadFromPath(const std::filesystem::path& library);
    NativeWebRtcRuntime(std::shared_ptr<NativeWebRtcModule> module,
                        std::uint32_t abiVersion,
                        std::uint32_t capabilities) noexcept;

    std::shared_ptr<NativeWebRtcModule> module_;
    std::uint32_t abiVersion_ = 0;
    std::uint32_t capabilities_ = 0;
};

struct NativeWebRtcLoadResult {
    std::optional<NativeWebRtcRuntime> runtime;
    std::optional<NativeWebRtcLoadError> error;
};

} // namespace helicoid::manager
