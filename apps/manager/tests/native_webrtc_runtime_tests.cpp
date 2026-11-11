#include "native_webrtc_runtime.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--bundled") {
        auto bundled = helicoid::manager::NativeWebRtcRuntime::loadBundled();
        require(bundled.runtime.has_value(),
                "the fixed application-bundle path must load the native library");
        require(bundled.runtime->supportsConnection(),
                "the packaged library must expose channel-bound connection capabilities");
        require(!bundled.runtime->supportsObservation(),
                "the packaged library must not claim unaccepted observation capabilities");

        std::uint32_t engineStatus = HS_STATUS_INTERNAL;
        auto engine = bundled.runtime->createEngine(engineStatus);
        require(engine && engineStatus == HS_STATUS_OK && engine->isOpen(),
                "the packaged native library must initialize its engine");
        require(engine->shutdown() == HS_STATUS_OK,
                "the packaged native library must shut down cleanly");
        return 0;
    }

    require(argc == 8,
            "expected valid, mismatched-ABI, incomplete-capability, and missing-export libraries");

    const auto bundled = helicoid::manager::NativeWebRtcRuntime::loadBundled();
    require(!bundled.runtime && bundled.error &&
                bundled.error->code == helicoid::manager::NativeWebRtcLoadErrorCode::LibraryUnavailable,
            "production loading must use only the fixed application-bundle location");

    const auto empty = helicoid::manager::NativeWebRtcRuntime::loadForTesting({});
    require(!empty.runtime && empty.error &&
                empty.error->code == helicoid::manager::NativeWebRtcLoadErrorCode::EmptyPath,
            "an empty library path must be rejected");

    const auto relative =
        helicoid::manager::NativeWebRtcRuntime::loadForTesting("./hs_webrtc_shared");
    require(!relative.runtime && relative.error &&
                relative.error->code == helicoid::manager::NativeWebRtcLoadErrorCode::PathMustBeAbsolute,
            "relative paths must not trigger current-directory library lookup");

    const auto missing = helicoid::manager::NativeWebRtcRuntime::loadForTesting(
        std::filesystem::path(argv[1]).concat(".missing"));
    require(!missing.runtime && missing.error &&
                missing.error->code == helicoid::manager::NativeWebRtcLoadErrorCode::LibraryUnavailable,
            "a missing bundled library must remain unavailable");

    const auto mismatch = helicoid::manager::NativeWebRtcRuntime::loadForTesting(argv[2]);
    require(!mismatch.runtime && mismatch.error &&
                mismatch.error->code == helicoid::manager::NativeWebRtcLoadErrorCode::AbiVersionMismatch,
            "a mismatched ABI must be refused");

    const auto incomplete = helicoid::manager::NativeWebRtcRuntime::loadForTesting(argv[3]);
    require(!incomplete.runtime && incomplete.error &&
                incomplete.error->code == helicoid::manager::NativeWebRtcLoadErrorCode::RequiredCapabilitiesMissing,
            "a library without channel-bound connection capabilities must be refused");

    const auto missingExport = helicoid::manager::NativeWebRtcRuntime::loadForTesting(argv[4]);
    require(!missingExport.runtime && missingExport.error &&
                missingExport.error->code == helicoid::manager::NativeWebRtcLoadErrorCode::MissingRequiredExport,
            "a library without the capability export must be refused");

    const auto missingConnectionExport =
        helicoid::manager::NativeWebRtcRuntime::loadForTesting(argv[5]);
    require(!missingConnectionExport.runtime && missingConnectionExport.error &&
                missingConnectionExport.error->code ==
                    helicoid::manager::NativeWebRtcLoadErrorCode::MissingRequiredExport,
            "a library missing a capability-required connection export must be refused");

    const auto missingObservationExport =
        helicoid::manager::NativeWebRtcRuntime::loadForTesting(argv[6]);
    require(!missingObservationExport.runtime && missingObservationExport.error &&
                missingObservationExport.error->code ==
                    helicoid::manager::NativeWebRtcLoadErrorCode::MissingRequiredExport,
            "a library advertising observation without its media export must be refused");

    const auto failedCapabilityQuery =
        helicoid::manager::NativeWebRtcRuntime::loadForTesting(argv[7]);
    require(!failedCapabilityQuery.runtime && failedCapabilityQuery.error &&
                failedCapabilityQuery.error->code ==
                    helicoid::manager::NativeWebRtcLoadErrorCode::CapabilityQueryFailed &&
                failedCapabilityQuery.error->status == HS_STATUS_UNAVAILABLE,
            "a capability query error must be returned without accepting the library");

    auto valid = helicoid::manager::NativeWebRtcRuntime::loadForTesting(argv[1]);
    require(valid.runtime.has_value(), "the pinned ABI and required connection capabilities must load");
    require(valid.runtime->abiVersion() == HS_ABI_VERSION, "the runtime must report the exact ABI version");
    require(valid.runtime->supportsConnection(), "the runtime must expose channel-bound connection support");
    require(!valid.runtime->supportsObservation(),
            "connection support alone must not claim capture, admission, and media support");

    auto movedRuntime = std::move(*valid.runtime);
    require(!valid.runtime->supportsConnection() &&
                movedRuntime.supportsConnection() &&
                movedRuntime.abiVersion() == HS_ABI_VERSION,
            "moving the runtime must clear stale capability claims from the source");

    std::uint32_t engineStatus = HS_STATUS_INTERNAL;
    require(valid.runtime->createEngine(engineStatus) == nullptr &&
                engineStatus == HS_STATUS_UNAVAILABLE,
            "a moved-from runtime must not initialize an engine");
    auto engine = movedRuntime.createEngine(engineStatus);
    require(engine && engineStatus == HS_STATUS_OK && engine->isOpen(),
            "the validated runtime must initialize and own an engine handle");
    require(engine->drain(250) == HS_STATUS_OK,
            "the engine wrapper must use the ABI drain operation");
    std::uint32_t peerStatus = HS_STATUS_INTERNAL;
    auto peer = engine->createPeer(peerStatus);
    require(peer && peerStatus == HS_STATUS_OK && peer->isOpen(),
            "the owned engine must create a native peer handle");
    auto movedPeer = std::move(peer);
    require(!peer && movedPeer && movedPeer->isOpen(),
            "moving a peer wrapper must preserve its single owned handle");
    std::uint32_t peerValue = 77;
    require(movedPeer->generation(peerValue) == HS_STATUS_UNAVAILABLE && peerValue == 0,
            "a failed peer-generation query must not retain a stale generation");
    peerValue = 77;
    require(movedPeer->state(peerValue) == HS_STATUS_UNAVAILABLE &&
                peerValue == HS_PEER_STATE_CLOSED,
            "a failed peer-state query must fail closed as closed");
    require(movedPeer->createProductDataChannel() == HS_STATUS_UNAVAILABLE,
            "the peer wrapper must return the native channel creation status");
    require(movedPeer->closeProductDataChannel() == HS_STATUS_UNAVAILABLE,
            "the peer wrapper must return the native channel close status");
    require(movedPeer->prepareVideo(0) == HS_STATUS_INVALID,
            "the peer wrapper rejects an unsupported video role");
    require(movedPeer->beginObservation({}, 100) == HS_STATUS_BOUNDS &&
                movedPeer->beginObservation(std::vector<std::uint8_t>{1}, 0) == HS_STATUS_BOUNDS,
            "the peer wrapper rejects empty bindings and zero-length leases");
    require(movedPeer->pushScreenFrame(std::vector<std::uint8_t>(3, 0), 1, 1, 4) ==
                HS_STATUS_BOUNDS &&
                movedPeer->pushScreenFrame(std::vector<std::uint8_t>(4, 0), 1, 1, 3) ==
                    HS_STATUS_BOUNDS,
            "the peer wrapper rejects undersized frame buffers and strides");
    std::string description = "stale";
    require(movedPeer->createOffer(description) == HS_STATUS_UNAVAILABLE && description.empty(),
            "failed offer creation must clear any stale SDP output");
    require(movedPeer->setRemoteDescription("pranswer", "v=0") == HS_STATUS_INVALID,
            "only offer and answer descriptions are accepted");
    require(movedPeer->setRemoteDescription("answer", "") == HS_STATUS_BOUNDS,
            "an empty remote description is rejected");
    require(movedPeer->addIceCandidate("") == HS_STATUS_BOUNDS &&
                movedPeer->addIceCandidate(std::string(4097, 'x')) == HS_STATUS_BOUNDS,
            "ICE candidates obey the signaling byte bound");
    require(movedPeer->pollIceCandidate(description) == HS_STATUS_UNAVAILABLE &&
                description.empty(),
            "failed ICE polling must clear any stale candidate output");
    require(movedPeer->setExpectedRemoteCertificate("") == HS_STATUS_BOUNDS &&
                movedPeer->setExpectedRemoteCertificate(std::string(8193, 'x')) == HS_STATUS_BOUNDS,
            "remote certificate pins obey the native certificate bound");
    require(movedPeer->exportLocalCertificate(description) == HS_STATUS_UNAVAILABLE &&
                description.empty(),
            "failed local certificate export must clear its output");
    require(movedPeer->exportVerifiedRemoteCertificate(description) == HS_STATUS_UNAVAILABLE &&
                description.empty(),
            "a peer without verified channel evidence cannot export remote identity");
    std::vector<std::uint8_t> binding{1};
    require(movedPeer->exportVerifiedChannelBinding(binding) == HS_STATUS_UNAVAILABLE &&
                binding.empty(),
            "failed channel-binding export must clear stale bytes");
    require(movedPeer->sendProductMessage({}) == HS_STATUS_BOUNDS &&
                movedPeer->sendProductMessage(std::vector<std::uint8_t>(4097, 1)) == HS_STATUS_BOUNDS,
            "product data-channel messages obey the native message bound");
    std::vector<std::uint8_t> productMessage{1};
    require(movedPeer->pollProductMessage(productMessage) == HS_STATUS_UNAVAILABLE &&
                productMessage.empty(),
            "failed product-message polling must clear stale bytes");
    helicoid::manager::NativeScreenFrame screenFrame;
    screenFrame.width = 2;
    screenFrame.height = 1;
    screenFrame.bgra = {1, 2, 3, 4, 5, 6, 7, 8};
    require(movedPeer->acquireScreenFrame(screenFrame) == HS_STATUS_UNAVAILABLE &&
                screenFrame.width == 0 && screenFrame.height == 0 && screenFrame.bgra.empty(),
            "an unavailable media bridge must clear stale pixels before returning");
    require(movedPeer->drain(1) == HS_STATUS_TIMEOUT && movedPeer->isOpen(),
            "a timed-out peer drain must retain its handle for a later barrier");
    require(movedPeer->drain(250) == HS_STATUS_OK && !movedPeer->isOpen(),
            "a later proven peer drain must retire its native handle");
    require(movedPeer->drain(250) == HS_STATUS_OK,
            "repeated wrapper peer drain must be harmless");
    auto movedEngine = std::move(engine);
    require(!engine && movedEngine && movedEngine->isOpen(),
            "moving an engine wrapper must preserve its single owned handle");
    require(movedEngine->shutdown() == HS_STATUS_TIMEOUT && !movedEngine->isOpen(),
            "a timed-out engine shutdown must clear the handle and retain its code");
    require(movedEngine->shutdown() == HS_STATUS_OK,
            "repeated wrapper shutdown must be harmless");
    require(movedEngine->drain(250) == HS_STATUS_NULL,
            "a closed engine cannot be drained");
    return 0;
}
