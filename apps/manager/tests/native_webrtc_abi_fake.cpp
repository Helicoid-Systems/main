#include "hs_webrtc.h"

#ifndef HS_TEST_FAKE_ABI_VERSION
#define HS_TEST_FAKE_ABI_VERSION HS_ABI_VERSION
#endif

#ifndef HS_TEST_FAKE_CAPABILITIES
#define HS_TEST_FAKE_CAPABILITIES \
    (HS_CAP_INITIALIZATION | HS_CAP_NEGOTIATION | HS_CAP_CHANNEL_EVIDENCE | HS_CAP_DRAIN)
#endif

#ifndef HS_TEST_CAPABILITY_STATUS
#define HS_TEST_CAPABILITY_STATUS HS_STATUS_OK
#endif

extern "C" {

HS_API uint32_t hs_abi_version(void)
{
    return HS_TEST_FAKE_ABI_VERSION;
}

#if !defined(HS_TEST_OMIT_CAPABILITY_EXPORT)
HS_API uint32_t hs_engine_capabilities(uint32_t* outMask)
{
    if (outMask == nullptr) return HS_STATUS_NULL;
    *outMask = HS_TEST_FAKE_CAPABILITIES;
    return HS_TEST_CAPABILITY_STATUS;
}
#endif

HS_API uint32_t hs_engine_init(hs_engine** out)
{
#if defined(HS_TEST_ENGINE_INIT_SUCCESS)
    if (out == nullptr) return HS_STATUS_NULL;
    *out = reinterpret_cast<hs_engine*>(static_cast<uintptr_t>(1));
    return HS_STATUS_OK;
#else
    (void)out;
    return HS_STATUS_UNAVAILABLE;
#endif
}
HS_API uint32_t hs_engine_shutdown(hs_engine*)
{
#if defined(HS_TEST_ENGINE_SHUTDOWN_TIMEOUT)
    static uint32_t calls = 0;
    if (calls++ == 0) return HS_STATUS_TIMEOUT;
#endif
    return HS_STATUS_OK;
}
HS_API uint32_t hs_engine_drain(hs_engine*, uint32_t) { return HS_STATUS_OK; }
HS_API uint32_t hs_peer_create(hs_engine*, hs_peer** out)
{
#if defined(HS_TEST_PEER_CREATE_SUCCESS)
    if (out == nullptr) return HS_STATUS_NULL;
    *out = reinterpret_cast<hs_peer*>(static_cast<uintptr_t>(2));
    return HS_STATUS_OK;
#else
    (void)out;
    return HS_STATUS_UNAVAILABLE;
#endif
}
HS_API uint32_t hs_peer_video_prepare(hs_peer*, uint32_t) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_close(hs_peer*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_free(hs_peer*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_generation(hs_peer*, uint32_t*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_state(hs_peer*, uint32_t*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_local_cert_export(hs_peer*, uint8_t*, uint32_t, uint32_t*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_channel_create(hs_peer*, const char*, size_t) { return HS_STATUS_UNAVAILABLE; }
#if !defined(HS_TEST_OMIT_PEER_CREATE_OFFER)
HS_API uint32_t hs_peer_create_offer(hs_peer*, char*, size_t, size_t*) { return HS_STATUS_UNAVAILABLE; }
#endif
HS_API uint32_t hs_peer_create_answer(hs_peer*, char*, size_t, size_t*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_set_local(hs_peer*, const char*, size_t) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_set_remote(hs_peer*, const char*, size_t, const char*, size_t) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_add_ice(hs_peer*, const char*, size_t) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_poll_ice(hs_peer*, char*, size_t, size_t*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_channel_send(hs_peer*, const uint8_t*, uint32_t) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_channel_poll(hs_peer*, uint8_t*, uint32_t, uint32_t*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_channel_close(hs_peer*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_expected_cert_set(hs_peer*, const char*, size_t) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_verified_channel_acquire(hs_peer*, hs_verified_channel**) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_verified_channel_export(
    hs_verified_channel*, uint8_t*, uint32_t, uint32_t*)
{
    return HS_STATUS_UNAVAILABLE;
}
HS_API uint32_t hs_verified_channel_binding_export(
    hs_verified_channel*, uint8_t*, uint32_t, uint32_t*)
{
    return HS_STATUS_UNAVAILABLE;
}
HS_API uint32_t hs_verified_channel_generation(hs_verified_channel*, uint32_t*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_verified_channel_valid(hs_verified_channel*, uint32_t*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_verified_channel_release(hs_verified_channel*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_drain(hs_peer*, uint32_t)
{
#if defined(HS_TEST_PEER_DRAIN_TIMEOUT_ONCE)
    static uint32_t calls = 0;
    if (calls++ == 0) return HS_STATUS_TIMEOUT;
#endif
    return HS_STATUS_OK;
}
HS_API uint32_t hs_capture_begin(hs_peer*, uint32_t) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_capture_end(hs_peer*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_admission_begin(hs_peer*, const uint8_t*, uint32_t, uint32_t)
{
    return HS_STATUS_UNAVAILABLE;
}
HS_API uint32_t hs_admission_end(hs_peer*) { return HS_STATUS_UNAVAILABLE; }
HS_API uint32_t hs_peer_video_frame_push(hs_peer*, const uint8_t*, uint32_t,
                                         uint32_t, uint32_t, uint32_t)
{
    return HS_STATUS_UNAVAILABLE;
}
#if !defined(HS_TEST_OMIT_MEDIA_FRAME_EXPORT)
HS_API uint32_t hs_media_frame_acquire(hs_peer*, uint32_t, uint8_t*, uint32_t*, uint32_t*)
{
    return HS_STATUS_UNAVAILABLE;
}
#endif

}
