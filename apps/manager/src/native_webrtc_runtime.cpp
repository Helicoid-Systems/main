#include "native_webrtc_runtime.h"
#include "secure_memory.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <dlfcn.h>
#else
#include <dlfcn.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>
#include <utility>
#include <vector>

namespace helicoid::manager {
namespace {

using AbiVersionFunction = std::uint32_t (*)();
using CapabilitiesFunction = std::uint32_t (*)(std::uint32_t*);
using EngineInitFunction = std::uint32_t (*)(hs_engine**);
using EngineShutdownFunction = std::uint32_t (*)(hs_engine*);
using EngineDrainFunction = std::uint32_t (*)(hs_engine*, std::uint32_t);
using PeerCreateFunction = std::uint32_t (*)(hs_engine*, hs_peer**);
using PeerDrainFunction = std::uint32_t (*)(hs_peer*, std::uint32_t);
using PeerValueFunction = std::uint32_t (*)(hs_peer*, std::uint32_t*);
using ChannelCreateFunction = std::uint32_t (*)(hs_peer*, const char*, std::size_t);
using CreateDescriptionFunction = std::uint32_t (*)(hs_peer*, char*, std::size_t, std::size_t*);
using SetRemoteDescriptionFunction = std::uint32_t (*)(
    hs_peer*, const char*, std::size_t, const char*, std::size_t);
using AddIceCandidateFunction = std::uint32_t (*)(hs_peer*, const char*, std::size_t);
using PollIceCandidateFunction = std::uint32_t (*)(hs_peer*, char*, std::size_t, std::size_t*);
using ChannelCloseFunction = std::uint32_t (*)(hs_peer*);
using SetExpectedCertificateFunction = std::uint32_t (*)(hs_peer*, const char*, std::size_t);
using CertificateExportFunction = std::uint32_t (*)(hs_peer*, std::uint8_t*, std::uint32_t, std::uint32_t*);
using VerifiedChannelAcquireFunction = std::uint32_t (*)(hs_peer*, hs_verified_channel**);
using VerifiedCertificateExportFunction = std::uint32_t (*)(
    hs_verified_channel*, std::uint8_t*, std::uint32_t, std::uint32_t*);
using ChannelBindingExportFunction = VerifiedCertificateExportFunction;
using VerifiedChannelReleaseFunction = std::uint32_t (*)(hs_verified_channel*);
using ChannelSendFunction = std::uint32_t (*)(hs_peer*, const std::uint8_t*, std::uint32_t);
using ChannelPollFunction = std::uint32_t (*)(hs_peer*, std::uint8_t*, std::uint32_t, std::uint32_t*);
using VideoPrepareFunction = std::uint32_t (*)(hs_peer*, std::uint32_t);
using AdmissionBeginFunction = std::uint32_t (*)(
    hs_peer*, const std::uint8_t*, std::uint32_t, std::uint32_t);
using PeerActionFunction = std::uint32_t (*)(hs_peer*);
using CaptureBeginFunction = std::uint32_t (*)(hs_peer*, std::uint32_t);
using VideoFramePushFunction = std::uint32_t (*)(
    hs_peer*, const std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using MediaFrameAcquireFunction = std::uint32_t (*)(
    hs_peer*, std::uint32_t, std::uint8_t*, std::uint32_t*, std::uint32_t*);

constexpr std::size_t maxObservationIceBytes = 4 * 1024;

constexpr std::uint32_t requiredConnectionCapabilities =
    HS_CAP_INITIALIZATION | HS_CAP_NEGOTIATION | HS_CAP_CHANNEL_EVIDENCE | HS_CAP_DRAIN;
constexpr std::uint32_t requiredObservationCapabilities =
    requiredConnectionCapabilities | HS_CAP_CAPTURE | HS_CAP_ADMISSION | HS_CAP_MEDIA;

constexpr std::array<std::string_view, 27> requiredConnectionExports{
    "hs_engine_init",
    "hs_engine_shutdown",
    "hs_engine_drain",
    "hs_peer_create",
    "hs_peer_close",
    "hs_peer_free",
    "hs_peer_generation",
    "hs_peer_state",
    "hs_peer_local_cert_export",
    "hs_channel_create",
    "hs_peer_create_offer",
    "hs_peer_create_answer",
    "hs_peer_set_local",
    "hs_peer_set_remote",
    "hs_peer_add_ice",
    "hs_peer_poll_ice",
    "hs_channel_send",
    "hs_channel_poll",
    "hs_channel_close",
    "hs_peer_expected_cert_set",
    "hs_verified_channel_acquire",
    "hs_verified_channel_export",
    "hs_verified_channel_binding_export",
    "hs_verified_channel_generation",
    "hs_verified_channel_valid",
    "hs_verified_channel_release",
    "hs_peer_drain",
};

constexpr std::array<std::string_view, 7> requiredObservationExports{
    "hs_peer_video_prepare",
    "hs_capture_begin",
    "hs_capture_end",
    "hs_admission_begin",
    "hs_admission_end",
    "hs_peer_video_frame_push",
    "hs_media_frame_acquire",
};

struct OpenModule {
#if defined(_WIN32)
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif
    std::atomic_bool retainUntilProcessExit{false};

    void retainAfterUnprovenTeardown() noexcept
    {
        retainUntilProcessExit.store(true, std::memory_order_release);
    }

    ~OpenModule()
    {
        // The bridge may still own callbacks/peers after a failed barrier.
        // Keep its code mapped until process exit rather than risk a late-call UAF.
        if (retainUntilProcessExit.load(std::memory_order_acquire)) return;
#if defined(_WIN32)
        if (handle != nullptr) FreeLibrary(handle);
#else
        if (handle != nullptr) dlclose(handle);
#endif
    }
};

std::shared_ptr<OpenModule> openModule(const std::filesystem::path& path)
{
    auto module = std::make_shared<OpenModule>();
#if defined(_WIN32)
    module->handle = LoadLibraryW(path.c_str());
#else
    module->handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    return module->handle == nullptr ? nullptr : module;
}

std::filesystem::path bundledLibraryPath()
{
#if defined(_WIN32)
    std::vector<wchar_t> executablePath(32768);
    const DWORD length = GetModuleFileNameW(
        nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    if (length == 0 || length >= executablePath.size()) return {};
    return std::filesystem::path(executablePath.data()).parent_path() /
           L"hs_webrtc_shared.dll";
#elif defined(__APPLE__)
    std::uint32_t requiredBytes = 0;
    (void)_NSGetExecutablePath(nullptr, &requiredBytes);
    if (requiredBytes == 0) return {};
    std::vector<char> executablePath(requiredBytes);
    if (_NSGetExecutablePath(executablePath.data(), &requiredBytes) != 0) return {};
    std::error_code error;
    const std::filesystem::path executable =
        std::filesystem::canonical(executablePath.data(), error);
    if (error || executable.empty()) return {};
    return executable.parent_path().parent_path() / "Frameworks" /
           "libhs_webrtc_shared.dylib";
#else
    std::error_code error;
    const std::filesystem::path executable =
        std::filesystem::read_symlink("/proc/self/exe", error);
    if (error || executable.empty()) return {};
    return executable.parent_path() / "libhs_webrtc_shared.so";
#endif
}

bool validText(std::string_view text, std::size_t maximumBytes)
{
    return !text.empty() && text.size() <= maximumBytes &&
           std::find(text.begin(), text.end(), '\0') == text.end();
}

std::uint32_t createDescription(hs_peer* peer,
                               CreateDescriptionFunction create,
                               std::string& outSdp)
{
    outSdp.clear();
    if (peer == nullptr || create == nullptr) return HS_STATUS_UNAVAILABLE;
    std::array<char, HS_MAX_SDP_BYTES + 1U> buffer{};
    std::size_t length = 0;
    const std::uint32_t status = create(peer, buffer.data(), buffer.size(), &length);
    if (status != HS_STATUS_OK) return status;
    if (length == 0 || length > HS_MAX_SDP_BYTES || buffer[length] != '\0' ||
        std::find(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(length), '\0') !=
            buffer.begin() + static_cast<std::ptrdiff_t>(length)) {
        return HS_STATUS_BOUNDS;
    }
    outSdp.assign(buffer.data(), length);
    return HS_STATUS_OK;
}

template <typename Function>
Function findFunction(const OpenModule& module, const char* name)
{
#if defined(_WIN32)
    return reinterpret_cast<Function>(GetProcAddress(module.handle, name));
#else
    return reinterpret_cast<Function>(dlsym(module.handle, name));
#endif
}

} // namespace

struct NativeWebRtcModule {
    std::shared_ptr<OpenModule> library;
    AbiVersionFunction abiVersion = nullptr;
    CapabilitiesFunction capabilities = nullptr;
    EngineInitFunction engineInit = nullptr;
    EngineShutdownFunction engineShutdown = nullptr;
    EngineDrainFunction engineDrain = nullptr;
    PeerCreateFunction peerCreate = nullptr;
    PeerDrainFunction peerDrain = nullptr;
    PeerValueFunction peerGeneration = nullptr;
    PeerValueFunction peerState = nullptr;
    ChannelCreateFunction channelCreate = nullptr;
    ChannelCloseFunction channelClose = nullptr;
    CreateDescriptionFunction createOffer = nullptr;
    CreateDescriptionFunction createAnswer = nullptr;
    SetRemoteDescriptionFunction setRemoteDescription = nullptr;
    AddIceCandidateFunction addIceCandidate = nullptr;
    PollIceCandidateFunction pollIceCandidate = nullptr;
    SetExpectedCertificateFunction setExpectedCertificate = nullptr;
    CertificateExportFunction localCertificateExport = nullptr;
    VerifiedChannelAcquireFunction verifiedChannelAcquire = nullptr;
    VerifiedCertificateExportFunction verifiedCertificateExport = nullptr;
    ChannelBindingExportFunction channelBindingExport = nullptr;
    VerifiedChannelReleaseFunction verifiedChannelRelease = nullptr;
    ChannelSendFunction channelSend = nullptr;
    ChannelPollFunction channelPoll = nullptr;
    VideoPrepareFunction videoPrepare = nullptr;
    AdmissionBeginFunction admissionBegin = nullptr;
    PeerActionFunction admissionEnd = nullptr;
    CaptureBeginFunction captureBegin = nullptr;
    PeerActionFunction captureEnd = nullptr;
    VideoFramePushFunction videoFramePush = nullptr;
    MediaFrameAcquireFunction mediaFrameAcquire = nullptr;
};

namespace {

void retainCodeAfterUnprovenTeardown(
    const std::shared_ptr<NativeWebRtcModule>& module) noexcept
{
    if (module && module->library) {
        module->library->retainAfterUnprovenTeardown();
    }
}

} // namespace

NativeWebRtcEngine::NativeWebRtcEngine(std::shared_ptr<NativeWebRtcModule> module,
                                       hs_engine* handle) noexcept
    : module_(std::move(module)), handle_(handle)
{
}

NativeWebRtcEngine::NativeWebRtcEngine(NativeWebRtcEngine&& other) noexcept
    : module_(std::move(other.module_)), handle_(std::exchange(other.handle_, nullptr))
{
}

NativeWebRtcEngine& NativeWebRtcEngine::operator=(NativeWebRtcEngine&& other) noexcept
{
    if (this != &other) {
        shutdown();
        module_ = std::move(other.module_);
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}

NativeWebRtcEngine::~NativeWebRtcEngine()
{
    shutdown();
}

std::uint32_t NativeWebRtcEngine::drain(std::uint32_t timeoutMs) noexcept
{
    if (handle_ == nullptr || !module_ || module_->engineDrain == nullptr) {
        return HS_STATUS_NULL;
    }
    return module_->engineDrain(handle_, timeoutMs);
}

std::uint32_t NativeWebRtcEngine::shutdown() noexcept
{
    if (handle_ == nullptr) return HS_STATUS_OK;
    hs_engine* handle = std::exchange(handle_, nullptr);
    const std::uint32_t status = module_ && module_->engineShutdown != nullptr
        ? module_->engineShutdown(handle)
        : HS_STATUS_INTERNAL;
    if (status != HS_STATUS_OK) retainCodeAfterUnprovenTeardown(module_);
    module_.reset();
    return status;
}

std::unique_ptr<NativeWebRtcPeer> NativeWebRtcEngine::createPeer(
    std::uint32_t& outStatus) const
{
    outStatus = HS_STATUS_INTERNAL;
    if (handle_ == nullptr || !module_ || module_->peerCreate == nullptr ||
        module_->peerDrain == nullptr) {
        outStatus = HS_STATUS_UNAVAILABLE;
        return nullptr;
    }

    hs_peer* handle = nullptr;
    outStatus = module_->peerCreate(handle_, &handle);
    if (outStatus != HS_STATUS_OK || handle == nullptr) {
        if (handle != nullptr && module_->peerDrain(handle, 5000) != HS_STATUS_OK) {
            retainCodeAfterUnprovenTeardown(module_);
        }
        if (outStatus == HS_STATUS_OK) outStatus = HS_STATUS_INTERNAL;
        return nullptr;
    }
    auto peer = std::unique_ptr<NativeWebRtcPeer>(
        new (std::nothrow) NativeWebRtcPeer(module_, handle));
    if (!peer) {
        if (module_->peerDrain(handle, 5000) != HS_STATUS_OK) {
            retainCodeAfterUnprovenTeardown(module_);
        }
        outStatus = HS_STATUS_EXHAUSTED;
        return nullptr;
    }
    return peer;
}

NativeWebRtcPeer::NativeWebRtcPeer(std::shared_ptr<NativeWebRtcModule> module,
                                   hs_peer* handle) noexcept
    : module_(std::move(module)), handle_(handle)
{
}

NativeWebRtcPeer::NativeWebRtcPeer(NativeWebRtcPeer&& other) noexcept
    : module_(std::move(other.module_)), handle_(std::exchange(other.handle_, nullptr))
{
}

NativeWebRtcPeer& NativeWebRtcPeer::operator=(NativeWebRtcPeer&& other) noexcept
{
    if (this != &other) {
        drain(5000);
        module_ = std::move(other.module_);
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}

NativeWebRtcPeer::~NativeWebRtcPeer()
{
    drain(5000);
}

std::uint32_t NativeWebRtcPeer::drain(std::uint32_t timeoutMs) noexcept
{
    if (handle_ == nullptr) return HS_STATUS_OK;
    if (!module_ || module_->peerDrain == nullptr) return HS_STATUS_INTERNAL;
    const std::uint32_t status = module_->peerDrain(handle_, timeoutMs);
    if (status == HS_STATUS_OK || status == HS_STATUS_NULL) {
        handle_ = nullptr;
        module_.reset();
    } else {
        retainCodeAfterUnprovenTeardown(module_);
    }
    return status;
}

std::uint32_t NativeWebRtcPeer::generation(std::uint32_t& outGeneration) noexcept
{
    outGeneration = 0;
    if (handle_ == nullptr || !module_ || module_->peerGeneration == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    const std::uint32_t status = module_->peerGeneration(handle_, &outGeneration);
    if (status != HS_STATUS_OK) outGeneration = 0;
    return status;
}

std::uint32_t NativeWebRtcPeer::state(std::uint32_t& outState) noexcept
{
    outState = HS_PEER_STATE_CLOSED;
    if (handle_ == nullptr || !module_ || module_->peerState == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    const std::uint32_t status = module_->peerState(handle_, &outState);
    if (status != HS_STATUS_OK) outState = HS_PEER_STATE_CLOSED;
    return status;
}

std::uint32_t NativeWebRtcPeer::prepareVideo(std::uint32_t role) noexcept
{
    if (handle_ == nullptr || !module_ || module_->videoPrepare == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    if (role != HS_VIDEO_ROLE_SEND_ONLY && role != HS_VIDEO_ROLE_RECEIVE_ONLY) {
        return HS_STATUS_INVALID;
    }
    return module_->videoPrepare(handle_, role);
}

std::uint32_t NativeWebRtcPeer::beginObservation(
    std::span<const std::uint8_t> channelBinding, std::uint32_t leaseMs) noexcept
{
    if (handle_ == nullptr || !module_ || module_->admissionBegin == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    if (channelBinding.empty() || channelBinding.size() > HS_MAX_CHANNEL_BINDING_BYTES ||
        leaseMs == 0 || leaseMs > HS_MAX_OBSERVATION_LEASE_MS) {
        return HS_STATUS_BOUNDS;
    }
    return module_->admissionBegin(
        handle_, channelBinding.data(), static_cast<std::uint32_t>(channelBinding.size()), leaseMs);
}

std::uint32_t NativeWebRtcPeer::endObservation() noexcept
{
    if (handle_ == nullptr || !module_ || module_->admissionEnd == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    return module_->admissionEnd(handle_);
}

std::uint32_t NativeWebRtcPeer::beginCapture(std::uint32_t expectedGeneration) noexcept
{
    if (handle_ == nullptr || !module_ || module_->captureBegin == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    return module_->captureBegin(handle_, expectedGeneration);
}

std::uint32_t NativeWebRtcPeer::endCapture() noexcept
{
    if (handle_ == nullptr || !module_ || module_->captureEnd == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    return module_->captureEnd(handle_);
}

std::uint32_t NativeWebRtcPeer::pushScreenFrame(
    std::span<const std::uint8_t> bgra, std::uint32_t width,
    std::uint32_t height, std::uint32_t stride) noexcept
{
    if (handle_ == nullptr || !module_ || module_->videoFramePush == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    if (width == 0 || width > HS_MAX_SCREEN_FRAME_WIDTH || height == 0 ||
        height > HS_MAX_SCREEN_FRAME_HEIGHT || stride < width * 4U ||
        static_cast<std::uint64_t>(stride) * height != bgra.size() ||
        bgra.size() > HS_MAX_SCREEN_FRAME_BYTES) {
        return HS_STATUS_BOUNDS;
    }
    return module_->videoFramePush(handle_, bgra.data(),
        static_cast<std::uint32_t>(bgra.size()), width, height, stride);
}

std::uint32_t NativeWebRtcPeer::createProductDataChannel() noexcept
{
    if (handle_ == nullptr || !module_ || module_->channelCreate == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    constexpr std::size_t labelLength = sizeof(HS_PRODUCT_CHANNEL_LABEL) - 1U;
    return module_->channelCreate(handle_, HS_PRODUCT_CHANNEL_LABEL, labelLength);
}

std::uint32_t NativeWebRtcPeer::closeProductDataChannel() noexcept
{
    if (handle_ == nullptr || !module_ || module_->channelClose == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    return module_->channelClose(handle_);
}

std::uint32_t NativeWebRtcPeer::createOffer(std::string& outSdp)
{
    return createDescription(handle_, module_ ? module_->createOffer : nullptr, outSdp);
}

std::uint32_t NativeWebRtcPeer::createAnswer(std::string& outSdp)
{
    return createDescription(handle_, module_ ? module_->createAnswer : nullptr, outSdp);
}

std::uint32_t NativeWebRtcPeer::setRemoteDescription(
    std::string_view type, std::string_view sdp) noexcept
{
    if (handle_ == nullptr || !module_ || module_->setRemoteDescription == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    if (type != "offer" && type != "answer") return HS_STATUS_INVALID;
    if (!validText(sdp, HS_MAX_SDP_BYTES)) return HS_STATUS_BOUNDS;
    return module_->setRemoteDescription(
        handle_, type.data(), type.size(), sdp.data(), sdp.size());
}

std::uint32_t NativeWebRtcPeer::addIceCandidate(std::string_view candidate) noexcept
{
    if (handle_ == nullptr || !module_ || module_->addIceCandidate == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    if (!validText(candidate, maxObservationIceBytes)) return HS_STATUS_BOUNDS;
    return module_->addIceCandidate(handle_, candidate.data(), candidate.size());
}

std::uint32_t NativeWebRtcPeer::pollIceCandidate(std::string& outCandidate)
{
    outCandidate.clear();
    if (handle_ == nullptr || !module_ || module_->pollIceCandidate == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    std::array<char, maxObservationIceBytes + 1U> buffer{};
    std::size_t length = 0;
    const std::uint32_t status = module_->pollIceCandidate(
        handle_, buffer.data(), buffer.size(), &length);
    if (status != HS_STATUS_OK) return status;
    if (length == 0 || length > maxObservationIceBytes || buffer[length] != '\0' ||
        std::find(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(length), '\0') !=
            buffer.begin() + static_cast<std::ptrdiff_t>(length)) {
        return HS_STATUS_BOUNDS;
    }
    outCandidate.assign(buffer.data(), length);
    return HS_STATUS_OK;
}

std::uint32_t NativeWebRtcPeer::setExpectedRemoteCertificate(
    std::string_view certificatePem) noexcept
{
    if (handle_ == nullptr || !module_ || module_->setExpectedCertificate == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    if (!validText(certificatePem, HS_MAX_CERT_PEM_BYTES)) return HS_STATUS_BOUNDS;
    return module_->setExpectedCertificate(
        handle_, certificatePem.data(), certificatePem.size());
}

std::uint32_t NativeWebRtcPeer::exportLocalCertificate(std::string& outCertificatePem)
{
    outCertificatePem.clear();
    if (handle_ == nullptr || !module_ || module_->localCertificateExport == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    std::array<std::uint8_t, HS_MAX_CERT_PEM_BYTES> buffer{};
    std::uint32_t length = 0;
    const std::uint32_t status = module_->localCertificateExport(
        handle_, buffer.data(), static_cast<std::uint32_t>(buffer.size()), &length);
    if (status != HS_STATUS_OK) return status;
    if (length == 0 || length > buffer.size() ||
        std::find(buffer.begin(), buffer.begin() + length, std::uint8_t{0}) !=
            buffer.begin() + length) {
        return HS_STATUS_BOUNDS;
    }
    outCertificatePem.assign(reinterpret_cast<const char*>(buffer.data()), length);
    return HS_STATUS_OK;
}

std::uint32_t NativeWebRtcPeer::exportVerifiedRemoteCertificate(
    std::string& outCertificatePem)
{
    outCertificatePem.clear();
    if (handle_ == nullptr || !module_ || module_->verifiedChannelAcquire == nullptr ||
        module_->verifiedCertificateExport == nullptr || module_->verifiedChannelRelease == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    hs_verified_channel* channel = nullptr;
    const std::uint32_t acquireStatus = module_->verifiedChannelAcquire(handle_, &channel);
    if (acquireStatus != HS_STATUS_OK) return acquireStatus;
    if (channel == nullptr) return HS_STATUS_INTERNAL;

    std::array<std::uint8_t, HS_MAX_CERT_PEM_BYTES> buffer{};
    std::uint32_t length = 0;
    const std::uint32_t exportStatus = module_->verifiedCertificateExport(
        channel, buffer.data(), static_cast<std::uint32_t>(buffer.size()), &length);
    const std::uint32_t releaseStatus = module_->verifiedChannelRelease(channel);
    if (exportStatus != HS_STATUS_OK) return exportStatus;
    if (releaseStatus != HS_STATUS_OK) return releaseStatus;
    if (length == 0 || length > buffer.size() ||
        std::find(buffer.begin(), buffer.begin() + length, std::uint8_t{0}) !=
            buffer.begin() + length) {
        return HS_STATUS_BOUNDS;
    }
    outCertificatePem.assign(reinterpret_cast<const char*>(buffer.data()), length);
    return HS_STATUS_OK;
}

std::uint32_t NativeWebRtcPeer::exportVerifiedChannelBinding(
    std::vector<std::uint8_t>& outBinding)
{
    outBinding.clear();
    if (handle_ == nullptr || !module_ || module_->verifiedChannelAcquire == nullptr ||
        module_->channelBindingExport == nullptr || module_->verifiedChannelRelease == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    hs_verified_channel* channel = nullptr;
    const std::uint32_t acquireStatus = module_->verifiedChannelAcquire(handle_, &channel);
    if (acquireStatus != HS_STATUS_OK) return acquireStatus;
    if (channel == nullptr) return HS_STATUS_INTERNAL;

    std::array<std::uint8_t, HS_MAX_CHANNEL_BINDING_BYTES> buffer{};
    std::uint32_t length = 0;
    const std::uint32_t exportStatus = module_->channelBindingExport(
        channel, buffer.data(), static_cast<std::uint32_t>(buffer.size()), &length);
    const std::uint32_t releaseStatus = module_->verifiedChannelRelease(channel);
    if (exportStatus != HS_STATUS_OK) return exportStatus;
    if (releaseStatus != HS_STATUS_OK) return releaseStatus;
    if (length == 0 || length > buffer.size()) return HS_STATUS_BOUNDS;
    outBinding.assign(buffer.begin(), buffer.begin() + length);
    return HS_STATUS_OK;
}

std::uint32_t NativeWebRtcPeer::sendProductMessage(
    std::span<const std::uint8_t> message) noexcept
{
    if (handle_ == nullptr || !module_ || module_->channelSend == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    if (message.empty() || message.size() > HS_MAX_MSG_BYTES) return HS_STATUS_BOUNDS;
    return module_->channelSend(
        handle_, message.data(), static_cast<std::uint32_t>(message.size()));
}

std::uint32_t NativeWebRtcPeer::pollProductMessage(std::vector<std::uint8_t>& outMessage)
{
    outMessage.clear();
    if (handle_ == nullptr || !module_ || module_->channelPoll == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }
    std::array<std::uint8_t, HS_MAX_MSG_BYTES> buffer{};
    std::uint32_t length = 0;
    const std::uint32_t status = module_->channelPoll(
        handle_, buffer.data(), static_cast<std::uint32_t>(buffer.size()), &length);
    if (status != HS_STATUS_OK) return status;
    if (length == 0 || length > buffer.size()) return HS_STATUS_BOUNDS;
    outMessage.assign(buffer.begin(), buffer.begin() + length);
    return HS_STATUS_OK;
}

NativeScreenFrame::NativeScreenFrame(NativeScreenFrame&& other) noexcept
    : width(std::exchange(other.width, 0)),
      height(std::exchange(other.height, 0)),
      bgra(std::move(other.bgra))
{
}

NativeScreenFrame& NativeScreenFrame::operator=(NativeScreenFrame&& other) noexcept
{
    if (this != &other) {
        clear();
        width = std::exchange(other.width, 0);
        height = std::exchange(other.height, 0);
        bgra = std::move(other.bgra);
    }
    return *this;
}

NativeScreenFrame::~NativeScreenFrame()
{
    clear();
}

void NativeScreenFrame::clear() noexcept
{
    hs_webrtc_internal::SecureZero(bgra);
    bgra.clear();
    width = 0;
    height = 0;
}

std::uint32_t NativeWebRtcPeer::acquireScreenFrame(NativeScreenFrame& outFrame)
{
    outFrame.clear();
    if (handle_ == nullptr || !module_ || module_->mediaFrameAcquire == nullptr) {
        return HS_STATUS_UNAVAILABLE;
    }

    std::vector<std::uint8_t> pixels(HS_MAX_SCREEN_FRAME_BYTES);
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    const std::uint32_t status = module_->mediaFrameAcquire(
        handle_, static_cast<std::uint32_t>(pixels.size()), pixels.data(), &width, &height);
    if (status != HS_STATUS_OK) {
        hs_webrtc_internal::SecureZero(pixels);
        return status;
    }
    if (width == 0 || height == 0 || width > HS_MAX_SCREEN_FRAME_WIDTH ||
        height > HS_MAX_SCREEN_FRAME_HEIGHT) {
        hs_webrtc_internal::SecureZero(pixels);
        return HS_STATUS_BOUNDS;
    }
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4U;
    const std::size_t byteCount = rowBytes * static_cast<std::size_t>(height);
    if (byteCount > pixels.size()) {
        hs_webrtc_internal::SecureZero(pixels);
        return HS_STATUS_BOUNDS;
    }

    pixels.resize(byteCount);
    outFrame.width = width;
    outFrame.height = height;
    outFrame.bgra = std::move(pixels);
    return HS_STATUS_OK;
}

NativeWebRtcRuntime::NativeWebRtcRuntime(std::shared_ptr<NativeWebRtcModule> module,
                                         std::uint32_t abiVersion,
                                         std::uint32_t capabilities) noexcept
    : module_(std::move(module)), abiVersion_(abiVersion), capabilities_(capabilities)
{
}

NativeWebRtcRuntime::NativeWebRtcRuntime(NativeWebRtcRuntime&& other) noexcept
    : module_(std::move(other.module_)),
      abiVersion_(std::exchange(other.abiVersion_, 0)),
      capabilities_(std::exchange(other.capabilities_, 0))
{
}

NativeWebRtcRuntime& NativeWebRtcRuntime::operator=(NativeWebRtcRuntime&& other) noexcept
{
    if (this != &other) {
        module_ = std::move(other.module_);
        abiVersion_ = std::exchange(other.abiVersion_, 0);
        capabilities_ = std::exchange(other.capabilities_, 0);
    }
    return *this;
}

NativeWebRtcLoadResult NativeWebRtcRuntime::loadBundled()
{
    return loadFromPath(bundledLibraryPath());
}

#if defined(HELICOID_MANAGER_NATIVE_WEBRTC_TESTING)
NativeWebRtcLoadResult NativeWebRtcRuntime::loadForTesting(
    const std::filesystem::path& library)
{
    return loadFromPath(library);
}
#endif

NativeWebRtcLoadResult NativeWebRtcRuntime::loadFromPath(
    const std::filesystem::path& bundledLibrary)
{
    if (bundledLibrary.empty()) {
        return {.error = NativeWebRtcLoadError{NativeWebRtcLoadErrorCode::EmptyPath}};
    }
    if (!bundledLibrary.is_absolute()) {
        return {.error = NativeWebRtcLoadError{
                    NativeWebRtcLoadErrorCode::PathMustBeAbsolute}};
    }

    auto library = openModule(bundledLibrary);
    if (!library) {
        return {.error = NativeWebRtcLoadError{NativeWebRtcLoadErrorCode::LibraryUnavailable}};
    }

    auto module = std::make_shared<NativeWebRtcModule>();
    module->library = std::move(library);
    module->abiVersion = findFunction<AbiVersionFunction>(
        *module->library, "hs_abi_version");
    module->capabilities = findFunction<CapabilitiesFunction>(
        *module->library, "hs_engine_capabilities");
    module->engineInit = findFunction<EngineInitFunction>(
        *module->library, "hs_engine_init");
    module->engineShutdown = findFunction<EngineShutdownFunction>(
        *module->library, "hs_engine_shutdown");
    module->engineDrain = findFunction<EngineDrainFunction>(
        *module->library, "hs_engine_drain");
    module->peerCreate = findFunction<PeerCreateFunction>(
        *module->library, "hs_peer_create");
    module->peerDrain = findFunction<PeerDrainFunction>(
        *module->library, "hs_peer_drain");
    module->peerGeneration = findFunction<PeerValueFunction>(
        *module->library, "hs_peer_generation");
    module->peerState = findFunction<PeerValueFunction>(
        *module->library, "hs_peer_state");
    module->channelCreate = findFunction<ChannelCreateFunction>(
        *module->library, "hs_channel_create");
    module->channelClose = findFunction<ChannelCloseFunction>(
        *module->library, "hs_channel_close");
    module->createOffer = findFunction<CreateDescriptionFunction>(
        *module->library, "hs_peer_create_offer");
    module->createAnswer = findFunction<CreateDescriptionFunction>(
        *module->library, "hs_peer_create_answer");
    module->setRemoteDescription = findFunction<SetRemoteDescriptionFunction>(
        *module->library, "hs_peer_set_remote");
    module->addIceCandidate = findFunction<AddIceCandidateFunction>(
        *module->library, "hs_peer_add_ice");
    module->pollIceCandidate = findFunction<PollIceCandidateFunction>(
        *module->library, "hs_peer_poll_ice");
    module->setExpectedCertificate = findFunction<SetExpectedCertificateFunction>(
        *module->library, "hs_peer_expected_cert_set");
    module->localCertificateExport = findFunction<CertificateExportFunction>(
        *module->library, "hs_peer_local_cert_export");
    module->verifiedChannelAcquire = findFunction<VerifiedChannelAcquireFunction>(
        *module->library, "hs_verified_channel_acquire");
    module->verifiedCertificateExport = findFunction<VerifiedCertificateExportFunction>(
        *module->library, "hs_verified_channel_export");
    module->channelBindingExport = findFunction<ChannelBindingExportFunction>(
        *module->library, "hs_verified_channel_binding_export");
    module->verifiedChannelRelease = findFunction<VerifiedChannelReleaseFunction>(
        *module->library, "hs_verified_channel_release");
    module->channelSend = findFunction<ChannelSendFunction>(
        *module->library, "hs_channel_send");
    module->channelPoll = findFunction<ChannelPollFunction>(
        *module->library, "hs_channel_poll");
    module->videoPrepare = findFunction<VideoPrepareFunction>(
        *module->library, "hs_peer_video_prepare");
    module->admissionBegin = findFunction<AdmissionBeginFunction>(
        *module->library, "hs_admission_begin");
    module->admissionEnd = findFunction<PeerActionFunction>(
        *module->library, "hs_admission_end");
    module->captureBegin = findFunction<CaptureBeginFunction>(
        *module->library, "hs_capture_begin");
    module->captureEnd = findFunction<PeerActionFunction>(
        *module->library, "hs_capture_end");
    module->videoFramePush = findFunction<VideoFramePushFunction>(
        *module->library, "hs_peer_video_frame_push");
    module->mediaFrameAcquire = findFunction<MediaFrameAcquireFunction>(
        *module->library, "hs_media_frame_acquire");
    if (module->abiVersion == nullptr || module->capabilities == nullptr) {
        return {.error = NativeWebRtcLoadError{
                    NativeWebRtcLoadErrorCode::MissingRequiredExport}};
    }
    for (const std::string_view name : requiredConnectionExports) {
        if (findFunction<void (*)()>(*module->library, name.data()) == nullptr) {
            return {.error = NativeWebRtcLoadError{
                        NativeWebRtcLoadErrorCode::MissingRequiredExport}};
        }
    }

    const std::uint32_t abiVersion = module->abiVersion();
    if (abiVersion != HS_ABI_VERSION) {
        return {.error = NativeWebRtcLoadError{
                    NativeWebRtcLoadErrorCode::AbiVersionMismatch,
                    HS_STATUS_OK,
                    abiVersion}};
    }

    std::uint32_t capabilities = 0;
    const std::uint32_t status = module->capabilities(&capabilities);
    if (status != HS_STATUS_OK) {
        return {.error = NativeWebRtcLoadError{
                    NativeWebRtcLoadErrorCode::CapabilityQueryFailed,
                    status,
                    abiVersion}};
    }

    if ((capabilities & requiredObservationCapabilities) == requiredObservationCapabilities) {
        for (const std::string_view name : requiredObservationExports) {
            if (findFunction<void (*)()>(*module->library, name.data()) == nullptr) {
                return {.error = NativeWebRtcLoadError{
                            NativeWebRtcLoadErrorCode::MissingRequiredExport}};
            }
        }
    }

    const std::uint32_t missing = requiredConnectionCapabilities & ~capabilities;
    if (missing != 0) {
        return {.error = NativeWebRtcLoadError{
                    NativeWebRtcLoadErrorCode::RequiredCapabilitiesMissing,
                    HS_STATUS_OK,
                    abiVersion,
                    missing}};
    }

    return {.runtime = NativeWebRtcRuntime(
                std::move(module), abiVersion, capabilities)};
}

bool NativeWebRtcRuntime::supportsConnection() const noexcept
{
    return (capabilities_ & requiredConnectionCapabilities) ==
           requiredConnectionCapabilities;
}

bool NativeWebRtcRuntime::supportsObservation() const noexcept
{
    return (capabilities_ & requiredObservationCapabilities) ==
           requiredObservationCapabilities;
}

std::unique_ptr<NativeWebRtcEngine> NativeWebRtcRuntime::createEngine(
    std::uint32_t& outStatus) const
{
    outStatus = HS_STATUS_INTERNAL;
    if (!module_ || module_->engineInit == nullptr || module_->engineShutdown == nullptr ||
        module_->engineDrain == nullptr || !supportsConnection()) {
        outStatus = HS_STATUS_UNAVAILABLE;
        return nullptr;
    }

    hs_engine* handle = nullptr;
    outStatus = module_->engineInit(&handle);
    if (outStatus != HS_STATUS_OK || handle == nullptr) {
        if (handle != nullptr && module_->engineShutdown(handle) != HS_STATUS_OK) {
            retainCodeAfterUnprovenTeardown(module_);
        }
        if (outStatus == HS_STATUS_OK) outStatus = HS_STATUS_INTERNAL;
        return nullptr;
    }
    auto engine = std::unique_ptr<NativeWebRtcEngine>(
        new (std::nothrow) NativeWebRtcEngine(module_, handle));
    if (!engine) {
        if (module_->engineShutdown(handle) != HS_STATUS_OK) {
            retainCodeAfterUnprovenTeardown(module_);
        }
        outStatus = HS_STATUS_EXHAUSTED;
        return nullptr;
    }
    return engine;
}

} // namespace helicoid::manager
